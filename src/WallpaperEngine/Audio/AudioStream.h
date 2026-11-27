#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

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

#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Audio/SoundSpatialMixer.h"

// TODO: FIND A BETTER PLACE TO DO THIS? OLD_API MIGHT EXIST BUT THIS DEFINE MIGHT NOT BE DEFINED...
#ifndef FF_API_FIFO_OLD_API
#define FF_API_FIFO_OLD_API (LIBAVUTIL_VERSION_MAJOR < 59)
#endif
#ifndef FF_API_OLD_CHANNEL_LAYOUT
#define FF_API_OLD_CHANNEL_LAYOUT (LIBAVUTIL_VERSION_MAJOR < 59)
#endif

#define MAX_QUEUE_SIZE (5 * 1024 * 1024)
#define MIN_FRAMES (25)
#define NO_AUDIO_STREAM (-1)

namespace WallpaperEngine::Audio {
class AudioContext;

using namespace WallpaperEngine::FileSystem;

/**
 * Represents a playable audio stream for the audio driver
 */
class AudioStream {
public:
    AudioStream (AudioContext& context, const std::string& filename, bool repeat = false);
    AudioStream (AudioContext& context, const ReadStreamSharedPtr& buffer, bool repeat = false, bool spatial = false);
    AudioStream (AudioContext& audioContext, AVCodecContext* context);
    ~AudioStream ();

    void queuePacket (AVPacket* pkt);

    /**
     * Queues an ordered drain/reset after every packet from the completed pass.
     * The file-reader thread must not call avcodec_flush_buffers() directly because
     * the SDL callback owns and operates the decoder context.
     */
    void queueDecoderFlush ();

    /**
     * Tries to get the next packet in the queue. Audio callbacks must never
     * block: an empty queue means silence until the reader supplies more data.
     *
     * @return whether a packet was dequeued
     */
    bool dequeuePacket ();

    /**
     * @return The audio context in use for this audio stream
     */
    [[nodiscard]] AudioContext& getAudioContext () const;

    /**
     * @return to the codec context, which provides information on the audio stream's format
     */
    [[nodiscard]] AVCodecContext* getContext () const;
    /**
     * @returns the format context, which controls how data is read off the audio stream
     */
    [[nodiscard]] AVFormatContext* getFormatContext () const;
    /**
     * @return The audio stream index of the given file
     */
    [[nodiscard]] int getAudioStream () const;
    /**
     * @return If the audio stream can be played or not
     */
    [[nodiscard]] bool isInitialized () const;
    /** Pause consumption without losing buffered audio or resetting the playhead. */
    void setPaused (bool paused) { this->m_paused = paused; }
    [[nodiscard]] bool isPaused () const { return this->m_paused; }
    /** Linear per-layer gain; other stream users retain unity gain. */
    void setGain (float gain) { this->m_gain.store (gain, std::memory_order_relaxed); }
    [[nodiscard]] float getGain () const { return this->m_gain.load (std::memory_order_relaxed); }
    /** Per-channel multipliers for the driver's float stereo output. */
    void setStereoGain (float left, float right) {
	this->m_stereoGain[0].store (left, std::memory_order_relaxed);
	this->m_stereoGain[1].store (right, std::memory_order_relaxed);
    }
    [[nodiscard]] float getStereoGain (int channel) const {
	return this->m_stereoGain[channel].load (std::memory_order_relaxed);
    }
    /** Select fallback output before registering this stream with the driver. */
    void useLegacyOutput ();
    [[nodiscard]] bool usesSpatialMixer () const { return this->m_spatial; }
    [[nodiscard]] int getOutputChannels () const { return this->m_outputChannels; }
    [[nodiscard]] int getOutputSampleRate () const { return this->m_outputSampleRate; }
    void setSpatialParameters (const SoundSpatialMixer::Parameters& parameters) {
        for (unsigned i = 0; i < 3; ++i) this->m_position[i].store (parameters.position[i], std::memory_order_relaxed);
        this->m_spatialGain.store (parameters.gain, std::memory_order_relaxed);
        this->m_attenuation.store (parameters.attenuation, std::memory_order_relaxed);
        this->m_minimumDistance.store (parameters.minimumDistance, std::memory_order_relaxed);
    }
    [[nodiscard]] SoundSpatialMixer::Parameters getSpatialParameters () const {
        SoundSpatialMixer::Parameters parameters;
        for (unsigned i = 0; i < 3; ++i) parameters.position[i] = this->m_position[i].load (std::memory_order_relaxed);
        parameters.gain = this->m_spatialGain.load (std::memory_order_relaxed);
        parameters.attenuation = this->m_attenuation.load (std::memory_order_relaxed);
        parameters.minimumDistance = this->m_minimumDistance.load (std::memory_order_relaxed);
        parameters.paused = this->isPaused ();
        return parameters;
    }
    /**
     * @param newRepeat true = repeat, false = no repeat
     */
    void setRepeat (bool newRepeat = true);
    /**
     * @return If the stream is to be repeated at the end or not
     */
    [[nodiscard]] bool isRepeat () const;
    /**
     * Marks a full playback pass (called by the read thread when the demuxer reaches EOF,
     * whether or not the stream loops afterwards)
     */
    void notifyCompletion () { this->m_completions++; }
    /**
     * @return How many full passes the stream has played so far
     */
    [[nodiscard]] uint32_t getCompletionCount () const { return this->m_completions; }
    /** Completed passes consumed by the output callback, including codec/resampler tails. */
    void notifyPlaybackCompletion () { this->m_playbackCompletions++; }
    [[nodiscard]] uint32_t getPlaybackCompletionCount () const { return this->m_playbackCompletions; }
    /** Duration of the selected file in seconds, without opening other alternatives. */
    [[nodiscard]] double getDuration () const;
    /**
     * Stops decoding and playback of the stream
     */
    void stop ();
    /**
     * @return The file data buffer
     */
    [[nodiscard]] ReadStreamSharedPtr& getBuffer ();
    /**
     * @return The SDL_cond used to signal waiting for data
     */
    [[nodiscard]] SDL_cond* getWaitCondition () const;
    /**
     * @return The data queue size
     */
    [[nodiscard]] size_t getQueueSize () const;
    /**
     * @return The amount of packets ready to be converted and played
     */
    [[nodiscard]] int getQueuePacketCount () const;
    /**
     * @return The duration (in seconds) of the queued data to be played
     */
    [[nodiscard]] int64_t getQueueDuration () const;
    /**
     * @return Time unit used for packet playback
     */
    [[nodiscard]] AVRational getTimeBase () const;
    /**
     * @return If the data queue is empty or not
     */
    [[nodiscard]] bool isQueueEmpty () const;
    /**
     * @return The SDL_mutex used for thread synchronization
     */
    [[nodiscard]] SDL_mutex* getMutex () const;

