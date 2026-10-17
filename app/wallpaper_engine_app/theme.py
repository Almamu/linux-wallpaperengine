"""Dark glass GTK4 visual system, based on the remote desktop revamp."""

from __future__ import annotations

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import Gdk, Gtk


CSS = """
window {
  background: rgba(9,7,6,0.56);
  color: #f8f4ef;
}
.app-root {
  background: transparent;
}
.sidebar {
  min-width: 218px;
  padding: 18px 14px 16px;
  background: linear-gradient(180deg, rgba(34,25,21,0.68) 0%, rgba(22,17,15,0.60) 56%, rgba(12,10,9,0.66) 100%);
  border-right: 1px solid rgba(255,255,255,0.07);
}
.brand-mark {
  min-width: 36px;
  min-height: 36px;
  border-radius: 11px;
  background: rgba(255,255,255,0.08);
}
.brand-title {
  font-size: 1.12em;
  font-weight: 800;
}
.brand-subtitle {
  font-size: 0.80em;
  opacity: 0.58;
}
.nav-button {
  min-height: 44px;
  padding: 0 12px;
  border-radius: 11px;
  border: 0;
  box-shadow: none;
  background: transparent;
}
.nav-button:hover {
  background: rgba(255,255,255,0.065);
}
.nav-button image {
  opacity: 0.88;
}
.nav-button label {
  font-weight: 600;
}
.main-area {
  padding: 18px 20px 16px;
}
.page-title {
  font-size: 1.75em;
  font-weight: 800;
}
.page-subtitle {
  opacity: 0.58;
}
.surface {
  border-radius: 14px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.06);
}
.library-tools {
  padding: 10px;
  border-radius: 14px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.06);
}
.search-entry {
  min-height: 38px;
}
.wallpaper-card {
  padding: 0;
  border-radius: 14px;
  min-width: 244px;
  min-height: 192px;
  border: 1px solid rgba(255,255,255,0.075);
  background: #15110f;
  box-shadow: 0 5px 18px rgba(0,0,0,0.22);
}
.wallpaper-card:hover {
  border-color: rgba(255,255,255,0.18);
  background: #191310;
}
.wallpaper-card image {
  border-radius: 13px 13px 0 0;
}
.card-copy {
  padding: 7px 10px 9px;
}
.card-title {
  font-weight: 700;
}
.badge {
  margin: 9px;
  padding: 3px 7px;
  border-radius: 7px;
  font-size: 0.72em;
  font-weight: 800;
  background: rgba(16,12,10,0.82);
  color: #fff;
}
.detail-panel {
  margin-left: 14px;
  padding: 14px;
  border-radius: 16px;
  background: rgba(255,255,255,0.04);
  border: 1px solid rgba(255,255,255,0.065);
}
.detail-hero {
  border-radius: 13px;
}
.detail-title {
  font-size: 1.45em;
  font-weight: 800;
}
.status-strip {
  padding: 12px;
  border-radius: 13px;
  background: rgba(255,255,255,0.055);
  border: 1px solid rgba(255,255,255,0.06);
}
.status-dot {
  min-width: 8px;
  min-height: 8px;
  border-radius: 99px;
  background: #45d483;
}
.subtle {
  opacity: 0.62;
}
.section-title {
  font-size: 1.1em;
  font-weight: 750;
}
.settings-row {
  padding: 12px 14px;
  border-radius: 12px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.055);
}
.empty-state {
  font-size: 1.05em;
  opacity: 0.62;
}
.theme-popover {
  min-width: 330px;
  padding: 16px;
}
.theme-heading {
  font-size: 1.15em;
  font-weight: 800;
}
.theme-preview {
  min-height: 74px;
  border-radius: 14px;
  background: linear-gradient(110deg, #ff641f 0%, #ffad1f 48%, #d7262e 100%);
  border: 1px solid rgba(255,255,255,0.12);
}
scale.hue-slider trough {
  min-height: 10px;
  border-radius: 99px;
  background: linear-gradient(90deg,
    #ff3030 0%, #ffd230 16%, #5fe05f 33%, #35dbe4 50%,
    #4a6cff 66%, #c84cff 83%, #ff3030 100%);
}
scale.intensity-slider trough {
  min-height: 8px;
  border-radius: 99px;
}
.preset-dot {
  min-width: 30px;
  min-height: 30px;
  padding: 0;
  border-radius: 99px;
  color: transparent;
  border: 2px solid rgba(255,255,255,0.16);
}
.preset-amber { background: #ff7417; }
.preset-gold { background: #f6b91f; }
.preset-red { background: #e43b2f; }
.preset-violet { background: #a24ce6; }
.preset-blue { background: #397be8; }
.preset-green { background: #39ad70; }
.toast {
  margin-top: 8px;
  padding: 9px 12px;
  border-radius: 10px;
  background: rgba(255,255,255,0.07);
}
.error {
  color: #ff8787;
}
button {
  border-radius: 10px;
}
entry, dropdown, spinbutton {
  border-radius: 10px;
}

/* Components added after the original glass revamp. */
.app-shell { background: transparent; }
.app-sidebar {
  min-width: 0;
  padding: 18px 14px 16px;
  background: linear-gradient(180deg, rgba(34,25,21,0.72) 0%, rgba(22,17,15,0.64) 56%, rgba(12,10,9,0.70) 100%);
  border-right: 1px solid rgba(255,255,255,0.07);
}
.app-sidebar-collapsed { padding: 14px 8px; }
.sidebar-brand { padding: 0 3px 8px; }
.sidebar-brand .brand-mark {
  min-width: 36px;
  min-height: 36px;
  border-radius: 11px;
  background: rgba(255,255,255,0.08);
}
.brand-overline { color: #f8f4ef; font-size: 1.12em; font-weight: 800; }
.brand-display { color: #aaa29b; font-size: 0.80em; font-weight: 500; }
.nav-caption { color: #958d86; font-size: 0.72em; font-weight: 800; letter-spacing: 1px; margin: 12px 7px 2px; }
button.nav-item {
  min-height: 44px;
  padding: 0 12px;
  border-radius: 11px;
  border: 0;
  box-shadow: none;
  color: #eee8e2;
  background: transparent;
  font-weight: 600;
}
button.nav-item:hover { background: rgba(255,255,255,0.065); }
button.nav-item-active,
button.nav-item-active:hover {
  background: rgba(255,255,255,0.09);
  color: #ffffff;
}
.workspace { padding: 18px 20px 16px; background: rgba(11,9,8,0.50); }
.workspace-header { min-height: 0; padding: 0; }
.page-title { font-size: 1.75em; font-weight: 800; color: #f8f4ef; }
.subtle, .page-subtitle { color: #a69e97; opacity: 1; }
.caption { color: #8c837c; font-size: 0.73em; }
.section-title { font-size: 1.1em; font-weight: 750; }
.detail-title { font-size: 1.45em; font-weight: 800; }
.sidebar-source,
.panel,
.inspector,
.gallery-empty {
  border-radius: 14px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.06);
}
.sidebar-source { padding: 10px; }
.sidebar-source label { color: #a69e97; font-size: 0.76em; }
.sidebar-source .source-badge { color: #f8f4ef; font-weight: 800; }
.sidebar-count { color: #8c837c; font-size: 0.73em; }
.status-strip { min-height: 0; }
.status-strip.error { border-color: rgba(255,135,135,0.3); }
.status-dot { color: #45d483; font-size: 0.9em; }
.status-dot.error, .status-dot.offline { color: #ff8787; }
.library-tools { padding: 10px; }
.library-tools searchentry { min-height: 38px; }
.library-page { padding: 0; }
gridview.wallpaper-gallery > child {
  margin: 5px;
  padding: 0;
  background: transparent;
}
gridview.wallpaper-gallery > child:selected {
  background: transparent;
}
.wallpaper-card {
  min-width: 240px;
  min-height: 0;
  padding: 0;
  border-radius: 14px;
  border: 1px solid rgba(255,255,255,0.075);
  background: rgba(35,29,27,0.48);
  box-shadow: 0 5px 18px rgba(0,0,0,0.22);
}
.wallpaper-card:hover {
  border-color: rgba(255,255,255,0.18);
  background: #191310;
}
.wallpaper-card.wallpaper-card-selected,
.wallpaper-card.wallpaper-card-selected:hover {
  border: 2px solid #ff8e31;
  background: #191310;
  box-shadow: 0 0 0 1px rgba(255,173,31,0.28), 0 7px 24px rgba(255,100,31,0.2);
}
.wallpaper-card picture,
.wallpaper-card image,
.artwork { border-radius: 13px 13px 0 0; background: #1b1512; }
.card-title { font-size: 0.82em; font-weight: 700; }
.card-meta { color: #a69e97; font-size: 0.70em; }
.inspector { padding: 14px; margin-left: 0; }
.pill,
.badge {
  min-height: 0;
  padding: 3px 7px;
  border-radius: 7px;
  font-size: 0.72em;
  font-weight: 800;
  background: rgba(16,12,10,0.82);
  color: #ffffff;
  border: 1px solid rgba(255,255,255,0.12);
}
.pill-accent, .badge-accent { border-color: rgba(255,115,23,0.38); }
.type-badge { border: 1px solid rgba(255,137,43,0.42); }
.favorite-heart {
  padding: 1px 4px;
  color: #fff6ef;
  font-size: 1.28em;
  font-weight: 600;
  text-shadow: 0 1px 5px rgba(0,0,0,0.72);
}
button.favorite-heart-button {
  min-width: 30px;
  min-height: 30px;
  padding: 0 5px;
  border: 1px solid rgba(255,255,255,0.17);
  border-radius: 99px;
  background: rgba(12,10,12,0.58);
  box-shadow: 0 2px 10px rgba(0,0,0,0.28);
}
button.favorite-heart-button:hover { background: rgba(255,100,120,0.42); }
button.filter-chip {
  min-height: 32px;
  padding: 5px 11px;
  border-radius: 9px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.08);
}
button.filter-chip:hover { background: rgba(255,255,255,0.09); }
button.filter-chip-active,
button.filter-chip-active:hover {
  color: #ffffff;
  background: linear-gradient(110deg, #ff7417 0%, #ffad1f 58%, #e43b2f 100%);
  border-color: #ff7417;
}
button.primary-action,
button.suggested-action {
  color: #ffffff;
  background: linear-gradient(110deg, #ff7417 0%, #ffad1f 58%, #e43b2f 100%);
  border-color: #ff7417;
  box-shadow: 0 4px 18px rgba(255,100,31,0.22);
}
button.primary-action:hover,
button.suggested-action:hover { color: #ffffff; }
button.ghost-action { background: transparent; border-color: transparent; }
button.ghost-action:hover { background: rgba(255,255,255,0.065); }
button.danger-action { color: #ff8787; background: rgba(255,135,135,0.08); }
button.language-choice {
  min-height: 56px;
  padding: 12px 16px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.075);
}
button.language-choice:hover { background: rgba(255,255,255,0.09); }
button.language-choice-active { border-color: #ff7417; }
.selection-bar {
  min-height: 48px;
  padding: 9px 12px;
  border-radius: 14px;
  color: #f8f4ef;
  background: rgba(29,20,16,0.96);
  border: 1px solid rgba(255,137,43,0.35);
  box-shadow: 0 12px 28px rgba(0,0,0,0.48);
}
.selection-bar.selection-bar-compact { min-height: 40px; padding: 6px 8px; }
.selection-bar-compact button { min-width: 34px; min-height: 34px; padding: 4px 7px; }
.selection-count { color: #ffad1f; font-weight: 800; }
.theme-popover { min-width: 330px; padding: 16px; }
popover.theme-popover { background: #19120f; border: 1px solid rgba(255,255,255,0.12); }
.theme-preview { min-height: 74px; }
scale.hue-slider trough highlight { background: transparent; }
scale.intensity-slider trough highlight { background: #ff7417; }
button.preset-dot { min-width: 30px; min-height: 30px; padding: 0; color: transparent; }
.settings-row { min-height: 0; }
.playlist-entry,
list row.playlist-item,
.playlist-item {
  min-height: 0;
  padding: 9px 12px;
  border-radius: 12px;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.06);
}
.playlist-entry:hover,
list row.playlist-item:hover,
.playlist-item:hover { background: rgba(255,255,255,0.09); }
list row.playlist-item:selected,
.playlist-item:selected { border-color: #ff7417; }
.toast { color: #f8f4ef; }
headerbar.topbar {
  min-height: 48px;
  padding: 4px 8px;
  background: rgba(16,12,10,0.58);
  border-bottom: 1px solid rgba(255,255,255,0.07);
  box-shadow: none;
}
headerbar.topbar button.window-action,
headerbar.topbar menubutton.theme-trigger > button {
  min-width: 30px;
  min-height: 30px;
  padding: 3px;
  color: #f8f4ef;
  background: rgba(255,255,255,0.045);
  border: 1px solid rgba(255,255,255,0.07);
  border-radius: 9px;
  box-shadow: none;
}
headerbar.topbar button.window-action:hover,
headerbar.topbar menubutton.theme-trigger > button:hover {
  background: rgba(255,255,255,0.10);
  border-color: rgba(255,255,255,0.16);
}
headerbar.topbar button.window-action-close:hover {
  color: #ff8787;
  background: rgba(255,135,135,0.14);
}
headerbar.topbar button.compact-button { min-height: 30px; padding: 4px 9px; }
headerbar.topbar button.primary-action { min-height: 30px; padding: 4px 11px; }
window.compact-height .workspace { padding: 10px 12px; }
window.compact-height .workspace-header { min-height: 0; }
window.compact-height .library-tools { padding: 7px; }
window.compact-height .status-strip { padding: 7px 9px; }
window.compact-height .theme-preview { min-height: 54px; }
window.compact-height .theme-popover { padding: 10px; }

/* A dark, tinted glass surface. KWin supplies the actual backdrop blur. */
window.glass-window {
  background: rgba(10,8,14,0.96);
}
window.glass-window .app-root {
  background: linear-gradient(122deg,
    rgba(92,61,126,0.32) 0%,
    rgba(20,17,29,0.22) 46%,
    rgba(148,61,39,0.28) 100%);
}
window.glass-window .app-sidebar {
  background: linear-gradient(180deg,
    rgba(34,25,43,0.97) 0%,
    rgba(22,18,29,0.96) 58%,
    rgba(14,12,19,0.97) 100%);
}
window.glass-window .workspace {
  background: rgba(11,9,15,0.95);
}
window.glass-window .sidebar-source,
window.glass-window .panel,
window.glass-window .inspector,
window.glass-window .gallery-empty,
window.glass-window .library-tools {
  background: rgba(22,19,28,0.94);
  border-color: rgba(225,211,255,0.13);
}
window.glass-window .status-strip {
  background: rgba(31,25,38,0.95);
  border-color: rgba(225,211,255,0.16);
}
window.glass-window .settings-row,
window.glass-window .playlist-item,
window.glass-window .playlist-entry {
  background: rgba(30,25,37,0.92);
  border-color: rgba(225,211,255,0.12);
}
window.glass-window .wallpaper-card {
  min-width: 240px;
  background: rgba(20,17,24,0.97);
  border-color: rgba(225,211,255,0.12);
}
window.glass-window .wallpaper-card:hover,
window.glass-window .wallpaper-card.wallpaper-card-selected {
  background: rgba(26,21,31,0.98);
}
window.glass-window headerbar.topbar {
  background: rgba(14,11,18,0.95);
}
"""


def install_theme(display: Gdk.Display) -> Gtk.CssProvider:
    """Install the dark glass visual system at application priority."""
    provider = Gtk.CssProvider()
    provider.load_from_data(CSS.encode("utf-8"))
    Gtk.StyleContext.add_provider_for_display(
        display, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )
    return provider
