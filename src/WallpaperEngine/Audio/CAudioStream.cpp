#include "CAudioStream.h"
#include "common.h"
#include <cassert>
#include <iostream>
#include <math.h>

// maximum size of the queue to prevent reading too much data
#define MAX_QUEUE_SIZE (5 * 1024 * 1024)
#define MIN_FRAMES 25

using namespace WallpaperEngine::Audio;

int audio_read_thread (void* arg) {
    SDL_mutex* waitMutex = SDL_CreateMutex ();
    auto* stream = static_cast<CAudioStream*> (arg);
    AVPacket* packet = av_packet_alloc ();
    int ret = 0;

    if (waitMutex == nullptr)
        sLog.exception ("Cannot create mutex for audio playback waiting");

    while (ret >= 0 && stream->getAudioContext ().getApplicationContext ().state.general.keepRunning) {
        // give the cpu some time to play the queued frames if there's enough info there
        if (stream->getQueueSize () >= MAX_QUEUE_SIZE ||
            (stream->getQueuePacketCount () > MIN_FRAMES &&
             (av_q2d (stream->getTimeBase ()) * stream->getQueueDuration () > 1.0))) {
            SDL_LockMutex (waitMutex);
            SDL_CondWaitTimeout (stream->getWaitCondition (), waitMutex, 10);
            SDL_UnlockMutex (waitMutex);
            continue;
        }

        ret = av_read_frame (stream->getFormatContext (), packet);

        if (ret == AVERROR_EOF) {
            // seek to the beginning of the file again
            avformat_seek_file (stream->getFormatContext (), stream->getAudioStream (), 0, 0, 0, ~AVSEEK_FLAG_FRAME);
            avcodec_flush_buffers (stream->getContext ());

            // ensure the thread is not killed if audio has to be looped
            if (stream->isRepeat ())
                ret = 0;

            continue;
        }

        // TODO: PROPERLY IMPLEMENT THIS
        if (packet->stream_index == stream->getAudioStream ())
            stream->queuePacket (packet);
        else
            av_packet_unref (packet);

        if (!stream->isInitialized ())
            break;
    }

    // stop the audio too just in case
    stream->stop ();
    SDL_DestroyMutex (waitMutex);

    return 0;
}

static int audio_read_data_callback (void* streamarg, uint8_t* buffer, int buffer_size) {
    const auto stream = static_cast<CAudioStream*> (streamarg);
    const int left = stream->getLength () - stream->getPosition ();

    buffer_size = FFMIN (buffer_size, left);

    memcpy (buffer, stream->getBuffer () + stream->getPosition (), buffer_size);
    // update position
    stream->setPosition (stream->getPosition () + buffer_size);

    return buffer_size;
}

int64_t audio_seek_data_callback (void* streamarg, int64_t offset, int whence) {
    const auto stream = static_cast<CAudioStream*> (streamarg);

    if (whence & AVSEEK_SIZE)
        return stream->getLength ();

    switch (whence) {
        case SEEK_CUR: stream->setPosition (stream->getPosition () + offset); break;

        case SEEK_SET: stream->setPosition (offset); break;
    }

    return offset;
}

CAudioStream::CAudioStream (CAudioContext& context, const std::string& filename) :
    m_audioContext (context),
    m_swrctx (nullptr) {
    this->loadCustomContent (filename.c_str ());
}

CAudioStream::CAudioStream (CAudioContext& context, const void* buffer, int length) :
    m_audioContext (context),
    m_swrctx (nullptr) {
    // setup a custom context first
    this->m_formatContext = avformat_alloc_context ();

    if (this->m_formatContext == nullptr)
        sLog.exception ("Cannot allocate ffmpeg format context");

    this->m_buffer = buffer;
    this->m_length = length;
    this->m_position = 0;

    // setup custom io for it
    this->m_formatContext->pb = avio_alloc_context (static_cast<uint8_t*> (av_malloc (4096)), 4096, 0, this,
                                                    &audio_read_data_callback, nullptr, &audio_seek_data_callback);

    if (this->m_formatContext->pb == nullptr)
        sLog.exception ("Cannot create avio context");

    // continue the normal load procedure
    this->loadCustomContent ();
}

CAudioStream::CAudioStream (CAudioContext& audioContext, AVCodecContext* context) :
    m_context (context),
    m_queue (new PacketQueue),
    m_audioContext (audioContext),
    m_swrctx (nullptr) {
    this->initialize ();
}

