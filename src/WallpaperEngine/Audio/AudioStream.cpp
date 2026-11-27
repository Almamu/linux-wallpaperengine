#include "AudioStream.h"
#include "WallpaperEngine/Logging/Log.h"
#include <cassert>
#include <cmath>
#include <iostream>

// maximum size of the queue to prevent reading too much data

using namespace WallpaperEngine::Audio;

int audio_read_thread (void* arg) {
    auto* stream = static_cast<AudioStream*> (arg);
    SDL_mutex* waitMutex = SDL_CreateMutex ();
    if (waitMutex == nullptr) {
	sLog.error ("Cannot create mutex for audio playback waiting");
	stream->queueDecoderFlush ();
	return -1;
    }
    AVPacket* packet = av_packet_alloc ();
    if (packet == nullptr) {
	SDL_DestroyMutex (waitMutex);
	sLog.error ("Cannot allocate packet for audio playback");
	stream->queueDecoderFlush ();
	return -1;
    }

    int ret = 0;
    while (ret >= 0 && stream->getAudioContext ().getApplicationContext ().state.general.keepRunning
	   && stream->isInitialized ()) {
	// give the cpu some time to play the queued frames if there's enough info there
	if (stream->getQueueSize () >= MAX_QUEUE_SIZE
	    || (stream->getQueuePacketCount () > MIN_FRAMES
		&& (av_q2d (stream->getTimeBase ()) * stream->getQueueDuration () > 1.0))) {
	    SDL_LockMutex (waitMutex);
	    SDL_CondWaitTimeout (stream->getWaitCondition (), waitMutex, 10);
	    SDL_UnlockMutex (waitMutex);
	    continue;
	}

	ret = av_read_frame (stream->getFormatContext (), packet);

	if (ret == AVERROR_EOF) {
	    // a full pass finished — the playback coordinator uses this to rotate soundtracks
	    stream->notifyCompletion ();

	    if (stream->isRepeat ()) {
		// The demuxer is owned by this thread, so it can seek immediately. Queue the
		// decoder reset after every packet from the old pass and before any packet
		// from the new pass. The SDL callback is the sole AVCodecContext owner.
		const int seekResult = avformat_seek_file (
		    stream->getFormatContext (), stream->getAudioStream (), 0, 0, 0, ~AVSEEK_FLAG_FRAME
		);
		if (seekResult < 0) {
		    sLog.error ("Cannot seek repeating audio stream: ", av_err2str (seekResult));
		    break;
		}

		stream->queueDecoderFlush ();
		ret = 0;
	    } else {
		// Let the decoder callback drain the queued tail before resetting. This also
		// keeps completion ordering deterministic for soundtrack rotation.
		stream->queueDecoderFlush ();
	    }

	    continue;
	}

	if (ret < 0) {
	    sLog.error ("Cannot read audio packet: ", av_err2str (ret));
	    // A corrupt/truncated tail still ends this pass. Drain its valid prefix
	    // so the layer can complete instead of waiting forever for more packets.
	    stream->queueDecoderFlush ();
	    break;
	}

	// TODO: PROPERLY IMPLEMENT THIS
	if (packet->stream_index == stream->getAudioStream ()) {
	    stream->queuePacket (packet);
	} else {
	    av_packet_unref (packet);
	}
    }

    // stop the audio too just in case
    av_packet_free (&packet);
    SDL_DestroyMutex (waitMutex);

    return 0;
}

static int audio_read_data_callback (void* streamarg, uint8_t* buffer, int buffer_size) {
    const auto stream = static_cast<AudioStream*> (streamarg);

    // check if we're at eof and return the right value
    if (stream->getBuffer ()->eof ()) {
	return AVERROR_EOF;
    }

    stream->getBuffer ()->read (reinterpret_cast<std::istream::char_type*> (buffer), buffer_size);

    if (stream->getBuffer ()->fail () && !stream->getBuffer ()->eof ()) {
	return AVERROR_INVALIDDATA;
    }

    // return read bytes only
    return stream->getBuffer ()->gcount ();
}

