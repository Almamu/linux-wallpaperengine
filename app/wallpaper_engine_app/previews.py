"""Responsive, cached thumbnails for the GTK library and detail panel.

Only GdkPixbuf decoding runs off the GTK thread. Widgets and the cache are
created and updated on the main loop, so callers can use ``preview`` like any
other GTK widget factory.
"""

from __future__ import annotations

from collections import OrderedDict
from itertools import count
import os
from pathlib import Path
from queue import PriorityQueue
import stat
from threading import Lock, Thread

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("GdkPixbuf", "2.0")
from gi.repository import Gdk, GdkPixbuf, GLib, Gtk


_PreviewKey = tuple[str, int, int]
_Signature = tuple[int, int]
_JobKey = tuple[_PreviewKey, _Signature]

def _worker_count() -> int:
    """Use available CPUs for independent image decodes, leaving UI headroom."""
    try:
        available_cpus = len(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        available_cpus = os.cpu_count() or 2
    return max(1, min(10, available_cpus - 2))


_WORKERS = _worker_count()
_CACHE_ITEMS = 512
# Keep several hundred 240 px thumbnails decoded so revisiting cards avoids I/O.
_CACHE_BYTES = 96 * 1024 * 1024
_GIF_HOVER_DELAY_MS = 120

# The cache is GTK-thread-only; pending jobs are shared with workers and locked.
_cache: OrderedDict[_PreviewKey, tuple[_Signature, GdkPixbuf.Pixbuf | None, int]] = OrderedDict()
_cache_bytes = 0
_pending: dict[_JobKey, list[Gtk.Stack]] = {}
_pending_lock = Lock()
_gif_decode_lock = Lock()
_tasks: PriorityQueue[tuple[int, int, _JobKey]] = PriorityQueue()
_queued_jobs: set[_JobKey] = set()
_inflight_jobs: set[_JobKey] = set()
_sequence = count()
_workers_started = False


def _signature(path: str) -> _Signature | None:
    try:
        info = Path(path).stat()
    except OSError:
        return None
    if not stat.S_ISREG(info.st_mode):
        return None
    return info.st_mtime_ns, info.st_size


def _fit_pixbuf(frame: GdkPixbuf.Pixbuf, width: int, height: int) -> GdkPixbuf.Pixbuf | None:
    """Scale a frame inside the preview area without cropping its contents."""
    source_width, source_height = frame.get_width(), frame.get_height()
    if source_width <= 0 or source_height <= 0:
        return None
    scale = min(width / source_width, height / source_height)
    fitted_width = max(1, round(source_width * scale))
    fitted_height = max(1, round(source_height * scale))
    return frame.scale_simple(fitted_width, fitted_height, GdkPixbuf.InterpType.BILINEAR)


def _decode(path: str, width: int, height: int) -> GdkPixbuf.Pixbuf | None:
    """Decode a preview at display size to limit CPU and memory use."""
    try:
        # Load only the still preview frame here. Animated GIFs use the separate
        # hover path, so their full animation is not parsed while filling rows.
        return GdkPixbuf.Pixbuf.new_from_file_at_scale(path, width, height, True)
    except (GLib.Error, OSError, TypeError, ValueError, MemoryError):
        # A missing or malformed Workshop preview should leave its placeholder.
        return None


def _cache_put(
    key: _PreviewKey, signature: _Signature, pixbuf: GdkPixbuf.Pixbuf | None
) -> None:
    global _cache_bytes
    previous = _cache.pop(key, None)
    if previous is not None:
        _cache_bytes -= previous[2]
    cost = pixbuf.get_rowstride() * pixbuf.get_height() if pixbuf else 0
    if cost > _CACHE_BYTES:
        return
    _cache[key] = (signature, pixbuf, cost)
    _cache_bytes += cost
    while len(_cache) > _CACHE_ITEMS or _cache_bytes > _CACHE_BYTES:
        _, (_, _, evicted_cost) = _cache.popitem(last=False)
        _cache_bytes -= evicted_cost


def _install(stack: Gtk.Stack, pixbuf: GdkPixbuf.Pixbuf | None) -> None:
    if pixbuf is None:
        return
    picture = Gtk.Picture.new_for_pixbuf(pixbuf)
    if hasattr(Gtk, "ContentFit"):
        picture.set_content_fit(Gtk.ContentFit.CONTAIN)
    else:
        picture.set_keep_aspect_ratio(True)
    picture.set_hexpand(True)
    picture.set_vexpand(True)
    stack.add_named(picture, "preview")
    # A static thumbnail may finish decoding after the pointer has entered a
    # GIF preview. Keep the animation visible until its hover ends.
    visible = stack.get_visible_child_name()
    if visible != "animated-preview":
        stack.set_visible_child(picture)


def _deliver(job: _JobKey, pixbuf: GdkPixbuf.Pixbuf | None) -> bool:
    key, signature = job
    with _pending_lock:
        _inflight_jobs.discard(job)
        waiting = _pending.pop(job, [])
    for stack in waiting:
        stack._preview_job = None
    # A Workshop update can replace a preview while a decode is in flight.
    if _signature(key[0]) != signature:
        return False
    _cache_put(key, signature, pixbuf)
    for stack in waiting:
        _install(stack, pixbuf)
    return False


def _worker() -> None:
    while True:
        _priority, _order, job = _tasks.get()
        scheduled = False
        try:
            # Grid views release previews as cells are recycled. Avoid decoding
            # jobs that have not started yet and no longer have a live widget.
            with _pending_lock:
                _queued_jobs.discard(job)
                active = job in _pending
                if active:
                    _inflight_jobs.add(job)
            if not active:
                continue
            key, _signature_value = job
            pixbuf = _decode(*key)
            GLib.idle_add(_deliver, job, pixbuf)
            scheduled = True
        finally:
            if not scheduled:
                with _pending_lock:
                    _inflight_jobs.discard(job)
            _tasks.task_done()


def _start_workers() -> None:
    global _workers_started
    if _workers_started:
        return
    _workers_started = True
    for index in range(_WORKERS):
        Thread(target=_worker, name=f"wallpaper-preview-{index}", daemon=True).start()


def preview(
    path: str | None,
    width: int,
    height: int,
    *,
    animation_path: str | None = None,
    hover_target: Gtk.Widget | None = None,
) -> Gtk.Widget:
    """Return a fixed-size widget that fills asynchronously with a first frame.

    Repeated requests for the same file and dimensions share decoding work and
    an LRU thumbnail. A changed file invalidates its previous cache entry.
    This function must be called on GTK's main thread.
    """
    width, height = max(1, int(width)), max(1, int(height))
    stack = Gtk.Stack()
    stack.add_css_class("wallpaper-preview")
    stack.set_size_request(width, height)
    stack.set_hexpand(False)
    stack.set_vexpand(False)
    # Leaving a GIF must detach its picture without an in-flight transition;
    # doing both at once can leave GTK drawing a removed child.
    stack.set_transition_type(Gtk.StackTransitionType.NONE)
    stack.set_transition_duration(0)
    placeholder = Gtk.Image.new_from_icon_name("image-x-generic-symbolic")
    placeholder.set_pixel_size(min(width, height) // 2)
    placeholder.add_css_class("dim-label")
    stack.add_named(placeholder, "placeholder")

    if not path:
        return stack
    signature = _signature(path)
    if signature is None:
        return stack

    def enable_gif_hover() -> None:
        gif_path = animation_path or (path if Path(path).suffix.casefold() == ".gif" else None)
        if not gif_path or Path(gif_path).suffix.casefold() != ".gif":
            return
        controller = Gtk.EventControllerMotion()
        # The preview sits below overlays (badges and controls). Capture the
        # pointer event at the preview surface so those children cannot hide
        # hover transitions.
        controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        state: dict[str, object] = {
            "animation": None,
            "iterator": None,
            "animated_picture": None,
            "ticker": 0,
            "hover_timer": 0,
            "last_frame": None,
            "loading": False,
            "hovering": False,
            "released": False,
        }

        def start_animation(
            animation: GdkPixbuf.PixbufAnimation, *, reset: bool = True
        ) -> None:
            if not state["hovering"]:
                return
            ticker = state.get("ticker")
            if isinstance(ticker, int) and ticker:
                GLib.source_remove(ticker)
                state["ticker"] = 0
            picture = state.get("animated_picture")
            if not isinstance(picture, Gtk.Picture):
                picture = Gtk.Picture()
                picture.set_size_request(width, height)
                picture.set_hexpand(True)
                picture.set_vexpand(True)
                picture.set_can_shrink(True)
                if hasattr(Gtk, "ContentFit"):
                    picture.set_content_fit(Gtk.ContentFit.CONTAIN)
                else:
                    picture.set_keep_aspect_ratio(True)
                state["animated_picture"] = picture
                stack.add_named(picture, "animated-preview")
            iterator = state["iterator"]
            if reset or iterator is None:
                iterator = animation.get_iter(None)
                state["iterator"] = iterator
                state["last_frame"] = None
            stack.set_visible_child(picture)

            def advance_frame() -> bool:
                state["ticker"] = 0
                if not state["hovering"]:
                    return False
                current = state["iterator"]
                target = state["animated_picture"]
                if current is None or target is None:
                    return False
                changed = current.advance(None)
                frame = current.get_pixbuf()
                # GdkPixbuf may return the same Pixbuf wrapper while mutating
                # its pixels for the next GIF frame. Use the iterator's
                # changed flag instead of Python object identity.
                if changed or state["last_frame"] is None:
                    fitted = _fit_pixbuf(frame, width, height)
                    if fitted is not None:
                        texture = Gdk.Texture.new_for_pixbuf(fitted)
                        target.set_paintable(texture)
                    state["last_frame"] = frame
                # Follow the GIF's frame delay instead of waking GTK on a
                # fixed 40 ms loop for animations with fewer frames.
                delay = max(20, int(current.get_delay_time()))
                state["ticker"] = GLib.timeout_add(delay, advance_frame)
                return False

            advance_frame()

        def deliver_animation(animation: GdkPixbuf.PixbufAnimation | None) -> bool:
            state["loading"] = False
            if (
                state["released"]
                or not state["hovering"]
                or animation is None
                or animation.is_static_image()
            ):
                return False
            state["animation"] = animation
            start_animation(animation)
            return False

        def load_animation() -> None:
            animation: GdkPixbuf.PixbufAnimation | None = None
            try:
                animation = GdkPixbuf.PixbufAnimation.new_from_file(gif_path)
            except (GLib.Error, OSError, TypeError, ValueError, MemoryError):
                pass
            finally:
                _gif_decode_lock.release()
            GLib.idle_add(deliver_animation, animation)

        def begin_animation_load() -> bool:
            state["hover_timer"] = 0
            if (
                not state["hovering"] or state["released"] or state["loading"]
            ):
                return False
            if not _gif_decode_lock.acquire(blocking=False):
                # Keep only the currently hovered preview waiting for the
                # decoder, instead of starting one thread per card.
                state["hover_timer"] = GLib.timeout_add(
                    _GIF_HOVER_DELAY_MS, begin_animation_load
                )
                return False
            state["loading"] = True
            try:
                Thread(target=load_animation, name="wallpaper-gif-preview", daemon=True).start()
            except RuntimeError:
                state["loading"] = False
                _gif_decode_lock.release()
            return False

        def schedule_animation_load() -> None:
            if (
                state["hovering"] and not state["loading"]
                and not state["hover_timer"]
            ):
                state["hover_timer"] = GLib.timeout_add(
                    _GIF_HOVER_DELAY_MS, begin_animation_load
                )

        def enter(_controller: Gtk.EventControllerMotion, _x: float, _y: float) -> None:
            state["hovering"] = True
            animation = state["animation"]
            if isinstance(animation, GdkPixbuf.PixbufAnimation):
                start_animation(animation)
                return
            # Ignore brief pointer crossings while scrolling; load only when
            # the pointer stays over this preview for the hover delay.
            schedule_animation_load()

        def leave(_controller: Gtk.EventControllerMotion) -> None:
            state["hovering"] = False
            hover_timer = state.get("hover_timer")
            if isinstance(hover_timer, int) and hover_timer:
                GLib.source_remove(hover_timer)
                state["hover_timer"] = 0
            ticker = state.get("ticker")
            if isinstance(ticker, int) and ticker:
                GLib.source_remove(ticker)
                state["ticker"] = 0
            picture = state.get("animated_picture")
            if isinstance(picture, Gtk.Picture):
                picture.set_paintable(None)
            if stack.get_child_by_name("preview") is not None:
                stack.set_visible_child_name("preview")
            else:
                stack.set_visible_child_name("placeholder")
            state["iterator"] = None
            state["last_frame"] = None
            state["animation"] = None

        controller.connect("enter", enter)
        controller.connect("leave", leave)
        target = hover_target if hover_target is not None else stack
        target.add_controller(controller)
        stack._preview_controller = controller
        stack._preview_hover_target = target
        stack._preview_animation_state = state

    # Attach hover handling before cache/pending early returns, so duplicate
    # requests for the same preview behave exactly like the first one.
    enable_gif_hover()

    key = (path, width, height)
    cached = _cache.get(key)
    if cached is not None and cached[0] == signature:
        _cache.move_to_end(key)
        _install(stack, cached[1])
        return stack

    job = (key, signature)
    enqueue = False
    with _pending_lock:
        waiting = _pending.get(job)
        if waiting is not None:
            waiting.append(stack)
            stack._preview_job = job
            return stack
        _pending[job] = [stack]
        stack._preview_job = job
        if job not in _queued_jobs and job not in _inflight_jobs:
            _queued_jobs.add(job)
            enqueue = True
    if enqueue:
        _start_workers()
        # Detail previews jump ahead of a large gallery's remaining thumbnails.
        priority = 0 if width >= 250 else 1
        _tasks.put((priority, next(_sequence), job))
    return stack


def release(stack: Gtk.Stack) -> None:
    """Detach a recycled preview widget and release its pending work."""
    job = getattr(stack, "_preview_job", None)
    if job is not None:
        with _pending_lock:
            waiting = _pending.get(job)
            if waiting is not None:
                waiting[:] = [candidate for candidate in waiting if candidate is not stack]
                if not waiting:
                    _pending.pop(job, None)
        stack._preview_job = None

    state = getattr(stack, "_preview_animation_state", None)
    if isinstance(state, dict):
        state["released"] = True
        state["hovering"] = False
        hover_timer = state.get("hover_timer")
        if isinstance(hover_timer, int) and hover_timer:
            GLib.source_remove(hover_timer)
        ticker = state.get("ticker")
        if isinstance(ticker, int) and ticker:
            GLib.source_remove(ticker)
        picture = state.get("animated_picture")
        if isinstance(picture, Gtk.Picture):
            picture.set_paintable(None)
        state["ticker"] = 0
        state["hover_timer"] = 0
        state["animated_picture"] = None
        state["last_frame"] = None
        state["iterator"] = None
        state["animation"] = None

    controller = getattr(stack, "_preview_controller", None)
    target = getattr(stack, "_preview_hover_target", None)
    if controller is not None and target is not None:
        target.remove_controller(controller)
    stack._preview_controller = None
    stack._preview_hover_target = None
    stack._preview_animation_state = None

    child = stack.get_first_child()
    while child is not None:
        following = child.get_next_sibling()
        stack.remove(child)
        child = following
