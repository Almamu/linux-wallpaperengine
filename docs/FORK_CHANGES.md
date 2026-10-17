# Fork changes versus upstream

This document records the complete net change in `xoykor/linux-wallpaperengine`
relative to `Almamu/linux-wallpaperengine` at upstream `main` commit
`b016d7d` (2026-09-27). It describes the state consolidated on the fork's
`main`, including the renderer, desktop application, packaging, and project
automation.

The comparison is a three-dot diff from upstream `main`. It currently contains
104 changed paths overall. The engine source accounts for 69 paths, with about
6,649 lines added and 857 removed. Across the complete fork diff, 14,432 lines
are added and 1,133 removed. These counts describe net file changes, not commit
totals; the history includes imported and merged upstream work.

## Overview

The fork adds three connected areas of work:

1. A GTK 4 desktop application for finding, previewing and controlling local
   Steam Workshop wallpapers, with favorites, playlists, per-display pins,
   language and appearance settings.
2. Linux renderer and SceneScript compatibility work, including multi-display
   startup, camera and image composition handling, Workshop parsing, skeletal
   models, video playback and shader compatibility.
3. Documentation, installation helpers, CI checks and a playlist rotation
   utility.

The GTK application does not download wallpapers or authenticate to Steam. It
reads the Steam installation and Workshop files already present on the machine.
The renderer and frontend intentionally reuse the user's Steam Workshop content
and host graphics/runtime libraries.

## Desktop application

### Library and previews

`app/wallpaper_engine_app/gui.py` implements the GTK 4 window and its Library,
Playlists, Languages and Settings pages. The Library supports search, type and
favorite filters, incremental card creation, selection of individual or
multiple wallpapers, range selection, and selected-wallpaper details. Applying
a wallpaper starts it on every display, stops a running playlist and clears
per-display pins. The details panel can mark or unmark favorites and apply a
wallpaper to all screens or a selected screen.

`app/wallpaper_engine_app/previews.py` reads image previews from each wallpaper's
own Workshop directory. It handles static image formats and animated GIFs,
decodes frames in background workers, scales frames into bounded preview sizes,
caches decoded results, and starts GIF animation on card hover. The catalog
model records both the preview poster and any animation source so a wallpaper
without a GIF does not block the card list.

Library cards use a compact fixed 240 px width with a 240×240 square preview
region. Images keep their original proportions inside that region, so
non-square previews are letterboxed instead of cropped. GIFs use their first
frame at rest and decode/play their animation asynchronously while the pointer
hovers over a card. The gallery uses GTK 4's recycling `Gtk.GridView` with a
`Gtk.FilterListModel` and `Gtk.MultiSelection` so large Workshop libraries only
bind visible card widgets.

Gallery scrolling uses GTK's native scroll path instead of a second wheel
animation that competes with the scroller's adjustment. A hovered GIF keeps
playing while the gallery scrolls; scrolling alone does not suspend preview
animation.

### Catalog, state and engine control

`model.py` locates Steam roots, parses library-folder metadata, scans the local
Workshop catalog, extracts project tags and properties, resolves preview files,
and validates/saves user settings. The catalog refresh path detects files added
by Steam while the app is open. Favorites, playlists, monitor selection,
language and other application preferences are stored under XDG user data
locations rather than in the repository or system configuration.

`daemon.py` owns the renderer process and playlist rotation. It resolves the
renderer path, starts and stops the engine, enforces per-screen wallpaper
assignments, advances playlists on a configurable interval, honors favorite
and shuffle options, and reports renderer status to the GUI. `ipc.py` defines
the local command channel; `__main__.py` starts the GUI/daemon entry points.
The desktop systemd service keeps the renderer running after the control
window closes.

### Appearance, localization and input

`theme.py` supplies the GTK styles and user-selectable accent palette.
`backdrop.py` requests KWin backdrop blur after the GTK window is mapped on
KDE X11/XWayland. The window uses a darker, higher-opacity plum/charcoal tint
with subtle violet and warm color shifts, retaining contrast while letting
the blurred background show through the glass. Other compositors may ignore
the KWin hint, so blur is not guaranteed outside KDE X11/XWayland; those
sessions still get the tinted surface. The library window has responsive page and navigation layouts,
keyboard shortcuts, localized labels, status feedback and per-monitor
controls.

`i18n.py` provides the language catalog and system-language fallback. The UI
offers English, Portuguese, German, Russian, Japanese, Mandarin Chinese,
Spanish and Hindi, and preserves an existing user's choice.

## Renderer and Linux engine changes

### Workshop discovery and data parsing