int64_t audio_seek_data_callback (void* streamarg, int64_t offset, int whence) {
    const auto stream = static_cast<AudioStream*> (streamarg);

    // reset error state
    stream->getBuffer ()->clear ();

    if (whence & AVSEEK_SIZE) {
	const auto current = stream->getBuffer ()->tellg ();
	stream->getBuffer ()->seekg (0, std::ios_base::end);
	const auto end = stream->getBuffer ()->tellg ();
	stream->getBuffer ()->seekg (current, std::ios_base::beg);
	return end;
    }

    switch (whence & ~AVSEEK_FORCE) {
	case SEEK_CUR:
	    stream->getBuffer ()->seekg (offset, std::ios_base::cur);
	    break;
	case SEEK_SET:
	    stream->getBuffer ()->seekg (offset, std::ios_base::beg);
	    break;
	case SEEK_END:
	    stream->getBuffer ()->seekg (offset, std::ios_base::end);
	    break;
	default:
	    return AVERROR (EINVAL);
    }

    const auto position = stream->getBuffer ()->tellg ();
    return position == std::streampos (-1) ? AVERROR (EIO) : static_cast<int64_t> (position);
}

AudioStream::AudioStream (AudioContext& context, const std::string& filename, const bool repeat) :
    m_audioContext (context), m_repeat (repeat) {
    try {
	this->loadCustomContent (filename.c_str ());
    } catch (...) {
	this->release ();
	throw;
    }
}

AudioStream::AudioStream (AudioContext& context, const ReadStreamSharedPtr& buffer, const bool repeat, const bool spatial) :
    m_spatial (spatial), m_audioContext (context), m_repeat (repeat) {
    try {
	// setup a custom context first
	this->m_formatContext = avformat_alloc_context ();

	if (this->m_formatContext == nullptr) {
	    sLog.exception ("Cannot allocate ffmpeg format context");
	}

	this->m_buffer = buffer;

	// setup custom io for it
	auto* ioBuffer = static_cast<uint8_t*> (av_malloc (4096));
	if (ioBuffer == nullptr) {
	    sLog.exception ("Cannot allocate audio IO buffer");
	}
	this->m_customIO = avio_alloc_context (
	    ioBuffer, 4096, 0, this, &audio_read_data_callback, nullptr, &audio_seek_data_callback
	);

	if (this->m_customIO == nullptr) {
	    av_free (ioBuffer);
	    sLog.exception ("Cannot create avio context");
	}
	this->m_formatContext->pb = this->m_customIO;
	this->m_formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;

	// continue the normal load procedure
	this->loadCustomContent ();
    } catch (...) {
	this->release ();
	throw;
    }
}

AudioStream::AudioStream (AudioContext& audioContext, AVCodecContext* context) :
    m_audioContext (audioContext), m_context (context) {
    try {
	this->m_queue = new PacketQueue;
	this->initialize ();
    } catch (...) {
	this->release ();
	throw;
    }
}

AudioStream::~AudioStream () { this->release (); }

void AudioStream::release () {
    // stop the audio
    this->stop ();

    if (this->m_audioThread != nullptr) {
	// wait for the thread to finish
	SDL_WaitThread (this->m_audioThread, nullptr);
    }

    this->m_audioThread = nullptr;

    // The FIFO owns individually allocated, reference-counted AVPackets. Freeing
    // only the FIFO storage leaks every packet still buffered at wallpaper teardown
    // (up to MAX_QUEUE_SIZE per sound layer). Audio-heavy scenes can have dozens of
    // layers, so repeated switches otherwise retain hundreds of MB per pass.
    while (this->m_queue != nullptr && this->m_queue->mutex != nullptr && this->m_queue->packetList != nullptr
	   && this->m_decodePacket != nullptr && this->dequeuePacket ()) {
	av_packet_unref (this->m_decodePacket);
    }

    if (this->m_swrctx != nullptr && swr_is_initialized (this->m_swrctx) == true) {
	swr_close (this->m_swrctx);
    }
    if (this->m_swrctx != nullptr) {
	swr_free (&this->m_swrctx);
    }
    if (this->m_decodePacket != nullptr) {
	av_packet_free (&this->m_decodePacket);
    }
    if (this->m_decodeFrame != nullptr) {
	av_frame_free (&this->m_decodeFrame);
    }
    if (this->m_queue != nullptr && this->m_queue->packetList != nullptr) {
#if FF_API_FIFO_OLD_API
	av_fifo_free (this->m_queue->packetList);
	this->m_queue->packetList = nullptr;
#else
	av_fifo_freep2 (&this->m_queue->packetList);
#endif /* FF_API_FIFO_OLD_API */
    }

    if (this->m_queue != nullptr) {
	SDL_DestroyCond (this->m_queue->cond);
	SDL_DestroyCond (this->m_queue->wait);
	SDL_DestroyMutex (this->m_queue->mutex);
    }

    delete this->m_queue;
    this->m_queue = nullptr;

    if (this->m_formatContext != nullptr) {
	avformat_close_input (&this->m_formatContext);
    }
    // Custom AVIO stays caller-owned, including after a failed open.
    // FFmpeg may replace its buffer while probing, so free the current buffer.
    if (this->m_customIO != nullptr) {
	av_freep (&this->m_customIO->buffer);
	avio_context_free (&this->m_customIO);
    }

    if (this->m_context != nullptr) {
	avcodec_free_context (&this->m_context);
    }
}

