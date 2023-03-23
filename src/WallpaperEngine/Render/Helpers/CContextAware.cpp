#include "CContextAware.h"

namespace WallpaperEngine::Render::Helpers
{
    CContextAware::CContextAware (CContextAware& from) :
        CContextAware (from.getContext ())
    {
    }

    CContextAware::CContextAware (CContextAware* from) :
        CContextAware (from->getContext ())
    {
    }

    CContextAware::CContextAware (CRenderContext& context) :
        m_context (context)
    {
    }

    CRenderContext& CContextAware::getContext () const
    {
        return this->m_context;
    }
}