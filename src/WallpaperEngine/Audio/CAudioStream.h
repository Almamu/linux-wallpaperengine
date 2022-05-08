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

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

namespace WallpaperEngine
{
    namespace Audio
    {
        class CAudioStream
        {
        public:
            CAudioStream (const std::string& filename);
            CAudioStream (void* buffer, int length);
            CAudioStream (AVCodecContext* context);

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
            void* getBuffer ();
            int getLength ();
            int getPosition ();
            void setPosition (int current);

            int decodeFrame (uint8_t* audioBuffer, int bufferSize);

        private:
            void loadCustomContent (const char* filename = nullptr);
            int resampleAudio (AVFrame * decoded_audio_frame, enum AVSampleFormat out_sample_fmt, int out_channels, int out_sample_rate, uint8_t* out_buf);
            bool doQueue (AVPacket* pkt);
            void initialize ();

            bool m_initialized;
            bool m_repeat;
            AVCodecContext* m_context = nullptr;
            AVFormatContext* m_formatContext = nullptr;
            int m_audioStream = -1;
            void* m_buffer;
            int m_length;
            int m_position = 0;

            struct MyAVPacketList
            {
                AVPacket *packet;
            };

            struct PacketQueue
            {
                AVFifoBuffer* packetList;
                int nb_packets;
                int size;
                int64_t duration;
                SDL_mutex *mutex;
                SDL_cond *cond;
            } *m_queue;
        };
    }
};