void AudioStream::loadCustomContent (const char* filename) {
    if (avformat_open_input (&this->m_formatContext, filename, nullptr, nullptr) != 0) {
	sLog.exception ("Cannot open audio file: ", filename);
    }
    if (avformat_find_stream_info (this->m_formatContext, nullptr) < 0) {
	sLog.exception ("Cannot determine file format: ", filename);
    }

    // find the audio stream
    for (unsigned int i = 0; i < this->m_formatContext->nb_streams; i++) {
	if (this->m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
	    && this->m_audioStream == NO_AUDIO_STREAM) {
	    this->m_audioStream = i;
	}
    }

    if (this->m_audioStream == NO_AUDIO_STREAM) {
	sLog.exception ("Cannot find an audio stream in file ", filename);
    }

    // get the decoder for it and alloc the required context
    const AVCodec* aCodec
	= avcodec_find_decoder (this->m_formatContext->streams[this->m_audioStream]->codecpar->codec_id);

    if (aCodec == nullptr) {
	sLog.exception ("Cannot initialize audio decoder for file: ", filename);
    }

    // alocate context
    this->m_context = avcodec_alloc_context3 (aCodec);
    if (this->m_context == nullptr) {
	sLog.exception ("Cannot allocate audio decoder context");
    }

    if (avcodec_parameters_to_context (this->m_context, this->m_formatContext->streams[this->m_audioStream]->codecpar)
	!= 0) {
	sLog.exception ("Cannot initialize audio decoder parameters");
    }

    // finally open
    if (avcodec_open2 (this->m_context, aCodec, nullptr) < 0) {
	sLog.exception ("Cannot open audio decoder");
    }

    // initialize default data
    this->m_queue = new PacketQueue;

    this->initialize ();

    // initialize an SDL thread to read the file
    this->m_audioThread = SDL_CreateThread (audio_read_thread, filename, this);
    if (this->m_audioThread == nullptr) {
	sLog.exception ("Cannot create audio playback reader: ", SDL_GetError ());
    }
}

void AudioStream::initialize () {
// allocate the FIFO buffer
#if FF_API_FIFO_OLD_API
    this->m_queue->packetList = av_fifo_alloc (sizeof (MyAVPacketList));
#else
    this->m_queue->packetList = av_fifo_alloc2 (1, sizeof (MyAVPacketList), AV_FIFO_FLAG_AUTO_GROW);
#endif
    if (this->m_queue->packetList == nullptr) {
	sLog.exception ("Cannot allocate audio packet queue");
    }

    this->configureResampler ();

    // setup the queue information
    this->m_queue->mutex = SDL_CreateMutex ();
    this->m_queue->cond = SDL_CreateCond ();
    this->m_queue->wait = SDL_CreateCond ();
    if (this->m_queue->mutex == nullptr || this->m_queue->cond == nullptr || this->m_queue->wait == nullptr) {
	sLog.exception ("Cannot initialize audio packet queue synchronization: ", SDL_GetError ());
    }

    this->m_decodeFrame = av_frame_alloc ();
    this->m_decodePacket = av_packet_alloc ();

    if (!this->m_decodeFrame) {
	sLog.exception ("Could not allocate AVFrame.\n");
    }
    if (!this->m_decodePacket) {
	sLog.exception ("Could not allocate AVPacket.\n");
    }

    this->m_initialized = true;
}