The engine now searches common native, Flatpak and Snap Steam locations,
additional Steam libraries listed by `libraryfolders.vdf`, and user-selected
asset paths. `src/Steam/FileSystem/FileSystem.cpp` and its header provide the
filesystem/library resolution changes.

The data model and parsers were extended for additional Wallpaper Engine
project and Workshop forms. This covers object variants and properties,
material and texture metadata, wallpaper settings, JSON compatibility,
object groups/parents, camera assets and project data that was previously
discarded or interpreted with the wrong type. The main implementation is in
`src/WallpaperEngine/Data/Model/` and `src/WallpaperEngine/Data/Parsers/`.

### Scene composition, transforms and camera

The renderer adds scene camera objects, perspective and orthographic camera
paths, camera script transforms and mouse-coordinate conversion for X11 and
Wayland. Scene composition layers are rendered through their own framebuffer
paths, with parent/child ordering, visibility, fill/fit/stretch handling,
post-processing and output viewport mapping accounted for in scene rendering.
`CModel` composes a model's current local transform with its complete authored
parent chain, from the root to the rendered model, and uses script-updated
values each frame. This fixes nested models and other parented assets
generically instead of adjusting individual wallpapers.

The camera code uses internal radians for GLM transforms and bridges
SceneScript layer angles through degrees at the JS API boundary. The script API
and renderer therefore retain their respective units without applying a
wallpaper-specific angle exception.

### Puppet and skeletal-model compatibility

`CModel` and `PuppetModel` add support for skinned MDLV models, bone hierarchy,
mesh weights, attachments, animation clips, animation layers and skinning.
The parser handles the MDLS0004 layout and valid helper bones, checks bone
indices against mesh weights, supports animation chunks following either
MDLA- or MDAT-pointed skeleton data, and tolerates variable record padding.
Invalid or absent skeleton data degrades to bind-pose rendering instead of
preventing unrelated scene assets from loading.

The parser accepts valid helper bones and animation records without rejecting
an otherwise usable rig. Named puppet animation layers are exposed to
SceneScript through a common `ScriptableObject` contract
(`getAnimationLayer(name)`, `rate`, `play()` and `stop()`). Both image and model
objects provide the lookup, so scripts can control puppet animation without
wallpaper-specific branches.

#### Reusable workflow for Puppet deformation issues

Use this workflow when a facial feature, hair strand, limb or other part drifts,
detaches or jitters during animation. It separates asset/parser problems from
render-order and coordinate-space problems without adding item-specific rules:

1. Compare the Workshop preview and packaged source assets with a repeatable
   renderer capture at the same output size and animation time. If the asset is
   already absent in the source, investigate package/asset lookup; if it moves
   only during playback, inspect animation, transforms and effects.
2. Trace the affected image through its parent and attachment chain, puppet
   mesh, bone weights and animation layers. Validate that weighted bone indices
   resolve against the selected skeleton and that parent transforms are updated
   for each frame.
3. Isolate a suspected effect or layer with controlled A/B runs, changing one
   input at a time and recording the time and settings. This identifies which
   stage causes the displacement while keeping the final correction in the
   shared renderer pipeline.
4. Check each effect's coordinate space and ordering. A source-UV displacement
   that belongs to a skinned image must run before skinning so the displaced
   pixels follow the mesh. Opacity and source-UV masks must use the same image
   geometry; displacement vectors must remain vector data rather than being
   treated as masks. Preserve the authored order of image/effect layers.
5. Verify the fix over time and with structurally different Puppet content.
   Keep the effect enabled, compare moving features against stable landmarks,
   and check that alpha edges and masks stay attached. Disabling an effect or
   adding an item-ID exception is useful only as a diagnostic, not as a generic
   correction.

These steps reflect the renderer invariants implemented in `CImage`: source-
space effects run before skinning, opacity follows the source pixels, auxiliary
maps are reprojected through the live puppet mesh when needed, and vector fields
are not accidentally warped as opacity masks. Skeleton parsing separately
validates layouts and bone indices, while invalid optional rig data falls back
to bind-pose rendering. Together these rules make the fixes reusable across
Puppet wallpapers that share the same failure mode.

### SceneScript runtime and render behavior

The scripting changes expand SceneScript built-ins, vector/math/color adapters,
layer property access, input values, camera transforms, script property parsing
and per-frame updates. A script without `update()` runs as an init-only script;
when `update()` has side effects and returns `undefined`, the property's
current value remains intact. Explicit `null` remains distinct. This supports
scripts that write another layer's property from a hook without replacing the
property to which the hook is attached.

### Video, texture, shader and output support

