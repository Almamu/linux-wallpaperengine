#pragma once

#include <glm/vec2.hpp>

namespace WallpaperEngine::Render::Drivers {
/**
 * Interface implemented by a host application that embeds the renderer inside
 * its own OpenGL context (for example a Qt Quick item inside plasmashell).
 *
 * The host owns the context, the surface and the frame clock; the renderer only
 * ever draws into the framebuffer the host hands it. Keeping this as a pure
 * interface means the library never has to link against the host's toolkit.
 */
class EmbeddedHost {
public:
    virtual ~EmbeddedHost () = default;

    /**
     * Resolves an OpenGL entrypoint using the host's context. GLEW is
     * initialized from this, so it must be valid before the driver is built.
     */
    [[nodiscard]] virtual void* getProcAddress (const char* name) const = 0;

    /**
     * @return The framebuffer the renderer must draw the final image into.
     *         This changes whenever the host resizes, so it is queried per frame.
     */
    [[nodiscard]] virtual unsigned int getDestinationFramebuffer () const = 0;

    /**
     * @return Size in physical pixels of the destination framebuffer.
     */
    [[nodiscard]] virtual glm::ivec2 getFramebufferSize () const = 0;

    /**
     * @return Seconds elapsed since the host started rendering. The renderer
     *         uses this to drive animations, so it must be monotonic.
     */
    [[nodiscard]] virtual float getRenderTime () const = 0;

    /**
     * @return Pointer position in physical pixels with a top-left origin, or a
     *         negative position when the pointer is not over the wallpaper.
     *         The driver converts this to the engine's bottom-left origin.
     */
    [[nodiscard]] virtual glm::dvec2 getPointerPosition () const = 0;

    /**
     * Whether the final blit has to be flipped vertically. Hosts that treat the
     * destination as a plain GL framebuffer want false; hosts that present it as
     * a top-left-origin texture want true.
     */
    [[nodiscard]] virtual bool wantsVerticalFlip () const = 0;
};
} // namespace WallpaperEngine::Render::Drivers