void AudioStream::useLegacyOutput () {
    if (!this->m_spatial) return;
    this->m_spatial = false;
    swr_free (&this->m_swrctx);
    this->configureResampler ();
}

void AudioStream::configureResampler () {
#if FF_API_OLD_CHANNEL_LAYOUT
    const int inputChannels = this->m_context->channels;
    const uint64_t inputMask = this->m_context->channel_layout ? this->m_context->channel_layout
        : inputChannels == 1 ? AV_CH_LAYOUT_MONO : inputChannels == 2 ? AV_CH_LAYOUT_STEREO : 0;
#else
    const int inputChannels = this->m_context->ch_layout.nb_channels;
    // Ordinary PCM WAVs may omit layout metadata. Mono/stereo still have an
    // unambiguous order; do not guess custom or unspecified multichannel order.
    const uint64_t inputMask = this->m_context->ch_layout.order == AV_CHANNEL_ORDER_NATIVE
        ? this->m_context->ch_layout.u.mask
        : this->m_context->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC
            ? inputChannels == 1 ? AV_CH_LAYOUT_MONO : inputChannels == 2 ? AV_CH_LAYOUT_STEREO : 0
            : 0;
#endif
    // These interleaved layouts have the same channel order as OpenAL1.21.1.
    // Unsupported layouts retain the existing SDL downmix rather than being
    // mislabeled as a different collection of virtual speakers.
    this->m_spatial = this->m_spatial && (inputMask == AV_CH_LAYOUT_MONO || inputMask == AV_CH_LAYOUT_STEREO
        || inputMask == AV_CH_LAYOUT_QUAD || inputMask == AV_CH_LAYOUT_5POINT1
        || inputMask == AV_CH_LAYOUT_5POINT1_BACK || inputMask == AV_CH_LAYOUT_6POINT1
        || inputMask == AV_CH_LAYOUT_7POINT1);
    this->m_outputChannels = this->m_spatial ? inputChannels : this->m_audioContext.getChannels ();
    // Native alBufferData receives the decoder rate; OpenAL owns source-rate conversion.
    this->m_outputSampleRate = this->m_spatial ? this->m_context->sample_rate : this->m_audioContext.getSampleRate ();
    this->m_outputFormat = this->m_spatial ? AV_SAMPLE_FMT_FLT : this->m_audioContext.getFormat ();
    const uint64_t outputMask = this->m_spatial ? inputMask
        : this->m_outputChannels == 1 ? AV_CH_LAYOUT_MONO
        : this->m_outputChannels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_SURROUND;
#if FF_API_OLD_CHANNEL_LAYOUT
    this->m_swrctx = swr_alloc_set_opts (
        nullptr, outputMask, this->m_outputFormat, this->m_outputSampleRate,
        inputMask, this->m_context->sample_fmt, this->m_context->sample_rate, 0, nullptr
    );
#else
    AVChannelLayout outputLayout {};
    if (av_channel_layout_from_mask (&outputLayout, outputMask) != 0) {
        sLog.exception ("Cannot get channel layout from mask");
    }
    const int result = swr_alloc_set_opts2 (
        &this->m_swrctx, &outputLayout, this->m_outputFormat, this->m_outputSampleRate,
        &this->m_context->ch_layout, this->m_context->sample_fmt, this->m_context->sample_rate, 0, nullptr
    );
    av_channel_layout_uninit (&outputLayout);
    if (result < 0) sLog.exception ("Cannot initialize audio conversion");
#endif

    if (this->m_swrctx == nullptr) {
	sLog.exception ("Cannot initialize swrctx for audio resampling");
    }

    // initialize the context
    if (swr_init (this->m_swrctx) < 0) {
	sLog.exception ("Failed to initialize the resampling context.");
    }

}

void AudioStream::queuePacket (AVPacket* pkt) {
    // clone the packet
    AVPacket* clone = av_packet_alloc ();

    if (clone == nullptr) {
	av_packet_unref (pkt);
	return;
    }

    av_packet_move_ref (clone, pkt);

    SDL_LockMutex (this->m_queue->mutex);
    const bool gotQueued = this->doQueue (clone);
    SDL_UnlockMutex (this->m_queue->mutex);

    if (!gotQueued) {
	av_packet_free (&clone);
    }
}

