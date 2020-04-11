#include "CVideo.h"

using namespace WallpaperEngine::Core;

CVideo::CVideo (
        const irr::io::path& filename) :
        CWallpaper (Type)
{
    if (avformat_open_input (&m_formatCtx, filename.c_str(), NULL, NULL) < 0)
        throw std::runtime_error ("Failed to open file");

    if (avformat_find_stream_info (m_formatCtx, NULL) < 0)
        throw std::runtime_error ("Failed to get stream info");

    // Find first video stream
    for (int i = 0; i < m_formatCtx->nb_streams; i++)
    {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStream = i;
            break;
        }
    }

    // Find first audio stream
    for (int i = 0; i < m_formatCtx->nb_streams; i++)
    {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStream = i; 
            break;
        }
    }

    // Only video stream is required
    if (m_videoStream == -1)
        throw std::runtime_error ("Failed to find video stream");

    AVCodec* codec = avcodec_find_decoder (m_formatCtx->streams[m_videoStream]->codecpar->codec_id);
    if (codec == nullptr)
        throw std::runtime_error ("Failed to find codec");

    m_codecCtx = avcodec_alloc_context3 (codec);
    if (avcodec_parameters_to_context (m_codecCtx, m_formatCtx->streams[m_videoStream]->codecpar))
        throw std::runtime_error ("Failed to copy codec parameters");

    if (avcodec_open2 (m_codecCtx, codec, NULL) < 0)
        throw std::runtime_error ("Failed to open codec");

    m_videoFrame = av_frame_alloc ();
    m_videoFrameRGB = av_frame_alloc ();
    if (m_videoFrameRGB == nullptr)
        throw std::runtime_error ("Failed to allocate video frame");
}

void CVideo::setSize (int width, int height)
{
    if (buffer != nullptr)
        av_free (buffer);

    if (m_swsCtx != nullptr)
        sws_freeContext (m_swsCtx);

    int numBytes = av_image_get_buffer_size (AV_PIX_FMT_RGB24, width, height, 1);
    buffer = (uint8_t*)av_malloc (numBytes * sizeof (uint8_t));

    av_image_fill_arrays (m_videoFrameRGB->data, m_videoFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, width, height, 1);

    m_swsCtx = sws_getContext (m_codecCtx->width, m_codecCtx->height,
                    m_codecCtx->pix_fmt,
                    width, height,
                    AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, NULL, NULL, NULL);
}

void CVideo::restartStream ()
{
    av_seek_frame (m_formatCtx, m_videoStream, 0, AVSEEK_FLAG_FRAME);
    avcodec_flush_buffers (m_codecCtx);
}

AVFormatContext* CVideo::getFormatContext ()
{
    return this->m_formatCtx;
}

AVCodecContext* CVideo::getCodecContext ()
{
    return this->m_codecCtx;
}

AVFrame* CVideo::getVideoFrame ()
{
    return this->m_videoFrame;
}

AVFrame* CVideo::getVideoFrameRGB ()
{
    return this->m_videoFrameRGB;
}

SwsContext* CVideo::getSwsContext ()
{
    return this->m_swsCtx;
}

int CVideo::getVideoStreamIndex ()
{
    return this->m_videoStream;
}

int CVideo::getAudioStreamIndex ()
{
    return this->m_audioStream;
}

const std::string CVideo::Type = "video";
