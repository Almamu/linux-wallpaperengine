#pragma once

namespace WallpaperEngine::Render {
class CRenderContext;

namespace Helpers {
/**
 * Small helper class that provides access to the CRenderContext
 * in use currently
 */
class CContextAware {
  public:
    virtual ~CContextAware () = default;
    /**
     * @param from Object to get the render context from
     */
    CContextAware (const CContextAware& from);
    /**
     * @param from Object to get the render context from
     */
    explicit CContextAware (const CContextAware* from);
    explicit CContextAware (CRenderContext& context);

    /**
     * @return The CRenderContext in use right now
     */
    [[nodiscard]] CRenderContext& getContext () const;

  private:
    CRenderContext& m_context;
};
} // namespace Helpers
} // namespace WallpaperEngine::Render