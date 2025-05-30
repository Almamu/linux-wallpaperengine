#include "CWallpaperState.h"
#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <algorithm>

using namespace WallpaperEngine::Render;

CWallpaperState::CWallpaperState (
    const TextureUVsScaling& textureUVsMode, const WallpaperEngine::Assets::ITexture::TextureFlags& clampMode) :
    m_textureUVsMode (textureUVsMode) {}

bool CWallpaperState::hasChanged (
    const glm::ivec4& viewport, const bool& vflip, const int& projectionWidth,const int& projectionHeight
) const {
    return this->viewport.width != viewport.z || this->viewport.height != viewport.w ||
           this->projection.width != projectionWidth || this->projection.height != projectionHeight ||
           this->vflip != vflip;
}

// Reset UVs to 0/1 values
void CWallpaperState::resetUVs () {
    this->UVs.ustart = 0;
    this->UVs.uend = 1;
    if (vflip) {
        this->UVs.vstart = 0.0f;
        this->UVs.vend = 1.0f;
    } else {
        this->UVs.vstart = 1.0f;
        this->UVs.vend = 0.0f;
    }
}

// Update Us coordinates for current viewport and projection
void CWallpaperState::updateUs (const int& projectionWidth, const int& projectionHeight) {
    const float viewportWidth = this->getViewportWidth ();
    const float viewportHeight = this->getViewportHeight ();
    const int newWidth = viewportHeight / projectionHeight * projectionWidth;
    const float newCenter = newWidth / 2.0f;
    const float viewportCenter = viewportWidth / 2.0;

    const float left = newCenter - viewportCenter;
    const float right = newCenter + viewportCenter;

    this->UVs.ustart = left / newWidth;
    this->UVs.uend = right / newWidth;
}

// Update Vs coordinates for current viewport and projection
void CWallpaperState::updateVs (const int& projectionWidth, const int& projectionHeight) {
    const float viewportWidth = this->getViewportWidth ();
    const float viewportHeight = this->getViewportHeight ();
    const int newHeight = viewportWidth / projectionWidth * projectionHeight;
    const float newCenter = newHeight / 2.0f;
    const float viewportCenter = viewportHeight / 2.0;

    const float down = newCenter - viewportCenter;
    const float up = newCenter + viewportCenter;

    if (vflip) {
        this->UVs.vstart = down / newHeight;
        this->UVs.vend = up / newHeight;
    } else {
        this->UVs.vstart = up / newHeight;
        this->UVs.vend = down / newHeight;
    }
}

template <> void CWallpaperState::updateTextureUVs<CWallpaperState::TextureUVsScaling::StretchUVs> () {
    this->resetUVs ();
}

template <> void CWallpaperState::updateTextureUVs<CWallpaperState::TextureUVsScaling::ZoomFillUVs> () {
    this->resetUVs ();

    const int viewportWidth = this->getViewportWidth ();
    const int viewportHeight = this->getViewportHeight ();
    int projectionWidth = this->getProjectionWidth ();
    int projectionHeight = this->getProjectionHeight ();

    const float m1 = static_cast<float> (viewportWidth) / projectionWidth;
    const float m2 = static_cast<float> (viewportHeight) / projectionHeight;
    const float m = std::max (m1, m2);
    projectionWidth *= m;
    projectionHeight *= m;

    if (projectionWidth != viewportWidth) {
        this->updateUs (projectionWidth, projectionHeight);
    } else if (projectionHeight != viewportHeight) {
        this->updateVs (projectionWidth, projectionHeight);
    }
}

template <> void CWallpaperState::updateTextureUVs<CWallpaperState::TextureUVsScaling::ZoomFitUVs> () {
    this->resetUVs ();

    const int viewportWidth = this->getViewportWidth ();
    const int viewportHeight = this->getViewportHeight ();
    int projectionWidth = this->getProjectionWidth ();
    int projectionHeight = this->getProjectionHeight ();

    const float m1 = static_cast<float> (viewportWidth) / projectionWidth;
    const float m2 = static_cast<float> (viewportHeight) / projectionHeight;
    const float m = std::min (m1, m2);
    projectionWidth *= m;
    projectionHeight *= m;

    if (projectionWidth != viewportWidth) {
        this->updateUs (projectionWidth, projectionHeight);
    } else if (projectionHeight != viewportHeight) {
        this->updateVs (projectionWidth, projectionHeight);
    }
}

template <> void CWallpaperState::updateTextureUVs<CWallpaperState::TextureUVsScaling::DefaultUVs> () {
    this->resetUVs ();

    const int viewportWidth = this->getViewportWidth ();
    const int viewportHeight = this->getViewportHeight ();
    const int projectionWidth = this->getProjectionWidth ();
    const int projectionHeight = this->getProjectionHeight ();

    if ((viewportHeight > viewportWidth && projectionWidth >= projectionHeight) ||
        (viewportWidth > viewportHeight && projectionHeight > projectionWidth)) {
        updateUs (projectionWidth, projectionHeight);
    }

    if ((viewportWidth > viewportHeight && projectionWidth >= projectionHeight) ||
        (viewportHeight > viewportWidth && projectionHeight > projectionWidth)) {
        updateVs (projectionWidth, projectionHeight);
    }
}

template <CWallpaperState::TextureUVsScaling T> void CWallpaperState::updateTextureUVs () {
    sLog.exception (
        "Using generic template for scaling is not allowed. Write specialization template for your scaling mode.\
     This message is for developers, if you are just user it's a bug.");
}

CWallpaperState::TextureUVsScaling CWallpaperState::getTextureUVsScaling () const {
    return this->m_textureUVsMode;
}

WallpaperEngine::Assets::ITexture::TextureFlags CWallpaperState::getClampingMode () const {
    return this->m_clampingMode;
}

void CWallpaperState::setTextureUVsStrategy (CWallpaperState::TextureUVsScaling strategy) {
    this->m_textureUVsMode = strategy;
}

int CWallpaperState::getViewportWidth () const {
    return this->viewport.width;
}

int CWallpaperState::getViewportHeight () const {
    return this->viewport.height;
}

int CWallpaperState::getProjectionWidth () const {
    return this->projection.width;
}

int CWallpaperState::getProjectionHeight () const {
    return this->projection.height;
}

void CWallpaperState::updateState (const glm::ivec4& viewport, const bool& vflip, const int& projectionWidth,
                                   const int& projectionHeight) {
    this->viewport.width = viewport.z;
    this->viewport.height = viewport.w;
    this->vflip = vflip;
    this->projection.width = projectionWidth;
    this->projection.height = projectionHeight;

    // Set texture UVs according to choosen scaling mode for this wallpaper
    switch (this->getTextureUVsScaling ()) {
        case CWallpaperState::TextureUVsScaling::StretchUVs:
            this->updateTextureUVs<CWallpaperState::TextureUVsScaling::StretchUVs> ();
            break;
        case CWallpaperState::TextureUVsScaling::ZoomFillUVs:
            this->updateTextureUVs<CWallpaperState::TextureUVsScaling::ZoomFillUVs> ();
            break;
        case CWallpaperState::TextureUVsScaling::ZoomFitUVs:
            this->updateTextureUVs<CWallpaperState::TextureUVsScaling::ZoomFitUVs> ();
            break;
        case CWallpaperState::TextureUVsScaling::DefaultUVs:
            this->updateTextureUVs<CWallpaperState::TextureUVsScaling::DefaultUVs> ();
            break;
        default:
            sLog.exception (
                "Switch case for specified scaling mode doesn't exist. Add your realisation in switch statement.\
                This message is for developers, if you are just user it's a bug.");
            break;
    }
}