    /**
     * Reads a frame from the audio stream, resamples it to the driver's settings
     * and returns the data ready to be played
     *
     * @param audioBuffer
     * @param bufferSize
     *
     * @return Bytes available, 0 if temporarily empty, or AVERROR_EOF once per fully drained pass.
     */
    int decodeFrame (uint8_t* audioBuffer, int bufferSize);

private:
    /**
     * Initializes ffmpeg to read the given file
     *
     * @param filename
     */
    void loadCustomContent (const char* filename = nullptr);
    void release ();
    /**
     * Converts the audio frame from the original format to one supported by the audio driver
     *
     * @param out_buf
     * @return
     */
    int resampleAudio (uint8_t* out_buf, const int out_size);
    int drainResampler (uint8_t* out_buf, int out_size);
    /**
     * Queues a packet into the play queue
     *
     * @param pkt
     * @return
     */
    bool doQueue (AVPacket* pkt, bool flushDecoder = false);
    /**
     * Initializes queues and ffmpeg resampling
     */
    void initialize ();
    void configureResampler ();

    std::vector<uint8_t> m_resampledPending;
    size_t m_resampledIndex = 0;
    bool m_spatial = false;
    int m_outputChannels = 0;
    int m_outputSampleRate = 0;
    AVSampleFormat m_outputFormat = AV_SAMPLE_FMT_NONE;
    std::atomic<float> m_position[3] { 0.0f, 0.0f, 0.0f };
    std::atomic<float> m_spatialGain = 1.0f, m_attenuation = 1.0f, m_minimumDistance = 1.0f;
    /** The SwrContext that handles resampling */
    SwrContext* m_swrctx = nullptr;
    /** The audio context this stream will be played under */
    AudioContext& m_audioContext;
    /** If this stream was properly initialized or not */
    std::atomic<bool> m_initialized = false;
    std::atomic<bool> m_paused = false;
    std::atomic<float> m_gain = 1.0f;
    std::atomic<float> m_stereoGain[2] { 1.0f, 1.0f };
    /** Repeat enabled? */
    std::atomic<bool> m_repeat = false;
    /** Full playback passes so far; written by the read thread, read by the render thread */
    std::atomic<uint32_t> m_completions = 0;
    std::atomic<uint32_t> m_playbackCompletions = 0;
    /** The codec context that contains the original audio format information */
    AVCodecContext* m_context = nullptr;
    /** The format context that controls how data is read off the file */
    AVFormatContext* m_formatContext = nullptr;
    AVIOContext* m_customIO = nullptr;
    /** The stream index for the audio being played */
    int m_audioStream = NO_AUDIO_STREAM;
    /** File data pointer */
    ReadStreamSharedPtr m_buffer = nullptr;

    struct MyAVPacketList {
	AVPacket* packet = nullptr;
	bool flushDecoder = false;
    };

    /** The packet used while decoding this stream */
    AVPacket* m_decodePacket = nullptr;
    /** The AV frame used while decoding this stream */
    AVFrame* m_decodeFrame = nullptr;
    /** The last dequeued entry is an ordered loop-boundary decoder reset */
    bool m_decoderFlushPending = false;
    bool m_draining = false;
    bool m_decoderFinished = false;

    /**
     * Packet queue information
     */
    struct PacketQueue {
#if FF_API_FIFO_OLD_API
	AVFifoBuffer* packetList = nullptr;
#else
	AVFifo* packetList = nullptr;
#endif
	// These counts are also read outside the queue lock by the reader and mixer.
	std::atomic<int> nb_packets = 0;
	std::atomic<size_t> size = 0;
	std::atomic<int64_t> duration = 0;
	SDL_mutex* mutex = nullptr;
	SDL_cond* wait = nullptr;
	SDL_cond* cond = nullptr;
    }* m_queue {};

    SDL_Thread* m_audioThread = nullptr;
};
} // namespace WallpaperEngine::Audio
