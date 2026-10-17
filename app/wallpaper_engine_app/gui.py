"""GTK4 control panel for the Linux Wallpaper Engine desktop service."""

from __future__ import annotations

import colorsys
import math
import os
import subprocess
import threading
import time
from typing import Callable

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Gdk, Gio, GLib, Gtk, Pango

from . import ipc, model
from .backdrop import enable_backdrop_blur
from .i18n import current_language, language_options, set_language, system_language, tr
from .previews import preview as _preview, release as _release_preview
from .theme import install_theme
from .tray import TrayBridge


SERVICE = "linux-wallpaperengine-app.service"
# Keep the v0.0.15 card dimensions and square preview tile.
CARD_WIDTH = 240
CARD_PREVIEW_HEIGHT = CARD_WIDTH
CARD_HEIGHT = CARD_PREVIEW_HEIGHT + 56
FILTERS = ("Todos", "Cenas", "Vídeos", "Favoritos")
SCALINGS = ("fill", "fit", "stretch", "default")
PAGES = {
    "library": ("Biblioteca", "Seus wallpapers instalados, prontos para usar."),
    "playlists": ("Playlists", "Organize a rotação do seu jeito."),
    "languages": ("Idiomas", "Escolha o idioma da interface."),
    "settings": ("Configurações", "Ajuste a reprodução e o aplicativo."),
}

def _label(text: str, *, css: str | None = None, wrap: bool = False) -> Gtk.Label:
    widget = Gtk.Label(label=text, xalign=0)
    widget.set_wrap(wrap)
    if wrap:
        widget.set_wrap_mode(Pango.WrapMode.WORD_CHAR)
    if css:
        widget.add_css_class(css)
    return widget


def _box(*, vertical: bool = False, spacing: int = 0) -> Gtk.Box:
    return Gtk.Box(
        orientation=Gtk.Orientation.VERTICAL if vertical else Gtk.Orientation.HORIZONTAL,
        spacing=spacing,
    )


def _badge(text: str, css: str | None = None) -> Gtk.Label:
    badge = _label(text, css="pill")
    if css:
        badge.add_css_class(css)
    return badge


def _icon_button(label: str, icon: str, *, tooltip: str | None = None) -> Gtk.Button:
    button = Gtk.Button()
    content = _box(spacing=7)
    content.set_valign(Gtk.Align.CENTER)
    content.append(Gtk.Image.new_from_icon_name(icon))
    content.append(_label(label))
    button.set_child(content)
    if tooltip:
        button.set_tooltip_text(tooltip)
    return button


def _release_previews_in(widget: Gtk.Widget) -> None:
    if isinstance(widget, Gtk.Stack) and (
        getattr(widget, "_preview_job", None) is not None
        or getattr(widget, "_preview_animation_state", None) is not None
        or getattr(widget, "_preview_controller", None) is not None
        or widget.has_css_class("wallpaper-preview")
    ):
        _release_preview(widget)
        return
    child = widget.get_first_child()
    while child is not None:
        following = child.get_next_sibling()
        _release_previews_in(child)
        child = following


def _clear(container: Gtk.Box | Gtk.FlowBox | Gtk.ListBox) -> None:
    child = container.get_first_child()
    while child is not None:
        following = child.get_next_sibling()
        _release_previews_in(child)
        container.remove(child)
        child = following


