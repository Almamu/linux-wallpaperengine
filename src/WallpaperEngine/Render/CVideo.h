#pragma once

#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace WallpaperEngine::Render
{
    class CVideo : public CWallpaper
    {
    public:
        CVideo (Core::CVideo* video, CContainer* container, CContext* context);

        Core::CVideo* getVideo ();

        int getWidth ();
        int getHeight ();

    protected:
        void renderFrame (glm::ivec4 viewport) override;

        friend class CWallpaper;

        static const std::string Type;

    private:
        void setSize (int width, int height);
        void restartStream ();
        void getNextFrame ();
        void writeFrameToImage ();
        void setupShaders ();

        AVFormatContext* m_formatCtx = nullptr;
        AVCodecContext* m_codecCtx = nullptr;
        AVFrame* m_videoFrameRGB = nullptr;
        AVFrame* m_videoFrame = nullptr;
        SwsContext* m_swsCtx = nullptr;
        uint8_t* m_buffer = nullptr;
        int m_videoStream = -1, m_audioStream = -1;

        Audio::CAudioStream* m_audio = nullptr;

        /**
         * The texture used for the video output
         */
        GLuint m_texture;
        GLuint m_texCoordBuffer;
        GLuint m_positionBuffer;
        GLuint m_shader;
        // shader variables
        GLint g_Texture0;
        GLint a_Position;
        GLint a_TexCoord;
    };
};
