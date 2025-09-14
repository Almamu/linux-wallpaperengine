#include "ContextAware.h"

namespace WallpaperEngine::Render::Helpers {
ContextAware::ContextAware (const ContextAware& from) : ContextAware (from.getContext ()) {}

ContextAware::ContextAware (const ContextAware* from) : ContextAware (from->getContext ()) {}

ContextAware::ContextAware (RenderContext& context) : m_context (context) {}

RenderContext& ContextAware::getContext () const {
    return this->m_context;
}
} // namespace WallpaperEngine::Render::Helpers