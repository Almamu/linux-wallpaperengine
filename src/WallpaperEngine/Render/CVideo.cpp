#include "CVideo.h"

using namespace WallpaperEngine;

using namespace WallpaperEngine::Render;

CVideo::CVideo (Core::CVideo* video, WallpaperEngine::Irrlicht::CContext* context) :
    CWallpaper (video, Type, context)
{
    if (avformat_open_input (&m_formatCtx, video->getFilename ().c_str (), NULL, NULL) < 0)
        throw std::runtime_error ("Failed to open video file");

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
    if (m_buffer != nullptr)
        av_free (m_buffer);

    if (m_swsCtx != nullptr)
        sws_freeContext (m_swsCtx);

    int numBytes = av_image_get_buffer_size (AV_PIX_FMT_RGB24, width, height, 1);
    m_buffer = (uint8_t*)av_malloc (numBytes * sizeof (uint8_t));

    av_image_fill_arrays (m_videoFrameRGB->data, m_videoFrameRGB->linesize, m_buffer, AV_PIX_FMT_RGB24, width, height, 1);

    m_swsCtx = sws_getContext (m_codecCtx->width, m_codecCtx->height,
                    m_codecCtx->pix_fmt,
                    width, height,
                    AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, NULL, NULL, NULL);
}

void CVideo::render ()
{
    irr::video::IVideoDriver* driver = m_context->getDevice ()->getVideoDriver ();
    int width = driver->getScreenSize ().Width;
    int height = driver->getScreenSize ().Height;

    m_frameImage = m_context->getDevice ()->getVideoDriver ()->createImage (irr::video::ECOLOR_FORMAT::ECF_R8G8B8,
                irr::core::dimension2du(width, height));

    setSize (width, height);
    getNextFrame ();
    writeFrameToImage ();

    driver->removeTexture (m_frameTexture);
    m_frameTexture = driver->addTexture ("frameTexture", m_frameImage);
    m_frameImage->drop ();

    driver->draw2DImage (m_frameTexture, irr::core::vector2di(0));
}

void CVideo::getNextFrame ()
{
    bool eof = false;
    AVPacket packet;
    packet.data = nullptr;
    
    // Find video streams packet
    do
    {
        if (packet.data != nullptr)
            av_packet_unref (&packet);

        int readError = av_read_frame (m_formatCtx, &packet);
        if (readError == AVERROR_EOF)
        {
            eof = true;
            break;
        }
        else if (readError < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            throw std::runtime_error (av_make_error_string (err, AV_ERROR_MAX_STRING_SIZE, readError));
        }
        
    } while (packet.stream_index != m_videoStream);

    // Send video stream packet to codec
    if (avcodec_send_packet (m_codecCtx, &packet) < 0)
        return;
    
    // Receive frame from codec
    if (avcodec_receive_frame (m_codecCtx, m_videoFrame) < 0)
        return;

    sws_scale (m_swsCtx, (uint8_t const* const*)m_videoFrame->data, m_videoFrame->linesize,
            0, m_codecCtx->height, m_videoFrameRGB->data, m_videoFrameRGB->linesize);

    av_packet_unref (&packet);

    if (eof)
        restartStream ();
}

void CVideo::writeFrameToImage ()
{
    uint8_t* frameData = m_videoFrameRGB->data[0];
    if (frameData == nullptr)
        return;

    irr::u32 imgWidth = m_frameImage->getDimension().Width;
    irr::u32 imgHeight = m_frameImage->getDimension().Height;
    
    unsigned char* data = (unsigned char*)m_frameImage->lock ();
    memcpy (data, frameData, imgWidth * imgHeight * 3);
    m_frameImage->unlock ();
}

void CVideo::restartStream ()
{
    av_seek_frame (m_formatCtx, m_videoStream, 0, AVSEEK_FLAG_FRAME);
    avcodec_flush_buffers (m_codecCtx);
}

Core::CVideo* CVideo::getVideo ()
{
    return this->getWallpaperData ()->as<Core::CVideo> ();
}

const std::string CVideo::Type = "video";