The fork changes MPV OpenGL playback, frame handling, synchronization and audio
options. Web/video wallpaper selection and texture upload paths are adjusted
for renderer lifecycle and memory ownership. Shader translation adds
compatibility handling for Workshop GLSL that relies on varying widths,
uniform-dependent constants, fragment texture coordinates or vector operand
widths that differ from desktop GLSL conventions. Post-process state and
framebuffer providers carry more of the project’s scene settings through the
render passes.

Linux display/input changes cover GLFW and Wayland mouse state, output viewports,
screen roots/spans and monitor geometry. They are intended to match the
renderer’s per-screen surface and SceneScript coordinate spaces.

## Install and release flow

`app/install.sh` installs the optional desktop control app for the current
user, creates its desktop entry and user service, and can remove those user
files. It does not install or replace the engine using a system package
manager. `contrib/rotation/` contains an independent playlist rotation
service/helper for users who do not use the GTK control app.

The former single-file bundle build, its runtime launcher, and the release
workflow that published it were removed from the fork. Native installation is
now the only supported distribution path: build the renderer from source and
install the desktop app with `app/install.sh`, or install the renderer through
the Arch PKGBUILD under `packaging/archlinux/`, which `.github/workflows/arch.yml`
publishes to the AUR as `linux-wallpaperengine-git`.

## CI and regression coverage

`.github/workflows/cmake.yml` adds Python syntax checks, installer shell
syntax, desktop-app regression tests, C++ formatting, unit-test and build
jobs. `.github/workflows/tests.yml` preserves a manually runnable engine test
job. `app/tests/test_regressions.py` covers selected application regressions,
including daemon state and catalog/config behavior. CI workflow details and
runtime dependencies should be reviewed independently from the renderer
changes because they affect different deployment paths.

## Generic renderer corrections and validation

The renderer corrections address general scene data and render-pipeline
behavior rather than embedding rules for particular Workshop items:

- Model transforms are composed through the authored parent chain using the
  current per-frame property values, so animation and script changes on an
  ancestor carry through to nested models.
- MDLS0004 parsing accepts supported skeleton layouts and valid helper bones,
  associates animation chunks with the parsed rig, and preserves bind-pose
  rendering when optional rig data is absent or invalid.
- SceneScript treats modules without `update()` as init-only and preserves a
  property when `update()` performs side effects but returns `undefined`.
  Explicit `null` remains a distinct value.
- Puppet animation-layer lookup is a shared script API for image and model
  layers.
- Waterwaves UV displacement on puppet images runs in source space before
  skeletal skinning when the effect has no explicit render target. The effect
  stays enabled, while displaced facial and body regions follow the puppet's
  bones. Source-space opacity and mask data are mapped onto the skinned mesh;
  flow maps are kept out of the mask-warp path.
- Puppet image effect ordering preserves the authored eye-layer order and
  applies source-UV masks consistently with the image geometry.
- In `fit` scaling mode, the wallpaper renderer draws the fitted image over a
  GPU-generated, blurred zoom-fill of the same wallpaper. The extra fill pass
  runs only for `fit`, keeps the main image unchanged, and maps correctly across
  spanned displays; its blur radius stays consistent in output pixels.

The renderer was built successfully for `v0.0.36`, and its GitHub release
workflow completed. Targeted visual checks of animated scenes
were successful, but they are not an exhaustive pass over all Workshop
content. Parser and packaging success alone do not prove that every scene
matches its preview on every graphics driver.

## Complete net file inventory

The following list is the path inventory from the current net diff
against upstream `main`, including the desktop application, integrated
packaging, renderer changes, this document, and the SceneScript API header.

### Build and automation

- `.github/workflows/codefactor-diagnostics.yml` — informational static-analysis diagnostics on branch pushes and pull requests.
- `.github/workflows/cmake.yml` — desktop app checks, C++ build, formatting and CI.
- `.github/workflows/tests.yml` — engine unit-test workflow.
- `.gitignore` — ignores for the fork's app and packaging outputs.
- `CMakeLists.txt` — frontend/build integration and related target changes.
- `setup.cfg` — Python analyzer configuration.

### Desktop application and user installation

