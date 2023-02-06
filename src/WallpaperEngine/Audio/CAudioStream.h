#pragma once

#include <string>

extern "C"
{
    #include <libavutil/fifo.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
};

#include <SDL.h>
#include <SDL_thread.h>

#include "WallpaperEngine/Audio/CAudioContext.h"

namespace WallpaperEngine::Audio
{
    class CAudioStream
    {
    public:
        CAudioStream (CAudioContext* context, const std::string& filename);
        CAudioStream (CAudioContext* context, const void* buffer, int length);
        CAudioStream (CAudioContext* audioContext, AVCodecContext* context);
        ~CAudioStream ();

        void queuePacket (AVPacket* pkt);

        /**
         * Gets the next packet in the queue
         *
         * WARNING: BLOCKS UNTIL SOME DATA IS READ FROM IT
         *
         * @return
         */
        void dequeuePacket (AVPacket* output);

        AVCodecContext* getContext ();
        AVFormatContext* getFormatContext ();
        int getAudioStream ();
        bool isInitialized ();
        void setRepeat (bool newRepeat = true);
        bool isRepeat ();
        void stop ();
        const void* getBuffer ();
        int getLength ();
        int getPosition ();
        void setPosition (int current);
        SDL_cond* getWaitCondition ();
        int getQueueSize ();
        int getQueuePacketCount ();
        int64_t getQueueDuration ();
        AVRational getTimeBase ();
        bool isQueueEmpty ();
        SDL_mutex* getMutex ();

        int decodeFrame (uint8_t* audioBuffer, int bufferSize);

    private:
        void loadCustomContent (const char* filename = nullptr);
        int resampleAudio (AVFrame * decoded_audio_frame, enum AVSampleFormat out_sample_fmt, int out_channels, int out_sample_rate, uint8_t* out_buf);
        bool doQueue (AVPacket* pkt);
        void initialize ();

#if FF_API_OLD_CHANNEL_LAYOUT
        AVChannelLayout m_out_channel_layout;
#endif

        SwrContext* m_swrctx;
        CAudioContext* m_audioContext;
        bool m_initialized;
        bool m_repeat;
        AVCodecContext* m_context = nullptr;
        AVFormatContext* m_formatContext = nullptr;
        int m_audioStream = -1;
        const void* m_buffer;
        int m_length;
        int m_position = 0;

        struct MyAVPacketList
        {
            AVPacket *packet;
        };

        struct PacketQueue
        {
            #if FF_API_FIFO_OLD_API
            AVFifo* packetList;
            #else
            AVFifoBuffer* packetList;
            #endif
            int nb_packets;
            int size;
            int64_t duration;
            SDL_mutex *mutex;
            SDL_cond *wait;
            SDL_cond *cond;
        } *m_queue;
    };
};
