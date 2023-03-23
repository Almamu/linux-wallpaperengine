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
}

#include <SDL.h>
#include <SDL_thread.h>

#include "WallpaperEngine/Audio/CAudioContext.h"

namespace WallpaperEngine::Audio
{
    class CAudioContext;

    /**
     * Represents a playable audio stream for the audio driver
     */
    class CAudioStream
    {
    public:
        CAudioStream (CAudioContext& context, const std::string& filename);
        CAudioStream (CAudioContext& context, const void* buffer, int length);
        CAudioStream (CAudioContext& audioContext, AVCodecContext* context);
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

        /**
         * @return The audio context in use for this audio stream
         */
        [[nodiscard]] CAudioContext& getAudioContext () const;

        /**
         * @return to the codec context, which provides information on the audio stream's format
         */
        AVCodecContext* getContext ();
        /**
         * @returns the format context, which controls how data is read off the audio stream
         */
        AVFormatContext* getFormatContext ();
        /**
         * @return The audio stream index of the given file
         */
        int getAudioStream ();
        /**
         * @return If the audio stream can be played or not
         */
        bool isInitialized ();
        /**
         * @param newRepeat true = repeat, false = no repeat
         */
        void setRepeat (bool newRepeat = true);
        /**
         * @return If the stream is to be repeated at the end or not
         */
        bool isRepeat ();
        /**
         * Stops decoding and playbak of the stream
         */
        void stop ();
        /**
         * @return The file data buffer
         */
        const void* getBuffer ();
        /**
         * @return The length of the file data buffer
         */
        int getLength ();
        /**
         * @return The read position of the data buffer
         */
        int getPosition ();
        /**
         * Updates the read position of the data buffer
         *
         * @param current
         */
        void setPosition (int current);
        /**
         * @return The SDL_cond used to signal waiting for data
         */
        SDL_cond* getWaitCondition ();
        /**
         * @return The data queue size
         */
        int getQueueSize ();
        /**
         * @return The amount of packets ready to be converted and played
         */
        int getQueuePacketCount ();
        /**
         * @return The duration (in seconds) of the queued data to be played
         */
        int64_t getQueueDuration ();
        /**
         * @return Time unit used for packet playback
         */
        AVRational getTimeBase ();
        /**
         * @return If the data queue is empty or not
         */
        bool isQueueEmpty ();
        /**
         * @return The SDL_mutex used for thread synchronization
         */
        SDL_mutex* getMutex ();

        /**
         * Reads a frame from the audio stream, resamples it to the driver's settings
         * and returns the data ready to be played
         *
         * @param audioBuffer
         * @param bufferSize
         *
         * @return The amount of bytes available or < 0 for error
         */
        int decodeFrame (uint8_t* audioBuffer, int bufferSize);

    private:
        /**
         * Initializes ffmpeg to read the given file
         *
         * @param filename
         */
        void loadCustomContent (const char* filename = nullptr);
        /**
         * Converts the audio frame from the original format to one supported by the audio driver
         *
         * @param decoded_audio_frame
         * @param out_buf
         * @return
         */
        int resampleAudio (AVFrame* decoded_audio_frame, uint8_t* out_buf);
        /**
         * Queues a packet into the play queue
         *
         * @param pkt
         * @return
         */
        bool doQueue (AVPacket* pkt);
        /**
         * Initializes queues and ffmpeg resampling
         */
        void initialize ();

#if FF_API_OLD_CHANNEL_LAYOUT
        /** Chanel layout needed for old FFMPEG versions */
        AVChannelLayout m_out_channel_layout;
#endif
        /** The SwrContext that handles resampling */
        SwrContext* m_swrctx;
        /** The audio context this stream will be played under */
        CAudioContext& m_audioContext;
        /** If this stream was properly initialized or not */
        bool m_initialized;
        /** Repeat enabled? */
        bool m_repeat;
        /** The codec context that contains the original audio format information */
        AVCodecContext* m_context = nullptr;
        /** The format context that controls how data is read off the file */
        AVFormatContext* m_formatContext = nullptr;
        /** The stream index for the audio being played */
        int m_audioStream = -1;
        /** File data pointer */
        const void* m_buffer;
        /** The length of the file data pointer */
        int m_length;
        /** The read position on the file data pointer */
        int m_position = 0;

        struct MyAVPacketList
        {
            AVPacket* packet;
        };

        /**
         * Packet queue information
         */
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
            SDL_mutex* mutex;
            SDL_cond* wait;
            SDL_cond* cond;
        }* m_queue;
    };
}