- `README.md` — fork overview, dependencies, Steam discovery, desktop app and packaging instructions.
- `app/install.sh` — per-user install/removal helper.
- `app/linux-wallpaperengine-app.desktop` — desktop launcher metadata.
- `app/linux-wallpaperengine-app.service` — user systemd service.
- `app/linux-wallpaperengine-app.svg` — app icon.
- `app/tests/test_regressions.py` — desktop app regression coverage.
- `app/wallpaper_engine_app/__init__.py` — package initializer.
- `app/wallpaper_engine_app/__main__.py` — application entry point.
- `app/wallpaper_engine_app/backdrop.py` — requests KWin backdrop blur on KDE X11/XWayland after the GTK window is mapped. The dark glass uses stronger dark plum/charcoal tint and higher surface opacity to retain contrast while keeping blurred/color-shifted backdrop visible through the outer surface. Other compositors, native Wayland sessions, and desktops that do not honor the KWin X11 property show the tinted translucent surfaces without guaranteed blur; the GTK UI remains usable.
- `app/wallpaper_engine_app/daemon.py` — engine process and playlist control.
- `app/wallpaper_engine_app/gui.py` — GTK 4 interface.
- `app/wallpaper_engine_app/i18n.py` — language selection and translations.
- `app/wallpaper_engine_app/ipc.py` — local GUI/daemon messages.
- `app/wallpaper_engine_app/model.py` — Steam catalog and user configuration.
- `app/wallpaper_engine_app/previews.py` — preview loading, caching and GIF hover playback.
- `app/wallpaper_engine_app/theme.py` — GTK theme and accent preferences.
- `app/wallpaper_engine_app/tray.py` — tray icon, menu and window visibility controls.

### Optional playlist rotation

- `contrib/rotation/install.sh` — optional rotation service installer.
- `contrib/rotation/linux-wallpaperengine-rotation.service` — systemd unit.
- `contrib/rotation/wallpaper-engine-rotate.py` — standalone rotation helper.

### Fork documentation

- `docs/FORK_CHANGES.md` — fork architecture, renderer changes, runtime behavior, packaging and upstream review context.
- `docs/screenshots/desktop-wallpaper-running.png` — desktop rendering screenshot.
- `docs/screenshots/library-empty-engine-running.png` — empty-library screenshot with engine running.
- `docs/screenshots/library-populated-engine-stopped.png` — populated-library screenshot with engine stopped.
- `docs/screenshots/settings-engine-running.png` — settings screenshot with engine running.

### Engine source: application, data and filesystem

- `src/main.cpp` — process startup changes.
- `src/Steam/FileSystem/FileSystem.cpp` — Steam library and asset path discovery.
- `src/Steam/FileSystem/FileSystem.h` — filesystem API updates.
- `src/WallpaperEngine/Application/ApplicationContext.cpp` — CLI and runtime configuration.
- `src/WallpaperEngine/Application/ApplicationContext.h` — application settings.
- `src/WallpaperEngine/Application/WallpaperApplication.cpp` — wallpaper launch and property setup.
- `src/WallpaperEngine/Application/WallpaperApplication.h` — application interface.
- `src/WallpaperEngine/Data/JSON.h` — JSON compatibility helpers.
- `src/WallpaperEngine/Data/Model/Material.h` — material fields.
- `src/WallpaperEngine/Data/Model/Object.h` — scene object variants and properties.
- `src/WallpaperEngine/Data/Model/Types.h` — dynamic model types.
- `src/WallpaperEngine/Data/Model/Wallpaper.h` — wallpaper/project settings.
- `src/WallpaperEngine/Data/Parsers/MaterialParser.cpp` — material parsing.
- `src/WallpaperEngine/Data/Parsers/ObjectParser.cpp` — object parsing and hierarchy.
- `src/WallpaperEngine/Data/Parsers/ObjectParser.h` — parser declarations.
- `src/WallpaperEngine/Data/Parsers/TextureParser.cpp` — texture parsing.
- `src/WallpaperEngine/Data/Parsers/WallpaperParser.cpp` — wallpaper/camera settings.

### Engine source: display, renderer and scripting

