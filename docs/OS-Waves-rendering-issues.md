# OS Waves (3014738359) - Rendering Issues Investigation

Investigation of rendering differences between linux-wallpaperengine and the native
Windows Wallpaper Engine for the OS Waves wallpaper (workshop ID 3014738359).

## Wallpaper Structure

The wallpaper consists of 8 scene objects rendered in this order:

| Object ID | Name | Type | Effects | Status |
|-----------|------|------|---------|--------|
| 85 | Solid1 | Image (solid) | None | Renders correctly |
| 173 | Windows_logo_-_2021.svg | Image | Pulse (audio) | Renders, audio response may differ |
| 57 | 1024px-Archlinux-icon-crystal-64.svg | Image | Pulse (audio) | Renders, audio response may differ |
| 61 | ubuntu-icon-logo-png-transparent | Image | Pulse (audio) | Renders, audio response may differ |
| 140 | Clock | Text | N/A | **Not rendered** (text unsupported) |
| 45 | cust | Image (passthrough) | None | **Possibly skipped** (passthrough w/o effects) |
| 71 | 13 | Image | Water waves | Renders via shader |
| 36 | 14 | Image | Water waves | Renders via shader |

Bloom is applied as a post-processing pass (via `wpenginelinux.json`).

## Issues Found

### 1. Script-based UserSettings not executed (CRITICAL)

**Files:** `src/WallpaperEngine/Data/Parsers/UserSettingParser.cpp:21-24`

The wallpaper defines 4 user settings with embedded JavaScript that dynamically
control object position (Y and Z axes) via slider properties. The engine currently
logs a warning and falls back to the default value, meaning objects are placed at
their raw default positions instead of the script-computed ones.

Example script from the wallpaper:
```javascript
export function update(value) {
    value.y = scriptProperties.posY;  // slider 0-100
    return value;
}
```

**Impact:** Object positioning differs from Windows. Sliders for position
adjustments are non-functional.

### 2. Text objects not supported (MEDIUM)

**Files:** `src/WallpaperEngine/Data/Parsers/ObjectParser.cpp:55-56`

Object 140 ("Clock") is a text object and is silently skipped. On Windows this
would render as a visible clock element.

**Impact:** Missing UI element in the wallpaper.

### 3. Orthogonal camera auto-size not implemented (MEDIUM)

**Files:** `src/WallpaperEngine/Render/Wallpapers/CScene.cpp:37-39`

When the scene camera uses `isAuto=true` for orthogonal projection, the engine
has a TODO to calculate the projection based on content size. Currently falls
through without adjusting.

**Impact:** If enabled, overall scale and positioning of all objects could be wrong.

### 4. Passthrough images without effects are skipped (LOW-MEDIUM)

**Files:** `src/WallpaperEngine/Render/Objects/CImage.cpp:226-230`

Object 45 ("cust") uses passthrough mode. The engine explicitly skips passthrough
images that have no effects attached:
```cpp
if (this->m_image.model->passthrough && this->m_image.effects.empty()) {
    return;
}
```

**Impact:** If this object is meant to composite content from behind, it won't
appear.

### 5. Audio spectrum processing may differ (MEDIUM)

**Files:** `src/WallpaperEngine/Render/Objects/Effects/CPass.cpp:625-630`

The pulse effect on the OS logos uses:
- `AUDIOPROCESSING=3`
- `audioamount=1.0`, `audiobounds=0.5-1.0`, `audioexponent=0.35`

The DSP algorithm for audio spectrum analysis may produce different results than
the Windows implementation.

**Impact:** Logo pulse animations may respond differently to audio.

### 6. Fullscreen/autosize behavior incomplete (LOW)

**Files:** `src/WallpaperEngine/Render/Objects/CImage.cpp:66`

There is a TODO asking what `autosize` should do. Fullscreen layers force size to
scene dimensions, but autosize behavior is undefined.

**Impact:** Minor sizing differences for affected layers.

## Fix Applied: parallaxDepth UserSetting Support

**Commit:** `fix: support UserSetting objects for parallaxDepth`

The original crash was caused by `parallaxDepth` being declared as `glm::vec2`
while the wallpaper's scene.json uses it as a UserSetting object:
```json
{"user": "parallaxstrength", "value": "0.19000 0.19000"}
```

### Changes made:
- `src/WallpaperEngine/Data/Model/Object.h`: Changed `parallaxDepth` from
  `glm::vec2` to `UserSettingUniquePtr` in both `ImageData` and `ParticleData`
- `src/WallpaperEngine/Data/Parsers/ObjectParser.cpp`: Changed 3 parse sites from
  `it.optional()` to `it.user()` to handle both plain values and UserSetting objects
- `src/WallpaperEngine/Render/Objects/CImage.cpp`: Updated access to dereference
  through the UserSetting wrapper (`->value->getVec2()`)

This follows the same pattern already used by `origin`, `scale`, `angles`,
`visible`, `alpha`, and `color`.

## Reproduction

```bash
linux-wallpaperengine \
  --assets-dir ~/.local/share/Steam/steamapps/common/wallpaper_engine/assets \
  --screen-root DP-2 --screen-root HDMI-A-1 \
  --fps 60 --silent \
  ~/.local/share/Steam/steamapps/workshop/content/431960/3014738359
```
