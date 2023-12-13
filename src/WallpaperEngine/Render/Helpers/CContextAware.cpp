#include "CContextAware.h"

namespace WallpaperEngine::Render::Helpers {
CContextAware::CContextAware (const CContextAware& from) : CContextAware (from.getContext ()) {}

CContextAware::CContextAware (const CContextAware* from) : CContextAware (from->getContext ()) {}

CContextAware::CContextAware (CRenderContext& context) : m_context (context) {}

CRenderContext& CContextAware::getContext () const {
    return this->m_context;
}
} // namespace WallpaperEngine::Render::Helpers