class WallpaperWindow(Gtk.ApplicationWindow):
    def __init__(self, app: Gtk.Application):
        super().__init__(application=app, title="Linux Wallpaper Engine")
        monitor = self.get_display().get_monitors().get_item(0)
        if monitor is not None:
            bounds = monitor.get_geometry()
            self._initial_size = (min(1440, int(bounds.width * 0.9)),
                                  min(870, int(bounds.height * 0.9)))
        else:
            self._initial_size = (1100, 720)
        self.set_default_size(*self._initial_size)
        self.set_size_request(680, 440)
        self.add_css_class("glass-window")
        # Apply the KWin hint once the X11/XWayland surface is mapped and has
        # a stable XID. The helper safely no-ops on other compositors.
        self.connect("map", lambda *_: enable_backdrop_blur(self))

        self.catalog: dict[str, dict] = {}
        self.config: dict = model.load_config()
        self._ui_preferences = model.load_ui_preferences(self.config)
        self.config.update({key: self._ui_preferences[key] for key in ("ui_hue", "ui_intensity")})
        set_language(self.config.get("language", "auto"))
        self.status: dict = {}
        self.selected_id: str | None = None
        self.service_available = False
        self._updating_controls = False
        self._pending_status = False
        self._catalog_generation = 0
        self._cards: dict[str, Gtk.Widget] = {}
        self._card_badges: dict[str, Gtk.Box] = {}
        self._gallery_all_ids: list[str] = []
        self._gallery_search: dict[str, str] = {}
        self._gallery_visible_ids: list[str] = []
        self._gallery_visible_set: set[str] = set()
        self._last_filtered_ids: list[str] | None = None
        self._selected_ids: set[str] = set()
        self._selection_anchor_id: str | None = None
        self._gallery_click_modifiers: Gdk.ModifierType | None = None
        self._gallery_click_on_button = False
        self._selection_rebuilding = False
        self._toast_timer = 0
        self.filter_index = 0
        self._filter_buttons: list[Gtk.Button] = []
        self._nav_buttons: dict[str, Gtk.Button] = {}
        self._nav_labels: list[Gtk.Label] = []
        self._compact_mode: str | None = None
        self._selection_compact = False
        self._last_height_band: tuple[bool, bool] | None = None
        self._last_pane_width: int | None = None
        self.playlist_name: str | None = None
        self._updating_playlists = False
        self._catalog_scan_pending = False
        self._command_queue: list[tuple[str, dict[str, object]]] = []
        self._command_busy = False
        self._mutation_generation = 0
        self._spin_timers: dict[str, int] = {}
        self._theme_save_timer = 0
        self._theme_updating = False
        self._ui_language = self.config.get("language", "auto")
        self._last_applied_config: dict | None = None
        self._tray_bridge: TrayBridge | None = None
        self._allow_close = False

        self._build()
        self.connect("close-request", self._close_requested)
        if self._ui_preferences.get("minimize_to_tray"):
            try:
                self._start_tray_bridge()
            except (OSError, RuntimeError) as exc:
                self._ui_preferences["minimize_to_tray"] = False
                self._set_switch_value(self.minimize_to_tray_switch, False)
                try:
                    self._ui_preferences = model.save_ui_preferences({"minimize_to_tray": False})
                except OSError:
                    pass
                self._notice(tr("Não foi possível iniciar a bandeja: {error}", error=exc), error=True)
        self._load_catalog()
        self._refresh_status()
        self._refresh_autostart()
        GLib.timeout_add_seconds(2, self._tick)
        GLib.timeout_add_seconds(15, self._check_library_changes)
        GLib.timeout_add(180, self._update_responsive)

    def _build(self) -> None:
        self._nav_buttons.clear()
        self._nav_labels.clear()
        self._filter_buttons.clear()
        self._cards.clear()
        self._card_badges.clear()
        self._gallery_filter = None
        self._language_buttons: dict[str, tuple[Gtk.Button, Gtk.Image]] = {}
        if not hasattr(self, "_theme_provider"):
            self._theme_provider = install_theme(self.get_display())
            self._dynamic_theme_provider = Gtk.CssProvider()
            Gtk.StyleContext.add_provider_for_display(
                self.get_display(), self._dynamic_theme_provider,
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION + 1,
            )
        icon_theme = Gtk.IconTheme.get_for_display(self.get_display())
        app_icon = (
            "linux-wallpaperengine-app" if icon_theme.has_icon("linux-wallpaperengine-app")
            else "preferences-desktop-wallpaper-symbolic"
        )

        header = Gtk.HeaderBar()
        header.add_css_class("topbar")
        header.set_show_title_buttons(False)
        header.set_title_widget(_label("Wallpaper Engine", css="section-title"))

        def window_button(label: str, symbol: str) -> tuple[Gtk.Button, Gtk.DrawingArea]:
            button = Gtk.Button()
            button.add_css_class("window-action")
            button.set_tooltip_text(tr(label))
            button.update_property([Gtk.AccessibleProperty.LABEL], [tr(label)])
            icon = Gtk.DrawingArea()
            icon.set_content_width(16)
            icon.set_content_height(16)

            def draw_icon(_area: Gtk.DrawingArea, cr: object,
                          width: int, height: int) -> None:
                color = button.get_style_context().get_color()
                cr.set_source_rgba(color.red, color.green, color.blue, color.alpha)
                cr.set_line_width(1.6)
                cr.set_line_cap(1)
                cr.set_line_join(1)
                cr.translate((width - 16) / 2, (height - 16) / 2)
                active_symbol = "restore" if symbol == "maximize" and self.is_maximized() else symbol
                if active_symbol == "minimize":
                    cr.move_to(3, 11.5)
                    cr.line_to(13, 11.5)
                elif active_symbol == "maximize":
                    cr.rectangle(3.5, 3.5, 9, 9)
                elif active_symbol == "restore":
                    cr.move_to(6, 3.5)
                    cr.line_to(12.5, 3.5)
                    cr.line_to(12.5, 10)
                    cr.rectangle(3.5, 5.5, 7, 7)
                else:
                    cr.move_to(3.5, 3.5)
                    cr.line_to(12.5, 12.5)
                    cr.move_to(12.5, 3.5)
                    cr.line_to(3.5, 12.5)
                cr.stroke()

            icon.set_draw_func(draw_icon)
            button.set_child(icon)
            return button, icon

        window_actions = _box(spacing=5)
        window_actions.add_css_class("window-actions")
        window_actions.set_valign(Gtk.Align.CENTER)
        minimize_button, _minimize_icon = window_button("Minimizar", "minimize")
        minimize_button.connect("clicked", lambda *_: self.minimize())
        window_actions.append(minimize_button)

        maximize_button, maximize_icon = window_button("Maximizar", "maximize")

        def toggle_maximized(*_args: object) -> None:
            if self.is_maximized():
                self.unmaximize()
            else:
                self.maximize()

        def sync_maximize_button(*_args: object) -> None:
            maximized = self.is_maximized()
            label = tr("Restaurar" if maximized else "Maximizar")
            maximize_icon.queue_draw()
            maximize_button.set_tooltip_text(label)
            maximize_button.update_property([Gtk.AccessibleProperty.LABEL], [label])

        maximize_button.connect("clicked", toggle_maximized)
        window_actions.append(maximize_button)

        close_button, _close_icon = window_button("Fechar", "close")
        close_button.add_css_class("window-action-close")
        close_button.connect("clicked", lambda *_: self.close())
        window_actions.append(close_button)
        header.pack_end(window_actions)
        self.theme_button = Gtk.MenuButton()
        self.theme_button.add_css_class("theme-trigger")
        theme_icon = Gtk.DrawingArea()
        theme_icon.set_content_width(18)
        theme_icon.set_content_height(18)

        def draw_theme_icon(_area: Gtk.DrawingArea, cr: object,
                            width: int, height: int) -> None:
            cr.translate((width - 18) / 2, (height - 18) / 2)
            cr.set_source_rgb(0.74, 0.82, 0.88)
            cr.set_line_width(1.5)
            cr.arc(9, 9, 7, 0, 2 * math.pi)
            cr.stroke()
            for x, y, red, green, blue in (
                (6, 7, 0.46, 0.88, 0.82),
                (11, 6, 0.97, 0.72, 0.32),
                (10, 11, 0.68, 0.48, 0.94),
            ):
                cr.set_source_rgb(red, green, blue)
                cr.arc(x, y, 1.5, 0, 2 * math.pi)
                cr.fill()

        theme_icon.set_draw_func(draw_theme_icon)
        self.theme_button.set_child(theme_icon)
        self.theme_button.set_tooltip_text(tr("Personalizar tema"))
        self.theme_button.update_property(
            [Gtk.AccessibleProperty.LABEL], [tr("Personalizar tema")]
        )
        self.theme_button.set_popover(self._build_theme_popover())
        self.theme_button.set_valign(Gtk.Align.CENTER)
        header.pack_end(self.theme_button)
        if hasattr(self, "_maximize_handler_id"):
            self.disconnect(self._maximize_handler_id)
        self._maximize_handler_id = self.connect("notify::maximized", sync_maximize_button)
        sync_maximize_button()
        self.set_titlebar(header)

        shell = _box(spacing=0)
        shell.add_css_class("app-shell")
        shell.add_css_class("app-root")
        self.set_child(shell)

        sidebar = _box(vertical=True, spacing=8)
        sidebar.add_css_class("app-sidebar")
        sidebar.add_css_class("sidebar")
        sidebar.set_size_request(216, -1)
        self.sidebar = sidebar
        shell.append(sidebar)

        brand_block = _box(spacing=10)
        brand_block.add_css_class("sidebar-brand")
        brand_icon = Gtk.Image.new_from_icon_name("preferences-desktop-wallpaper-symbolic")
        brand_icon.set_pixel_size(24)
        brand_icon.add_css_class("brand-mark")
        brand_block.append(brand_icon)
        brand_copy = _box(vertical=True, spacing=0)
        brand_copy.append(_label("Wallpaper Engine", css="brand-overline"))
        brand_copy.append(_label("for Linux", css="brand-display"))
        brand_block.append(brand_copy)
        sidebar.append(brand_block)
        self.sidebar_brand = brand_block
        self.sidebar_compact_logo = Gtk.Image.new_from_icon_name(app_icon)
        self.sidebar_compact_logo.set_pixel_size(26)
        self.sidebar_compact_logo.set_visible(False)
        sidebar.append(self.sidebar_compact_logo)
        self.sidebar_caption = _label(tr("ESPAÇO DE TRABALHO"), css="nav-caption")
        self.sidebar_caption.set_visible(False)
        sidebar.append(self.sidebar_caption)

        self.tabs = Gtk.Stack()
        self.tabs.set_hhomogeneous(False)
        self.tabs.set_vhomogeneous(False)
        self.tabs.set_hexpand(True)
        self.tabs.set_vexpand(True)
        for name, label, icon in (
            ("library", "Biblioteca", "view-grid-symbolic"),
            ("playlists", "Playlists", "view-list-symbolic"),
            ("languages", "Idiomas", "preferences-desktop-locale-symbolic"),
            ("settings", "Configurações", "preferences-system-symbolic"),
        ):
            button = _icon_button(tr(label), icon)
            button.add_css_class("nav-item")
            button.add_css_class("nav-button")
            button.connect("clicked", lambda _button, target=name: self.tabs.set_visible_child_name(target))
            button.set_tooltip_text(tr(label))
            nav_label = button.get_child().get_last_child()
            nav_label.set_ellipsize(Pango.EllipsizeMode.END)
            nav_label.set_max_width_chars(17)
            self._nav_labels.append(nav_label)
            sidebar.append(button)
            self._nav_buttons[name] = button

        spacer = _box()
        spacer.set_vexpand(True)
        sidebar.append(spacer)
        source = _box(vertical=True, spacing=6)
        source.add_css_class("sidebar-source")
        source.append(_badge(tr("BIBLIOTECA STEAM"), "source-badge"))
        source_text = _label(tr("Assine novos wallpapers no Wallpaper Engine original."), wrap=True)
        source_text.set_max_width_chars(20)
        source.append(source_text)
        sidebar.append(source)
        self.sidebar_source = source
        source.set_visible(False)
        self.sidebar_library_count = _label(tr("Lendo biblioteca…"), css="sidebar-count")
        self.sidebar_library_count.set_visible(False)
        sidebar.append(self.sidebar_library_count)

        main = _box(vertical=True, spacing=0)
        main.add_css_class("workspace")
        main.add_css_class("main-area")
        main.set_hexpand(True)
        main.set_vexpand(True)
        shell.append(main)

        top = _box(spacing=14)
        top.add_css_class("workspace-header")
        titles = _box(vertical=True, spacing=4)
        titles.set_hexpand(True)
        self.page_title = _label(tr("Biblioteca"), css="page-title")
        self.page_subtitle = _label(tr(PAGES["library"][1]), css="subtle")
        titles.append(self.page_title)
        titles.append(self.page_subtitle)
        top.append(titles)

        self.reload_button = _icon_button(tr("Atualizar"), "view-refresh-symbolic", tooltip=tr("Atualizar biblioteca · Ctrl+R"))
        self.reload_label = self.reload_button.get_child().get_last_child()
        self.reload_button.add_css_class("compact-button")
        self.reload_button.connect("clicked", lambda *_: self._reload())
        header.pack_end(self.reload_button)

        self.next_button = _icon_button(tr("Próximo"), "media-skip-forward-symbolic")
        self.next_label = self.next_button.get_child().get_last_child()
        self.next_button.add_css_class("compact-button")
        self.next_button.connect("clicked", lambda *_: self._command("next"))
        header.pack_end(self.next_button)

        self.power_button = Gtk.Button(label=tr("Iniciar"))
        self.power_button.add_css_class("primary-action")
        self.power_button.connect("clicked", self._toggle_power)
        header.pack_end(self.power_button)
        main.append(top)

        self.status_strip = _box(vertical=True, spacing=7)
        self.status_strip.add_css_class("status-strip")
        status_head = _box(spacing=7)
        self.status_dot = _label("●", css="status-dot")
        status_head.append(self.status_dot)
        status_head.append(_label(tr("Estado do motor"), css="section-title"))
        self.status_strip.append(status_head)
        self.status_label = _label(tr("Verificando serviço…"))
        self.status_label.set_max_width_chars(26)
        self.status_label.set_wrap(True)
        self.status_strip.append(self.status_label)
        self.countdown_label = _label("")
        self.countdown_label.add_css_class("subtle")
        self.status_strip.append(self.countdown_label)
        sidebar.append(self.status_strip)

        self.tabs.add_titled(self._build_library(), "library", tr("Biblioteca"))
        self.tabs.add_titled(self._build_playlists(), "playlists", tr("Playlists"))
        self.tabs.add_titled(self._build_languages(), "languages", tr("Idiomas"))
        self.tabs.add_titled(self._build_settings(), "settings", tr("Configurações"))
        self.tabs.connect("notify::visible-child-name", self._page_changed)
        self.tabs.set_visible_child_name("library")
        self._page_changed(self.tabs, None)
        main.append(self.tabs)

        self.message = _label("")
        self.message.set_visible(False)
        self.message.set_wrap(True)
        self.message.add_css_class("toast")
        main.append(self.message)

        if not hasattr(self, "_keys"):
            self._keys = Gtk.EventControllerKey()
            self._keys.connect("key-pressed", self._key_pressed)
            self.add_controller(self._keys)
        self._apply_theme_preview()
        # Apply the monitor's compact layout before the first size negotiation.
        # Otherwise the wide sidebar can force a window larger than a HiDPI screen.
        self._update_responsive()

    def _build_theme_popover(self) -> Gtk.Popover:
        popover = Gtk.Popover()
        popover.add_css_class("theme-popover")
        body = _box(vertical=True, spacing=11)
        body.add_css_class("theme-popover")

        heading = _box(spacing=8)
        title = _label(tr("Tema"), css="section-title")
        title.set_hexpand(True)
        heading.append(title)
        reset = Gtk.Button(label=tr("Redefinir"))
        reset.connect("clicked", lambda *_: self._set_theme_preset(24, 88))
        heading.append(reset)
        body.append(heading)

        preview = _box()
        preview.add_css_class("theme-preview")
        body.append(preview)

        body.append(_label(tr("Matiz"), css="subtle"))
        self.theme_hue = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 360, 1)
        self.theme_hue.set_draw_value(False)
        self.theme_hue.set_has_origin(False)
        self.theme_hue.set_value(int(self.config.get("ui_hue", 24)))
        self.theme_hue.add_css_class("hue-slider")
        self.theme_hue.connect("value-changed", self._theme_changed)
        body.append(self.theme_hue)

        body.append(_label(tr("Intensidade"), css="subtle"))
        self.theme_intensity = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 35, 100, 1)
        self.theme_intensity.set_draw_value(False)
        self.theme_intensity.set_value(int(self.config.get("ui_intensity", 88)))
        self.theme_intensity.add_css_class("intensity-slider")
        self.theme_intensity.connect("value-changed", self._theme_changed)
        body.append(self.theme_intensity)

        body.append(_label(tr("Predefinições"), css="subtle"))
        presets = _box(spacing=8)
        for hue, intensity, name, css_class in (
            (24, 88, "Âmbar", "preset-amber"),
            (38, 94, "Dourado", "preset-gold"),
            (8, 92, "Vermelho", "preset-red"),
            (285, 82, "Violeta", "preset-violet"),
            (215, 82, "Azul", "preset-blue"),
            (150, 76, "Verde", "preset-green"),
        ):
            button = Gtk.Button(label="●")
            button.add_css_class("preset-dot")
            button.add_css_class(css_class)
            button.set_tooltip_text(tr(name))
            button.update_property([Gtk.AccessibleProperty.LABEL], [tr(name)])
            button.connect(
                "clicked", lambda _button, h=hue, i=intensity: self._set_theme_preset(h, i)
            )
            presets.append(button)
        body.append(presets)
        popover.set_child(body)
        return popover

    def _set_theme_preset(self, hue: int, intensity: int) -> None:
        self.theme_hue.set_value(hue)
        self.theme_intensity.set_value(intensity)

    def _theme_changed(self, _widget: Gtk.Scale) -> None:
        if self._theme_updating:
            return
        self._apply_theme_preview()
        if self._theme_save_timer:
            GLib.source_remove(self._theme_save_timer)
        self._theme_save_timer = GLib.timeout_add(350, self._commit_theme)

    def _commit_theme(self) -> bool:
        self._theme_save_timer = 0
        settings = {
            "ui_hue": int(round(self.theme_hue.get_value())),
            "ui_intensity": int(round(self.theme_intensity.get_value())),
        }
        try:
            self._ui_preferences = model.save_ui_preferences(settings)
            self.config.update(settings)
            self._apply_config()
        except (OSError, ValueError) as exc:
            self._notice(tr("Não foi possível salvar o tema: {error}", error=exc), error=True)
        return False

    @staticmethod
    def _theme_rgb(hue: float, saturation: float, value: float) -> str:
        red, green, blue = colorsys.hsv_to_rgb((hue % 360) / 360.0, saturation, value)
        return f"#{round(red * 255):02x}{round(green * 255):02x}{round(blue * 255):02x}"

    def _apply_theme_preview(self) -> None:
        hue = float(self.theme_hue.get_value())
        intensity = float(self.theme_intensity.get_value())
        value = 0.62 + (max(35.0, min(100.0, intensity)) - 35.0) / 65.0 * 0.38
        accent = self._theme_rgb(hue, 0.92, value)
        accent2 = self._theme_rgb(hue + 26, 0.86, min(1.0, value + 0.04))
        hot = self._theme_rgb(hue - 13, 0.94, min(1.0, value + 0.02))
        glow = self._theme_rgb(hue, 0.75, min(1.0, value))
        dynamic_css = f"""
        .accent-button,
        button.primary-action,
        button.suggested-action,
        button.filter-chip-active,
        button.filter-chip-active:hover,
        button.nav-item-active,
        button.nav-item-active:hover {{
          color: #ffffff;
          background: linear-gradient(110deg, {accent} 0%, {accent2} 58%, {hot} 100%);
          border-color: {accent};
          box-shadow: 0 4px 18px alpha({glow}, 0.22);
        }}
        .wallpaper-card.wallpaper-card-selected,
        .wallpaper-card.wallpaper-card-selected:hover {{
          border: 2px solid {accent};
          box-shadow: 0 0 0 1px alpha({accent2}, 0.32), 0 7px 24px alpha({glow}, 0.22);
        }}
        .pill-accent, .badge-accent {{ border-color: alpha({accent}, 0.38); }}
        .theme-preview {{
          background: linear-gradient(110deg, {hot} 0%, {accent2} 52%, {accent} 100%);
          box-shadow: inset 0 0 0 1px alpha({accent2}, 0.18);
        }}
        .selection-count {{ color: {accent}; }}
        scale.intensity-slider highlight {{
          background: linear-gradient(90deg, {accent}, {accent2});
        }}
        selection {{ background-color: {accent}; }}
        """
        self._dynamic_theme_provider.load_from_data(dynamic_css.encode())

    def _page_changed(self, stack: Gtk.Stack, _property: object) -> None:
        name = stack.get_visible_child_name() or "library"
        title, subtitle = PAGES[name]
        self.page_title.set_text(tr(title))
        self.page_subtitle.set_text(tr(subtitle))
        for page, button in self._nav_buttons.items():
            if page == name:
                button.add_css_class("nav-item-active")
                button.add_css_class("active")
            else:
                button.remove_css_class("nav-item-active")
                button.remove_css_class("active")

    def _key_pressed(self, _controller: Gtk.EventControllerKey, keyval: int,
                     _keycode: int, state: Gdk.ModifierType) -> bool:
        ctrl = bool(state & Gdk.ModifierType.CONTROL_MASK)
        if ctrl and keyval in (Gdk.KEY_f, Gdk.KEY_F):
            self.tabs.set_visible_child_name("library")
            self.search.grab_focus()
            return True
        if ctrl and keyval in (Gdk.KEY_r, Gdk.KEY_R):
            self._reload()
            return True
        if ctrl and keyval in (Gdk.KEY_n, Gdk.KEY_N):
            self.tabs.set_visible_child_name("playlists")
            self._playlist_name_dialog(tr("Criar playlist"), None)
            return True
        if ctrl and keyval in (Gdk.KEY_1, Gdk.KEY_2, Gdk.KEY_3, Gdk.KEY_4):
            self.tabs.set_visible_child_name(
                ("library", "playlists", "languages", "settings")[keyval - Gdk.KEY_1]
            )
            return True
        if keyval == Gdk.KEY_Escape and self.search.has_focus():
            self.search.set_text("")
            return True
        return False

    def _responsive_geometry(self) -> tuple[int, int, str, int, tuple[bool, bool]]:
        width, height = self.get_width(), self.get_height()
        if width <= 0:
            width = self._initial_size[0]
        if height <= 0:
            height = self._initial_size[1]
        mode = "small" if width < 1100 else "compact" if width < 1200 else "wide"
        return width, height, mode, self.library_panes.get_width(), (height < 700, height < 980)

    def _apply_height_layout(self, low_height: bool) -> None:
        if low_height:
            self.add_css_class("compact-height")
        else:
            self.remove_css_class("compact-height")
        self.library_page.set_spacing(5 if low_height else 16)
        self.library_page.set_margin_top(6 if low_height else 18)
        self.library_page.set_margin_bottom(6 if low_height else 18)
        self.selection_hint.set_visible(not low_height)

    def _apply_sidebar_layout(self, mode: str) -> int:
        collapsed = mode != "wide"
        if collapsed:
            self.sidebar.add_css_class("app-sidebar-collapsed")
        else:
            self.sidebar.remove_css_class("app-sidebar-collapsed")
        self.sidebar.set_size_request(70 if collapsed else 216, -1)
        self.sidebar_brand.set_visible(not collapsed)
        self.sidebar_compact_logo.set_visible(collapsed)
        self.sidebar_caption.set_visible(False)
        self.sidebar_source.set_visible(False)
        self.sidebar_library_count.set_visible(False)
        self.status_strip.set_visible(not collapsed)
        for label in self._nav_labels:
            label.set_visible(not collapsed)
        self.page_subtitle.set_visible(mode == "wide")
        self.reload_label.set_visible(False)
        self.next_label.set_visible(False)
        self.hero.set_visible(False)
        self.hero_art.set_visible(mode == "wide")
        inspector_width = 275 if mode == "small" else 305 if collapsed else 350
        self.detail_shell.set_size_request(inspector_width, -1)
        return inspector_width

    def _apply_selection_layout(self, mode: str, pane_width: int, inspector_width: int) -> None:
        selection_compact = mode != "wide" or pane_width - inspector_width < 780
        self._selection_compact = selection_compact
        count = len(self._selected_ids)
        self.selection_count.set_text(
            str(count) if selection_compact else tr(
                "{count} selecionado" if count == 1 else "{count} selecionados",
                count=count,
            )
        )
        self.bulk_playlist_label.set_visible(not selection_compact)
        self.selection_create_button.set_visible(not selection_compact)
        self.selection_clear_button.set_visible(not selection_compact)
        if selection_compact:
            self.selection_bar.add_css_class("selection-bar-compact")
        else:
            self.selection_bar.remove_css_class("selection-bar-compact")

    def _update_responsive(self) -> bool:
        if self._allow_close:
            return False
        if not self.get_visible():
            return True
        _width, _height, mode, pane_width, height_band = self._responsive_geometry()
        size_changed = (
            self._last_pane_width is None
            or abs(pane_width - self._last_pane_width) >= 24
        )
        if mode == self._compact_mode and self._last_height_band == height_band and not size_changed:
            return True

        previous_mode = self._compact_mode
        self._compact_mode = mode
        self._last_height_band = height_band
        self._last_pane_width = pane_width
        self._apply_height_layout(height_band[0])
        inspector_width = self._apply_sidebar_layout(mode)
        if pane_width > 0:
            self.library_panes.set_position(max(220, pane_width - inspector_width - 12))
        self._apply_selection_layout(mode, pane_width, inspector_width)
        if previous_mode != mode and self.selected_id:
            self._show_details(self.selected_id)
        return True

    def _update_responsive_once(self) -> bool:
        """Run one responsive layout pass from an idle callback."""
        self._update_responsive()
        return False

    def _build_library(self) -> Gtk.Widget:
        self._gallery_all_ids = []
        self._gallery_search = {}
        self._gallery_visible_ids = []
        self._gallery_visible_set = set()
        self._last_filtered_ids = None
        page = _box(vertical=True, spacing=16)
        page.add_css_class("library-page")
        self.library_page = page
        page.set_margin_top(18)
        page.set_margin_start(22)
        page.set_margin_end(22)
        page.set_margin_bottom(18)

        hero = _box(spacing=22)
        hero.add_css_class("hero")
        hero.set_visible(False)
        self.hero = hero
        self.hero_art = _box()
        self.hero_art.add_css_class("hero-preview")
        self.hero_art.set_size_request(400, 225)
        self.hero_art.set_hexpand(False)
        self.hero_art.set_valign(Gtk.Align.CENTER)
        self.hero_art.append(_preview(None, 400, 225))
        hero.append(self.hero_art)
        hero_copy = _box(vertical=True, spacing=8)
        hero_copy.set_valign(Gtk.Align.CENTER)
        hero_copy.set_hexpand(True)
        self.hero_kicker = _label(tr("SUA BIBLIOTECA"), css="page-kicker")
        self.hero_title = _label(tr("Seus wallpapers, no seu ritmo."), css="hero-title", wrap=True)
        self.hero_note = _label(tr("Escolha um wallpaper ou monte uma playlist para começar."), css="hero-copy", wrap=True)
        self.hero_stats = _label(tr("Lendo biblioteca…"), css="hero-copy", wrap=True)
        self.hero_stats.set_max_width_chars(32)
        hero_copy.append(self.hero_kicker)
        hero_copy.append(self.hero_title)
        hero_copy.append(self.hero_note)
        hero_copy.append(self.hero_stats)
        hero.append(hero_copy)
        page.append(hero)
        self._hero_id: str | None = None

        tools = _box(spacing=10)
        tools.add_css_class("library-tools")
        self.search = Gtk.SearchEntry()
        self.search.set_placeholder_text(tr("Buscar em seus wallpapers…  Ctrl+F"))
        self.search.set_hexpand(True)
        self.search.connect("search-changed", lambda *_: self._filter_cards())
        tools.append(self.search)
        self.filter = Gtk.DropDown.new_from_strings([tr(name) for name in FILTERS])
        self.filter.connect("notify::selected", lambda dropdown, *_: self._set_filter(dropdown.get_selected()))
        tools.append(self.filter)
        self.library_count = _label(tr("Carregando…"), css="subtle")
        tools.append(self.library_count)
        self._set_filter(0)
        page.append(tools)
        self.selection_hint = _label(
            tr("Clique para selecionar · Ctrl+clique para escolher vários · Shift+clique para um intervalo"),
            css="caption", wrap=True,
        )
        page.append(self.selection_hint)

        panes = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        panes.set_wide_handle(True)
        panes.set_position(600)
        panes.set_shrink_end_child(False)
        panes.set_hexpand(True)
        panes.set_vexpand(True)
        self.library_panes = panes

        gallery_scroll = Gtk.ScrolledWindow()
        gallery_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self._gallery_model = Gtk.StringList.new([])
        self._gallery_filter = Gtk.CustomFilter.new(self._gallery_filter_item)
        self._gallery_filtered_model = Gtk.FilterListModel.new(
            self._gallery_model, self._gallery_filter
        )
        self._gallery_selection = Gtk.MultiSelection.new(self._gallery_filtered_model)
        self._gallery_selection.connect("selection-changed", self._selection_changed)
        factory = Gtk.SignalListItemFactory.new()
        factory.connect("setup", self._gallery_item_setup)
        factory.connect("bind", self._gallery_item_bind)
        factory.connect("unbind", self._gallery_item_unbind)
        self.gallery = Gtk.GridView.new(self._gallery_selection, factory)
        self.gallery.add_css_class("wallpaper-gallery")
        self.gallery.set_halign(Gtk.Align.FILL)
        self.gallery.set_valign(Gtk.Align.START)
        self.gallery.set_hexpand(True)
        self.gallery.set_min_columns(1)
        self.gallery.set_max_columns(6)
        # Keep click-to-apply separate from the view's built-in multi-selection.
        self.gallery.set_single_click_activate(False)
        self.gallery.connect("activate", self._card_activated)
        gallery_click = Gtk.GestureClick()
        gallery_click.set_button(1)
        gallery_click.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        gallery_click.connect("pressed", self._gallery_pressed)
        gallery_click.connect("released", self._gallery_released)
        self.gallery.add_controller(gallery_click)
        gallery_scroll.set_child(self.gallery)
        self.gallery_state = Gtk.Stack()
        self.gallery_state.set_hhomogeneous(False)
        self.gallery_state.set_vhomogeneous(False)
        self.gallery_state.add_named(gallery_scroll, "gallery")
        empty = _box(vertical=True, spacing=12)
        empty.add_css_class("gallery-empty")
        empty.set_halign(Gtk.Align.CENTER)
        empty.set_valign(Gtk.Align.CENTER)
        empty.append(Gtk.Image.new_from_icon_name("image-x-generic-symbolic"))
        self.empty_title = _label(tr("Sua biblioteca está vazia"), css="section-title")
        self.empty_text = _label(tr("Assine wallpapers no Wallpaper Engine original e aguarde o download pela Steam."),
                                 css="subtle", wrap=True)
        self.empty_text.set_justify(Gtk.Justification.CENTER)
        empty.append(self.empty_title)
        empty.append(self.empty_text)
        empty_reload = _icon_button(tr("Atualizar biblioteca"), "view-refresh-symbolic")
        empty_reload.connect("clicked", lambda *_: self._reload())
        empty.append(empty_reload)
        self.gallery_state.add_named(empty, "empty")
        gallery_overlay = Gtk.Overlay()
        gallery_overlay.set_child(self.gallery_state)
        panes.set_start_child(gallery_overlay)

        detail_scroll = Gtk.ScrolledWindow()
        detail_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        detail_scroll.set_hexpand(True)
        detail_scroll.set_vexpand(True)
        detail_shell = _box(vertical=True)
        detail_shell.add_css_class("inspector")
        detail_shell.set_size_request(350, -1)
        self.detail_shell = detail_shell
        self.detail = _box(vertical=True, spacing=12)
        detail_scroll.set_child(self.detail)
        detail_shell.append(detail_scroll)
        panes.set_end_child(detail_shell)
        self._show_details(None)
        self.selection_revealer = Gtk.Revealer()
        self.selection_revealer.set_transition_type(Gtk.RevealerTransitionType.SLIDE_UP)
        self.selection_revealer.set_halign(Gtk.Align.CENTER)
        self.selection_revealer.set_valign(Gtk.Align.END)
        self.selection_revealer.set_margin_bottom(18)
        selection_bar = _box(spacing=10)
        selection_bar.add_css_class("selection-bar")
        self.selection_bar = selection_bar
        self.selection_count = _label(tr("0 selecionados"), css="selection-count")
        selection_bar.append(self.selection_count)
        self.bulk_playlist_button = Gtk.MenuButton()
        bulk_label = _box(spacing=7)
        bulk_label.append(Gtk.Image.new_from_icon_name("list-add-symbolic"))
        self.bulk_playlist_label = _label(tr("Adicionar à playlist"))
        bulk_label.append(self.bulk_playlist_label)
        self.bulk_playlist_button.set_child(bulk_label)
        self.bulk_playlist_button.set_tooltip_text(tr("Adicionar à playlist"))
        self.bulk_playlist_button.add_css_class("primary-action")
        selection_bar.append(self.bulk_playlist_button)
        create_playlist = _icon_button(tr("Criar playlist"), "list-add-symbolic")
        create_playlist.connect(
            "clicked", lambda *_: self._playlist_name_dialog(
                tr("Criar playlist"), None, initial_ids=self._selected_in_order()
            )
        )
        selection_bar.append(create_playlist)
        self.selection_create_button = create_playlist
        clear_selection = Gtk.Button(label=tr("Limpar seleção"))
        clear_selection.add_css_class("ghost-action")
        clear_selection.connect("clicked", lambda *_: self._clear_gallery_selection())
        selection_bar.append(clear_selection)
        self.selection_clear_button = clear_selection
        self.selection_revealer.set_child(selection_bar)
        gallery_overlay.add_overlay(self.selection_revealer)
        self._refresh_bulk_actions()
        page.append(panes)
        return page

    def _set_filter(self, index: int) -> None:
        self.filter_index = index
        if hasattr(self, "filter") and self.filter.get_selected() != index:
            self.filter.set_selected(index)
        for position, button in enumerate(self._filter_buttons):
            if position == index:
                button.add_css_class("filter-chip-active")
            else:
                button.remove_css_class("filter-chip-active")
        if self._gallery_filter is not None:
            self._filter_cards()

    def _gallery_item_id_at(self, position: int) -> str | None:
        item = self._gallery_filtered_model.get_item(position)
        return item.get_string() if isinstance(item, Gtk.StringObject) else None

    def _card_activated(self, _gallery: Gtk.GridView, position: int) -> None:
        if self._gallery_click_on_button:
            return
        modifiers = self._gallery_click_modifiers
        if modifiers is None:
            seat = self.get_display().get_default_seat()
            keyboard = seat.get_keyboard() if seat else None
            modifiers = keyboard.get_modifier_state() if keyboard else Gdk.ModifierType(0)
        if modifiers & (Gdk.ModifierType.CONTROL_MASK | Gdk.ModifierType.SHIFT_MASK):
            return
        wallpaper_id = self._gallery_item_id_at(position)
        if wallpaper_id is not None:
            self._play_gallery_card(wallpaper_id)

    def _inspect_gallery_card(self, wallpaper_id: str) -> bool:
        if wallpaper_id in self.catalog and self.selected_id != wallpaper_id:
            self._show_details(wallpaper_id)
        return False

    def _play_gallery_card(self, wallpaper_id: str) -> bool:
        if wallpaper_id in self.catalog:
            self._selection_anchor_id = wallpaper_id
            self._show_details(wallpaper_id)
            self._command("select", id=wallpaper_id)
        return False

    def _gallery_pressed(self, gesture: Gtk.GestureClick, _count: int,
                         x: float, y: float) -> None:
        self._gallery_click_modifiers = gesture.get_current_event_state()
        target = self.gallery.pick(x, y, Gtk.PickFlags.DEFAULT)
        self._gallery_click_on_button = False
        self._gallery_clicked_id: str | None = None
        while target is not None and target is not self.gallery:
            if isinstance(target, Gtk.Button):
                self._gallery_click_on_button = True
                break
            wallpaper_id = getattr(target, "wallpaper_id", None)
            if isinstance(wallpaper_id, str):
                self._gallery_clicked_id = wallpaper_id
            target = target.get_parent()

    def _gallery_released(self, gesture: Gtk.GestureClick, count: int,
                          x: float, y: float) -> None:
        if self._gallery_click_on_button:
            GLib.idle_add(self._clear_gallery_click_modifiers)
            return
        modifiers = self._gallery_click_modifiers or gesture.get_current_event_state()
        wallpaper_id = self._gallery_clicked_id
        if wallpaper_id is not None:
            if modifiers & Gdk.ModifierType.SHIFT_MASK:
                GLib.idle_add(
                    self._select_gallery_range, self._selection_anchor_id,
                    wallpaper_id, bool(modifiers & Gdk.ModifierType.CONTROL_MASK),
                )
            else:
                self._selection_anchor_id = wallpaper_id
                if not modifiers & Gdk.ModifierType.CONTROL_MASK and count == 1:
                    GLib.idle_add(self._inspect_gallery_card, wallpaper_id)
        GLib.idle_add(self._clear_gallery_click_modifiers)

    def _clear_gallery_click_modifiers(self) -> bool:
        self._gallery_click_modifiers = None
        self._gallery_click_on_button = False
        self._gallery_clicked_id = None
        return False

    def _select_gallery_range(self, anchor: str | None, end: str, extend: bool) -> bool:
        visible = self._gallery_visible_ids
        if end not in visible:
            return False
        if anchor not in visible:
            anchor = end
        first, last = sorted((visible.index(anchor), visible.index(end)))
        self._selection_rebuilding = True
        try:
            if not extend:
                self._selected_ids.clear()
            self._gallery_selection.select_range(first, last - first + 1, not extend)
        finally:
            self._selection_rebuilding = False
        self._selection_changed(self._gallery_selection, first, last - first + 1)
        return False

    def _gallery_filter_item(self, item: Gtk.StringObject, _data: object = None) -> bool:
        return item.get_string() in self._gallery_visible_set

    def _create_gallery_card(self) -> Gtk.Box:
        card = _box(vertical=True, spacing=5)
        card.add_css_class("wallpaper-card")
        card.set_size_request(CARD_WIDTH, CARD_HEIGHT)
        card.set_halign(Gtk.Align.START)
        card.set_valign(Gtk.Align.START)
        card.set_hexpand(False)

        content = _box(vertical=True, spacing=5)
        content.set_size_request(CARD_WIDTH, CARD_HEIGHT - 2)
        content.set_halign(Gtk.Align.START)
        content.set_valign(Gtk.Align.START)
        content.set_hexpand(False)
        artwork = Gtk.Overlay()
        artwork.add_css_class("artwork")
        artwork.set_size_request(CARD_WIDTH, CARD_PREVIEW_HEIGHT)
        artwork.set_halign(Gtk.Align.START)
        artwork.set_hexpand(False)

        type_badge = _badge("")
        type_badge.add_css_class("type-badge")
        type_badge.set_halign(Gtk.Align.START)
        type_badge.set_valign(Gtk.Align.START)
        type_badge.set_margin_top(9)
        type_badge.set_margin_start(9)
        artwork.add_overlay(type_badge)

        badges = _box(spacing=4)
        badges.set_halign(Gtk.Align.END)
        badges.set_valign(Gtk.Align.START)
        badges.set_margin_top(8)
        badges.set_margin_end(8)
        live_badge = _badge(tr("● AO VIVO"), "pill-accent")
        live_badge.set_visible(False)
        badges.append(live_badge)
        heart = Gtk.Button(label="♡")
        heart.add_css_class("favorite-heart")
        heart.add_css_class("favorite-heart-button")
        heart.connect(
            "clicked",
            lambda _button, target=card: self._toggle_favorite(
                getattr(target, "wallpaper_id", None)
            ),
        )
        badges.append(heart)
        artwork.add_overlay(badges)
        content.append(artwork)

        title = _label("", css="card-title")
        title.set_ellipsize(Pango.EllipsizeMode.END)
        title.set_single_line_mode(True)
        title.set_size_request(CARD_WIDTH - 20, -1)
        title.set_max_width_chars(24)
        title.set_hexpand(False)
        title.set_halign(Gtk.Align.START)
        title.set_margin_start(10)
        title.set_margin_end(10)
        content.append(title)

        subtitle = _label("", css="card-meta")
        subtitle.set_ellipsize(Pango.EllipsizeMode.END)
        subtitle.set_single_line_mode(True)
        subtitle.set_size_request(CARD_WIDTH - 20, -1)
        subtitle.set_max_width_chars(24)
        subtitle.set_hexpand(False)
        subtitle.set_halign(Gtk.Align.START)
        subtitle.set_margin_start(10)
        subtitle.set_margin_end(10)
        subtitle.set_margin_bottom(8)
        content.append(subtitle)
        card.append(content)

        card.wallpaper_id = None
        card._gallery_artwork = artwork
        card._gallery_type_badge = type_badge
        card._gallery_badges = badges
        card._gallery_title = title
        card._gallery_subtitle = subtitle
        card._gallery_preview = None
        return card

    def _gallery_item_setup(
        self, _factory: Gtk.SignalListItemFactory, list_item: Gtk.ListItem
    ) -> None:
        list_item.set_child(self._create_gallery_card())

    def _gallery_item_bind(
        self, _factory: Gtk.SignalListItemFactory, list_item: Gtk.ListItem
    ) -> None:
        item = list_item.get_item()
        card = list_item.get_child()
        if not isinstance(item, Gtk.StringObject) or not isinstance(card, Gtk.Box):
            return
        wallpaper_id = item.get_string()
        details = self.catalog.get(wallpaper_id)
        if details is None:
            return

        card.wallpaper_id = wallpaper_id
        if wallpaper_id in self._selected_ids:
            card.add_css_class("wallpaper-card-selected")
        else:
            card.remove_css_class("wallpaper-card-selected")
        card._gallery_type_badge.set_text(
            tr("CENA" if details.get("type") == "scene" else "VÍDEO")
        )
        card._gallery_title.set_text(str(details.get("title") or wallpaper_id))
        tags = details.get("tags") or []
        kind = tr("Cena" if details.get("type") == "scene" else "Vídeo")
        card._gallery_subtitle.set_text(
            " · ".join(str(tag) for tag in tags[:2]) if tags else kind
        )
        preview_widget = _preview(
            details.get("preview"), CARD_WIDTH, CARD_PREVIEW_HEIGHT,
            animation_path=details.get("preview_animation"),
            hover_target=card._gallery_artwork,
        )
        card._gallery_artwork.set_child(preview_widget)
        card._gallery_preview = preview_widget
        self._cards[wallpaper_id] = card
        self._card_badges[wallpaper_id] = card._gallery_badges
        self._refresh_card_indicator(wallpaper_id)

    def _gallery_item_unbind(
        self, _factory: Gtk.SignalListItemFactory, list_item: Gtk.ListItem
    ) -> None:
        card = list_item.get_child()
        if not isinstance(card, Gtk.Box):
            return
        wallpaper_id = getattr(card, "wallpaper_id", None)
        if isinstance(wallpaper_id, str):
            if self._cards.get(wallpaper_id) is card:
                self._cards.pop(wallpaper_id, None)
                self._card_badges.pop(wallpaper_id, None)
        preview_widget = card._gallery_preview
        if isinstance(preview_widget, Gtk.Stack):
            _release_preview(preview_widget)
        card._gallery_artwork.set_child(None)
        card._gallery_preview = None
        card.wallpaper_id = None

    def _refresh_card_indicator(self, wallpaper_id: str) -> None:
        container = self._card_badges.get(wallpaper_id)
        if container is None:
            return
        live_badge = container.get_first_child()
        heart = live_badge.get_next_sibling() if live_badge else None
        if not isinstance(live_badge, Gtk.Label) or not isinstance(heart, Gtk.Button):
            return
        favorites = set(self.config.get("favorites", []))
        current = self.status.get("current_id") if self.status.get("renderer_running") else None
        favorite = wallpaper_id in favorites
        live_badge.set_visible(wallpaper_id == current)
        heart.set_label("♥" if favorite else "♡")
        heart.set_tooltip_text(
            tr("Remover dos favoritos" if favorite else "Adicionar aos favoritos")
        )

    def _selection_changed(
        self, _selection: Gtk.MultiSelection, _position: int = 0, _n_items: int = 0
    ) -> None:
        if self._selection_rebuilding:
            return
        selection = self._gallery_selection.get_selection()
        visible_selected = {
            self._gallery_visible_ids[selection.get_nth(index)]
            for index in range(selection.get_size())
            if selection.get_nth(index) < len(self._gallery_visible_ids)
        }
        keep_hidden = self._gallery_click_modifiers is None or bool(
            self._gallery_click_modifiers
            & (Gdk.ModifierType.CONTROL_MASK | Gdk.ModifierType.SHIFT_MASK)
        )
        hidden_selected = (
            self._selected_ids - self._gallery_visible_set if keep_hidden else set()
        )
        self._selected_ids = hidden_selected | visible_selected
        for item_id, child in self._cards.items():
            if item_id in self._selected_ids:
                child.add_css_class("wallpaper-card-selected")
            else:
                child.remove_css_class("wallpaper-card-selected")
        self.selection_revealer.set_reveal_child(len(self._selected_ids) > 1)
        count_text = tr(
            "{count} selecionado" if len(self._selected_ids) == 1 else "{count} selecionados",
            count=len(self._selected_ids),
        )
        self.selection_count.set_tooltip_text(count_text)
        self.selection_count.set_text(
            str(len(self._selected_ids)) if self._selection_compact else count_text
        )
        if len(self._selected_ids) == 1:
            self._show_details(next(iter(self._selected_ids)))

    def _clear_gallery_selection(self) -> None:
        self._selected_ids.clear()
        self._selection_rebuilding = True
        try:
            self._gallery_selection.unselect_all()
        finally:
            self._selection_rebuilding = False
        self._selection_changed(self._gallery_selection)

    def _selected_in_order(self) -> list[str]:
        return [item_id for item_id in self._gallery_all_ids if item_id in self._selected_ids]

    def _refresh_bulk_actions(self) -> None:
        popover = Gtk.Popover()
        choices = _box(vertical=True, spacing=5)
        choices.set_margin_top(10)
        choices.set_margin_bottom(10)
        choices.set_margin_start(10)
        choices.set_margin_end(10)
        choices.append(_label(tr("ADICIONAR À PLAYLIST"), css="page-kicker"))
        playlists = self.config.get("playlists") or {}
        for name in playlists:
            button = Gtk.Button(label=name)
            button.add_css_class("ghost-action")
            button.connect("clicked", lambda _button, target=name: self._bulk_add_to_playlist(target))
            choices.append(button)
        if not playlists:
            choices.append(_label(tr("Ainda não há playlists."), css="subtle"))
        create = _icon_button(tr("Criar playlist"), "list-add-symbolic")
        create.connect("clicked", lambda *_: self._playlist_name_dialog(
            tr("Criar playlist"), None, initial_ids=self._selected_in_order()
        ))
        choices.append(create)
        clear = Gtk.Button(label=tr("Limpar seleção"))
        clear.add_css_class("ghost-action")
        clear.connect("clicked", lambda *_: self._clear_gallery_selection())
        choices.append(clear)
        popover.set_child(choices)
        self.bulk_playlist_button.set_popover(popover)

    def _bulk_add_to_playlist(self, name: str) -> None:
        self._playlist_add_many(name, self._selected_in_order())
        self._clear_gallery_selection()

    def _build_playlists(self) -> Gtk.Widget:
        page = _box(vertical=True, spacing=16)
        page.set_margin_top(20)
        page.set_margin_start(22)
        page.set_margin_end(22)
        page.set_margin_bottom(18)

        toolbar = _box(spacing=10)
        toolbar.add_css_class("library-tools")
        heading = _label(tr("Suas playlists"), css="section-title")
        heading.set_hexpand(True)
        toolbar.append(heading)
        self.playlist_active_label = _badge(tr("Rotação pela biblioteca"))
        toolbar.append(self.playlist_active_label)
        create = _icon_button(tr("Nova playlist"), "list-add-symbolic", tooltip=tr("Criar playlist · Ctrl+N"))
        create.add_css_class("primary-action")
        create.connect("clicked", lambda *_: self._playlist_name_dialog(tr("Criar playlist"), None))
        toolbar.append(create)
        page.append(toolbar)

        panes = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        panes.set_wide_handle(True)
        panes.set_position(285)
        panes.set_hexpand(True)
        panes.set_vexpand(True)

        left_scroll = Gtk.ScrolledWindow()
        left_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        left_scroll.add_css_class("panel")
        self.playlist_list = Gtk.ListBox()
        self.playlist_list.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.playlist_list.connect("row-selected", self._playlist_selected)
        left_scroll.set_child(self.playlist_list)
        panes.set_start_child(left_scroll)

        right_scroll = Gtk.ScrolledWindow()
        right_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        right_scroll.add_css_class("panel")
        self.playlist_detail = _box(vertical=True, spacing=12)
        right_scroll.set_child(self.playlist_detail)
        panes.set_end_child(right_scroll)
        page.append(panes)
        self._refresh_playlists()
        return page

    def _refresh_playlists(self) -> None:
        if not hasattr(self, "playlist_list"):
            return
        playlists = self.config.get("playlists") or {}
        active = self.config.get("active_playlist")
        self.playlist_active_label.set_text(
            tr("Ativa: {name}", name=active) if active else tr("Rotação pela biblioteca")
        )
        if self.playlist_name not in playlists:
            self.playlist_name = next(iter(playlists), None)

        self._updating_playlists = True
        _clear(self.playlist_list)
        chosen_row = None
        for name, identifiers in playlists.items():
            row = Gtk.ListBoxRow()
            row.playlist_name = name
            content = _box(spacing=10)
            content.add_css_class("playlist-item")
            first = next((item_id for item_id in identifiers if item_id in self.catalog), None)
            content.append(_preview(self.catalog[first].get("preview") if first else None, 75, 48))
            copy = _box(vertical=True, spacing=3)
            copy.set_valign(Gtk.Align.CENTER)
            copy.set_hexpand(True)
            copy.append(_label(name, css="card-title"))
            copy.append(_label(tr("{count} wallpapers", count=len(identifiers)), css="card-meta"))
            content.append(copy)
            if name == active:
                content.append(_badge(tr("ATIVA"), "pill-accent"))
            row.set_child(content)
            self.playlist_list.append(row)
            if name == self.playlist_name:
                chosen_row = row
        if chosen_row is not None:
            self.playlist_list.select_row(chosen_row)
        self._updating_playlists = False
        self._show_playlist_detail()
        if hasattr(self, "bulk_playlist_button"):
            self._refresh_bulk_actions()

    def _playlist_selected(self, _listbox: Gtk.ListBox, row: Gtk.ListBoxRow | None) -> None:
        if self._updating_playlists:
            return
        self.playlist_name = row.playlist_name if row is not None else None
        self._show_playlist_detail()

    def _show_empty_playlist_detail(self) -> None:
        self.playlist_detail.append(_label(tr("COMECE POR AQUI"), css="page-kicker"))
        self.playlist_detail.append(_label(
            tr("Uma trilha para cada clima."), css="detail-title", wrap=True
        ))
        self.playlist_detail.append(_label(
            tr("Crie uma playlist e escolha vários wallpapers instalados de uma vez. Você decide a ordem ou deixa a reprodução aleatória."),
            css="subtle", wrap=True,
        ))
        create = _icon_button(tr("Criar minha primeira playlist"), "list-add-symbolic")
        create.add_css_class("primary-action")
        create.connect("clicked", lambda *_: self._playlist_name_dialog(tr("Criar playlist"), None))
        self.playlist_detail.append(create)

    def _append_playlist_heading(self, name: str) -> None:
        self.playlist_detail.append(_label(tr("EDITOR DE PLAYLIST"), css="page-kicker"))
        heading = _box(spacing=8)
        label = _label(name, css="detail-title")
        label.set_hexpand(True)
        heading.append(label)
        rename = Gtk.Button(label=tr("Renomear"))
        rename.add_css_class("compact-button")
        rename.connect("clicked", lambda *_: self._playlist_name_dialog(tr("Renomear playlist"), name))
        heading.append(rename)
        delete = Gtk.Button(label=tr("Excluir"))
        delete.add_css_class("danger-action")
        delete.add_css_class("compact-button")
        delete.connect("clicked", lambda *_: self._playlist_delete(name))
        heading.append(delete)
        self.playlist_detail.append(heading)

    def _append_playlist_controls(self, name: str, identifiers: list[str]) -> None:
        active = self.config.get("active_playlist") == name
        playlist_state = (
            "Selecionada para a rotação" if active and self.config.get("rotation_enabled")
            else "Selecionada, com rotação pausada" if active else "Pronta para ativar"
        )
        self.playlist_detail.append(_label(tr(
            "{count} itens · {state}", count=len(identifiers), state=tr(playlist_state)
        ), css="subtle"))
        activation = Gtk.Button(
            label=tr("Voltar à biblioteca na rotação" if active else "Ativar e iniciar rotação")
        )
        activation.set_sensitive(active or any(item_id in self.catalog for item_id in identifiers))
        if not active:
            activation.add_css_class("suggested-action")

        def toggle_playlist(*_args: object) -> None:
            self._set_settings(
                active_playlist=None if active else name,
                rotation_enabled=True,
            )
            if not active and self.service_available and not self.status.get("running"):
                self._command("start")

        activation.connect("clicked", toggle_playlist)
        self.playlist_detail.append(activation)
        self.playlist_detail.append(_label(
            tr("A ordem abaixo vale quando a opção aleatória está desligada. Itens salvos que não estão instalados serão ignorados até reaparecerem na biblioteca."),
            css="subtle", wrap=True,
        ))

        add_button = _icon_button(tr("Adicionar wallpapers"), "list-add-symbolic")
        add_button.add_css_class("primary-action")
        add_button.set_sensitive(any(item_id not in identifiers for item_id in self.catalog))
        add_button.connect("clicked", lambda *_: self._playlist_add_dialog(name))
        self.playlist_detail.append(add_button)

        if self.selected_id and self.selected_id not in identifiers:
            selected_title = self.catalog.get(self.selected_id, {}).get("title", self.selected_id)
            add_selected = Gtk.Button(
                label=tr("Adicionar selecionado: {title}", title=selected_title)
            )
            add_selected.connect("clicked", lambda *_: self._playlist_add_id(name, self.selected_id))
            self.playlist_detail.append(add_selected)

    def _append_playlist_order(self, name: str, identifiers: list[str]) -> None:
        self.playlist_detail.append(_label(tr("ORDEM DE REPRODUÇÃO"), css="page-kicker"))
        if not identifiers:
            self.playlist_detail.append(_label(
                tr("Esta playlist ainda está vazia. Adicione wallpapers instalados para começar."),
                css="empty-state", wrap=True,
            ))
        for position, wallpaper_id in enumerate(identifiers):
            item = self.catalog.get(wallpaper_id)
            title = item.get("title", wallpaper_id) if item else wallpaper_id
            if not item:
                title = tr("{title} · não instalado", title=title)
            row = _box(spacing=7)
            row.add_css_class("settings-row")
            row.append(_preview(item.get("preview") if item else None, 72, 45))
            caption = _label(f"{position + 1}. {title}", wrap=True)
            caption.set_hexpand(True)
            row.append(caption)
            up = Gtk.Button(label="↑")
            up.set_sensitive(position > 0)
            up.set_tooltip_text(tr("Mover para cima"))
            up.connect(
                "clicked",
                lambda _button, item_id=wallpaper_id: self._playlist_move(name, item_id, -1),
            )
            row.append(up)
            down = Gtk.Button(label="↓")
            down.set_sensitive(position < len(identifiers) - 1)
            down.set_tooltip_text(tr("Mover para baixo"))
            down.connect(
                "clicked",
                lambda _button, item_id=wallpaper_id: self._playlist_move(name, item_id, 1),
            )
            row.append(down)
            remove = Gtk.Button(label=tr("Remover"))
            remove.connect(
                "clicked",
                lambda _button, item_id=wallpaper_id: self._playlist_remove(name, item_id),
            )
            row.append(remove)
            self.playlist_detail.append(row)

    def _show_playlist_detail(self) -> None:
        if not hasattr(self, "playlist_detail"):
            return
        _clear(self.playlist_detail)
        playlists = self.config.get("playlists") or {}
        name = self.playlist_name
        if name is None or name not in playlists:
            self._show_empty_playlist_detail()
            return

        identifiers = playlists[name]
        self._append_playlist_heading(name)
        self._append_playlist_controls(name, identifiers)
        self._append_playlist_order(name, identifiers)

    def _save_playlist_name(
        self,
        entry: Gtk.Entry,
        previous: str | None,
        initial_ids: list[str] | None,
    ) -> bool:
        try:
            name = model.normalize_playlist_name(entry.get_text())
        except ValueError as exc:
            self._notice(str(exc), error=True)
            return False

        playlists = dict(self.config.get("playlists") or {})
        if name != previous and name.casefold() in {value.casefold() for value in playlists}:
            self._notice(tr("Já existe uma playlist com esse nome."), error=True)
            return False

        if previous is not None and previous in playlists:
            playlists = {
                name if key == previous else key: value
                for key, value in playlists.items()
            }
            active = self.config.get("active_playlist")
            settings = {
                "playlists": playlists,
                "active_playlist": name if active == previous else active,
            }
        else:
            playlists[name] = list(dict.fromkeys(
                item_id for item_id in (initial_ids or []) if item_id in self.catalog
            ))
            settings = {"playlists": playlists}

        self.playlist_name = name
        self._set_settings(**settings)
        if initial_ids:
            self._clear_gallery_selection()
        return True

    def _playlist_name_dialog(self, title: str, previous: str | None,
                              initial_ids: list[str] | None = None) -> None:
        dialog = Gtk.Dialog(title=title, transient_for=self, modal=True)
        dialog.add_button(tr("Cancelar"), Gtk.ResponseType.CANCEL)
        dialog.add_button(tr("Salvar"), Gtk.ResponseType.ACCEPT)
        content = dialog.get_content_area()
        content.set_margin_top(15)
        content.set_margin_bottom(15)
        content.set_margin_start(15)
        content.set_margin_end(15)
        entry = Gtk.Entry()
        entry.set_placeholder_text(tr("Nome da playlist"))
        entry.set_text(previous or "")
        entry.set_activates_default(True)
        dialog.set_default_response(Gtk.ResponseType.ACCEPT)
        content.append(entry)

        def response(_dialog: Gtk.Dialog, code: int) -> None:
            if code == Gtk.ResponseType.ACCEPT and not self._save_playlist_name(
                entry, previous, initial_ids
            ):
                return
            dialog.destroy()

        dialog.connect("response", response)
        dialog.present()

    def _playlist_delete(self, name: str) -> None:
        if name not in (self.config.get("playlists") or {}):
            return
        dialog = Gtk.Dialog(title=tr("Excluir playlist"), transient_for=self, modal=True)
        dialog.add_button(tr("Cancelar"), Gtk.ResponseType.CANCEL)
        dialog.add_button(tr("Excluir"), Gtk.ResponseType.ACCEPT)
        content = dialog.get_content_area()
        content.set_margin_top(15)
        content.set_margin_bottom(15)
        content.set_margin_start(15)
        content.set_margin_end(15)
        content.append(_label(
            tr("Excluir ‘{name}’? Os wallpapers instalados continuarão na biblioteca.", name=name),
            wrap=True,
        ))

        def response(_dialog: Gtk.Dialog, code: int) -> None:
            if code == Gtk.ResponseType.ACCEPT:
                playlists = dict(self.config.get("playlists") or {})
                if name in playlists:
                    del playlists[name]
                    active = self.config.get("active_playlist")
                    self.playlist_name = next(iter(playlists), None)
                    self._set_settings(
                        playlists=playlists,
                        active_playlist=None if active == name else active,
                    )
            dialog.destroy()

        dialog.connect("response", response)
        dialog.present()

    def _playlist_add_dialog(self, name: str) -> None:
        playlists = self.config.get("playlists") or {}
        if name not in playlists:
            return
        available = sorted(
            (item_id for item_id in self.catalog if item_id not in playlists[name]),
            key=lambda item_id: str(self.catalog[item_id].get("title", item_id)).casefold(),
        )
        if not available:
            self._notice(tr("Todos os wallpapers instalados já estão nesta playlist."))
            return

        dialog = Gtk.Dialog(title=tr("Adicionar a {name}", name=name), transient_for=self, modal=True)
        dialog.set_default_size(650, 580)
        dialog.add_button(tr("Cancelar"), Gtk.ResponseType.CANCEL)
        add_button = dialog.add_button(tr("Adicionar selecionados"), Gtk.ResponseType.ACCEPT)
        add_button.add_css_class("suggested-action")
        add_button.set_sensitive(False)
        content = dialog.get_content_area()
        content.set_spacing(12)
        content.set_margin_top(16)
        content.set_margin_bottom(16)
        content.set_margin_start(18)
        content.set_margin_end(18)
        content.append(_label(tr("Escolha vários wallpapers instalados"), css="section-title"))
        search = Gtk.SearchEntry()
        search.set_placeholder_text(tr("Buscar por nome, tag ou ID"))
        content.append(search)
        selection_count = _label(tr("Nenhum selecionado"), css="subtle")
        content.append(selection_count)
        listing = Gtk.ListBox()
        listing.set_selection_mode(Gtk.SelectionMode.NONE)
        rows: dict[str, Gtk.ListBoxRow] = {}
        checks: dict[str, Gtk.CheckButton] = {}

        def update_selection(_button: Gtk.CheckButton | None = None) -> None:
            total = sum(button.get_active() for button in checks.values())
            selection_count.set_text(tr("{count} selecionados", count=total) if total else tr("Nenhum selecionado"))
            add_button.set_sensitive(bool(total))

        for item_id in available:
            item = self.catalog[item_id]
            row = Gtk.ListBoxRow()
            row.add_css_class("playlist-entry")
            line = _box(spacing=11)
            line.set_margin_top(4)
            line.set_margin_bottom(4)
            check = Gtk.CheckButton()
            check.set_valign(Gtk.Align.CENTER)
            check.set_tooltip_text(tr("Selecionar {title}", title=item.get("title") or item_id))
            check.connect("toggled", update_selection)
            checks[item_id] = check
            line.append(check)
            line.append(_preview(item.get("preview"), 74, 46))
            copy = _box(vertical=True, spacing=2)
            copy.set_valign(Gtk.Align.CENTER)
            copy.set_hexpand(True)
            title = _label(str(item.get("title") or item_id), css="card-title")
            title.set_ellipsize(Pango.EllipsizeMode.END)
            copy.append(title)
            copy.append(_label(
                tr("{kind} · {wallpaper_id}",
                   kind=tr("Cena" if item.get("type") == "scene" else "Vídeo"), wallpaper_id=item_id),
                css="card-meta",
            ))
            line.append(copy)
            row.set_child(line)
            listing.append(row)
            rows[item_id] = row

        def filter_rows(_entry: Gtk.SearchEntry) -> None:
            query = search.get_text().strip().casefold()
            for item_id, row in rows.items():
                item = self.catalog[item_id]
                haystack = " ".join(
                    [str(item.get("title", "")), item_id]
                    + [str(tag) for tag in item.get("tags", [])]
                ).casefold()
                row.set_visible(query in haystack)

        search.connect("search-changed", filter_rows)
        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scroll.set_vexpand(True)
        scroll.set_child(listing)
        content.append(scroll)

        def response(_dialog: Gtk.Dialog, code: int) -> None:
            if code == Gtk.ResponseType.ACCEPT:
                self._playlist_add_many(
                    name, [item_id for item_id in available if checks[item_id].get_active()]
                )
            dialog.destroy()

        dialog.connect("response", response)
        dialog.present()
        search.grab_focus()

    def _playlist_add_many(self, name: str, identifiers: list[str]) -> None:
        playlists = {key: list(value) for key, value in (self.config.get("playlists") or {}).items()}
        if name not in playlists:
            return
        added = 0
        for item_id in identifiers:
            if item_id in self.catalog and item_id not in playlists[name]:
                playlists[name].append(item_id)
                added += 1
        if added:
            self._set_settings(playlists=playlists)

    def _playlist_add_id(self, name: str, wallpaper_id: str | None) -> None:
        if wallpaper_id is None or wallpaper_id not in self.catalog:
            return
        if wallpaper_id in (self.config.get("playlists") or {}).get(name, []):
            self._notice(tr("Este wallpaper já está na playlist."))
            return
        self._playlist_add_many(name, [wallpaper_id])

    def _playlist_remove(self, name: str, wallpaper_id: str) -> None:
        playlists = {key: list(value) for key, value in (self.config.get("playlists") or {}).items()}
        if name not in playlists:
            return
        playlists[name] = [item for item in playlists[name] if item != wallpaper_id]
        self._set_settings(playlists=playlists)

    def _playlist_move(self, name: str, wallpaper_id: str, step: int) -> None:
        playlists = {key: list(value) for key, value in (self.config.get("playlists") or {}).items()}
        if name not in playlists or wallpaper_id not in playlists[name]:
            return
        index = playlists[name].index(wallpaper_id)
        other = index + step
        if 0 <= other < len(playlists[name]):
            playlists[name][index], playlists[name][other] = playlists[name][other], playlists[name][index]
            self._set_settings(playlists=playlists)

    def _build_languages(self) -> Gtk.Widget:
        scroll = Gtk.ScrolledWindow()
        body = _box(vertical=True, spacing=20)
        body.set_margin_top(24)
        body.set_margin_start(22)
        body.set_margin_end(22)
        body.set_margin_bottom(24)
        body.set_halign(Gtk.Align.CENTER)
        body.set_hexpand(True)
        scroll.set_child(body)

        intro = _box(vertical=True, spacing=7)
        intro.add_css_class("panel")
        intro.append(_label(tr("IDIOMA DA INTERFACE"), css="page-kicker"))
        intro.append(_label(tr("Escolha o idioma da interface."), css="section-title"))
        intro.append(_label(tr("Aplica a tradução imediatamente."), css="subtle", wrap=True))
        body.append(intro)

        options = _box(vertical=True, spacing=8)
        body.append(options)
        selected = self.config.get("language", "auto")
        detected_language = dict(language_options()).get(system_language(), "English")
        for code, native_name in language_options():
            button = Gtk.Button()
            button.add_css_class("language-choice")
            button.set_hexpand(True)
            content = _box(spacing=14)
            content.set_valign(Gtk.Align.CENTER)
            name = (
                f"{tr('Automático (sistema)')} · {detected_language}"
                if code == "auto" else native_name
            )
            name_label = _label(name)
            name_label.set_hexpand(True)
            name_label.set_ellipsize(Pango.EllipsizeMode.END)
            content.append(name_label)
            code_label = _label(code.replace("_", "-").upper(), css="subtle")
            content.append(code_label)
            mark = Gtk.Image.new_from_icon_name("object-select-symbolic")
            content.append(mark)
            button.set_child(content)
            button.set_tooltip_text(name)
            button.connect("clicked", self._choose_language, code)
            options.append(button)
            self._language_buttons[code] = (button, mark)
        self._refresh_language_choices(selected)
        return scroll

    def _choose_language(self, _button: Gtk.Button, code: str) -> None:
        if code != self.config.get("language", "auto"):
            self._set_settings(language=code)

    def _refresh_language_choices(self, selected: str) -> None:
        for code, (button, mark) in self._language_buttons.items():
            active = code == selected
            if active:
                button.add_css_class("language-choice-active")
            else:
                button.remove_css_class("language-choice-active")
            mark.set_visible(active)

    def _build_settings(self) -> Gtk.Widget:
        scroll = Gtk.ScrolledWindow()
        body = _box(vertical=True, spacing=18)
        body.set_margin_top(24)
        body.set_margin_start(22)
        body.set_margin_end(22)
        body.set_margin_bottom(24)
        body.set_halign(Gtk.Align.CENTER)
        body.set_hexpand(True)
        scroll.set_child(body)

        def group(kicker: str, title: str, description: str) -> Gtk.Box:
            section = _box(vertical=True, spacing=10)
            section.add_css_class("panel")
            section.append(_label(kicker, css="page-kicker"))
            section.append(_label(title, css="section-title"))
            section.append(_label(description, css="subtle", wrap=True))
            body.append(section)
            return section

        playback = group(tr("01 · REPRODUÇÃO"), tr("Uma coleção sempre em movimento"),
                         tr("Controle a troca automática e escolha de onde vêm os próximos wallpapers."))
        self.rotation_switch = self._switch_row(
            playback, tr("Troca automática"), tr("Escolhe outro wallpaper após o intervalo."),
            "rotation_enabled"
        )
        self.favorites_switch = self._switch_row(
            playback, tr("Usar só favoritos"), tr("Limita a rotação aos wallpapers marcados."),
            "only_favorites"
        )
        self.shuffle_switch = self._switch_row(
            playback, tr("Ordem aleatória"), tr("Embaralha os wallpapers elegíveis."), "shuffle"
        )
        self.interval_spin = self._spin_row(
            playback, tr("Intervalo entre trocas"), tr("Minutos"), "interval_minutes", 1, 1440
        )

        rendering = group(tr("02 · MOTOR"), tr("Imagem e desempenho"),
                          tr("Ajustes que afetam a renderização dos wallpapers em cada tela."))
        self.fps_spin = self._spin_row(rendering, tr("Limite de quadros"), "FPS", "fps", 10, 240)
        self.scaling_drop = Gtk.DropDown.new_from_strings(
            tuple(tr(label) for label in ("Preencher", "Ajustar", "Esticar", "Padrão"))
        )
        self.scaling_drop.connect("notify::selected", self._scaling_changed)
        self._widget_row(rendering, tr("Escala"), tr("Como a imagem ocupa a tela."), self.scaling_drop)
        self.mute_switch = self._switch_row(
            rendering, tr("Silenciar"), tr("Executa o renderizador sem áudio."), "mute"
        )

        application = group(tr("03 · APLICATIVO"), tr("Sempre pronto quando você entrar"),
                            tr("Configure o serviço e o caminho do motor instalado neste computador."))
        self.autostart_switch = Gtk.Switch()
        self.autostart_switch.connect("notify::active", self._autostart_changed)
        self._widget_row(
            application,
            tr("Ativar o serviço com a sessão"),
            tr("Ativa o serviço do usuário quando a sessão gráfica é iniciada."),
            self.autostart_switch,
        )
        self.minimize_to_tray_switch = Gtk.Switch()
        self.minimize_to_tray_switch.set_active(
            bool(self._ui_preferences.get("minimize_to_tray", False))
        )
        self.minimize_to_tray_switch.connect(
            "notify::active", self._minimize_to_tray_changed
        )
        self._widget_row(
            application,
            tr("Minimizar para a bandeja ao fechar"),
            tr("Mantém o aplicativo aberto na bandeja; use o menu do ícone para reabrir ou sair."),
            self.minimize_to_tray_switch,
        )
        self.start_app_switch = Gtk.Switch()
        self.start_app_switch.set_active(
            bool(self._ui_preferences.get("start_app_with_session", False))
        )
        self.start_app_switch.connect("notify::active", self._start_app_changed)
        self._widget_row(
            application,
            tr("Iniciar o aplicativo com o sistema"),
            tr("Abre o painel quando você entra na sessão gráfica."),
            self.start_app_switch,
        )
        self.renderer_entry = Gtk.Entry()
        self.renderer_entry.set_placeholder_text(tr("Detectar linux-wallpaperengine no PATH"))
        self.renderer_entry.set_hexpand(True)
        renderer_save = Gtk.Button(label=tr("Salvar"))
        renderer_save.connect(
            "clicked",
            lambda *_: self._set_settings(
                renderer_path=self.renderer_entry.get_text().strip() or "auto"
            ),
        )
        renderer_controls = _box(spacing=8)
        renderer_controls.append(self.renderer_entry)
        renderer_controls.append(renderer_save)
        self._widget_row(
            application,
            tr("Executável do motor"),
            tr("Caminho personalizado, se necessário."),
            renderer_controls,
        )

        info = _label(
            tr("Descubra e assine novos itens no Wallpaper Engine original. A Steam faz o download; este app acompanha sua biblioteca local. O motor continua renderizando quando outra janela cobre o wallpaper, preservando a estabilidade do vídeo e do áudio da sessão."),
            css="subtle",
            wrap=True,
        )
        info.add_css_class("panel")
        body.append(info)
        return scroll

    def _widget_row(self, parent: Gtk.Box, title: str, note: str, control: Gtk.Widget) -> None:
        row = _box(spacing=20)
        row.add_css_class("settings-row")
        text = _box(vertical=True, spacing=3)
        text.set_hexpand(True)
        heading = _label(title)
        heading.set_wrap(True)
        heading.set_hexpand(True)
        text.append(heading)
        text.append(_label(note, css="subtle", wrap=True))
        row.append(text)
        control.set_valign(Gtk.Align.CENTER)
        row.append(control)
        parent.append(row)

    def _switch_row(self, parent: Gtk.Box, title: str, note: str, key: str) -> Gtk.Switch:
        control = Gtk.Switch()
        control.connect("notify::active", self._switch_changed, key)
        self._widget_row(parent, title, note, control)
        return control

    def _spin_row(
        self, parent: Gtk.Box, title: str, unit: str, key: str, low: int, high: int
    ) -> Gtk.SpinButton:
        control = Gtk.SpinButton.new_with_range(low, high, 1)
        control.set_numeric(True)
        control.set_width_chars(5)
        control.connect("value-changed", self._spin_changed, key)
        self._widget_row(parent, title, unit, control)
        return control

    def _background(self, job: Callable, done: Callable) -> None:
        def worker() -> None:
            try:
                value = job()
                GLib.idle_add(done, value, None)
            except (OSError, RuntimeError, ValueError, TypeError, subprocess.SubprocessError) as exc:
                GLib.idle_add(done, None, exc)

        threading.Thread(target=worker, daemon=True).start()

    def _load_catalog(self) -> None:
        self._catalog_generation += 1
        generation = self._catalog_generation
        self.library_count.set_text(tr("Lendo biblioteca…"))

        def done(value: dict | None, error: Exception | None) -> None:
            if generation != self._catalog_generation:
                return
            if error:
                self._notice(tr("Erro ao ler wallpapers: {error}", error=error), error=True)
                self.library_count.set_text(tr("Falha na leitura"))
                return
            self.catalog = value or {}
            self.library_count.set_text(tr("{count} wallpapers", count=len(self.catalog)))
            self.sidebar_library_count.set_text(tr("{count} wallpapers instalados", count=len(self.catalog)))
            selected_ids = self._selected_ids & self.catalog.keys()
            self._selected_ids = set(selected_ids)
            identifiers = sorted(
                self.catalog, key=lambda item: str(self.catalog[item].get("title", item)).casefold()
            )
            self._gallery_all_ids = identifiers
            self._gallery_search = {
                wallpaper_id: " ".join(
                    [str(self.catalog[wallpaper_id].get("title", "")), wallpaper_id]
                    + [str(tag) for tag in self.catalog[wallpaper_id].get("tags", [])]
                ).casefold()
                for wallpaper_id in identifiers
            }
            self._selection_rebuilding = True
            try:
                self._gallery_model.splice(
                    0, self._gallery_model.get_n_items(), identifiers
                )
            finally:
                self._selection_rebuilding = False
            self._last_filtered_ids = None
            self._filter_cards()
            selected = self.selected_id if self.selected_id in self.catalog else None
            self._show_details(selected)
            self._refresh_playlists()
            self._refresh_hero()

        self._background(model.scan_catalog, done)

    def _refresh_card_indicators(self) -> None:
        for wallpaper_id in tuple(self._card_badges):
            self._refresh_card_indicator(wallpaper_id)

    def _refresh_card_indicators_once(self) -> bool:
        self._refresh_card_indicators()
        return False

    def _filter_cards(self) -> None:
        query = self.search.get_text().strip().casefold()
        mode = self.filter_index
        favorites = set(self.config.get("favorites", []))
        selected_ids = self._selected_ids & self.catalog.keys()
        visible_ids: list[str] = []
        for wallpaper_id in self._gallery_all_ids:
            item = self.catalog[wallpaper_id]
            matches = query in self._gallery_search.get(wallpaper_id, "")
            if mode == 1:
                matches &= item.get("type") == "scene"
            elif mode == 2:
                matches &= item.get("type") == "video"
            elif mode == 3:
                matches &= wallpaper_id in favorites
            if matches:
                visible_ids.append(wallpaper_id)

        selection_changed = selected_ids != self._selected_ids
        self._selected_ids = set(selected_ids)
        if visible_ids != self._last_filtered_ids or selection_changed:
            self._last_filtered_ids = list(visible_ids)
            self._gallery_visible_ids = visible_ids
            self._gallery_visible_set = set(visible_ids)
            was_rebuilding = self._selection_rebuilding
            self._selection_rebuilding = True
            try:
                self._gallery_selection.unselect_all()
                self._gallery_filter.changed(Gtk.FilterChange.DIFFERENT)
                for position, wallpaper_id in enumerate(visible_ids):
                    if wallpaper_id in selected_ids:
                        self._gallery_selection.select_item(position, False)
            finally:
                self._selection_rebuilding = was_rebuilding
            if not was_rebuilding:
                self._selection_changed(self._gallery_selection)

        self.library_count.set_text(
            tr("{visible} de {total}", visible=len(visible_ids), total=len(self.catalog))
        )
        if visible_ids:
            self.gallery_state.set_visible_child_name("gallery")
        else:
            if self.catalog:
                self.empty_title.set_text(tr("Nada por aqui"))
                self.empty_text.set_text(tr("Tente outro termo ou altere o filtro para ver mais wallpapers."))
            else:
                self.empty_title.set_text(tr("Sua biblioteca está vazia"))
                self.empty_text.set_text(
                    tr("Assine wallpapers no Wallpaper Engine original e aguarde o download pela Steam.")
                )
            self.gallery_state.set_visible_child_name("empty")

    def _refresh_hero(self) -> None:
        current = self.status.get("current_id")
        playing = bool(self.status.get("renderer_running")) and current in self.catalog
        hero_id = current if playing else (self.selected_id if self.selected_id in self.catalog else next(iter(self.catalog), None))
        item = self.catalog.get(hero_id or "", {})
        self.hero_kicker.set_text(tr("EM REPRODUÇÃO AGORA" if playing else "SUA BIBLIOTECA"))
        self.hero_title.set_text(
            str(item.get("title")) if playing else tr("Seus wallpapers, no seu ritmo.")
        )
        self.hero_note.set_text(
            tr("Este wallpaper está ativo na sua área de trabalho.")
            if playing else tr("Escolha um wallpaper ou monte uma playlist para começar.")
        )
        favorites = len(set(self.config.get("favorites", [])) & self.catalog.keys())
        self.hero_stats.set_text(tr(
            "{count} instalados   ·   {favorites} favoritos   ·   {screens} telas",
            count=len(self.catalog), favorites=favorites,
            screens=len(self.status.get("screens") or []),
        ))
        if hero_id != self._hero_id:
            self._hero_id = hero_id
            _clear(self.hero_art)
            self.hero_art.append(_preview(item.get("preview"), 400, 225))

    def _show_empty_details(self) -> None:
        message = _label(
            tr("Escolha um wallpaper na biblioteca para ver os detalhes e aplicar na tela."),
            css="empty-state",
            wrap=True,
        )
        message.set_margin_top(40)
        self.detail.append(message)

    def _append_detail_header(self, selected: str, item: dict[str, object]) -> None:
        self.detail.append(_label(tr("WALLPAPER SELECIONADO"), css="page-kicker"))
        preview_width = 310 if self._compact_mode in (None, "wide") else 240
        self.detail.append(_preview(item.get("preview"), preview_width, round(preview_width * 9 / 16)))
        self.detail.append(_label(str(item.get("title") or selected), css="detail-title", wrap=True))

        metadata = Gtk.FlowBox()
        metadata.set_selection_mode(Gtk.SelectionMode.NONE)
        metadata.set_min_children_per_line(1)
        metadata.set_max_children_per_line(3)
        metadata.set_column_spacing(7)
        metadata.set_row_spacing(7)
        metadata.append(_badge(tr("CENA" if item.get("type") == "scene" else "VÍDEO"), "pill-accent"))
        metadata.append(_badge(tr("ID {wallpaper_id}", wallpaper_id=selected)))
        if self.status.get("current_id") == selected and self.status.get("renderer_running"):
            metadata.append(_badge(tr("● EM USO"), "pill-accent"))
        self.detail.append(metadata)

        tags = item.get("tags", [])
        if tags:
            self.detail.append(_label(" · ".join(str(tag) for tag in tags[:8]), css="subtle", wrap=True))

        applied = _icon_button(tr("Aplicar agora"), "media-playback-start-symbolic")
        applied.add_css_class("primary-action")
        applied.connect("clicked", lambda *_: self._command("select", id=selected))
        self.detail.append(applied)
        self.detail.append(_label(
            tr("Aplica em todas as telas, interrompe a playlist ativa e libera fixações por monitor."),
            css="caption", wrap=True,
        ))

        favorite = selected in set(self.config.get("favorites", []))
        favorite_button = _icon_button(
            tr("Remover dos favoritos" if favorite else "Adicionar aos favoritos"),
            "starred-symbolic" if favorite else "non-starred-symbolic",
        )
        favorite_button.connect("clicked", lambda *_: self._toggle_favorite(selected))
        self.detail.append(favorite_button)

    def _append_detail_properties(self, selected: str, item: dict[str, object]) -> None:
        properties = item.get("properties", {})
        if not isinstance(properties, dict) or not properties:
            return

        self.detail.append(_label(tr("OPÇÕES DO WALLPAPER"), css="page-kicker"))
        self.detail.append(_label(
            tr("Opções booleanas definidas pelo autor do wallpaper."),
            css="caption", wrap=True,
        ))
        saved_properties = self.config.get("wallpaper_properties", {}).get(selected, {})
        for name, metadata in properties.items():
            if not isinstance(metadata, dict):
                continue
            row = _box(spacing=8)
            row.set_valign(Gtk.Align.CENTER)
            row.add_css_class("settings-row")
            label = _label(str(metadata.get("label") or name), wrap=True)
            label.set_hexpand(True)
            row.append(label)
            option = Gtk.Switch()
            option.set_valign(Gtk.Align.CENTER)
            option.set_active(bool(saved_properties.get(name, metadata.get("value", False))))
            option.set_tooltip_text(str(name))
            option.connect(
                "notify::active",
                lambda switch, _pspec, item_id=selected, property_name=name:
                    self._wallpaper_property_changed(switch, item_id, property_name),
            )
            row.append(option)
            self.detail.append(row)

    def _append_detail_playlists(self, selected: str) -> None:
        self.detail.append(_label(tr("PLAYLISTS"), css="page-kicker"))
        playlist_names = list((self.config.get("playlists") or {}).keys())
        if not playlist_names:
            create_playlist = _icon_button(tr("Criar primeira playlist"), "list-add-symbolic")
            create_playlist.connect(
                "clicked", lambda *_: self._playlist_name_dialog(tr("Criar playlist"), None)
            )
            self.detail.append(create_playlist)
            return

        add_row = _box(vertical=True, spacing=7)
        display_names = [name if len(name) <= 25 else name[:24] + "…" for name in playlist_names]
        playlist_picker = Gtk.DropDown.new_from_strings(display_names)
        if self.playlist_name in playlist_names:
            playlist_picker.set_selected(playlist_names.index(self.playlist_name))
        playlist_picker.set_hexpand(True)
        add_row.append(playlist_picker)
        add_playlist = Gtk.Button(label=tr("Adicionar"))
        add_playlist.connect(
            "clicked", lambda *_: self._playlist_add_id(
                playlist_names[playlist_picker.get_selected()], selected
            )
        )
        add_row.append(add_playlist)
        self.detail.append(add_row)

    def _append_detail_screens(self, selected: str) -> None:
        screens = self.status.get("screens") or []
        if not screens:
            return

        assignments = self.config.get("screen_assignments", {})
        self.detail.append(_label(tr("TELAS"), css="page-kicker"))
        for screen in screens:
            row = _box(spacing=6)
            row.set_valign(Gtk.Align.CENTER)
            row.add_css_class("settings-row")
            name = _label(str(screen))
            name.set_hexpand(True)
            row.append(name)
            assigned = assignments.get(screen)
            if assigned == selected:
                button = Gtk.Button(label=tr("Liberar"))
                button.connect(
                    "clicked", lambda _button, target=screen: self._command(
                        "assign", screen=target, id=None
                    )
                )
            else:
                button = Gtk.Button(label=tr("Fixar aqui"))
                button.connect(
                    "clicked", lambda _button, target=screen: self._command(
                        "assign", screen=target, id=selected
                    )
                )
            row.append(button)
            self.detail.append(row)
            if assigned and assigned != selected:
                other = self.catalog.get(assigned, {}).get("title", assigned)
                self.detail.append(
                    _label(tr("Fixado: {title}", title=other), css="caption", wrap=True)
                )

    def _show_details(self, wallpaper_id: str | None) -> None:
        self.selected_id = wallpaper_id if wallpaper_id in self.catalog else None
        self.detail_shell.set_visible(self.selected_id is not None)
        self._last_pane_width = None
        GLib.idle_add(self._update_responsive_once)
        _clear(self.detail)
        if self.selected_id is None:
            self._show_empty_details()
            return

        selected = self.selected_id
        item = self.catalog[selected]
        self._append_detail_header(selected, item)
        self._append_detail_properties(selected, item)
        self._append_detail_playlists(selected)
        self._append_detail_screens(selected)
        self._refresh_hero()

    def _toggle_favorite(self, wallpaper_id: str | None) -> None:
        if wallpaper_id is None:
            return
        favorites = set(self.config.get("favorites", []))
        if wallpaper_id in favorites:
            favorites.remove(wallpaper_id)
        else:
            favorites.add(wallpaper_id)
        self._set_settings(favorites=sorted(favorites))
        if self.selected_id == wallpaper_id:
            self._show_details(wallpaper_id)

    def _wallpaper_property_changed(
        self, switch: Gtk.Switch, wallpaper_id: str, property_name: str
    ) -> None:
        properties = {
            item_id: dict(values)
            for item_id, values in self.config.get("wallpaper_properties", {}).items()
        }
        properties.setdefault(wallpaper_id, {})[property_name] = switch.get_active()
        self._set_settings(wallpaper_properties=properties)

    def _set_settings(self, **settings: object) -> None:
        previous_language = self.config.get("language", "auto")
        try:
            self.config = model.validate_config(dict(self.config, **settings))
        except ValueError as exc:
            self._notice(str(exc), error=True)
            self._apply_config()
            return
        if self.config.get("language", "auto") != previous_language:
            set_language(self.config["language"])
            self._rebuild_localized_ui()
        else:
            self._apply_config()
        if "favorites" in settings:
            GLib.idle_add(self._refresh_card_indicators_once)
            self._refresh_hero()
        if {"playlists", "active_playlist"} & settings.keys():
            self._refresh_playlists()
            self._show_details(self.selected_id)
        self._command("set", settings=settings)

    def _switch_changed(self, widget: Gtk.Switch, _property: object, key: str) -> None:
        if not self._updating_controls:
            self._set_settings(**{key: widget.get_active()})

    def _spin_changed(self, widget: Gtk.SpinButton, key: str) -> None:
        if self._updating_controls:
            return
        previous = self._spin_timers.pop(key, 0)
        if previous:
            GLib.source_remove(previous)
        value = widget.get_value_as_int()

        def apply() -> bool:
            self._spin_timers.pop(key, None)
            self._set_settings(**{key: value})
            return False

        self._spin_timers[key] = GLib.timeout_add(300, apply)

    def _scaling_changed(self, widget: Gtk.DropDown, _property: object) -> None:
        if not self._updating_controls:
            self._set_settings(scaling=SCALINGS[widget.get_selected()])

    def _rebuild_localized_ui(self) -> None:
        page = self.tabs.get_visible_child_name() or "library"
        query = self.search.get_text()
        filter_index = self.filter_index
        selected = self.selected_id
        playlist = self.playlist_name
        self._selected_ids.intersection_update(self.catalog)
        self._catalog_generation += 1
        self._compact_mode = None
        self._last_height_band = None
        self._last_pane_width = None
        self._ui_language = self.config.get("language", "auto")
        root = self.get_child()
        if root is not None:
            _release_previews_in(root)
        self._cards.clear()
        self._card_badges.clear()
        self._last_filtered_ids = None
        self._build()
        self.selected_id = selected
        self.playlist_name = playlist
        self.search.set_text(query)
        self._set_filter(filter_index)
        self._apply_config()
        self._refresh_playlists()
        self._show_details(selected)
        identifiers = sorted(
            self.catalog,
            key=lambda item: str(self.catalog[item].get("title", item)).casefold(),
        )
        self._gallery_all_ids = identifiers
        self._gallery_search = {
            wallpaper_id: " ".join(
                [str(self.catalog[wallpaper_id].get("title", "")), wallpaper_id]
                + [str(tag) for tag in self.catalog[wallpaper_id].get("tags", [])]
            ).casefold()
            for wallpaper_id in identifiers
        }
        self._selection_rebuilding = True
        try:
            self._gallery_model.splice(0, self._gallery_model.get_n_items(), identifiers)
        finally:
            self._selection_rebuilding = False
        self._last_filtered_ids = None
        self._filter_cards()
        self.tabs.set_visible_child_name(page)
        if self.status:
            self._apply_status(self.status, accept_config=False)
        else:
            self._refresh_status()
        self._refresh_autostart()
        if self._tray_bridge is not None and self._tray_bridge.running:
            try:
                self._stop_tray_bridge()
                self._start_tray_bridge()
            except (OSError, RuntimeError):
                pass
        GLib.idle_add(self._update_responsive_once)

    def _apply_config(self) -> None:
        self._updating_controls = True
        try:
            for widget, key in (
                (self.rotation_switch, "rotation_enabled"),
                (self.favorites_switch, "only_favorites"),
                (self.shuffle_switch, "shuffle"),
                (self.mute_switch, "mute"),
            ):
                widget.set_active(bool(self.config.get(key, False)))
            self.favorites_switch.set_sensitive(self.config.get("active_playlist") is None)
            self.interval_spin.set_value(int(self.config.get("interval_minutes", 10)))
            self.fps_spin.set_value(int(self.config.get("fps", 30)))
            scaling = self.config.get("scaling", "fill")
            self.scaling_drop.set_selected(SCALINGS.index(scaling) if scaling in SCALINGS else 0)
            self._refresh_language_choices(self.config.get("language", "auto"))
            if not self.renderer_entry.has_focus():
                renderer = str(self.config.get("renderer_path") or "auto")
                self.renderer_entry.set_text("" if renderer == "auto" else renderer)
        finally:
            self._updating_controls = False
        self._theme_updating = True
        try:
            self.theme_hue.set_value(int(self.config.get("ui_hue", 24)))
            self.theme_intensity.set_value(int(self.config.get("ui_intensity", 88)))
        finally:
            self._theme_updating = False
        self._apply_theme_preview()
        self._filter_cards()
        self._last_applied_config = dict(self.config)

    def _command(self, command: str, **kwargs: object) -> None:
        self._mutation_generation += 1
        self._command_queue.append((command, kwargs))
        self._drain_commands()

    def _drain_commands(self) -> None:
        if self._command_busy or not self._command_queue:
            return
        command, kwargs = self._command_queue.pop(0)
        self._command_busy = True

        def done(value: dict | None, error: Exception | None) -> None:
            self._command_busy = False
            if error:
                self._notice(tr("Não foi possível executar {command}: {error}",
                                command=command, error=error), error=True)
                self._command_queue.clear()
                self._refresh_status()
                return
            if value:
                self._apply_status(value, accept_config=not self._command_queue)
            self._notice(tr("Alteração aplicada."))
            self._drain_commands()

        self._background(lambda: ipc.request(command, **kwargs), done)

    def _refresh_status(self) -> None:
        if self._pending_status or self._command_busy or self._command_queue:
            return
        self._pending_status = True
        request_generation = self._mutation_generation
        had_pending_command = self._command_busy or bool(self._command_queue)

        def done(value: dict | None, error: Exception | None) -> None:
            self._pending_status = False
            if had_pending_command or request_generation != self._mutation_generation:
                return
            if error:
                if self.service_available:
                    self._notice(tr("Serviço indisponível: {error}", error=error), error=True)
                self.service_available = False
                self.status = {}
                self.status_strip.add_css_class("error")
                self.status_dot.set_text("○")
                self.status_label.set_text(tr("Serviço parado ou ainda não instalado"))
                self.power_button.set_label(tr("Iniciar"))
                self.next_button.set_sensitive(False)
                self.countdown_label.set_text("")
                self._refresh_hero()
                self._refresh_card_indicators()
                return
            self.service_available = True
            self._apply_status(value or {})

        self._background(lambda: ipc.request("status"), done)

    def _accept_status_config(self, status: dict, accept_config: bool) -> bool:
        if not accept_config:
            return False
        updated_config = dict(status.get("config") or self.config)
        updated_config.update({
            key: self._ui_preferences[key] for key in ("ui_hue", "ui_intensity")
        })
        configuration_changed = updated_config != self.config
        self.config = updated_config
        if self.config.get("language", "auto") != self._ui_language:
            set_language(self.config.get("language", "auto"))
            self._rebuild_localized_ui()
            return True
        if configuration_changed or self._last_applied_config != self.config:
            self._apply_config()
        return False

    def _update_status_controls(self, status: dict) -> str | None:
        running = bool(status.get("renderer_running"))
        self.status_dot.set_text("●" if running else "○")
        current_id = status.get("current_id")
        if running and current_id:
            item = self.catalog.get(current_id, {})
            title = item.get("title") or current_id
            self.status_label.set_text(tr("Em execução: {title}", title=title))
        elif status.get("error"):
            self.status_label.set_text(tr("Aguardando: {error}", error=status["error"]))
        elif status.get("running"):
            self.status_label.set_text(tr("Iniciando wallpaper…"))
        else:
            self.status_label.set_text(tr("Serviço ativo · wallpaper parado"))

        self.power_button.set_label(tr("Parar" if status.get("running") else "Iniciar"))
        screens = status.get("screens") or []
        assignments = self.config.get("screen_assignments", {})
        has_rotating_screen = any(screen not in assignments for screen in screens)
        self.next_button.set_sensitive(bool(status.get("running")) and has_rotating_screen)
        self._update_countdown()
        self._refresh_hero()
        return current_id

    def _refresh_status_dependents(
        self,
        status: dict,
        current_id: str | None,
        old_current: object,
        old_screens: object,
        old_assignments: object,
        old_favorites: object,
        old_playlists: object,
        old_active_playlist: object,
    ) -> None:
        cards_changed = (
            current_id != old_current
            or status.get("screens") != old_screens
            or self.config.get("screen_assignments") != old_assignments
            or self.config.get("favorites") != old_favorites
        )
        if cards_changed:
            self._refresh_card_indicators()
            self._show_details(self.selected_id)

        playlists_changed = (
            self.config.get("playlists") != old_playlists
            or self.config.get("active_playlist") != old_active_playlist
        )
        if playlists_changed:
            self._refresh_playlists()
            self._show_details(self.selected_id)

    def _apply_status(self, status: dict, *, accept_config: bool = True) -> None:
        old_current = self.status.get("current_id")
        old_screens = self.status.get("screens")
        old_assignments = self.config.get("screen_assignments")
        old_favorites = self.config.get("favorites")
        old_playlists = self.config.get("playlists")
        old_active_playlist = self.config.get("active_playlist")
        self.status = status
        self.status_strip.remove_css_class("error")

        if self._accept_status_config(status, accept_config):
            return
        current_id = self._update_status_controls(status)
        self._refresh_status_dependents(
            status,
            current_id,
            old_current,
            old_screens,
            old_assignments,
            old_favorites,
            old_playlists,
            old_active_playlist,
        )

    def _update_countdown(self) -> None:
        deadline = self.status.get("next_change_at")
        if not deadline or not self.config.get("rotation_enabled"):
            self.countdown_label.set_text("")
            return
        remaining = max(0, int(float(deadline) - time.time()))
        minutes, seconds = divmod(remaining, 60)
        self.countdown_label.set_text(tr("Próxima troca em {minutes:02d}:{seconds:02d}",
                                         minutes=minutes, seconds=seconds))

    def _toggle_power(self, _button: Gtk.Button) -> None:
        if not self.service_available:
            self._systemctl("start")
        elif self.status.get("running"):
            self._command("stop")
        else:
            self._command("start")

    def _systemctl(self, verb: str) -> None:
        def run() -> None:
            result = subprocess.run(
                ["systemctl", "--user", verb, SERVICE],
                capture_output=True,
                text=True,
                timeout=15,
                check=False,
            )
            if result.returncode:
                raise RuntimeError(result.stderr.strip() or result.stdout.strip() or tr("systemctl falhou"))

        def done(_value: object, error: Exception | None) -> None:
            if error:
                verb_label = tr(
                    {"start": "iniciar", "enable": "ativar", "disable": "desativar"}.get(verb, verb)
                )
                self._notice(tr("Não foi possível {verb} o serviço: {error}",
                                verb=verb_label, error=error), error=True)
            else:
                self._refresh_status()
            self._refresh_autostart()

        self._background(run, done)

    def _tray_icon_file(self) -> str | None:
        configured = os.environ.get("LINUX_WALLPAPERENGINE_ICON_FILE")
        candidates = [
            configured,
            os.path.expanduser(
                "~/.local/share/icons/hicolor/scalable/apps/linux-wallpaperengine-app.svg"
            ),
            os.path.join(
                os.path.dirname(os.path.dirname(__file__)), "linux-wallpaperengine-app.svg"
            ),
        ]
        return next((path for path in candidates if path and os.path.isfile(path)), None)

    def _restore_from_tray(self) -> None:
        self.present()
        self._update_countdown()
        self._refresh_status()
        self._check_library_changes()
        self._update_responsive()

    def _start_tray_bridge(self) -> None:
        if self._tray_bridge is not None and self._tray_bridge.running:
            return
        self._stop_tray_bridge()
        bridge = TrayBridge(self._restore_from_tray, self._quit_from_tray)
        bridge.start(
            open_label=tr("Restaurar a janela"),
            quit_label=tr("Sair do aplicativo"),
            icon_file=self._tray_icon_file(),
        )
        self._tray_bridge = bridge

    def _stop_tray_bridge(self) -> None:
        bridge, self._tray_bridge = self._tray_bridge, None
        if bridge is not None:
            bridge.stop()

    def _close_requested(self, _window: Gtk.Window) -> bool:
        if self._allow_close:
            self._stop_tray_bridge()
            return False
        if self._ui_preferences.get("minimize_to_tray"):
            try:
                self._start_tray_bridge()
            except (OSError, RuntimeError) as exc:
                self._notice(tr("Não foi possível iniciar a bandeja: {error}", error=exc), error=True)
                self._set_switch_value(self.minimize_to_tray_switch, False)
                self._ui_preferences["minimize_to_tray"] = False
                try:
                    self._ui_preferences = model.save_ui_preferences({"minimize_to_tray": False})
                except OSError as save_error:
                    self._notice(tr("Não foi possível salvar a preferência: {error}", error=save_error), error=True)
                return False
            self.hide()
            return True
        self._stop_tray_bridge()
        return False

    def _quit_from_tray(self) -> None:
        self._allow_close = True
        self._stop_tray_bridge()
        self.close()

    def _set_switch_value(self, switch: Gtk.Switch, value: bool) -> None:
        self._updating_controls = True
        switch.set_active(value)
        self._updating_controls = False

    def _minimize_to_tray_changed(self, widget: Gtk.Switch, _property: object) -> None:
        if self._updating_controls:
            return
        enabled = widget.get_active()
        bridge: TrayBridge | None = None
        try:
            if enabled:
                bridge = TrayBridge(self._restore_from_tray, self._quit_from_tray)
                bridge.start(
                    open_label=tr("Restaurar a janela"),
                    quit_label=tr("Sair do aplicativo"),
                    icon_file=self._tray_icon_file(),
                )
            self._ui_preferences = model.save_ui_preferences({"minimize_to_tray": enabled})
        except (OSError, RuntimeError, ValueError) as exc:
            if bridge is not None:
                bridge.stop()
            self._set_switch_value(widget, not enabled)
            self._notice(tr("Não foi possível alterar a preferência: {error}", error=exc), error=True)
            return
        if enabled:
            self._tray_bridge = bridge
        else:
            self._stop_tray_bridge()

    def _start_app_changed(self, widget: Gtk.Switch, _property: object) -> None:
        if self._updating_controls:
            return
        enabled = widget.get_active()
        previous = bool(self._ui_preferences.get("start_app_with_session", False))
        try:
            model.set_app_autostart(enabled)
            self._ui_preferences = model.save_ui_preferences({"start_app_with_session": enabled})
        except (OSError, RuntimeError, ValueError) as exc:
            try:
                model.set_app_autostart(previous)
            except (OSError, RuntimeError, ValueError):
                pass
            self._set_switch_value(widget, previous)
            self._notice(tr("Não foi possível alterar a preferência: {error}", error=exc), error=True)

    def _refresh_autostart(self) -> None:
        def run() -> bool:
            result = subprocess.run(
                ["systemctl", "--user", "is-enabled", SERVICE],
                capture_output=True,
                text=True,
                timeout=5,
                check=False,
            )
            return result.returncode == 0 and result.stdout.strip() == "enabled"

        def done(value: bool | None, _error: Exception | None) -> None:
            self._updating_controls = True
            self.autostart_switch.set_active(bool(value))
            self._updating_controls = False

        self._background(run, done)

    def _autostart_changed(self, widget: Gtk.Switch, _property: object) -> None:
        if not self._updating_controls:
            self._systemctl("enable" if widget.get_active() else "disable")

    def _check_library_changes(self) -> bool:
        if self._allow_close:
            return False
        if not self.get_visible():
            return True
        if self._catalog_scan_pending:
            return True
        self._catalog_scan_pending = True

        def done(catalog: dict | None, error: Exception | None) -> None:
            self._catalog_scan_pending = False
            if error:
                return
            if catalog != self.catalog:
                self._load_catalog()

        self._background(model.scan_catalog, done)
        return True

    def _reload(self) -> None:
        self._load_catalog()
        if self.service_available:
            self._command("reload")

    def _tick(self) -> bool:
        if self._allow_close:
            return False
        if not self.get_visible():
            return True
        if self.config.get("language") == "auto" and current_language() != system_language():
            set_language("auto")
            self._rebuild_localized_ui()
        self._update_countdown()
        self._refresh_status()
        return True

    def _notice(self, text: str, *, error: bool = False) -> None:
        self.message.set_text(text)
        self.message.set_visible(True)
        self.message.remove_css_class("error")
        if error:
            self.message.add_css_class("error")
        if self._toast_timer:
            GLib.source_remove(self._toast_timer)
        self._toast_timer = GLib.timeout_add_seconds(5, self._hide_notice)

    def _hide_notice(self) -> bool:
        self.message.set_visible(False)
        self._toast_timer = 0
        return False


class WallpaperApplication(Gtk.Application):
    def __init__(self):
        super().__init__(
            application_id="io.github.xoykor.LinuxWallpaperEngineApp",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self) -> None:
        window = self.props.active_window
        if window is None:
            window = WallpaperWindow(self)
        window.present()


def main() -> int:
    return WallpaperApplication().run(None)