- `src/WallpaperEngine/Input/Drivers/GLFWMouseInput.cpp` — GLFW pointer state.
- `src/WallpaperEngine/Input/Drivers/WaylandMouseInput.cpp` — Wayland pointer/output state.
- `src/WallpaperEngine/Render/CTexture.cpp` — texture upload and resource handling.
- `src/WallpaperEngine/Render/CFBO.cpp` — framebuffer diagnostics and transparent layer initialization.
- `src/WallpaperEngine/Render/CWallpaper.cpp` — wallpaper output/lifecycle.
- `src/WallpaperEngine/Render/CWallpaper.h` — wallpaper renderer interface.
- `src/WallpaperEngine/Render/Camera.cpp` — scene projection and scripted camera.
- `src/WallpaperEngine/Render/Camera.h` — camera state API.
- `src/WallpaperEngine/Render/FBOProvider.cpp` — framebuffer creation and sizing.
- `src/WallpaperEngine/Render/FBOProvider.h` — framebuffer declarations.
- `src/WallpaperEngine/Render/Objects/CCameraObject.h` — scene camera object.
- `src/WallpaperEngine/Render/Objects/CImage.cpp` — image transforms, effects and composition.
- `src/WallpaperEngine/Render/Objects/CImage.h` — image-layer API.
- `src/WallpaperEngine/Render/Objects/CModel.cpp` — model loading, matrices, skinning and drawing.
- `src/WallpaperEngine/Render/Objects/CModel.h` — model renderer declaration.
- `src/WallpaperEngine/Render/Objects/CParticle.cpp` — particle render state.
- `src/WallpaperEngine/Render/Objects/CRenderable.cpp` — shared renderable setup.
- `src/WallpaperEngine/Render/Objects/CText.cpp` — text rendering integration.
- `src/WallpaperEngine/Render/Objects/Effects/CPass.cpp` — shader pass setup and compatibility.
- `src/WallpaperEngine/Render/Objects/Effects/CPass.h` — shader pass interface.
- `src/WallpaperEngine/Render/Objects/PuppetModel.cpp` — skinned MDLV parsing and animation evaluation.
- `src/WallpaperEngine/Render/Objects/PuppetModel.h` — puppet model data structures.
- `src/WallpaperEngine/Render/PostProcessSettings.h` — post-process values.
- `src/WallpaperEngine/Render/SpanInfo.h` — bounds used to map a wallpaper spanning multiple outputs.
- `src/WallpaperEngine/Render/Shaders/GLSLContext.cpp` — Workshop GLSL conversion.
- `src/WallpaperEngine/Render/Shaders/ShaderUnit.cpp` — shader compile/link handling.
- `src/WallpaperEngine/Render/Shaders/ShaderUnit.h` — shader unit declarations.
- `src/WallpaperEngine/Render/WallpaperState.cpp` — per-wallpaper render state.
- `src/WallpaperEngine/Render/WallpaperState.h` — render state API.
- `src/WallpaperEngine/Render/Wallpapers/CScene.cpp` — scene composition, camera and mouse mapping.
- `src/WallpaperEngine/Render/Wallpapers/CScene.h` — scene renderer interface.
- `src/WallpaperEngine/Render/Wallpapers/CVideo.cpp` — video wallpaper output.
- `src/WallpaperEngine/Render/Wallpapers/CVideo.h` — video renderer interface.
- `src/WallpaperEngine/Render/Wallpapers/CWeb.cpp` — web wallpaper output.
- `src/WallpaperEngine/Render/Wallpapers/CWeb.h` — web renderer interface.
- `src/WallpaperEngine/VideoPlayback/MPV/GLPlayer.cpp` — MPV OpenGL integration.
- `src/WallpaperEngine/VideoPlayback/MPV/GLPlayer.h` — MPV player state.
- `src/WallpaperEngine/Scripting/Adapters/ScriptableObjectAdapter.cpp` — SceneScript layer properties and animation-layer API.
- `src/WallpaperEngine/Scripting/Adapters/ScriptableObjectAdapter.h` — animation-layer interface and state.
- `src/WallpaperEngine/Scripting/Adapters/VectorAdapter.cpp` — vectors, angle units and vector methods.
- `src/WallpaperEngine/Scripting/Adapters/VectorAdapter.h` — vector adapter declaration.
- `src/WallpaperEngine/Scripting/EngineObject.cpp` — engine object bindings.
- `src/WallpaperEngine/Scripting/InputObject.cpp` — input bindings.
- `src/WallpaperEngine/Scripting/Modules/ColorModule.cpp` — color API.
- `src/WallpaperEngine/Scripting/Modules/MathModule.cpp` — math API.
- `src/WallpaperEngine/Scripting/Modules/MathModule.h` — math module declarations.
- `src/WallpaperEngine/Scripting/SceneObject.cpp` — SceneScript scene/camera functions.
- `src/WallpaperEngine/Scripting/ScriptEngine.cpp` — script loading and update behavior.
- `src/WallpaperEngine/Scripting/ScriptEngine.h` — script engine interface.
- `src/WallpaperEngine/Scripting/ScriptPropertiesObject.cpp` — script property handling.
- `src/WallpaperEngine/Scripting/ScriptableObject.cpp` — layer property registration.
- `src/WallpaperEngine/Scripting/ScriptableObject.h` — scriptable layer interface.

## Current state

The renderer, desktop application, and packaging changes described here are
consolidated on the fork's `main`. Release `v0.0.36` was published as an
integrated engine-and-frontend bundle; that bundle flow has since been removed
and native installation is the only supported distribution path. This document
describes the fork's current net
changes against upstream; it does not claim that every Workshop scene has
received an exhaustive visual audit.
