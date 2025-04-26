#pragma once

#include "WallpaperEngine/Core/CWallpaper.h"
#include "WallpaperEngine/Core/Core.h"

// Chromium Embedded Framework
#include "include/cef_render_handler.h"
#include "include/cef_client.h"
#include "include/cef_app.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace WallpaperEngine::Core::Wallpapers {
class CWeb : public CWallpaper {
  public:
    CWeb (std::string filename, const CProject& project);

    const std::string& getFilename () const;

  protected:
    friend class CWallpaper;

    const std::string m_filename;
};
}