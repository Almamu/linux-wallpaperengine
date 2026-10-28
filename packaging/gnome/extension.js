/* exported init */
'use strict';

const {Clutter, Gio, GLib, Meta} = imports.gi;
const Main = imports.ui.main;
const UnlockDialog = imports.ui.unlockDialog;
const ExtensionUtils = imports.misc.extensionUtils;

class WallpaperExtension {
    /** Connect lifecycle signals and install desktop and lock background support. */
    enable() {
        if (!Meta.is_wayland_compositor())
            throw new Error('The wallpaper extension requires a Wayland session');

        this._enabled = true;
        this._signals = [];
        this._renderers = new Map();
        this._recoveryId = 0;
        this._keepAwakeId = 0;
        this._lockBackgrounds = new Map();
        this._settings = ExtensionUtils.getSettings();
        this._wasLocked = this._isLocked();
        this._connect(global.window_manager, 'map', (_wm, actor) => this._attach(actor));
        this._connect(global.display, 'in-fullscreen-changed', () => this._updatePause());
        this._connect(Main.layoutManager, 'monitors-changed', () => this._restart());
        this._connect(Main.sessionMode, 'updated', () => this._syncSession());
        this._connect(Main.screenShield, 'locked-changed', () => this._syncSession());
        this._connect(Main.screenShield, 'active-changed', () => this._updateLockScreenPower());
        this._connect(Main.screenShield, 'lock-screen-shown', () => this._updateLockScreenPower());
        this._connect(this._settings, 'changed', (_settings, key) => {
            if (key === 'paused')
                this._updatePause();
            else if (key === 'keep-lock-screen-on')
                this._updateLockScreenPower();
            else if (key === 'lock-wallpapers' || key === 'lock-mirrors')
                this._refreshLockBackgrounds();
            else
                this._restart();
        });
        this._installLockBackgrounds();
        if (Main.layoutManager._startingUp)
            this._connect(Main.layoutManager, 'startup-complete', () => this._launch());
        else
            this._launch();
        this._updateLockScreenPower();
    }

    /** Track a signal connection so disable can disconnect it. */
    _connect(object, signal, callback) {
        this._signals.push([object, object.connect(signal, callback)]);
    }

    /** Include the transition into the unlock dialog before the locked flag changes. */
    _isLocked() {
        return Main.sessionMode.currentMode === 'unlock-dialog' || Main.screenShield.locked;
    }

    /** Start the desktop renderer and rebuild any existing lock backgrounds. */
    _launch() {
        if (!this._enabled || !Main.layoutManager.primaryMonitor)
            return;
        this._desktop = this._ensureRenderer(this._settings.get_string('wallpaper'),
            Main.layoutManager.primaryMonitor);
        const dialog = Main.screenShield._dialog;
        if (dialog)
            dialog._updateBackgrounds();
        this._updatePause();
    }

    /** Build a muted native-window command without invoking a shell. */
    _rendererCommand(wallpaper, monitor) {
        const executable = GLib.find_program_in_path(this._settings.get_string('engine-path'));
        if (!executable) {
            log('linux-wallpaperengine: engine-path does not point to an executable');
            return null;
        }
        const argv = [
            executable, '--window', `0x0x${monitor.width}x${monitor.height}`,
            '--fps', String(this._settings.get_uint('fps')),
            '--silent', '--noautomute', '--disable-mouse', '--no-fullscreen-pause',
        ];
        const assets = this._settings.get_string('assets-dir');
        if (assets)
            argv.push('--assets-dir', assets);
        argv.push(wallpaper);

        return argv;
    }