void AudioStream::queueDecoderFlush () {
    SDL_LockMutex (this->m_queue->mutex);
    const bool gotQueued = this->doQueue (nullptr, true);
    SDL_UnlockMutex (this->m_queue->mutex);

    if (!gotQueued) {
	sLog.error ("Cannot queue audio decoder flush");
    }
}

bool AudioStream::doQueue (AVPacket* pkt, const bool flushDecoder) {
    MyAVPacketList entry { .packet = pkt, .flushDecoder = flushDecoder };

#if FF_API_FIFO_OLD_API
    if (av_fifo_space (this->m_queue->packetList) < static_cast<int> (sizeof (entry))) {
	if (av_fifo_grow (this->m_queue->packetList, sizeof (entry)) < 0) {
	    return false;
	}
    }

    av_fifo_generic_write (this->m_queue->packetList, &entry, sizeof (entry), nullptr);
#else
    // write the entry if possible
    if (av_fifo_write (this->m_queue->packetList, &entry, 1) < 0) {
	return false;
    }
#endif

    this->m_queue->nb_packets.fetch_add (1, std::memory_order_relaxed);
    this->m_queue->size.fetch_add ((entry.packet != nullptr ? entry.packet->size : 0) + sizeof (entry),
				   std::memory_order_relaxed);
    this->m_queue->duration.fetch_add (entry.packet != nullptr ? entry.packet->duration : 0, std::memory_order_relaxed);

    SDL_CondSignal (this->m_queue->cond);

    return true;
}

bool AudioStream::dequeuePacket () {
    MyAVPacketList entry {};
    bool dequeued = false;
    this->m_decoderFlushPending = false;

    SDL_LockMutex (this->m_queue->mutex);

#if FF_API_FIFO_OLD_API
    int ret = -1;

    if (av_fifo_size (this->m_queue->packetList) >= static_cast<int> (sizeof (entry))) {
	ret = av_fifo_generic_read (this->m_queue->packetList, &entry, sizeof (entry), nullptr);
    }
#else
    const int ret = av_fifo_read (this->m_queue->packetList, &entry, 1);
#endif

    if (ret >= 0) {
	this->m_queue->nb_packets.fetch_sub (1, std::memory_order_relaxed);
	this->m_queue->size.fetch_sub ((entry.packet != nullptr ? entry.packet->size : 0) + sizeof (entry),
				       std::memory_order_relaxed);
	this->m_queue->duration.fetch_sub (entry.packet != nullptr ? entry.packet->duration : 0, std::memory_order_relaxed);

	if (entry.flushDecoder) {
	    this->m_decoderFlushPending = true;
	} else if (entry.packet != nullptr) {
	    av_packet_unref (this->m_decodePacket);
	    av_packet_move_ref (this->m_decodePacket, entry.packet);
	    av_packet_free (&entry.packet);
	}
	dequeued = true;
    }

    SDL_UnlockMutex (this->m_queue->mutex);

    return dequeued;
}

AVCodecContext* AudioStream::getContext () const { return this->m_context; }

AVFormatContext* AudioStream::getFormatContext () const { return this->m_formatContext; }

int AudioStream::getAudioStream () const { return this->m_audioStream; }

bool AudioStream::isInitialized () const { return this->m_initialized; }

void AudioStream::setRepeat (const bool newRepeat) { this->m_repeat = newRepeat; }

bool AudioStream::isRepeat () const { return this->m_repeat; }

double AudioStream::getDuration () const {
    if (this->m_formatContext == nullptr) {
	return 0.0;
    }
    if (this->m_audioStream != NO_AUDIO_STREAM) {
	const auto* stream = this->m_formatContext->streams[this->m_audioStream];
	if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
	    return stream->duration * av_q2d (stream->time_base);
	}
    }
    return this->m_formatContext->duration > 0 ? static_cast<double> (this->m_formatContext->duration) / AV_TIME_BASE
					       : 0.0;
}

ReadStreamSharedPtr& AudioStream::getBuffer () { return this->m_buffer; }

