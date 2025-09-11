#pragma once

namespace WallpaperEngine::Render {
class RenderContext;

namespace Helpers {
/**
 * Small helper class that provides access to the CRenderContext
 * in use currently
 */
class ContextAware {
  public:
    virtual ~ContextAware () = default;
    /**
     * @param from Object to get the render context from
     */
    ContextAware (const ContextAware& from);
    /**
     * @param from Object to get the render context from
     */
    explicit ContextAware (const ContextAware* from);
    explicit ContextAware (RenderContext& context);

    /**
     * @return The CRenderContext in use right now
     */
    [[nodiscard]] RenderContext& getContext () const;

  private:
    RenderContext& m_context;
};
} // namespace Helpers
} // namespace WallpaperEngine::Render