    /** Reuse identical projects or spawn a native client and reap it asynchronously. */
    _ensureRenderer(wallpaper, monitor) {
        if (!wallpaper)
            return null;
        if (this._renderers.has(wallpaper))
            return this._renderers.get(wallpaper);
        const argv = this._rendererCommand(wallpaper, monitor);
        if (!argv)
            return null;

        const launcher = new Gio.SubprocessLauncher({flags: Gio.SubprocessFlags.NONE});
        const renderer = {wallpaper, signals: [], clones: [], paused: false, actor: null};
        try {
            renderer.client = Meta.WaylandClient.new(launcher);
            renderer.process = renderer.client.spawnv(global.display, argv);
        } catch (error) {
            logError(error, 'linux-wallpaperengine: failed to start renderer');
            return null;
        } finally {
            launcher.close();
        }
        this._renderers.set(wallpaper, renderer);
        // A desktop project requested by a lock monitor must retain its desktop role.
        if (wallpaper === this._settings.get_string('wallpaper'))
            this._desktop = renderer;
        // Keep the native client alive until its child has been reaped, even on disable.
        renderer.process.wait_async(null, (source, result) => {
            source.wait_finish(result);
            renderer.exited = true;
            if (renderer.shutdownId) {
                GLib.source_remove(renderer.shutdownId);
                renderer.shutdownId = 0;
            }
            if (this._renderers.get(wallpaper) !== renderer)
                return;
            this._renderers.delete(wallpaper);
            if (renderer === this._desktop)
                this._desktop = null;
            this._clearWindow(renderer);
            for (const binding of this._lockBackgrounds.values()) {
                if (binding.renderer === renderer)
                    binding.renderer = null;
            }
            this._scheduleRecovery();
            log('linux-wallpaperengine: renderer exited');
        });
        return renderer;
    }

