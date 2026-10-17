from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest import mock

APP_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP_DIR))

from wallpaper_engine_app import daemon, i18n, model


class SteamLibraryTests(unittest.TestCase):
    def test_libraryfolders_reads_new_and_legacy_formats(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            steamapps = Path(temporary)
            (steamapps / "libraryfolders.vdf").write_text(
                '"libraryfolders"\n'
                '{\n'
                '\t"1"\n'
                '\t{\n'
                '\t\t"path"\t\t"/mnt/Games/SteamLibrary"\n'
                '\t}\n'
                '\t"2"\t"/srv/Steam"\n'
                '}\n',
                encoding="utf-8",
            )
            self.assertEqual(
                model._libraryfolders_paths(steamapps),
                [
                    Path("/mnt/Games/SteamLibrary/steamapps"),
                    Path("/srv/Steam/steamapps"),
                ],
            )

    def test_scan_catalog_resolves_assets_from_primary_steam_library(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            primary = root / "primary/steamapps"
            secondary = root / "secondary/steamapps"
            assets_dir = primary / "common/wallpaper_engine/assets"
            assets_dir.mkdir(parents=True)
            item_dir = secondary / "workshop/content" / model.WORKSHOP_APP_ID / "12345"
            item_dir.mkdir(parents=True)
            (item_dir / "project.json").write_text(
                '{"title": "Cross Library Scene", "type": "scene"}',
                encoding="utf-8",
            )

            with mock.patch.object(
                model, "steamapps_roots", return_value=[primary, secondary]
            ):
                catalog = model.scan_catalog()

            self.assertIn("12345", catalog)
            self.assertEqual(catalog["12345"]["assets"], str(assets_dir))

    def test_xrandr_outputs_prefers_active_geometry_outputs(self) -> None:
        sample_output = (
            "Screen 0: minimum 320 x 200, current 2560 x 1440, maximum 16384 x 16384\n"
            "eDP-1 connected (normal left inverted right x axis y axis)\n"
            "DP-1 connected primary 2560x1440+0+0 (normal left inverted right x axis y axis) 597mm x 336mm\n"
            "HDMI-1 disconnected (normal left inverted right x axis y axis)\n"
        )
        completed = mock.Mock(stdout=sample_output)
        with (
            mock.patch.object(model.shutil, "which", return_value="/usr/bin/xrandr"),
            mock.patch.object(model.subprocess, "run", return_value=completed),
        ):
            self.assertEqual(model._xrandr_outputs(), ["DP-1"])


class UiPreferenceTests(unittest.TestCase):
    def test_ui_preferences_keep_legacy_theme_values_and_store_switches(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config_file = root / "config.json"
            ui_file = root / "ui.json"
            config_file.write_text('{"ui_hue": 123, "ui_intensity": 73}', encoding="utf-8")

            with (
                mock.patch.object(model, "CONFIG_FILE", config_file),
                mock.patch.object(model, "UI_PREFERENCES_FILE", ui_file),
            ):
                preferences = model.save_ui_preferences(
                    {"minimize_to_tray": True, "start_app_with_session": True}
                )

                self.assertEqual(preferences["ui_hue"], 123)
                self.assertEqual(preferences["ui_intensity"], 73)
                self.assertTrue(preferences["minimize_to_tray"])
                self.assertTrue(preferences["start_app_with_session"])
                self.assertEqual(model.load_ui_preferences(), preferences)

    def test_app_autostart_writes_a_quoted_entry_and_removes_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            home = root / "home with spaces"
            launcher = home / ".local/bin/linux-wallpaperengine-app"
            launcher.parent.mkdir(parents=True)
            launcher.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            launcher.chmod(0o700)
            config_dir = root / "config/linux-wallpaperengine-app"
            autostart_file = root / "config/autostart/linux-wallpaperengine-app.desktop"

            with (
                mock.patch.object(model, "CONFIG_DIR", config_dir),
                mock.patch.object(Path, "home", return_value=home),
            ):
                model.set_app_autostart(True)
                entry = autostart_file.read_text(encoding="utf-8")
                self.assertIn(f'Exec="{launcher}"', entry)
                self.assertTrue(autostart_file.exists())

                model.set_app_autostart(False)
                self.assertFalse(autostart_file.exists())


class DaemonRegressionTests(unittest.TestCase):
    def test_output_refresh_preserves_pending_wallpaper_during_shutdown(self) -> None:
        class RunningChild:
            def poll(self) -> int | None:
                return None

        service = daemon.WallpaperDaemon()
        service.catalog = {"1": {"title": "Current"}, "2": {"title": "Requested"}}
        service.outputs = ["DP-1"]
        service.current_id = "1"
        service.pending_id = "2"
        service.child = RunningChild()
        service.terminating = True

        with (
            mock.patch.object(
                model, "scan_catalog", side_effect=[service.catalog, {"2": {"title": "Requested"}}]
            ),
            mock.patch.object(
                model, "detect_outputs", side_effect=[["DP-1", "HDMI-A-1"], ["DP-1", "HDMI-A-1"]]
            ),
        ):
            service._refresh(force=True)
            service._refresh(force=True)

        self.assertEqual(service.pending_id, "2")

    def test_fixed_assignment_is_restored_after_crash_cooldown(self) -> None:
        class RunningChild:
            def poll(self) -> int | None:
                return None

        service = daemon.WallpaperDaemon()
        service.config = model.validate_config({"screen_assignments": {"DP-1": "1"}})
        service.catalog = {
            "1": {"title": "Fixed", "path": "/wallpapers/1", "assets": "/missing"},
            "2": {"title": "Fallback", "path": "/wallpapers/2", "assets": "/missing"},
        }
        service.outputs = ["DP-1"]
        service.screens = {"DP-1": "2"}
        service.current_id = "2"
        service.child = RunningChild()
        service.failed["1"] = time.time() - daemon._CRASH_COOLDOWN - 1

        with (
            mock.patch.object(service, "_reap_child"),
            mock.patch.object(service, "_refresh"),
            mock.patch.object(service, "_publish"),
        ):
            service._tick()

        self.assertTrue(service.terminating)
        self.assertEqual(service.pending_id, "2")

    def test_disabling_rotation_persists_current_wallpaper(self) -> None:
        service = daemon.WallpaperDaemon()
        service.config = model.validate_config({})
        service.catalog = {"123": {"title": "Current"}}
        service.current_id = "123"
        service.active = False

        with (
            mock.patch.object(service, "_refresh"),
            mock.patch.object(service, "_tick"),
            mock.patch.object(model, "save_config", side_effect=lambda value: value),
        ):
            status = service._dispatch(
                {"command": "set", "settings": {"rotation_enabled": False}}
            )

        self.assertFalse(status["config"]["rotation_enabled"])
        self.assertEqual(status["config"]["selected_id"], "123")

    def test_renderer_crash_cools_down_every_active_wallpaper(self) -> None:
        class CrashedChild:
            def poll(self) -> int:
                return 1

        service = daemon.WallpaperDaemon()
        service.config = model.validate_config({})
        service.catalog = {
            "1": {"title": "One"},
            "2": {"title": "Two"},
        }
        service.child = CrashedChild()
        service.child_group = None
        service.child_started = time.monotonic()
        service.current_id = "1"
        service.screens = {"DP-1": "1", "HDMI-A-1": "2"}

        service._reap_child()

        self.assertIn("1", service.failed)
        self.assertIn("2", service.failed)

    def test_crashing_fixed_wallpaper_falls_back_to_rotation_item(self) -> None:
        service = daemon.WallpaperDaemon()
        service.config = model.validate_config(
            {"screen_assignments": {"DP-1": "1"}}
        )
        service.outputs = ["DP-1", "HDMI-A-1"]
        service.catalog = {
            "1": {"assets": "/missing", "path": "/wallpapers/1", "title": "Bad"},
            "2": {"assets": "/missing", "path": "/wallpapers/2", "title": "Good"},
        }
        service.failed["1"] = time.time()

        with mock.patch.object(service, "_resolve_renderer", return_value="/bin/true"):
            _command, _environment, screens = service._build_command("2")

        self.assertEqual(screens["DP-1"], "2")
        self.assertEqual(screens["HDMI-A-1"], "2")

    def test_explicit_select_and_assign_clear_crash_cooldown(self) -> None:
        service = daemon.WallpaperDaemon()
        service.config = model.validate_config({})
        service.catalog = {
            "1": {"assets": "/missing", "path": "/wallpapers/1", "title": "One"},
            "2": {"assets": "/missing", "path": "/wallpapers/2", "title": "Two"},
        }
        service.outputs = ["DP-1"]
        service.failed["1"] = time.time()
        service.failed["2"] = time.time()
        service.crash_streak = 3

        with (
            mock.patch.object(service, "_refresh"),
            mock.patch.object(model, "save_config", side_effect=lambda value: value),
        ):
            service._command_select({"command": "select", "id": "1"})
            self.assertNotIn("1", service.failed)
            self.assertEqual(service.crash_streak, 0)
            self.assertEqual(service._pending_wallpaper(), "1")

            service._command_assign({"command": "assign", "screen": "DP-1", "id": "2"})
            self.assertNotIn("2", service.failed)

    def test_settings_and_assign_preserve_pending_wallpaper_while_terminating(self) -> None:
        class RunningChild:
            def poll(self) -> int | None:
                return None

        service = daemon.WallpaperDaemon()
        service.config = model.validate_config({})
        service.catalog = {
            "1": {"title": "Old"},
            "2": {"title": "New"},
        }
        service.outputs = ["DP-1"]
        service.current_id = "1"
        service.pending_id = "2"
        service.child = RunningChild()
        service.terminating = True

        with (
            mock.patch.object(service, "_refresh"),
            mock.patch.object(model, "save_config", side_effect=lambda value: value),
        ):
            service._command_set({"command": "set", "settings": {"fps": 60}})
            self.assertEqual(service.pending_id, "2")

            service._command_assign({"command": "assign", "screen": "DP-1", "id": "1"})
            self.assertEqual(service.pending_id, "2")

    def test_stop_then_start_during_termination_schedules_pending_wallpaper(self) -> None:
        class RunningChild:
            def poll(self) -> int | None:
                return None

        service = daemon.WallpaperDaemon()
        service.config = model.validate_config({"rotation_enabled": False, "selected_id": "1"})
        service.catalog = {"1": {"title": "One"}}
        service.outputs = ["DP-1"]
        service.current_id = "1"
        service.child = RunningChild()

        service._command_stop({"command": "stop"})
        self.assertFalse(service.active)
        self.assertTrue(service.terminating)
        self.assertIsNone(service.pending_id)

        service._command_start({"command": "start"})
        self.assertTrue(service.active)
        self.assertEqual(service.pending_id, "1")


class TranslationCompletenessTests(unittest.TestCase):
    def test_supported_languages_cover_all_translation_rows(self) -> None:
        seen_keys: set[str] = set()
        for row in i18n._ROWS:
            self.assertEqual(len(row), len(i18n._CODES))
            self.assertTrue(all(isinstance(entry, str) and entry for entry in row))
            self.assertNotIn(row[0], seen_keys)
            seen_keys.add(row[0])


if __name__ == "__main__":
    unittest.main()
