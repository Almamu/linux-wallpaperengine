# Image objects
Image objects are simple quads that display an image in them. These are usually used in orthogonal scenes to display animated effects.

Looks like every image object has two framebuffers used for ping-pong when rendering, these are usually registered as:
- _rt_imageLayerComposite_{id}_a
- _rt_imageLayerComposite_{id}_b

There's special textures used for various things:
- _rt_FullFrameBuffer: Represents the things currently rendered to the background's output
- _rt_HalfCompoBuffer1: Unknown for now
- _rt_HalfCompoBuffer2: Unknown for now
