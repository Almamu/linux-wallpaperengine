#include "CVideo.h"

using namespace WallpaperEngine;

using namespace WallpaperEngine::Render;

CVideo::CVideo (Core::CVideo* video, WallpaperEngine::Irrlicht::CContext* context) :
    CWallpaper (video, Type, context)
{
}

void CVideo::render ()
{
    irr::video::IVideoDriver* driver = m_context->getDevice ()->getVideoDriver ();
    int width = driver->getScreenSize ().Width;
    int height = driver->getScreenSize ().Height;

    m_frameImage = m_context->getDevice ()->getVideoDriver ()->createImage (irr::video::ECOLOR_FORMAT::ECF_R8G8B8,
                irr::core::dimension2du(width, height));

    getVideo ()->setSize (width, height);
    getNextFrame ();
    writeFrameToImage ();

    driver->removeTexture (m_frameTexture);
    m_frameTexture = driver->addTexture ("frameTexture", m_frameImage);
    m_frameImage->drop ();

    driver->draw2DImage (m_frameTexture, irr::core::vector2di(0));
}

void CVideo::getNextFrame ()
{
    Core::CVideo* videoData = getVideo ();
    bool eof = false;
    AVPacket packet;
    packet.data = nullptr;
    
    // Find video streams packet
    do
    {
        if (packet.data != nullptr)
            av_packet_unref (&packet);

        int readError = av_read_frame (videoData->getFormatContext (), &packet);
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
        
    } while (packet.stream_index != videoData->getVideoStreamIndex ());

    // Send video stream packet to codec
    if (avcodec_send_packet (videoData->getCodecContext (), &packet) < 0)
        return;
    
    // Receive frame from codec
    if (avcodec_receive_frame (videoData->getCodecContext (), videoData->getVideoFrame ()) < 0)
        return;

    sws_scale (videoData->getSwsContext (), (uint8_t const* const*)videoData->getVideoFrame ()->data, videoData->getVideoFrame ()->linesize,
            0, videoData->getCodecContext ()->height, videoData->getVideoFrameRGB ()->data, videoData->getVideoFrameRGB ()->linesize);

    av_packet_unref (&packet);

    if (eof)
        videoData->restartStream ();
}

void CVideo::writeFrameToImage ()
{
    uint8_t* frameData = getVideo ()->getVideoFrameRGB ()->data[0];
    if (frameData == nullptr)
        return;

    irr::u32 imgWidth = m_frameImage->getDimension().Width;
    irr::u32 imgHeight = m_frameImage->getDimension().Height;
    
    unsigned char* data = (unsigned char*)m_frameImage->lock ();
    memcpy (data, frameData, imgWidth * imgHeight * 3);
    m_frameImage->unlock ();
}

Core::CVideo* CVideo::getVideo ()
{
    return this->getWallpaperData ()->as<Core::CVideo> ();
}

const std::string CVideo::Type = "video";
