# Reverse Engineering Tools
These are the tools and binaries that I use for understanding what's going behind the scenes:

## d3dcompiler_47
A small shim to debug interactions with the d3dcompiler.
It's used to add extra information to wallpaper's themselves, like include processed shader before It's compiled
by d3d. Especially useful when something shader-related doesn't work so It can be inspected either manually
(with the logfile it creates) or in RenderDoc with shader names, source...

For it to work it has to live alongside the real d3dcompiler_74 dll renamed to d3dcompiler_47original.dll

## RenderDoc.cap
A base capture file that allows for Wallpaper Engine to run backgrounds, when used in conjunction with the
d3dcompiler_47 shim allows for taking captures of rendering, inspecting all the rendering performed by Wallpaper Engine.
