#include "CAudioStream.h"
#include <cassert>
#include <iostream>
#include <math.h>

extern int g_AudioVolume;
extern bool g_KeepRunning;

using namespace WallpaperEngine::Audio;

// callback for sdl to play our audio
void audio_callback (void* userdata, uint8_t* stream, int length)
{
    auto audioStream = static_cast <CAudioStream*> (userdata);
    int len1, audio_size;

    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while (length > 0 && g_KeepRunning)
    {
        if (audio_buf_index >= audio_buf_size)
        {
            /* We have already sent all our data; get more */
            audio_size = audioStream->decodeFrame (audio_buf, sizeof (audio_buf));

            if (audio_size < 0)
            {
                /* If error, output silence */
                audio_buf_size = 1024; // arbitrary?
                memset(audio_buf, 0, audio_buf_size);
            }
            else
            {
                audio_buf_size = audio_size;
            }

            audio_buf_index = 0;
        }

        len1 = audio_buf_size - audio_buf_index;

        if (len1 > length)
            len1 = length;

        // mix the audio so the volume is right
        SDL_MixAudio (stream, (uint8_t*) audio_buf + audio_buf_index, len1, g_AudioVolume);

        length -= len1;
        stream += len1;
        audio_buf_index += len1;

        if (audioStream->isInitialized () == false)
            break;
    }
}

int audio_read_thread (void* arg)
{
    CAudioStream* stream = static_cast <CAudioStream*> (arg);
    AVPacket* packet = av_packet_alloc ();
    int ret = 0;

    while (ret >= 0 && g_KeepRunning == true)
    {
        ret = av_read_frame (stream->getFormatContext (), packet);

        if (ret == AVERROR_EOF)
        {
            // seek to the beginning of the file again
            av_seek_frame (stream->getFormatContext (), stream->getAudioStream (), 0, AVSEEK_FLAG_FRAME);
            avcodec_flush_buffers (stream->getContext ());
            continue;
        }

        // TODO: PROPERLY IMPLEMENT THIS
        if (packet->stream_index == stream->getAudioStream ())
            stream->queuePacket (packet);
        else
            av_packet_unref (packet);

        if (stream->isInitialized () == false)
            break;
    }

    // stop the audio too just in case
    stream->stop ();

    return 0;
}

static int audio_read_data_callback (void* streamarg, uint8_t* buffer, int buffer_size)
{
    auto stream = static_cast <CAudioStream*> (streamarg);
    int left = stream->getLength () - stream->getPosition ();

    buffer_size = FFMIN (buffer_size, left);

    memcpy (buffer, (uint8_t*) stream->getBuffer () + stream->getPosition (), buffer_size);
    // update position
    stream->setPosition (stream->getPosition () + buffer_size);

    return buffer_size;
}

int64_t audio_seek_data_callback (void* streamarg, int64_t offset, int whence)
{
    auto stream = static_cast <CAudioStream*> (streamarg);

    if (whence & AVSEEK_SIZE)
        return stream->getLength ();

    switch (whence)
    {
        case SEEK_CUR:
            stream->setPosition (stream->getPosition () + offset);
            break;

        case SEEK_SET:
            stream->setPosition (offset);
            break;
    }

    return offset;
}

CAudioStream::CAudioStream (const std::string& filename)
{
    // do not do anything if sdl audio was not initialized
    if (SDL_WasInit (SDL_INIT_AUDIO) != SDL_INIT_AUDIO)
        return;

    this->loadCustomContent (filename.c_str ());
}

CAudioStream::CAudioStream (void* buffer, int length)
{
    // do not do anything if sdl audio was not initialized
    if (SDL_WasInit (SDL_INIT_AUDIO) != SDL_INIT_AUDIO)
        return;

    // setup a custom context first
    this->m_formatContext = avformat_alloc_context ();

    if (this->m_formatContext == nullptr)
        throw std::runtime_error ("Cannot allocate format context");

    this->m_buffer = buffer;
    this->m_length = length;
    this->m_position = 0;

    // setup custom io for it
    this->m_formatContext->pb = avio_alloc_context (
        static_cast <uint8_t*> (av_malloc (4096)),
        4096,
        0,
        static_cast <void*> (this),
        &audio_read_data_callback,
        nullptr,
        &audio_seek_data_callback
    );

    if (this->m_formatContext->pb == nullptr)
        throw std::runtime_error ("Cannot create avio context");

    // continue the normal load procedure
    this->loadCustomContent ();
}

