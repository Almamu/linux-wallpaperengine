import tkinter as tk
from tkinter import ttk
import subprocess
import shlex
import json
import os
import threading
import pystray
from PIL import Image, ImageTk, ImageDraw # No ImageFont for simple icon

CONFIG_FILE = "wpe_gui_config.json"
WPE_APP_ID = "431960"
LOCALE_DIR = "locales"

class I18n:
    def __init__(self, app_instance):
        self.app = app_instance
        self.locale_data = {}
        self.current_language_code_var = tk.StringVar(value="en")
        self.current_language_display_var = tk.StringVar(value="English")

        self.available_languages = {
            "en": "English",
            "ru": "Русский",
            "de": "Deutsch",
            "uk": "Українська",
            "es": "Español",
            "fr": "Français"
        }
        self.lang_codes = list(self.available_languages.keys())
        self.lang_display_names = [self.available_languages[code] for code in self.lang_codes]

    def load_translation(self, lang_code, update_gui=True):
        filepath = os.path.join(LOCALE_DIR, f"{lang_code}.json")
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                self.locale_data = json.load(f)
            self.current_language_code_var.set(lang_code)
            self.current_language_display_var.set(self.available_languages.get(lang_code, "English"))
            if update_gui:
                self.app.update_language_in_gui()
            return True
        except FileNotFoundError:
            self.app.safe_set_status(f"Error: Translation file not found for {lang_code}.")
            self.app.safe_set_status(f"Error: Файл перевода не найден для {lang_code}.")
            return False
        except json.JSONDecodeError:
            self.app.safe_set_status(f"Error: Invalid JSON in translation file for {lang_code}.")
            self.app.safe_set_status(f"Error: Неверный JSON в файле перевода для {lang_code}.")
            return False

    def gettext(self, key, **kwargs):
        text = self.locale_data.get(key, key)
        return text.format(**kwargs)

_ = None

class WallpaperGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.i18n = I18n(self)
        global _
        _ = self.i18n.gettext

        self.current_language_code = "en"
        self.load_config()

        self.i18n.load_translation(self.current_language_code, update_gui=False) 
        
        self.title(_("app_title"))
        self.geometry("850x800")
        self.resizable(True, True)

        self.configure_styles()

        self.preview_image_ref = None

        self.init_vars_after_lang_load()

        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill="both", expand=True, padx=20, pady=20)

        self.control_tab = ttk.Frame(self.notebook, style='Card.TFrame')
        self.library_tab = ttk.Frame(self.notebook, style='Card.TFrame')
        self.settings_tab = ttk.Frame(self.notebook, style='Card.TFrame')

        self.notebook.add(self.control_tab, text=_("control_tab"))
        self.notebook.add(self.library_tab, text=_("local_library_tab"))
        self.notebook.add(self.settings_tab, text=_("settings_tab"))

        self.create_control_widgets(self.control_tab)
        self.create_library_widgets(self.library_tab)
        self.create_settings_widgets(self.settings_tab)
        
        self.update_language_in_gui()

        self.i18n.current_language_code_var.trace_add('write', self._on_language_change_event)

        self.tray_icon = None
        self.protocol("WM_DELETE_WINDOW", self._on_closing)
        self._create_tray_icon()


    def configure_styles(self):
        style = ttk.Style(self)
        
        # --- Color Palette (Dark Theme) ---
        self.bg_primary = "#1A1A1A"      # Deep dark background
        self.bg_card = "#2C2C2E"         # Dark gray for cards
        self.fg_primary = "#FFFFFF"      # Pure white for primary text (Increased contrast)
        self.fg_secondary = "#8E8E93"    # Medium gray for secondary text/placeholders
        self.accent_blue = "#0A84FF"     # Apple Blue
        self.accent_blue_dark = "#005CE6" # Darker blue for active states
        self.accent_red = "#FF453A"      # Red for danger/stop
        self.accent_green = "#32D74B"    # Green for success
        self.border_light = "#48484A"    # Thin light border for separation
        self.border_medium = "#5C5C5E"   # More noticeable border
        self.divider = "#3A3A3C"         # Thin divider line

        # Set overall background color for the Tk window
        self.tk_setPalette(background=self.bg_primary, foreground=self.fg_primary)
        
        style.theme_use('alt') 

        # --- Font Definitions ---
        # Using generic Tkinter default fonts for better cross-platform reliability
        # and letting Tkinter choose the best system-wide sans-serif
        self.default_font = ('TkDefaultFont', 11, 'normal')
        self.heading_font = ('TkDefaultFont', 13, 'bold')
        self.button_font = ('TkDefaultFont', 12, 'bold')
        self.secondary_font = ('TkDefaultFont', 10, 'normal')
        self.monospace_font = ('TkFixedFont', 10)
        
        print(f"DEBUG: Default Font: {self.default_font}")
        print(f"DEBUG: Heading Font: {self.heading_font}")
        print(f"DEBUG: Button Font: {self.button_font}")
        print(f"DEBUG: fg_primary: {self.fg_primary}, bg_primary: {self.bg_primary}")
        
        # --- General Widget Styling ---
        style.configure('.', font=self.default_font, background=self.bg_primary, foreground=self.fg_primary) 
        
        style.configure('TLabel', background=self.bg_primary, foreground=self.fg_primary, padding=2)
        style.configure('TLabelframe', background=self.bg_card, foreground=self.fg_primary, 
                        borderwidth=1, relief="flat", bordercolor=self.border_light, 
                        font=self.heading_font, padding=[20, 15, 20, 15])
        style.configure('TLabelframe.Label', background=self.bg_card, foreground=self.fg_primary, 
                        font=self.heading_font, padding=[0, 0, 0, 10])

        # --- Buttons ---
        style.configure('TButton', font=self.button_font, 
                        background=self.accent_blue, foreground="white", 
                        padding=[18, 10], relief="flat", borderwidth=0, 
                        focusthickness=0)
        style.map('TButton', 
                  background=[('active', self.accent_blue_dark), ('pressed', self.accent_blue_dark)],
                  foreground=[('active', 'white'), ('pressed', 'white')])

        style.configure('Secondary.TButton', background=self.border_light, foreground=self.fg_primary)
        style.map('Secondary.TButton',
                  background=[('active', self.border_medium), ('pressed', self.border_medium)],
                  foreground=[('active', self.fg_primary), ('pressed', self.fg_primary)])

        style.configure('Danger.TButton', background=self.accent_red, foreground="white")
        style.map('Danger.TButton',
                  background=[('active', '#CC3333'), ('pressed', '#A02222')],
                  foreground=[('active', 'white'), ('pressed', 'white')])

        # --- Entries & Comboboxes ---
        style.configure('TEntry', font=self.default_font, fieldbackground=self.bg_card, foreground=self.fg_primary, 
                        borderwidth=1, relief="solid", bordercolor=self.border_medium, 
                        padding=[10, 8])
        style.map('TEntry', 
                  bordercolor=[('focus', self.accent_blue)], 
                  highlightbackground=[('focus', self.accent_blue)], 
                  highlightcolor=[('focus', self.accent_blue)], 
                  highlightthickness=[('focus', 1)])

        style.configure('TCombobox', font=self.default_font, fieldbackground=self.bg_card, foreground=self.fg_primary, 
                        borderwidth=1, relief="solid", bordercolor=self.border_medium,
                        padding=[10, 8])
        style.map('TCombobox', 
                  fieldbackground=[('readonly', self.bg_card)], 
                  selectbackground=[('readonly', self.accent_blue)], 
                  selectforeground=[('readonly', 'white')],
                  bordercolor=[('focus', self.accent_blue)], 
                  highlightbackground=[('focus', self.accent_blue)], 
                  highlightcolor=[('focus', self.accent_blue)], 
                  highlightthickness=[('focus', 1)])
        
        # --- Checkbuttons (Toggle-like) ---
        style.configure('TCheckbutton', font=self.default_font, background=self.bg_card, foreground=self.fg_primary, 
                        indicatoron=False, relief='flat', borderwidth=0, padding=[10, 5])
        style.map('TCheckbutton', 
                  background=[('selected', self.accent_blue), ('active', self.bg_card)],
                  foreground=[('selected', 'white'), ('active', self.fg_primary)])

        # --- Scales ---
        style.configure('TScale', background=self.bg_card, troughcolor=self.border_light, 
                        sliderrelief='flat', borderwidth=0, sliderthickness=20, 
                        troughrelief='flat')
        style.map('TScale', 
                  background=[('active', self.bg_card)],
                  sliderfill=[('active', self.accent_blue), ('!disabled', self.accent_blue)],
                  troughcolor=[('active', self.accent_blue)],
                  bordercolor=[('active', self.accent_blue)])
        
        # --- Notebook (Tabs - Pill-style approximation) ---
        style.configure('TNotebook', background=self.bg_primary, borderwidth=0, tabposition='nw')
        style.configure('TNotebook.Tab', background=self.bg_primary, foreground=self.fg_secondary, 
                        padding=[20, 10], font=self.button_font, borderwidth=0, relief='flat')
        style.map('TNotebook.Tab', 
                  background=[('selected', self.bg_card), ('active', self.bg_card)],
                  foreground=[('selected', self.fg_primary), ('active', self.fg_primary)],
                  expand=[('selected', [2, 2, 2, 2])],
                  lightcolor=[('selected', self.accent_blue)],
                  bordercolor=[('selected', self.accent_blue)])
        style.configure('TNotebook.Client', padding=[15, 15, 15, 15], background=self.bg_card, borderwidth=0, relief='flat')

        style.configure('Card.TFrame', background=self.bg_card, borderwidth=0)

        # --- Treeview ---
        style.configure('Treeview', font=self.default_font, background=self.bg_card, foreground=self.fg_primary,
                        fieldbackground=self.bg_card, borderwidth=0, relief='flat', rowheight=32)
        style.configure('Treeview.Heading', font=self.button_font, background=self.bg_card, foreground=self.fg_primary,
                        relief='flat', padding=[10, 10], borderwidth=0, bordercolor=self.divider)
        style.map('Treeview', background=[('selected', self.accent_blue)], 
                              foreground=[('selected', 'white')])
        
        # --- Scrollbar ---
        style.configure('TScrollbar', troughcolor=self.bg_primary, background=self.fg_secondary, 
                        bordercolor=self.bg_primary, arrowcolor=self.fg_primary, relief='flat')
        style.map('TScrollbar', background=[('active', self.fg_primary)])

        # --- PanedWindow ---
        style.configure('TPanedwindow', background=self.bg_card, borderwidth=0)
        style.configure('TPanedwindow.sash', background=self.border_light, borderwidth=0)


    def _on_language_change_event(self, *args):
        selected_code = self.i18n.current_language_code_var.get()
        self.i18n.load_translation(selected_code, update_gui=True)
        self.save_config()

    def update_language_in_gui(self):
        self.title(_("app_title"))
        self.notebook.tab(self.control_tab, text=_("control_tab"))
        self.notebook.tab(self.library_tab, text=_("local_library_tab"))
        self.notebook.tab(self.settings_tab, text=_("settings_tab"))

        for widget in self.control_tab.winfo_children():
            widget.destroy()
        for widget in self.library_tab.winfo_children():
            widget.destroy()
        for widget in self.settings_tab.winfo_children():
            widget.destroy()
        
        self.create_control_widgets(self.control_tab)
        self.create_library_widgets(self.library_tab)
        self.create_settings_widgets(self.settings_tab)
        
        self.safe_set_status(_("status_ready"))

    def safe_set_status(self, message):
        self.after(0, lambda: self.status_var.set(message))

    def safe_populate_treeview(self, wallpapers):
        self.after(0, lambda: self.populate_treeview(wallpapers))

    def init_vars_after_lang_load(self):
        self.screens = self.detect_screens()
        self.screen_var = tk.StringVar(value=self.screens[0] if self.screens else "")
        self.background_id_var = tk.StringVar()
        self.silent_var = tk.BooleanVar()
        self.volume_var = tk.IntVar(value=15)
        self.no_automute_var = tk.BooleanVar()
        self.no_audio_processing_var = tk.BooleanVar()
        self.fps_var = tk.IntVar(value=30)
        self.scaling_var = tk.StringVar(value='default')
        self.clamp_var = tk.StringVar(value='clamp')
        self.disable_mouse_var = tk.BooleanVar()
        self.disable_parallax_var = tk.BooleanVar()
        self.no_fullscreen_pause_var = tk.BooleanVar()
        self.set_property_var = tk.StringVar()
        self.status_var = tk.StringVar(value=_("status_ready"))

    def save_config(self):
        config_data = {
            "current_language": self.i18n.current_language_code_var.get()
        }
        try:
            with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
                json.dump(config_data, f, indent=4)
            self.safe_set_status(_("status_config_saved"))
            return True
        except IOError as e:
            self.safe_set_status(f"{_('status_error_saving_config')}: {e}")
            return False

    def load_config(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
                    config_data = json.load(f)
                    self.current_language_code = config_data.get("current_language", "en")
            except (IOError, json.JSONDecodeError) as e:
                self.status_var.set(f"{_('status_error_loading_config')}: {e}")
        self.i18n.current_language_code_var.set(self.current_language_code)

    def detect_screens(self):
        try:
            result = subprocess.run(['xrandr', '--query'], capture_output=True, text=True, check=True)
            lines = result.stdout.splitlines()
            connected_screens = []
            primary_screen = None
            for line in lines:
                if " connected" in line:
                    parts = line.split()
                    screen_name = parts[0]
                    connected_screens.append(screen_name)
                    if "primary" in line:
                        primary_screen = screen_name
            
            if primary_screen:
                connected_screens.insert(0, connected_screens.pop(connected_screens.index(primary_screen)))
            
            return connected_screens if connected_screens else ["eDP-1"]
        except (subprocess.CalledProcessError, FileNotFoundError):
            return ["eDP-1"]

    def scan_local_wallpapers_thread(self):
        threading.Thread(target=self.scan_local_wallpapers, daemon=True).start()

    def find_workshop_dir(self):
        steam_paths = [
            os.path.expanduser("~/.local/share/Steam"),
            os.path.expanduser("~/.steam/steam")
        ]
        for path in steam_paths:
            potential_path = os.path.join(path, "steamapps/workshop/content/431960")
            if os.path.isdir(potential_path):
                return potential_path
        return None

    def scan_local_wallpapers(self):
        self.safe_set_status(_("status_searching_local"))
        workshop_dir = self.find_workshop_dir()

        if not workshop_dir:
            self.safe_set_status(_("status_no_wallpaper_dir"))
            return

        wallpapers = []
        try:
            for item_id in os.listdir(workshop_dir):
                item_path = os.path.join(workshop_dir, item_id)
                if os.path.isdir(item_path):
                    project_file = os.path.join(item_path, 'project.json')
                    if os.path.isfile(project_file):
                        with open(project_file, 'r', encoding='utf-8') as f:
                            data = json.load(f)
                            title = data.get('title', _("untitled"))
                            wallpapers.append({'title': title, 'id': item_id})
        except Exception as e:
            self.safe_set_status(f"{_('status_scanning_error')}: {e}")
            return
            
        if not wallpapers:
            self.safe_set_status(_("status_no_local_wallpapers"))
            return

        self.safe_populate_treeview(wallpapers)
        self.safe_set_status(f"{_('status_local_wallpapers_found').format(count=len(wallpapers))}")

    def populate_treeview(self, wallpapers):
        self.wallpaper_tree.delete(*self.wallpaper_tree.get_children())
        for wallpaper in sorted(wallpapers, key=lambda x: x['title'].lower()):
            self.wallpaper_tree.insert('', tk.END, values=(wallpaper['title'], wallpaper['id']))

    def on_wallpaper_select(self, event):
        selected_items = self.wallpaper_tree.selection()
        if not selected_items: return
        
        selected_item = self.wallpaper_tree.item(selected_items[0])
        wallpaper_id = selected_item['values'][1]

        self.background_id_var.set(wallpaper_id)
        self.build_and_run_command()

        if Image is None or ImageTk is None:
            self.preview_label.config(text=_("preview_not_available"))
            return

        try:
            workshop_dir = self.find_workshop_dir()
            if not workshop_dir: return

            item_path = os.path.join(workshop_dir, str(wallpaper_id))
            project_file = os.path.join(item_path, 'project.json')

            if not os.path.isfile(project_file):
                self.preview_label.config(image='', text=_("project_json_not_found"))
                return

            with open(project_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            preview_filename = data.get('preview')
            if not preview_filename:
                self.preview_label.config(image='', text=_("preview_not_specified"))
                return
            
            image_path = os.path.join(item_path, str(preview_filename))
            if not os.path.isfile(image_path):
                self.preview_label.config(image='', text=_("preview_file_not_found"))
                return

            img = Image.open(image_path)
            img.thumbnail((400, 225))
            
            self.preview_image_ref = ImageTk.PhotoImage(img)
            self.preview_label.config(image=self.preview_image_ref)

        except Exception as e:
            self.preview_label.config(image='', text=_("preview_loading_error").format(error=e))
            self.preview_image_ref = None

    def build_and_run_command(self, preview_mode=False):
        self.stop_wallpapers(silent=True)
        bg_id = self.background_id_var.get()
        if not bg_id:
            self.status_var.set(_("status_error_empty_id")); return

        cmd = ['linux-wallpaperengine']
        if self.silent_var.get(): cmd.append('--silent')
        else:
            if self.volume_var.get() != 15: cmd.extend(['--volume', str(self.volume_var.get())])
        if self.no_automute_var.get(): cmd.append('--noautomute')
        if self.no_audio_processing_var.get(): cmd.append('--no-audio-processing')
        if self.fps_var.get() != 30: cmd.extend(['--fps', str(self.fps_var.get())])
        if self.no_fullscreen_pause_var.get(): cmd.append('--no-fullscreen-pause')
        if self.disable_mouse_var.get(): cmd.append('--disable-mouse')
        if self.disable_parallax_var.get(): cmd.append('--disable-parallax')
        if self.set_property_var.get():
            for prop in self.set_property_var.get().split(): cmd.extend(['--set-property', prop])

        if preview_mode: cmd.append(bg_id)
        else:
            screen = self.screen_var.get()
            if not screen: self.status_var.set(_("status_error_screen_not_selected")); return
            cmd.extend(['--screen-root', screen, '--bg', bg_id])
            if self.scaling_var.get() != 'default': cmd.extend(['--scaling', self.scaling_var.get()])
            if self.clamp_var.get() != 'clamp': cmd.extend(['--clamp', self.clamp_var.get()])

        try:
            command_str = shlex.join(cmd)
            subprocess.Popen(cmd)
            self.status_var.set(f"{_('status_command_launched')}{command_str}")
        except (FileNotFoundError, Exception) as e: self.status_var.set(f"{_('status_error_launch_command')}{e}")

    def stop_wallpapers(self, silent=False):
        try:
            subprocess.run(['pkill', '-f', 'linux-wallpaperengine'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if not silent: self.status_var.set(_("status_all_stopped"))
        except (subprocess.CalledProcessError, FileNotFoundError):
            if not silent: self.status_var.set(_("status_no_processes_found"))

    def create_control_widgets(self, container):
        for widget in container.winfo_children():
            widget.destroy()

        container.columnconfigure(0, weight=1)
        main_controls_frame = ttk.Labelframe(container, text=_("main_controls_frame"), padding="15", style='Card.TLabelframe')
        audio_frame = ttk.Labelframe(container, text=_("audio_frame"), padding="15", style='Card.TLabelframe')
        perf_frame = ttk.Labelframe(container, text=_("perf_frame"), padding="15", style='Card.TLabelframe')
        adv_frame = ttk.Labelframe(container, text=_("adv_frame"), padding="15", style='Card.TLabelframe')
        action_frame = ttk.Frame(container, padding="15", style='Card.TFrame')
        status_frame = ttk.Frame(container, padding="10", style='Card.TFrame')

        main_controls_frame.grid(row=0, column=0, sticky="ew", padx=10, pady=8)
        audio_frame.grid(row=1, column=0, sticky="ew", padx=10, pady=8)
        perf_frame.grid(row=2, column=0, sticky="ew", padx=10, pady=8)
        adv_frame.grid(row=3, column=0, sticky="ew", padx=10, pady=8)
        action_frame.grid(row=4, column=0, sticky="ew", padx=10, pady=8)
        status_frame.grid(row=5, column=0, sticky="ew", padx=10, pady=8)

        main_controls_frame.columnconfigure(1, weight=1)
        audio_frame.columnconfigure(1, weight=1)
        perf_frame.columnconfigure(1, weight=1); perf_frame.columnconfigure(3, weight=1)
        adv_frame.columnconfigure(1, weight=1)
        action_frame.columnconfigure(0, weight=1); action_frame.columnconfigure(1, weight=1); action_frame.columnconfigure(2, weight=1)
        status_frame.columnconfigure(0, weight=1)

        ttk.Label(main_controls_frame, text=_("wallpaper_id_path_label"), style='TLabel').grid(row=0, column=0, sticky="w", padx=5, pady=2)
        ttk.Entry(main_controls_frame, textvariable=self.background_id_var, style='TEntry').grid(row=0, column=1, sticky="ew", padx=5, pady=2)
        ttk.Label(main_controls_frame, text=_("screen_label"), style='TLabel').grid(row=1, column=0, sticky="w", padx=5, pady=2)
        ttk.Combobox(main_controls_frame, textvariable=self.screen_var, values=self.screens, state='readonly', style='TCombobox').grid(row=1, column=1, sticky="ew", padx=5, pady=2)
        
        ttk.Checkbutton(audio_frame, text=_("silent_checkbox"), variable=self.silent_var, style='TCheckbutton').grid(row=0, column=0, columnspan=2, sticky="w", padx=5)
        ttk.Label(audio_frame, text=_("volume_label"), style='TLabel').grid(row=1, column=0, sticky="w", padx=5, pady=5)
        ttk.Scale(audio_frame, from_=0, to=100, orient=tk.HORIZONTAL, variable=self.volume_var, style='TScale').grid(row=1, column=1, sticky="ew", padx=5, pady=5)
        ttk.Checkbutton(audio_frame, text=_("no_automute_checkbox"), variable=self.no_automute_var, style='TCheckbutton').grid(row=2, column=0, columnspan=2, sticky="w", padx=5)
        ttk.Checkbutton(audio_frame, text=_("no_audio_processing_checkbox"), variable=self.no_audio_processing_var, style='TCheckbutton').grid(row=3, column=0, columnspan=2, sticky="w", padx=5)

        ttk.Label(perf_frame, text=_("fps_label"), style='TLabel').grid(row=0, column=0, sticky="w", padx=5, pady=5)
        ttk.Scale(perf_frame, from_=1, to=144, orient=tk.HORIZONTAL, variable=self.fps_var, style='TScale').grid(row=0, column=1, sticky="ew", padx=5, pady=5)
        ttk.Label(perf_frame, text=_("scaling_label"), style='TLabel').grid(row=1, column=0, sticky="w", padx=5, pady=5)
        ttk.Combobox(perf_frame, textvariable=self.scaling_var, values=['stretch', 'fit', 'fill', 'default'], state='readonly', style='TCombobox').grid(row=1, column=1, sticky="ew", padx=5, pady=5)
        ttk.Label(perf_frame, text=_("clamp_label"), style='TLabel').grid(row=1, column=2, sticky="w", padx=5, pady=5)
        ttk.Combobox(perf_frame, textvariable=self.clamp_var, values=['clamp', 'border', 'repeat'], state='readonly', style='TCombobox').grid(row=1, column=3, sticky="ew", padx=5, pady=5)
        ttk.Checkbutton(perf_frame, text=_("disable_mouse_checkbox"), variable=self.disable_mouse_var, style='TCheckbutton').grid(row=2, column=0, columnspan=2, sticky="w", padx=5)
        ttk.Checkbutton(perf_frame, text=_("disable_parallax_checkbox"), variable=self.disable_parallax_var, style='TCheckbutton').grid(row=3, column=0, columnspan=2, sticky="w", padx=5)
        ttk.Checkbutton(perf_frame, text=_("no_fullscreen_pause_checkbox"), variable=self.no_fullscreen_pause_var, style='TCheckbutton').grid(row=4, column=0, columnspan=4, sticky="w", padx=5)

        ttk.Label(adv_frame, text=_("set_property_label"), style='TLabel').grid(row=0, column=0, sticky="w", padx=5, pady=5)
        ttk.Entry(adv_frame, textvariable=self.set_property_var, style='TEntry').grid(row=0, column=1, sticky="ew", padx=5, pady=5)
        ttk.Label(adv_frame, text=_("set_property_example"), foreground=self.fg_secondary, style='TLabel').grid(row=1, column=1, sticky="w", padx=5, pady=2)
        
        ttk.Button(action_frame, text=_("set_wallpaper_button"), command=self.build_and_run_command, style='TButton').grid(row=0, column=0, padx=5, pady=5, sticky="ew")
        ttk.Button(action_frame, text=_("preview_in_window_button"), command=lambda: self.build_and_run_command(preview_mode=True), style='TButton').grid(row=0, column=1, padx=5, pady=5, sticky="ew")
        ttk.Button(action_frame, text=_("stop_button"), command=self.stop_wallpapers, style='TButton').grid(row=0, column=2, padx=5, pady=5, sticky="ew")
        
        status_label = ttk.Label(status_frame, textvariable=self.status_var, wraplength=600, style='TLabel')
        status_label.grid(row=0, column=0, sticky="w", padx=5, pady=5)

    def create_library_widgets(self, container):
        for widget in container.winfo_children():
            widget.destroy()

        container.rowconfigure(0, weight=1)
        container.columnconfigure(1, weight=1)

        paned_window = ttk.PanedWindow(container, orient=tk.HORIZONTAL)
        paned_window.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        list_container = ttk.Frame(paned_window, padding=10, style='Card.TFrame')
        list_container.rowconfigure(1, weight=1)
        list_container.columnconfigure(0, weight=1)
        paned_window.add(list_container, weight=1)

        ttk.Button(list_container, text=_("scan_local_wallpapers_button"), command=self.scan_local_wallpapers_thread, style='TButton').grid(row=0, column=0, sticky="ew", pady=10)
        
        list_frame = ttk.Labelframe(list_container, text=_("found_wallpapers_frame"), padding=10, style='Card.TLabelframe')
        list_frame.grid(row=1, column=0, sticky="nsew")
        list_frame.columnconfigure(0, weight=1)
        list_frame.rowconfigure(0, weight=1)
        
        self.wallpaper_tree = ttk.Treeview(list_frame, columns=('Title', 'ID'), show='headings', style='Treeview')
        self.wallpaper_tree.heading('Title', text=_("treeview_title_column"))
        self.wallpaper_tree.heading('ID', text=_("treeview_id_column"))
        self.wallpaper_tree.column('ID', width=120, stretch=tk.NO, anchor='center')
        self.wallpaper_tree.column('Title', width=200, stretch=tk.YES, anchor='w')
        self.wallpaper_tree.grid(row=0, column=0, sticky="nsew")
        self.wallpaper_tree.bind('<<TreeviewSelect>>', self.on_wallpaper_select)
        
        scrollbar = ttk.Scrollbar(list_frame, orient="vertical", command=self.wallpaper_tree.yview, style='TScrollbar')
        self.wallpaper_tree.configure(yscrollcommand=scrollbar.set)
        scrollbar.grid(row=0, column=1, sticky='ns')

        preview_container = ttk.Labelframe(paned_window, text=_("preview_frame"), padding=10, style='Card.TLabelframe')
        paned_window.add(preview_container, weight=2)
        
        self.preview_label = ttk.Label(preview_container, text=_("preview_placeholder"), anchor=tk.CENTER, background=self.bg_card, foreground=self.fg_secondary, style='TLabel')
        self.preview_label.pack(expand=True, fill=tk.BOTH, padx=10, pady=10)

    def create_settings_widgets(self, container):
        for widget in container.winfo_children():
            widget.destroy()

        container.columnconfigure(1, weight=1)

        settings_frame = ttk.Labelframe(container, text=_("settings_tab"), padding="15", style='Card.TLabelframe')
        settings_frame.pack(side=tk.TOP, fill=tk.X, padx=10, pady=8)
        settings_frame.columnconfigure(1, weight=1)

        ttk.Label(settings_frame, text=_("language_label"), style='TLabel').grid(row=0, column=0, sticky="w", padx=5, pady=5)
        
        lang_combobox = ttk.Combobox(settings_frame, textvariable=self.i18n.current_language_display_var, 
                                     values=self.i18n.lang_display_names, state='readonly', style='TCombobox')
        lang_combobox.grid(row=0, column=1, sticky="ew", padx=5, pady=5)
        
        def on_lang_select(event):
            selected_display_name = self.i18n.current_language_display_var.get()
            for code, name in self.i18n.available_languages.items():
                if name == selected_display_name:
                    self.i18n.current_language_code_var.set(code)
                    break
        
        lang_combobox.bind("<<ComboboxSelected>>", on_lang_select)


    def _create_tray_icon(self):
        if not pystray or not Image or not ImageDraw:
            # Fallback for when pystray or PIL components are not available
            # Use self.safe_set_status here? But _create_tray_icon is called during init
            # before the GUI is fully drawn. Best to assume it works or log.
            # self.safe_set_status(_("tray_missing_deps_error")) # This would fail too early
            return

        # Create a simple blue circle icon
        width, height = 64, 64
        image = Image.new('RGBA', (width, height), (0, 0, 0, 0)) # Transparent background
        draw = ImageDraw.Draw(image)
        draw.ellipse((0, 0, width, height), fill=self.accent_blue, outline=self.accent_blue_dark, width=2)
        
        menu = (
            pystray.MenuItem(_("show_window_tray_menu"), self._show_window),
            pystray.MenuItem(_("exit_tray_menu"), self._quit_app)
        )
        self.tray_icon = pystray.Icon("wallpaper_gui", image, _("app_title"), menu)

    def _on_closing(self):
        self.withdraw()
        if self.tray_icon:
            threading.Thread(target=self.tray_icon.run, daemon=True).start()

    def _show_window(self, icon, item):
        self.after(0, self.deiconify)
        if self.tray_icon:
            self.tray_icon.stop()

    def _quit_app(self, icon, item):
        if self.tray_icon:
            self.tray_icon.stop()
        self.after(0, self.quit)


if __name__ == "__main__":
    app = WallpaperGUI()
    app.mainloop()