SDL_cond* AudioStream::getWaitCondition () const { return this->m_queue->wait; }

size_t AudioStream::getQueueSize () const { return this->m_queue->size.load (std::memory_order_relaxed); }

int AudioStream::getQueuePacketCount () const { return this->m_queue->nb_packets.load (std::memory_order_relaxed); }

AVRational AudioStream::getTimeBase () const {
    if (this->m_audioStream == NO_AUDIO_STREAM) {
	return { 0, 0 };
    }

    return this->m_formatContext->streams[this->m_audioStream]->time_base;
}

int64_t AudioStream::getQueueDuration () const { return this->m_queue->duration.load (std::memory_order_relaxed); }

bool AudioStream::isQueueEmpty () const { return this->getQueuePacketCount () == 0; }

SDL_mutex* AudioStream::getMutex () const { return this->m_queue->mutex; }

void AudioStream::stop () {
    if (!this->m_initialized.exchange (false)) {
	return;
    }

    // Wake both the decoder callback and the file-reader thread. In particular,
    // wallpaper teardown must not wait forever for an audio callback which is
    // blocked on an empty packet queue.
    if (this->m_queue != nullptr) {
	SDL_LockMutex (this->m_queue->mutex);
	SDL_CondBroadcast (this->m_queue->cond);
	SDL_CondBroadcast (this->m_queue->wait);
	SDL_UnlockMutex (this->m_queue->mutex);
    }
}

int AudioStream::resampleAudio (uint8_t* out_buf, const int out_size) {
    int out_linesize = 0;
    int ret = 0;
    uint8_t** resampled_data = nullptr;

    if (out_buf == nullptr || out_size <= 0) {
	return -1;
    }

    // retrieve number of audio samples (per channel)
    const int in_nb_samples = this->m_decodeFrame->nb_samples;
    if (in_nb_samples <= 0) {
	sLog.error ("in_nb_samples error.");
	return -1;
    }

    // retrieve output samples number taking into account the progressive delay
    const int out_nb_samples = av_rescale_rnd (
	swr_get_delay (this->m_swrctx, this->getContext ()->sample_rate) + in_nb_samples,
	this->m_outputSampleRate, this->getContext ()->sample_rate, AV_ROUND_UP
    );

    // check output samples number was correctly retrieved
    if (out_nb_samples <= 0) {
	sLog.error ("av_rescale_rnd error");
	return -1;
    }

    // Allocate for the configured output layout, including preserved sound channels. Allocating with the input
    // channel count under-allocated mono sources while swr wrote stereo data,
    // corrupting the heap and eventually tripping the stack protector.
    const int out_nb_channels = this->m_outputChannels;
    ret = av_samples_alloc_array_and_samples (
	&resampled_data, &out_linesize, out_nb_channels, out_nb_samples, this->m_outputFormat, 1
    );

    if (ret < 0) {
	sLog.error ("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.");
	return -1;
    }

    const auto freeResampledData = [&resampled_data] () {
	if (resampled_data != nullptr) {
	    av_freep (&resampled_data[0]);
	    av_freep (&resampled_data);
	}
    };

    // do the actual audio data resampling
    ret = swr_convert (
	this->m_swrctx, resampled_data, out_nb_samples, const_cast<const uint8_t**> (this->m_decodeFrame->data),
	this->m_decodeFrame->nb_samples
    );

    // check audio conversion was successful
    if (ret < 0) {
	sLog.error ("swr_convert_error.");
	freeResampledData ();
	return -1;
    }

    // Get the required buffer size for the given audio parameters
    const int resampled_data_size
	= av_samples_get_buffer_size (&out_linesize, out_nb_channels, ret, this->m_outputFormat, 1);

    // check audio buffer size
    if (resampled_data_size < 0) {
	sLog.error ("av_samples_get_buffer_size error.");
	freeResampledData ();
	return -1;
    }

    // copy the resampled data to the output buffer up to out_size bytes
    const int frameBytes = out_nb_channels * av_get_bytes_per_sample (this->m_outputFormat);
    const int copied_size = std::min (resampled_data_size, out_size - out_size % frameBytes);
    memcpy (out_buf, resampled_data[0], copied_size);
    if (copied_size < resampled_data_size) {
        this->m_resampledPending.assign (resampled_data[0] + copied_size, resampled_data[0] + resampled_data_size);
        this->m_resampledIndex = 0;
    }
    freeResampledData ();

    // The caller uses this as the readable byte count for its fixed-size
    // buffer, so returning the pre-clamp size would make it read past the end.
    return copied_size;
}

