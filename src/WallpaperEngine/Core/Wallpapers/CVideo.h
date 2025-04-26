#pragma once

#include "WallpaperEngine/Core/CWallpaper.h"
#include "WallpaperEngine/Core/Core.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace WallpaperEngine::Core::Wallpapers {
class CVideo : public CWallpaper {
  public:
    CVideo (std::string filename, std::shared_ptr <const CProject> project);

    const std::string& getFilename () const;

  protected:
    friend class CWallpaper;

    const std::string m_filename;
};
} // namespace WallpaperEngine::Core