CAudioStream::~CAudioStream () {
    if (this->m_swrctx != nullptr && swr_is_initialized (this->m_swrctx) == true)
        swr_close (this->m_swrctx);
    if (this->m_swrctx != nullptr)
        swr_free (&this->m_swrctx);

    // TODO: FREE EVERYTHING ELSE THAT THIS CLASS HOLDS!
}

void CAudioStream::loadCustomContent (const char* filename) {
    if (avformat_open_input (&this->m_formatContext, filename, nullptr, nullptr) != 0)
        sLog.exception ("Cannot open audio file: ", filename);
    if (avformat_find_stream_info (this->m_formatContext, nullptr) < 0)
        sLog.exception ("Cannot determine file format: ", filename);

    // find the audio stream
    for (int i = 0; i < this->m_formatContext->nb_streams; i++) {
        if (this->m_formatContext->streams [i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && this->m_audioStream < 0)
            this->m_audioStream = i;
    }

    if (this->m_audioStream == -1)
        sLog.exception ("Cannot find an audio stream in file ", filename);

    // get the decoder for it and alloc the required context
    const AVCodec* aCodec =
        avcodec_find_decoder (this->m_formatContext->streams [this->m_audioStream]->codecpar->codec_id);

    if (aCodec == nullptr)
        sLog.exception ("Cannot initialize audio decoder for file: ", filename);

    // alocate context
    AVCodecContext* avCodecContext = avcodec_alloc_context3 (aCodec);

    if (avcodec_parameters_to_context (avCodecContext,
                                       this->m_formatContext->streams [this->m_audioStream]->codecpar) != 0)
        sLog.exception ("Cannot initialize audio decoder parameters");

    // finally open
    avcodec_open2 (avCodecContext, aCodec, nullptr);

    // initialize default data
    this->m_context = avCodecContext;
    this->m_queue = new PacketQueue;

    this->initialize ();

    // initialize an SDL thread to read the file
    SDL_CreateThread (audio_read_thread, filename, this);
}

void CAudioStream::initialize () {
#if FF_API_FIFO_OLD_API
    // allocate the FIFO buffer
    this->m_queue->packetList = av_fifo_alloc2 (1, sizeof (MyAVPacketList), AV_FIFO_FLAG_AUTO_GROW);
#else
    this->m_queue->packetList = av_fifo_alloc (sizeof (MyAVPacketList));
#endif

    int64_t out_channel_layout;

    // set output audio channels based on the input audio channels
    switch (this->m_audioContext.getChannels ()) {
        case 1: out_channel_layout = AV_CH_LAYOUT_MONO; break;
        case 2: out_channel_layout = AV_CH_LAYOUT_STEREO; break;
        default: out_channel_layout = AV_CH_LAYOUT_SURROUND; break;
    }

#if FF_API_OLD_CHANNEL_LAYOUT
    av_channel_layout_from_mask (&this->m_out_channel_layout, out_channel_layout);

    swr_alloc_set_opts2 (&this->m_swrctx, &this->m_out_channel_layout, this->m_audioContext.getFormat (),
                         this->m_audioContext.getSampleRate (), &this->m_context->ch_layout,
                         this->m_context->sample_fmt, this->m_context->sample_rate, 0, nullptr);
#else
    // initialize swrctx
    this->m_swrctx = swr_alloc_set_opts (nullptr, out_channel_layout, this->m_audioContext.getFormat (),
                                         this->m_audioContext.getSampleRate (), this->getContext ()->channel_layout,
                                         this->getContext ()->sample_fmt, this->getContext ()->sample_rate, 0, nullptr);
#endif
    if (this->m_swrctx == nullptr)
        sLog.exception ("Cannot initialize swrctx for audio resampling");

    // initialize the context
    if (swr_init (this->m_swrctx) < 0)
        sLog.exception ("Failed to initialize the resampling context.");

    // setup the queue information
    this->m_queue->mutex = SDL_CreateMutex ();
    this->m_queue->cond = SDL_CreateCond ();
    this->m_queue->wait = SDL_CreateCond ();

    this->m_initialized = true;
}

void CAudioStream::queuePacket (AVPacket* pkt) {
    // clone the packet
    AVPacket* clone = av_packet_alloc ();

    if (clone == nullptr) {
        av_packet_unref (clone);
        return;
    }

    av_packet_move_ref (clone, pkt);

    SDL_LockMutex (this->m_queue->mutex);
    const bool gotQueued = this->doQueue (clone);
    SDL_UnlockMutex (this->m_queue->mutex);

    if (!gotQueued)
        av_packet_free (&pkt);
}

bool CAudioStream::doQueue (AVPacket* pkt) {
    MyAVPacketList entry {pkt};

#if FF_API_FIFO_OLD_API
    // write the entry if possible
    if (av_fifo_write (this->m_queue->packetList, &entry, 1) < 0)
        return false;
#else
    if (av_fifo_space (this->m_queue->packetList) < sizeof (entry))
        if (av_fifo_grow (this->m_queue->packetList, sizeof (entry)) < 0)
            return false;

    av_fifo_generic_write (this->m_queue->packetList, &entry, sizeof (entry), nullptr);
#endif

    this->m_queue->nb_packets++;
    this->m_queue->size += entry.packet->size + sizeof (entry);
    this->m_queue->duration += entry.packet->duration;

    SDL_CondSignal (this->m_queue->cond);

    return true;
}

void CAudioStream::dequeuePacket (AVPacket* output) {
    MyAVPacketList entry;

    SDL_LockMutex (this->m_queue->mutex);

    while (this->m_audioContext.getApplicationContext ().state.general.keepRunning) {

#if FF_API_FIFO_OLD_API
        int ret = av_fifo_read (this->m_queue->packetList, &entry, 1);
#else
        int ret = -1;

        if (av_fifo_size (this->m_queue->packetList) >= sizeof (entry))
            ret = av_fifo_generic_read (this->m_queue->packetList, &entry, sizeof (entry), nullptr);
#endif

        // enough data available, read it
        if (ret >= 0) {
            this->m_queue->nb_packets--;
            this->m_queue->size -= entry.packet->size + sizeof (entry);
            this->m_queue->duration -= entry.packet->duration;

            // move the reference and free the old one
            av_packet_move_ref (output, entry.packet);
            av_packet_free (&entry.packet);
            break;
        }

        // make the thread wait if nothing was available
        SDL_CondWait (this->m_queue->cond, this->m_queue->mutex);
    }

    SDL_UnlockMutex (this->m_queue->mutex);
}

AVCodecContext* CAudioStream::getContext () {
    return this->m_context;
}

AVFormatContext* CAudioStream::getFormatContext () {
    return this->m_formatContext;
}

int CAudioStream::getAudioStream () {
    return this->m_audioStream;
}

bool CAudioStream::isInitialized () {
    return this->m_initialized;
}

void CAudioStream::setRepeat (bool newRepeat) {
    this->m_repeat = newRepeat;
}

bool CAudioStream::isRepeat () {
    return this->m_repeat;
}

const void* CAudioStream::getBuffer () {
    return this->m_buffer;
}

int CAudioStream::getLength () {
    return this->m_length;
}

int CAudioStream::getPosition () {
    return this->m_position;
}

void CAudioStream::setPosition (int current) {
    this->m_position = current;
}

SDL_cond* CAudioStream::getWaitCondition () {
    return this->m_queue->wait;
}

int CAudioStream::getQueueSize () {
    return this->m_queue->size;
}

int CAudioStream::getQueuePacketCount () {
    return this->m_queue->nb_packets;
}

AVRational CAudioStream::getTimeBase () {
    return this->m_formatContext->streams [this->m_audioStream]->time_base;
}

int64_t CAudioStream::getQueueDuration () {
    return this->m_queue->duration;
}

bool CAudioStream::isQueueEmpty () {
    return this->m_queue->nb_packets == 0;
}

SDL_mutex* CAudioStream::getMutex () {
    return this->m_queue->mutex;
}

void CAudioStream::stop () {
    if (!this->isInitialized ())
        return;

    // stop the threads running
    this->m_initialized = false;
}

int CAudioStream::resampleAudio (const AVFrame* decoded_audio_frame, uint8_t* out_buf) {
    int out_linesize = 0;
    int ret;
    int out_nb_channels;
    int out_nb_samples;
    uint8_t** resampled_data = nullptr;
    int resampled_data_size;

    // retrieve number of audio samples (per channel)
    const int in_nb_samples = decoded_audio_frame->nb_samples;
    if (in_nb_samples <= 0) {
        sLog.error ("in_nb_samples error.");
        return -1;
    }

    int max_out_nb_samples = out_nb_samples = av_rescale_rnd (in_nb_samples, this->m_audioContext.getSampleRate (),
                                                              this->getContext ()->sample_rate, AV_ROUND_UP);

    // check rescaling was successful
    if (max_out_nb_samples <= 0) {
        sLog.error ("av_rescale_rnd error.");
        return -1;
    }

    // get number of output audio channels
#if FF_API_OLD_CHANNEL_LAYOUT
    out_nb_channels = this->getContext ()->ch_layout.nb_channels;
#else
    int64_t out_channel_layout;

    // set output audio channels based on the input audio channels
    switch (this->m_audioContext.getChannels ()) {
        case 1: out_channel_layout = AV_CH_LAYOUT_MONO; break;
        case 2: out_channel_layout = AV_CH_LAYOUT_STEREO; break;
        default: out_channel_layout = AV_CH_LAYOUT_SURROUND; break;
    }

    out_nb_channels = av_get_channel_layout_nb_channels (out_channel_layout);
#endif
    ret = av_samples_alloc_array_and_samples (&resampled_data, &out_linesize, out_nb_channels, out_nb_samples,
                                              this->m_audioContext.getFormat (), 0);

    if (ret < 0) {
        sLog.error ("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.");
        return -1;
    }

    // retrieve output samples number taking into account the progressive delay
    out_nb_samples =
        av_rescale_rnd (swr_get_delay (this->m_swrctx, this->getContext ()->sample_rate) + in_nb_samples,
                        this->m_audioContext.getSampleRate (), this->getContext ()->sample_rate, AV_ROUND_UP);

    // check output samples number was correctly retrieved
    if (out_nb_samples <= 0) {
        sLog.error ("av_rescale_rnd error");
        return -1;
    }

    if (out_nb_samples > max_out_nb_samples) {
        // free memory block and set pointer to NULL
        av_free (resampled_data [0]);

        // Allocate a samples buffer for out_nb_samples samples
        ret = av_samples_alloc (resampled_data, &out_linesize, out_nb_channels, out_nb_samples,
                                this->m_audioContext.getFormat (), 1);

        // check samples buffer correctly allocated
        if (ret < 0) {
            sLog.error ("av_samples_alloc failed.");
            return -1;
        }

        max_out_nb_samples = out_nb_samples;
    }

    // do the actual audio data resampling
    ret = swr_convert (this->m_swrctx, resampled_data, max_out_nb_samples,
                       const_cast<const uint8_t**> (decoded_audio_frame->data), decoded_audio_frame->nb_samples);

    // check audio conversion was successful
    if (ret < 0) {
        sLog.error ("swr_convert_error.");
        return -1;
    }

    // Get the required buffer size for the given audio parameters
    resampled_data_size =
        av_samples_get_buffer_size (&out_linesize, out_nb_channels, ret, this->m_audioContext.getFormat (), 1);

    // check audio buffer size
    if (resampled_data_size < 0) {
        sLog.error ("av_samples_get_buffer_size error.");
        return -1;
    }

    // copy the resampled data to the output buffer
    memcpy (out_buf, resampled_data [0], resampled_data_size);

    /*
     * Memory Cleanup.
     */
    if (resampled_data) {
        // free memory block and set pointer to NULL
        av_freep (&resampled_data [0]);
    }

    av_freep (&resampled_data);
    resampled_data = nullptr;

    return resampled_data_size;
}

int CAudioStream::decodeFrame (uint8_t* audioBuffer, int bufferSize) {
    AVPacket* pkt = av_packet_alloc ();
    static uint8_t* audio_pkt_data = nullptr;
    static int audio_pkt_size = 0;

    int len1, data_size;

    // allocate a new frame, used to decode audio packets
    static AVFrame* avFrame = nullptr;
    avFrame = av_frame_alloc ();
    if (!avFrame) {
        sLog.error ("Could not allocate AVFrame.\n");
        return -1;
    }

    // block until there's any data in the buffers
    while (this->m_audioContext.getApplicationContext ().state.general.keepRunning) {
        while (audio_pkt_size > 0) {
            int got_frame = 0;
            int ret = avcodec_receive_frame (this->getContext (), avFrame);

            if (ret == 0)
                got_frame = 1;
            if (ret == AVERROR (EAGAIN))
                ret = 0;
            if (ret == 0)
                ret = avcodec_send_packet (this->getContext (), pkt);
            if (ret < 0 && ret != AVERROR (EAGAIN))
                return -1;

            len1 = pkt->size;

            if (len1 < 0) {
                // if error, skip frame
                audio_pkt_size = 0;
                break;
            }

            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;

            if (got_frame) {
                // audio resampling
                data_size = this->resampleAudio (avFrame, audioBuffer);
                assert (data_size <= bufferSize);
            }
            if (data_size <= 0) {
                // no data found, keep waiting
                continue;
            }
            // some data was found
            return data_size;
        }
        if (pkt->data)
            av_packet_unref (pkt);

        this->dequeuePacket (pkt);

        audio_pkt_data = pkt->data;
        audio_pkt_size = pkt->size;
    }

    return 0;
}

CAudioContext& CAudioStream::getAudioContext () const {
    return this->m_audioContext;
}