CAudioStream::CAudioStream(AVCodecContext* context)
    : m_context (context),
    m_queue (new PacketQueue)
{
    // do not do anything if sdl audio was not initialized
    if (SDL_WasInit (SDL_INIT_AUDIO) != SDL_INIT_AUDIO)
        return;

    this->initialize ();
}

void CAudioStream::loadCustomContent (const char* filename)
{
    const AVCodec* aCodec = nullptr;
    AVCodecContext* avCodecContext = nullptr;

    if (avformat_open_input (&this->m_formatContext, filename, nullptr, nullptr) != 0)
        throw std::runtime_error ("Cannot open audio file");
    if (avformat_find_stream_info (this->m_formatContext, nullptr) < 0)
        throw std::runtime_error ("Cannot determine file format");

    // find the audio stream
    for (int i = 0; i< this->m_formatContext->nb_streams; i ++)
    {
        if (this->m_formatContext->streams [i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && this->m_audioStream < 0)
            this->m_audioStream = i;
    }

    if (this->m_audioStream == -1)
        throw std::runtime_error ("Cannot find audio stream in file");

    // get the decoder for it and alloc the required context
    aCodec = avcodec_find_decoder (this->m_formatContext->streams [this->m_audioStream]->codecpar->codec_id);

    if (aCodec == nullptr)
        throw std::runtime_error ("Cannot initialize audio decoder");

    // alocate context
    avCodecContext = avcodec_alloc_context3 (aCodec);

    if (avcodec_parameters_to_context (avCodecContext, this->m_formatContext->streams [this->m_audioStream]->codecpar) != 0)
        throw std::runtime_error ("Cannot initialize audio decoder parameters");

    // finally open
    avcodec_open2 (avCodecContext, aCodec, nullptr);

    // initialize default data
    this->m_context = avCodecContext;
    this->m_queue = new PacketQueue;

    this->initialize ();

    // initialize an SDL thread to read the file
    SDL_CreateThread (audio_read_thread, this);
}

void CAudioStream::initialize ()
{
    // allocate the FIFO buffer
    this->m_queue->packetList = av_fifo_alloc (sizeof (MyAVPacketList));

    // setup the queue information
    this->m_queue->mutex = SDL_CreateMutex ();
    this->m_queue->cond = SDL_CreateCond ();

    // take control of the audio device

    SDL_AudioSpec requestedSpec, finalSpec;

    // Set audio settings from codec info
    requestedSpec.freq = this->m_context->sample_rate;
    requestedSpec.format = AUDIO_S16SYS;
    requestedSpec.channels = this->m_context->channels;
    requestedSpec.silence = 0;
    requestedSpec.samples = SDL_AUDIO_BUFFER_SIZE;
    requestedSpec.callback = audio_callback;
    requestedSpec.userdata = this;

    if (SDL_OpenAudio (&requestedSpec, &finalSpec) < 0) {
        std::cerr << "SDL_OpenAudio: " << SDL_GetError () << std::endl;
        return;
    }

    SDL_PauseAudio (0);

    this->m_initialized = true;
}

void CAudioStream::queuePacket(AVPacket *pkt)
{
    // clone the packet
    AVPacket* clone = av_packet_alloc ();

    if (clone == nullptr)
    {
        av_packet_unref (clone);
        return;
    }

    av_packet_move_ref (clone, pkt);

    SDL_LockMutex (this->m_queue->mutex);
    bool gotQueued = this->doQueue (clone);
    SDL_UnlockMutex (this->m_queue->mutex);

    if (gotQueued == false)
        av_packet_free (&pkt);
}

bool CAudioStream::doQueue (AVPacket* pkt)
{
    MyAVPacketList entry;

    // ensure the FIFO has enough space to hold the new entry
    if (av_fifo_space (this->m_queue->packetList) < sizeof (entry))
    {
        if (av_fifo_grow (this->m_queue->packetList, sizeof (entry)) < 0)
        {
            return false;
        }
    }

    entry.packet = pkt;

    // write to the FIFO
    av_fifo_generic_write (this->m_queue->packetList, &entry, sizeof (entry), nullptr);

    this->m_queue->nb_packets ++;
    this->m_queue->size += entry.packet->size + sizeof (entry);
    this->m_queue->duration += entry.packet->duration;

    SDL_CondSignal (this->m_queue->cond);

    return true;
}

void CAudioStream::dequeuePacket (AVPacket* output)
{
    MyAVPacketList entry;

    SDL_LockMutex (this->m_queue->mutex);

    while (g_KeepRunning)
    {
        // enough data available, read it
        if (av_fifo_size (this->m_queue->packetList) >= sizeof (entry))
        {
            av_fifo_generic_read (this->m_queue->packetList, &entry, sizeof (entry), nullptr);

            this->m_queue->nb_packets --;
            this->m_queue->size -= entry.packet->duration;

            // move the reference and free the old one
            av_packet_move_ref (output, entry.packet);
            av_packet_free (&entry.packet);
            break;
        }

        // make the thread wait if nothing was available
        SDL_CondWaitTimeout (this->m_queue->cond, this->m_queue->mutex, 1000);
    }

    SDL_UnlockMutex (this->m_queue->mutex);
}

AVCodecContext* CAudioStream::getContext ()
{
    return this->m_context;
}

AVFormatContext* CAudioStream::getFormatContext ()
{
    return this->m_formatContext;
}

int CAudioStream::getAudioStream ()
{
    return this->m_audioStream;
}

bool CAudioStream::isInitialized ()
{
    return this->m_initialized;
}

void CAudioStream::setRepeat (bool newRepeat)
{
    this->m_repeat = newRepeat;
}

bool CAudioStream::isRepeat ()
{
    return this->m_repeat;
}

void* CAudioStream::getBuffer ()
{
    return this->m_buffer;
}

int CAudioStream::getLength ()
{
    return this->m_length;
}

int CAudioStream::getPosition ()
{
    return this->m_position;
}

void CAudioStream::setPosition (int current)
{
    this->m_position = current;
}

void CAudioStream::stop ()
{
    if (this->isInitialized () == false)
        return;

    // pause audio
    SDL_PauseAudio (1);

    // stop the threads running
    this->m_initialized = false;
}

int CAudioStream::resampleAudio (
    AVFrame * decoded_audio_frame,
    enum AVSampleFormat out_sample_fmt,
    int out_channels,
    int out_sample_rate,
    uint8_t * out_buf
)
{
    SwrContext * swr_ctx = NULL;
    int ret = 0;
    int64_t in_channel_layout = this->getContext ()->channel_layout;
    int64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    int out_nb_channels = 0;
    int out_linesize = 0;
    int in_nb_samples = 0;
    int out_nb_samples = 0;
    int max_out_nb_samples = 0;
    uint8_t ** resampled_data = NULL;
    int resampled_data_size = 0;

    swr_ctx = swr_alloc();

    if (!swr_ctx)
    {
        printf("swr_alloc error.\n");
        return -1;
    }

    // get input audio channels
    in_channel_layout = (this->getContext ()->channels ==
                         av_get_channel_layout_nb_channels(this->getContext ()->channel_layout)) ?   // 2
                            this->getContext ()->channel_layout :
                            av_get_default_channel_layout(this->getContext ()->channels);

    // check input audio channels correctly retrieved
    if (in_channel_layout <= 0)
    {
        printf("in_channel_layout error.\n");
        return -1;
    }

    // set output audio channels based on the input audio channels
    if (out_channels == 1)
    {
        out_channel_layout = AV_CH_LAYOUT_MONO;
    }
    else if (out_channels == 2)
    {
        out_channel_layout = AV_CH_LAYOUT_STEREO;
    }
    else
    {
        out_channel_layout = AV_CH_LAYOUT_SURROUND;
    }

    // retrieve number of audio samples (per channel)
    in_nb_samples = decoded_audio_frame->nb_samples;
    if (in_nb_samples <= 0)
    {
        printf("in_nb_samples error.\n");
        return -1;
    }

    // Set SwrContext parameters for resampling
    av_opt_set_int(   // 3
        swr_ctx,
        "in_channel_layout",
        in_channel_layout,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        swr_ctx,
        "in_sample_rate",
        this->getContext ()->sample_rate,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
        swr_ctx,
        "in_sample_fmt",
        this->getContext ()->sample_fmt,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        swr_ctx,
        "out_channel_layout",
        out_channel_layout,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        swr_ctx,
        "out_sample_rate",
        out_sample_rate,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
        swr_ctx,
        "out_sample_fmt",
        out_sample_fmt,
        0
    );

    // Once all values have been set for the SwrContext, it must be initialized
    // with swr_init().
    ret = swr_init(swr_ctx);;
    if (ret < 0)
    {
        printf("Failed to initialize the resampling context.\n");
        return -1;
    }

    max_out_nb_samples = out_nb_samples = av_rescale_rnd(
        in_nb_samples,
        out_sample_rate,
        this->getContext ()->sample_rate,
        AV_ROUND_UP
    );

    // check rescaling was successful
    if (max_out_nb_samples <= 0)
    {
        printf("av_rescale_rnd error.\n");
        return -1;
    }

    // get number of output audio channels
    out_nb_channels = av_get_channel_layout_nb_channels(out_channel_layout);

    ret = av_samples_alloc_array_and_samples(
        &resampled_data,
        &out_linesize,
        out_nb_channels,
        out_nb_samples,
        out_sample_fmt,
        0
    );

    if (ret < 0)
    {
        printf("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n");
        return -1;
    }

    // retrieve output samples number taking into account the progressive delay
    out_nb_samples = av_rescale_rnd(
        swr_get_delay(swr_ctx, this->getContext ()->sample_rate) + in_nb_samples,
        out_sample_rate,
        this->getContext ()->sample_rate,
        AV_ROUND_UP
    );

    // check output samples number was correctly retrieved
    if (out_nb_samples <= 0)
    {
        printf("av_rescale_rnd error\n");
        return -1;
    }

    if (out_nb_samples > max_out_nb_samples)
    {
        // free memory block and set pointer to NULL
        av_free(resampled_data[0]);

        // Allocate a samples buffer for out_nb_samples samples
        ret = av_samples_alloc(
            resampled_data,
            &out_linesize,
            out_nb_channels,
            out_nb_samples,
            out_sample_fmt,
            1
        );

        // check samples buffer correctly allocated
        if (ret < 0)
        {
            printf("av_samples_alloc failed.\n");
            return -1;
        }

        max_out_nb_samples = out_nb_samples;
    }

    if (swr_ctx)
    {
        // do the actual audio data resampling
        ret = swr_convert(
            swr_ctx,
            resampled_data,
            out_nb_samples,
            (const uint8_t **) decoded_audio_frame->data,
            decoded_audio_frame->nb_samples
        );

        // check audio conversion was successful
        if (ret < 0)
        {
            printf("swr_convert_error.\n");
            return -1;
        }

        // Get the required buffer size for the given audio parameters
        resampled_data_size = av_samples_get_buffer_size(
            &out_linesize,
            out_nb_channels,
            ret,
            out_sample_fmt,
            1
        );

        // check audio buffer size
        if (resampled_data_size < 0)
        {
            printf("av_samples_get_buffer_size error.\n");
            return -1;
        }
    }
    else
    {
        printf("swr_ctx null error.\n");
        return -1;
    }

    // copy the resampled data to the output buffer
    memcpy(out_buf, resampled_data[0], resampled_data_size);

    /*
     * Memory Cleanup.
     */
    if (resampled_data)
    {
        // free memory block and set pointer to NULL
        av_freep(&resampled_data[0]);
    }

    av_freep(&resampled_data);
    resampled_data = NULL;

    if (swr_ctx)
    {
        // Free the given SwrContext and set the pointer to NULL
        swr_free(&swr_ctx);
    }

    return resampled_data_size;
}

int CAudioStream::decodeFrame (uint8_t* audioBuffer, int bufferSize)
{
    AVPacket *pkt = av_packet_alloc ();
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;

    int len1, data_size = 0;

    // allocate a new frame, used to decode audio packets
    static AVFrame * avFrame = NULL;
    avFrame = av_frame_alloc();
    if (!avFrame)
    {
        printf("Could not allocate AVFrame.\n");
        return -1;
    }

    for (; g_KeepRunning;) {
        while (audio_pkt_size > 0) {
            int got_frame = 0;
            int ret = avcodec_receive_frame(this->getContext (), avFrame);

            if (ret == 0)
            {
                got_frame = 1;
            }
            if (ret == AVERROR(EAGAIN))
            {
                ret = 0;
            }
            if (ret == 0)
            {
                ret = avcodec_send_packet(this->getContext (), pkt);
            }
            if (ret == AVERROR(EAGAIN))
            {
                ret = 0;
            }
            else if (ret < 0)
            {
                return -1;
            }
            else
            {
                len1 = pkt->size;
            }

            if (len1 < 0)
            {
                // if error, skip frame
                audio_pkt_size = 0;
                break;
            }

            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;

            if (got_frame) {
                // audio resampling
                data_size = this->resampleAudio (
                    avFrame,
                    AV_SAMPLE_FMT_S16,
                    this->getContext ()->channels,
                    this->getContext ()->sample_rate,
                    audioBuffer
                );
                assert(data_size <= bufferSize);
            }
            if (data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if (pkt->data)
            av_packet_unref(pkt);

        this->dequeuePacket (pkt);

        audio_pkt_data = pkt->data;
        audio_pkt_size = pkt->size;
    }

    return 0;
}