    /** Retry failed playback after a delay, using current settings and lock bindings. */
    _scheduleRecovery() {
        if (!this._enabled || this._recoveryId)
            return;
        this._recoveryId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 5, () => {
            this._recoveryId = 0;
            if (this._enabled)
                this._launch();
            return GLib.SOURCE_REMOVE;
        });
    }

    /** Clone only an owned native window, keeping the source minimized. */
    _attach(actor) {
        const window = actor.meta_window;
        const renderer = [...this._renderers.values()].find(candidate =>
            candidate.client.owns_window(window));
        if (!renderer || renderer.actor)
            return;
        const monitor = Main.layoutManager.primaryMonitor;
        renderer.actor = actor;
        renderer.window = window;
        renderer.client.hide_from_window_list(window);
        window.move_to_monitor(monitor.index);
        window.move_resize_frame(false, monitor.x, monitor.y, monitor.width, monitor.height);
        if (renderer === this._desktop)
            this._rebuildDesktopClones(renderer);
        for (const binding of this._lockBackgrounds.values()) {
            if (binding.renderer === renderer)
                this._cloneLockBackground(binding);
        }
        // Clutter keeps the cloned surface painted while the source window is minimized.
        renderer.signals.push([window, window.connect('notify::minimized', () => {
            if (this._enabled && !window.minimized)
                window.minimize();
        })]);
        renderer.signals.push([window, window.connect('unmanaged', () => this._clearWindow(renderer))]);
        window.minimize();
        this._updatePause();
    }

    /** Recreate and restack every output after lock/display transitions. */
    _rebuildDesktopClones(renderer) {
        for (const clone of renderer.clones)
            clone.destroy();
        renderer.clones = [];
        for (const output of Main.layoutManager.monitors) {
            const clone = new Clutter.Clone({
                source: renderer.actor, reactive: false,
                x: output.x, y: output.y, width: output.width, height: output.height,
            });
            Main.layoutManager._backgroundGroup.add_child(clone);
            renderer.clones.push(clone);
        }
    }

    /** Wrap per-monitor background creation while preserving GNOME authentication UI. */
    _installLockBackgrounds() {
        const extension = this;
        const originalCreate = UnlockDialog.UnlockDialog.prototype._createBackground;
        this._originalCreateBackground = originalCreate;
        this._createBackground = function (monitorIndex) {
            originalCreate.call(this, monitorIndex);
            if (extension._enabled)
                extension._bindLockBackground(this._backgroundGroup.get_last_child(), monitorIndex);
        };
        UnlockDialog.UnlockDialog.prototype._createBackground = this._createBackground;
        const shield = Main.screenShield;
        const originalShown = shield._lockScreenShown;
        this._originalLockScreenShown = originalShown;
        this._lockScreenShown = function (params) {
            if (extension._enabled && extension._settings.get_boolean('keep-lock-screen-on')) {
                this._longLightbox.lightOff();
                this._shortLightbox.lightOff();
                params = {...params, fadeToBlack: false};
            }
            originalShown.call(this, params);
        };
        shield._lockScreenShown = this._lockScreenShown;
    }

    /** Keep the locked display visible using GNOME's normal wake signal, without unlocking. */
    _updateLockScreenPower() {
        if (this._keepAwakeId) {
            GLib.source_remove(this._keepAwakeId);
            this._keepAwakeId = 0;
        }
        if (!this._enabled || !this._settings.get_boolean('keep-lock-screen-on') ||
            !this._isLocked() || !Main.screenShield.active)
            return;
        this._wakeLockScreen();
        // GNOME 42 grants fifteen seconds per WakeUpScreen request.
        this._keepAwakeId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 10, () => {
            this._wakeLockScreen();
            return GLib.SOURCE_CONTINUE;
        });
    }

    /** Remove blanking overlays and request display power without touching authentication. */
    _wakeLockScreen() {
        Main.screenShield._longLightbox.lightOff();
        Main.screenShield._shortLightbox.lightOff();
        Main.screenShield.emit('wake-up-screen');
    }

    /** Select the configured project or desktop fallback for one lock monitor. */
    _bindLockBackground(widget, monitorIndex) {
        const monitor = Main.layoutManager.monitors[monitorIndex];
        const wallpapers = this._settings.get_strv('lock-wallpapers');
        const wallpaper = wallpapers[monitorIndex] || this._settings.get_string('wallpaper');
        const blur = widget.get_effect('blur');
        const binding = {
            blur, blurEnabled: blur ? blur.enabled : false,
            widget, monitorIndex, backgrounds: widget.get_children(), clone: null,
            renderer: this._ensureRenderer(wallpaper, monitor),
        };
        binding.destroyId = widget.connect('destroy', () => this._lockBackgrounds.delete(widget));
        this._lockBackgrounds.set(widget, binding);
        if (binding.renderer && binding.renderer.actor)
            this._cloneLockBackground(binding);
        this._updatePause();
    }

    /** Add a clear, optionally mirrored video while preserving the clock and prompt. */
    _cloneLockBackground(binding) {
        if (binding.clone)
            return;
        const monitor = Main.layoutManager.monitors[binding.monitorIndex];
        const mirrored = this._settings.get_value('lock-mirrors').deep_unpack()[binding.monitorIndex];
        binding.clone = new Clutter.Clone({
            source: binding.renderer.actor, reactive: false,
            width: monitor.width, height: monitor.height,
        });
        if (mirrored) {
            binding.clone.set_pivot_point(0.5, 0.5);
            binding.clone.scale_x = -1;
        }
        binding.widget.add_child(binding.clone);
        if (binding.blur)
            binding.blur.enabled = false;
        for (const background of binding.backgrounds)
            background.hide();
    }

    /** Update lock settings without restarting desktop playback. */
    _refreshLockBackgrounds() {
        for (const renderer of this._renderers.values()) {
            if (renderer !== this._desktop)
                this._stopRenderer(renderer);
        }
        const dialog = Main.screenShield._dialog;
        if (dialog)
            dialog._updateBackgrounds();
        this._updatePause();
    }

    /** Stop lock-only renderers after unlock and recompute visibility. */
    _syncSession() {
        const locked = this._isLocked();
        if (!locked) {
            for (const renderer of this._renderers.values()) {
                if (renderer !== this._desktop)
                    this._stopRenderer(renderer);
            }
            if (this._wasLocked && this._desktop && this._desktop.actor)
                this._rebuildDesktopClones(this._desktop);
        }
        this._wasLocked = locked;
        this._updatePause();
        this._updateLockScreenPower();
    }

    /** Suspend mapped renderers only when manually paused or unused. */
    _updatePause() {
        const locked = this._isLocked();
        const monitors = Main.layoutManager.monitors;
        for (const renderer of this._renderers.values()) {
            let visible = false;
            for (let index = 0; index < renderer.clones.length; index++) {
                const clone = renderer.clones[index];
                clone.visible = !locked && !global.display.get_monitor_in_fullscreen(monitors[index].index);
                visible ||= clone.visible;
            }
            if (locked) {
                visible ||= [...this._lockBackgrounds.values()].some(binding =>
                    binding.renderer === renderer && binding.clone);
            }
            // Do not suspend a client before its first window has mapped.
            const paused = !!renderer.actor && (this._settings.get_boolean('paused') || !visible);
            if (paused !== renderer.paused) {
                renderer.process.send_signal(paused ? 19 : 18); // SIGSTOP / SIGCONT on Linux
                renderer.paused = paused;
            }
        }
    }

    /** Destroy owned clones and restore the original lock background actors. */
    _clearWindow(renderer) {
        for (const [object, id] of renderer.signals)
            object.disconnect(id);
        renderer.signals = [];
        for (const clone of renderer.clones)
            clone.destroy();
        renderer.clones = [];
        for (const binding of this._lockBackgrounds.values()) {
            if (binding.renderer !== renderer)
                continue;
            if (binding.clone)
                binding.clone.destroy();
            binding.clone = null;
            if (binding.blur)
                binding.blur.enabled = binding.blurEnabled;
            for (const background of binding.backgrounds)
                background.show();
        }
        renderer.actor = null;
        renderer.window = null;
    }

    /** Terminate and reap one renderer, forcing exit after a bounded grace period. */
    _stopRenderer(renderer) {
        this._renderers.delete(renderer.wallpaper);
        if (renderer.paused)
            renderer.process.send_signal(18); // Let a stopped process handle SIGTERM.
        renderer.process.send_signal(15);
        // A hidden Wayland client can remain blocked waiting for a frame callback.
        // Bound shutdown so unlocking/disable cannot leave a decoder running.
        renderer.shutdownId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 5000, () => {
            renderer.shutdownId = 0;
            if (!renderer.exited)
                renderer.process.force_exit();
            return GLib.SOURCE_REMOVE;
        });
        this._clearWindow(renderer);
    }

    /** Stop all renderers before restart or disable. */
    _stop() {
        if (this._recoveryId) {
            GLib.source_remove(this._recoveryId);
            this._recoveryId = 0;
        }
        for (const renderer of this._renderers.values())
            this._stopRenderer(renderer);
        this._desktop = null;
    }

    /** Recreate playback with the current settings and monitor layout. */
    _restart() {
        if (!this._enabled || Main.layoutManager._startingUp)
            return;
        this._stop();
        this._launch();
    }

    /** Restore the background hook, disconnect signals and stop owned processes. */
    disable() {
        this._enabled = false;
        this._updateLockScreenPower();
        for (const [object, id] of this._signals)
            object.disconnect(id);
        this._signals = [];
        this._stop();
        for (const binding of this._lockBackgrounds.values())
            binding.widget.disconnect(binding.destroyId);
        this._lockBackgrounds.clear();
        if (UnlockDialog.UnlockDialog.prototype._createBackground === this._createBackground)
            UnlockDialog.UnlockDialog.prototype._createBackground = this._originalCreateBackground;
        if (Main.screenShield._lockScreenShown === this._lockScreenShown)
            Main.screenShield._lockScreenShown = this._originalLockScreenShown;
        const dialog = Main.screenShield._dialog;
        if (dialog)
            dialog._updateBackgrounds();
        this._settings = null;
    }
}

/** Create the extension instance for GNOME Shell. */
function init() {
    return new WallpaperExtension();
}