int AudioStream::drainResampler (uint8_t* out_buf, const int out_size) {
    const int bytesPerFrame
	= av_get_bytes_per_sample (this->m_outputFormat) * this->m_outputChannels;
    if (bytesPerFrame <= 0 || out_size < bytesPerFrame) {
	return 0;
    }
    uint8_t* output[] = { out_buf };
    const int samples = swr_convert (this->m_swrctx, output, out_size / bytesPerFrame, nullptr, 0);
    return samples > 0 ? samples * bytesPerFrame : 0;
}

int AudioStream::decodeFrame (uint8_t* audioBuffer, const int bufferSize) {
    const int frameBytes = this->m_outputChannels * av_get_bytes_per_sample (this->m_outputFormat);
    if (frameBytes <= 0 || bufferSize < frameBytes) return 0;
    if (!this->m_resampledPending.empty ()) {
        const auto count = std::min (size_t (bufferSize - bufferSize % frameBytes), this->m_resampledPending.size () - this->m_resampledIndex);
        std::memcpy (audioBuffer, this->m_resampledPending.data () + this->m_resampledIndex, count);
        this->m_resampledIndex += count;
        if (this->m_resampledIndex == this->m_resampledPending.size ()) this->m_resampledPending.clear ();
        return static_cast<int> (count);
    }
    // FFmpeg's send/receive state belongs exclusively to the SDL callback. The
    // reader thread only demuxes packets and queues loop-boundary reset markers.
    while (this->m_audioContext.getApplicationContext ().state.general.keepRunning && this->isInitialized ()) {
	if (this->m_decoderFinished) {
	    return 0;
	}
	int ret = avcodec_receive_frame (this->getContext (), this->m_decodeFrame);
	if (ret == 0) {
	    return this->resampleAudio (audioBuffer, bufferSize);
	}
	if (ret != AVERROR (EAGAIN) && ret != AVERROR_EOF) {
	    sLog.error ("Cannot receive decoded audio frame: ", av_err2str (ret));
	    return -1;
	}
	if (ret == AVERROR_EOF && this->m_draining) {
	    const int tail = this->drainResampler (audioBuffer, bufferSize);
	    if (tail > 0) {
		return tail;
	    }
	    this->m_draining = false;
	    if (this->isRepeat ()) {
		avcodec_flush_buffers (this->getContext ());
		swr_close (this->m_swrctx);
		swr_init (this->m_swrctx);
	    } else {
		this->m_decoderFinished = true;
	    }
	    // The callback reaches this only after consuming the previous decoded
	    // buffer. Publish playback completion there, not at demuxer read-ahead EOF.
	    return AVERROR_EOF;
	}

	// A packet is retained only when avcodec_send_packet() previously asked us
	// to receive more output first. Never drop it on EAGAIN.
	if (this->m_decodePacket->data != nullptr) {
	    ret = avcodec_send_packet (this->getContext (), this->m_decodePacket);
	    if (ret == 0) {
		av_packet_unref (this->m_decodePacket);
		continue;
	    }
	    if (ret == AVERROR (EAGAIN)) {
		return 0;
	    }

	    sLog.error ("Cannot send audio packet for decoding: ", av_err2str (ret));
	    av_packet_unref (this->m_decodePacket);
	    return -1;
	}

	if (!this->dequeuePacket ()) {
	    return 0;
	}

	if (this->m_decoderFlushPending) {
	    // Sending a null packet releases delayed codec frames. The resampler
	    // has its own delayed samples, drained above before the pass completes.
	    ret = avcodec_send_packet (this->getContext (), nullptr);
	    if (ret < 0 && ret != AVERROR_EOF) {
		sLog.error ("Cannot drain audio decoder: ", av_err2str (ret));
		return -1;
	    }
	    this->m_draining = true;
	    continue;
	}
    }

    return 0;
}

AudioContext& AudioStream::getAudioContext () const { return this->m_audioContext; }
