# 🧪 Usage

Basic syntax:

```bash
./linux-wallpaperengine [options] <background_id or path>
```

You can use either:
- A Steam Workshop ID (e.g. `1845706469`)
- A path to a background folder

---

## 🔧 Common Options

| Option | Description |
|--------|-------------|
| `--silent` | Mute background audio |
| `--volume <val>` | Set audio volume |
| `--noautomute` | Don't mute when other apps play audio |
| `--no-audio-processing` | Disable audio reactive features |
| `--fps <val>` | Limit frame rate |
| `--window <XxYxWxH>` | Run in windowed mode with custom size/position |
| `--screen-root <screen>` | Set as background for specific screen |
| `--bg <id/path>` | Assign a background to a specific screen (use after `--screen-root`) |
| `--scaling <mode>` | Wallpaper scaling: `stretch`, `fit`, `fill`, or `default` |
| `--clamping <mode>` | Set texture clamping: `clamp`, `border`, `repeat` |
| `--assets-dir <path>` | Set custom path for assets |
| `--screenshot <file>` | Save screenshot (PNG, JPEG, BMP) |
| `--list-properties` | Show customizable properties of a wallpaper |
| `--set-property name=value` | Override a specific property |
| `--disable-mouse` | Disable mouse interaction |
| `--disable-parallax` | Disable parallax effect on backgrounds that support it |
| `--no-fullscreen-pause` | Prevent pausing while fullscreen apps are running |

---

## 💡 Examples

### Run a background by ID
```bash
./linux-wallpaperengine 1845706469
```

### Run a background from a folder
```bash
./linux-wallpaperengine ~/backgrounds/1845706469/
```

### Assign backgrounds to screens with scaling
```bash
./linux-wallpaperengine \
  --scaling stretch --screen-root eDP-1 --bg 2667198601 \
  --scaling fill --screen-root HDMI-1 --bg 2667198602
```

### Run in a window
```bash
./linux-wallpaperengine --window 0x0x1280x720 1845706469
```

### Limit FPS to save power
```bash
./linux-wallpaperengine --fps 30 1845706469
```

### Take a screenshot
```bash
./linux-wallpaperengine --screenshot ~/wallpaper.png 1845706469
```

This can be useful as output for pywal or other color systems that use images as basis to generate a set of colors
to apply to your system.

### View and change properties
```bash
./linux-wallpaperengine --list-properties 2370927443
```

The output includes all the relevant information for each of the different properties:
```
barcount - slider
	Description: Bar Count
	Value: 64
	Minimum value: 16
	Maximum value: 64
	Step: 1

bloom - boolean
	Description: Bloom
	Value: 0
frequency - combolist
	Description: Frequency
	Value: 2
		Posible values:
		16 -> 1
		32 -> 2
		64 -> 3

owl - boolean
	Description: Owl
	Value: 0
rain - boolean
	Description: Rain
	Value: 1
schemecolor - color
	Description: ui_browse_properties_scheme_color
	R: 0.14902 G: 0.23137 B: 0.4 A: 1
visualizer - boolean
	Description: <hr>Add Visualizer<hr>
	Value: 1
visualizercolor - color
	Description: Bar Color
	R: 0.12549 G: 0.215686 B: 0.352941 A: 1
visualizeropacity - slider
	Description: Bar Opacity
	Value: 1
	Minimum value: 0
	Maximum value: 1
	Step: 0.1

visualizerwidth - slider
	Description: Bar Spacing
	Value: 0.25
	Minimum value: 0
	Maximum value: 0.5
	Step: 0.01
```

Any of these values can be modified with the --set-property switch. Say you want to enable the bloom in this background, you would do so like this:
```
./linux-wallpaperengine --set-property bloom=1 2370927443
```