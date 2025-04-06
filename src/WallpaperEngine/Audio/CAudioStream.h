#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <SDL.h>
#include <SDL_thread.h>

#include "WallpaperEngine/Audio/CAudioContext.h"

// TODO: FIND A BETTER PLACE TO DO THIS? OLD_API MIGHT EXIST BUT THIS DEFINE MIGHT NOT BE DEFINED...
#ifndef FF_API_FIFO_OLD_API
#define 	FF_API_FIFO_OLD_API   (LIBAVUTIL_VERSION_MAJOR < 59)
#endif
#ifndef FF_API_OLD_CHANNEL_LAYOUT
#define 	FF_API_OLD_CHANNEL_LAYOUT   (LIBAVUTIL_VERSION_MAJOR < 59)
#endif

namespace WallpaperEngine::Audio {
class CAudioContext;

/**
 * Represents a playable audio stream for the audio driver
 */
class CAudioStream {
  public:
    CAudioStream (CAudioContext& context, const std::string& filename);
    CAudioStream (CAudioContext& context, const uint8_t* buffer, uint32_t length);
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
    [[nodiscard]] int getAudioStream () const;
    /**
     * @return If the audio stream can be played or not
     */
    [[nodiscard]] bool isInitialized () const;
    /**
     * @param newRepeat true = repeat, false = no repeat
     */
    void setRepeat (bool newRepeat = true);
    /**
     * @return If the stream is to be repeated at the end or not
     */
    [[nodiscard]] bool isRepeat () const;
    /**
     * Stops decoding and playbak of the stream
     */
    void stop ();
    /**
     * @return The file data buffer
     */
    [[nodiscard]] const uint8_t* getBuffer ();
    /**
     * @return The length of the file data buffer
     */
    [[nodiscard]] uint32_t getLength () const;
    /**
     * @return The read position of the data buffer
     */
    [[nodiscard]] uint32_t getPosition () const;
    /**
     * Updates the read position of the data buffer
     *
     * @param current
     */
    void setPosition (uint32_t current);
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
    int resampleAudio (const AVFrame* decoded_audio_frame, uint8_t* out_buf);
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

    /** The SwrContext that handles resampling */
    SwrContext* m_swrctx;
    /** The audio context this stream will be played under */
    CAudioContext& m_audioContext;
    /** If this stream was properly initialized or not */
    bool m_initialized {};
    /** Repeat enabled? */
    bool m_repeat {};
    /** The codec context that contains the original audio format information */
    AVCodecContext* m_context = nullptr;
    /** The format context that controls how data is read off the file */
    AVFormatContext* m_formatContext = nullptr;
    /** The stream index for the audio being played */
    int m_audioStream;
    /** File data pointer */
    const uint8_t* m_buffer {};
    /** The length of the file data pointer */
    uint32_t m_length {};
    /** The read position on the file data pointer */
    uint32_t m_position = 0;

    struct MyAVPacketList {
        AVPacket* packet;
    };

    /**
     * Packet queue information
     */
    struct PacketQueue {
#if FF_API_FIFO_OLD_API
        AVFifoBuffer* packetList;
#else
        AVFifo* packetList;
#endif
        int nb_packets;
        int size;
        int64_t duration;
        SDL_mutex* mutex;
        SDL_cond* wait;
        SDL_cond* cond;
    }* m_queue {};
};
} // namespace WallpaperEngine::Audio
