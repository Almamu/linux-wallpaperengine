#include "AudioStream.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

// maximum size of the queue to prevent reading too much data

using namespace WallpaperEngine::Audio;

int audio_read_thread (void* arg) {
    SDL_mutex* waitMutex = SDL_CreateMutex ();
    auto* stream = static_cast<AudioStream*> (arg);
    AVPacket* packet = av_packet_alloc ();
    int ret = 0;
    bool inputExhausted = false;

    if (waitMutex == nullptr) {
	sLog.exception ("Cannot create mutex for audio playback waiting");
    }

	while (stream->getAudioContext ().getApplicationContext ().state.general.keepRunning && stream->isInitialized ()) {
		if (stream->resetPlaybackIfRequested ()) {
		    inputExhausted = false;
		    ret = 0;
		}

		if (inputExhausted) {
		    SDL_LockMutex (waitMutex);
		    SDL_CondWaitTimeout (stream->getWaitCondition (), waitMutex, 10);
		    SDL_UnlockMutex (waitMutex);
		    continue;
		}

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
		    if (stream->isRepeat ()) {
			// Looping is the one case where reaching EOF immediately starts another pass.
			avformat_seek_file (
			    stream->getFormatContext (), stream->getAudioStream (), 0, 0, 0, ~AVSEEK_FLAG_FRAME
			);
			avcodec_flush_buffers (stream->getContext ());
			ret = 0;
		    } else {
			// Keep the reader thread alive so SceneScript stop()/play() can rewind
			// and reuse a non-looping sound layer later.
			inputExhausted = true;
			ret = 0;
		    }

		    continue;
		}

		if (ret < 0) {
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

    switch (whence) {
	case SEEK_CUR:
	    stream->getBuffer ()->seekg (offset, std::ios_base::cur);
	    break;
	case SEEK_SET:
	    stream->getBuffer ()->seekg (offset, std::ios_base::beg);
	    break;
	case SEEK_END:
	    stream->getBuffer ()->seekg (offset, std::ios_base::end);
	    break;
    }

    return 0;
}

AudioStream::AudioStream (AudioContext& context, const std::string& filename) : m_audioContext (context) {
    this->loadCustomContent (filename.c_str ());
}

AudioStream::AudioStream (AudioContext& context, const ReadStreamSharedPtr& buffer) : m_audioContext (context) {
    // setup a custom context first
    this->m_formatContext = avformat_alloc_context ();

    if (this->m_formatContext == nullptr) {
	sLog.exception ("Cannot allocate ffmpeg format context");
    }

    this->m_buffer = buffer;

    // setup custom io for it
    this->m_formatContext->pb = avio_alloc_context (
	static_cast<uint8_t*> (av_malloc (4096)), 4096, 0, this, &audio_read_data_callback, nullptr,
	&audio_seek_data_callback
    );

    if (this->m_formatContext->pb == nullptr) {
	sLog.exception ("Cannot create avio context");
    }

    // continue the normal load procedure
    this->loadCustomContent ();
}

AudioStream::AudioStream (AudioContext& audioContext, AVCodecContext* context) :
    m_audioContext (audioContext), m_context (context), m_queue (new PacketQueue) {
    this->initialize ();
}

AudioStream::~AudioStream () {
    // stop the audio
    this->stop ();

    if (this->m_audioThread != nullptr) {
	// wait for the thread to finish
	SDL_WaitThread (this->m_audioThread, nullptr);
    }

    this->m_audioThread = nullptr;

    if (this->m_queue != nullptr) {
		SDL_LockMutex (this->m_queue->mutex);
		this->clearQueueLocked ();
		SDL_UnlockMutex (this->m_queue->mutex);
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
		SDL_DestroyCond (this->m_queue->wait);
		SDL_DestroyCond (this->m_queue->cond);
		SDL_DestroyMutex (this->m_queue->mutex);
		delete this->m_queue;
		this->m_queue = nullptr;
    }

	if (this->m_decodeMutex != nullptr) {
		SDL_DestroyMutex (this->m_decodeMutex);
		this->m_decodeMutex = nullptr;
	}

    if (this->m_formatContext != nullptr) {
	avformat_free_context (this->m_formatContext);
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
    AVCodecContext* avCodecContext = avcodec_alloc_context3 (aCodec);

    if (avcodec_parameters_to_context (avCodecContext, this->m_formatContext->streams[this->m_audioStream]->codecpar)
	!= 0) {
	sLog.exception ("Cannot initialize audio decoder parameters");
    }

    // finally open
    avcodec_open2 (avCodecContext, aCodec, nullptr);

    // initialize default data
    this->m_context = avCodecContext;
    this->m_queue = new PacketQueue;

    this->initialize ();

    // initialize an SDL thread to read the file
    this->m_audioThread = SDL_CreateThread (audio_read_thread, filename, this);
}

void AudioStream::initialize () {
// allocate the FIFO buffer
#if FF_API_FIFO_OLD_API
    this->m_queue->packetList = av_fifo_alloc (sizeof (MyAVPacketList));
#else
    this->m_queue->packetList = av_fifo_alloc2 (1, sizeof (MyAVPacketList), AV_FIFO_FLAG_AUTO_GROW);
#endif

#if FF_API_OLD_CHANNEL_LAYOUT
    int64_t out_channel_layout;

    // set output audio channels based on the input audio channels
    switch (this->m_audioContext.getChannels ()) {
	case 1:
	    out_channel_layout = AV_CH_LAYOUT_MONO;
	    break;
	case 2:
	    out_channel_layout = AV_CH_LAYOUT_STEREO;
	    break;
	default:
	    out_channel_layout = AV_CH_LAYOUT_SURROUND;
	    break;
    }

    // initialize swrctx
    this->m_swrctx = swr_alloc_set_opts (
	nullptr, out_channel_layout, this->m_audioContext.getFormat (), this->m_audioContext.getSampleRate (),
	this->getContext ()->channel_layout, this->getContext ()->sample_fmt, this->getContext ()->sample_rate, 0,
	nullptr
    );
#else
    AVChannelLayout out_channel_layout;
    int64_t out_channel_mask;

    // set output audio channels based on the input audio channels
    switch (this->m_audioContext.getChannels ()) {
	case 1:
	    out_channel_mask = AV_CH_LAYOUT_MONO;
	    break;
	case 2:
	    out_channel_mask = AV_CH_LAYOUT_STEREO;
	    break;
	default:
	    out_channel_mask = AV_CH_LAYOUT_SURROUND;
	    break;
    }

    if (av_channel_layout_from_mask (&out_channel_layout, out_channel_mask) != 0) {
	sLog.exception ("Cannot get channel layout from mask");
    }

    swr_alloc_set_opts2 (
	&this->m_swrctx, &out_channel_layout, this->m_audioContext.getFormat (), this->m_audioContext.getSampleRate (),
	&this->m_context->ch_layout, this->m_context->sample_fmt, this->m_context->sample_rate, 0, nullptr
    );
#endif

    if (this->m_swrctx == nullptr) {
	sLog.exception ("Cannot initialize swrctx for audio resampling");
    }

    // initialize the context
    if (swr_init (this->m_swrctx) < 0) {
	sLog.exception ("Failed to initialize the resampling context.");
    }

    // setup the queue information
    this->m_queue->mutex = SDL_CreateMutex ();
    this->m_queue->cond = SDL_CreateCond ();
    this->m_queue->wait = SDL_CreateCond ();
	this->m_decodeMutex = SDL_CreateMutex ();

    this->m_decodeFrame = av_frame_alloc ();
    this->m_decodePacket = av_packet_alloc ();

    if (!this->m_decodeFrame) {
	sLog.exception ("Could not allocate AVFrame.\n");
    }
    if (!this->m_decodePacket) {
	sLog.exception ("Could not allocate AVPacket.\n");
    }

	if (this->m_queue->mutex == nullptr || this->m_queue->cond == nullptr || this->m_queue->wait == nullptr
	    || this->m_decodeMutex == nullptr) {
		sLog.exception ("Could not initialize audio synchronization primitives");
	}

	this->m_initialized.store (true);
}

void AudioStream::queuePacket (AVPacket* pkt) {
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

    if (!gotQueued) {
	av_packet_free (&pkt);
    }
}

bool AudioStream::doQueue (AVPacket* pkt) {
    MyAVPacketList entry { pkt };

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

    this->m_queue->nb_packets++;
    this->m_queue->size += entry.packet->size + sizeof (entry);
    this->m_queue->duration += entry.packet->duration;

    SDL_CondSignal (this->m_queue->cond);

    return true;
}

bool AudioStream::dequeuePacket () {
    MyAVPacketList entry {};
	bool dequeued = false;

    SDL_LockMutex (this->m_queue->mutex);

	while (this->m_audioContext.getApplicationContext ().state.general.keepRunning && this->isInitialized ()
	       && this->isPlaying ()) {
#if FF_API_FIFO_OLD_API
	int ret = -1;

	if (av_fifo_size (this->m_queue->packetList) >= static_cast<int> (sizeof (entry))) {
	    ret = av_fifo_generic_read (this->m_queue->packetList, &entry, sizeof (entry), nullptr);
	}
#else
	const int ret = av_fifo_read (this->m_queue->packetList, &entry, 1);
#endif

	// enough data available, read it
	if (ret >= 0) {
	    this->m_queue->nb_packets--;
	    this->m_queue->size -= entry.packet->size + sizeof (entry);
	    this->m_queue->duration -= entry.packet->duration;

	    // move the reference and free the old one
		    av_packet_move_ref (this->m_decodePacket, entry.packet);
		    av_packet_free (&entry.packet);
		    dequeued = true;
		    break;
	}

	// make the thread wait if nothing was available
	SDL_CondWait (this->m_queue->cond, this->m_queue->mutex);
    }

    SDL_UnlockMutex (this->m_queue->mutex);
	return dequeued;
}

AVCodecContext* AudioStream::getContext () const { return this->m_context; }

AVFormatContext* AudioStream::getFormatContext () const { return this->m_formatContext; }

int AudioStream::getAudioStream () const { return this->m_audioStream; }

bool AudioStream::isInitialized () const { return this->m_initialized.load (); }

void AudioStream::setRepeat (const bool newRepeat) { this->m_repeat.store (newRepeat); }

bool AudioStream::isRepeat () const { return this->m_repeat.load (); }

void AudioStream::play () {
    if (!this->isInitialized ()) {
		return;
    }

	this->m_playback.play ();
	SDL_CondBroadcast (this->m_queue->cond);
	SDL_CondBroadcast (this->m_queue->wait);
}

void AudioStream::pause () {
    if (!this->isInitialized ()) {
		return;
    }

	this->m_playback.pause ();
	SDL_CondBroadcast (this->m_queue->cond);
}

void AudioStream::stopPlayback () {
    if (!this->isInitialized ()) {
		return;
    }

	this->m_playback.stop ();
	SDL_CondBroadcast (this->m_queue->cond);
	SDL_CondBroadcast (this->m_queue->wait);
}

bool AudioStream::isPlaying () const {
    return this->m_playback.isPlaying ();
}

void AudioStream::setVolume (float volume) { this->m_playback.setVolume (volume); }

float AudioStream::getVolume () const { return this->m_playback.volume (); }

std::uint64_t AudioStream::getPlaybackGeneration () const { return this->m_playback.generation (); }

void AudioStream::clearQueueLocked () {
    if (this->m_queue == nullptr || this->m_queue->packetList == nullptr) {
		return;
    }

	while (this->m_queue->nb_packets > 0) {
		MyAVPacketList entry {};
#if FF_API_FIFO_OLD_API
		if (av_fifo_size (this->m_queue->packetList) < static_cast<int> (sizeof (entry))
		    || av_fifo_generic_read (this->m_queue->packetList, &entry, sizeof (entry), nullptr) < 0) {
		    break;
		}
#else
		if (av_fifo_read (this->m_queue->packetList, &entry, 1) < 0) {
		    break;
		}
#endif
		if (entry.packet != nullptr) {
		    av_packet_free (&entry.packet);
		}
	}

	this->m_queue->nb_packets = 0;
	this->m_queue->size = 0;
	this->m_queue->duration = 0;
}

bool AudioStream::resetPlaybackIfRequested () {
	if (!this->m_playback.resetPending () || this->m_decodeMutex == nullptr || this->m_queue == nullptr) {
		return false;
	}

	SDL_LockMutex (this->m_decodeMutex);
	const auto generation = this->m_playback.generation ();
	if (this->m_playback.completedGeneration () == generation) {
		SDL_UnlockMutex (this->m_decodeMutex);
		return false;
	}

	SDL_LockMutex (this->m_queue->mutex);
	this->clearQueueLocked ();
	SDL_UnlockMutex (this->m_queue->mutex);

	if (this->m_formatContext != nullptr && this->m_audioStream != NO_AUDIO_STREAM) {
		if (avformat_seek_file (this->m_formatContext, this->m_audioStream, 0, 0, 0, ~AVSEEK_FLAG_FRAME) < 0) {
		    sLog.error ("Failed to rewind audio stream");
		}
	}
	if (this->m_context != nullptr) {
		avcodec_flush_buffers (this->m_context);
	}
	if (this->m_decodePacket != nullptr) {
		av_packet_unref (this->m_decodePacket);
	}
	if (this->m_decodeFrame != nullptr) {
		av_frame_unref (this->m_decodeFrame);
	}
	if (this->m_swrctx != nullptr) {
		swr_close (this->m_swrctx);
		if (swr_init (this->m_swrctx) < 0) {
		    sLog.error ("Failed to reset audio resampler");
		}
	}
	this->m_audioPacketSize = 0;
	this->m_playback.markResetComplete (generation);
	SDL_UnlockMutex (this->m_decodeMutex);
	return true;
}

ReadStreamSharedPtr& AudioStream::getBuffer () { return this->m_buffer; }

SDL_cond* AudioStream::getWaitCondition () const { return this->m_queue->wait; }

size_t AudioStream::getQueueSize () const { return this->m_queue->size; }

int AudioStream::getQueuePacketCount () const { return this->m_queue->nb_packets; }

AVRational AudioStream::getTimeBase () const {
    if (this->m_audioStream == NO_AUDIO_STREAM) {
	return { 0, 0 };
    }

    return this->m_formatContext->streams[this->m_audioStream]->time_base;
}

int64_t AudioStream::getQueueDuration () const { return this->m_queue->duration; }

bool AudioStream::isQueueEmpty () const { return this->m_queue->nb_packets == 0; }

SDL_mutex* AudioStream::getMutex () const { return this->m_queue->mutex; }

void AudioStream::stop () {
	if (!this->m_initialized.exchange (false)) {
		return;
	    }

	this->m_playback.pause ();
	if (this->m_queue != nullptr) {
		SDL_CondBroadcast (this->m_queue->cond);
		SDL_CondBroadcast (this->m_queue->wait);
	}
}

int AudioStream::resampleAudio (uint8_t* out_buf, const int out_size) {
    int out_linesize = 0;
    int ret;
    int out_nb_channels;
    int out_nb_samples;
    uint8_t** resampled_data = nullptr;
    int resampled_data_size;

    // retrieve number of audio samples (per channel)
    const int in_nb_samples = this->m_decodeFrame->nb_samples;
    if (in_nb_samples <= 0) {
	sLog.error ("in_nb_samples error.");
	return -1;
    }

    int max_out_nb_samples = out_nb_samples = av_rescale_rnd (
	in_nb_samples, this->m_audioContext.getSampleRate (), this->getContext ()->sample_rate, AV_ROUND_UP
    );

    // check rescaling was successful
    if (max_out_nb_samples <= 0) {
	sLog.error ("av_rescale_rnd error.");
	return -1;
    }

    // get number of output audio channels
#if FF_API_OLD_CHANNEL_LAYOUT
    int64_t out_channel_layout;

    // set output audio channels based on the input audio channels
    switch (this->m_audioContext.getChannels ()) {
	case 1:
	    out_channel_layout = AV_CH_LAYOUT_MONO;
	    break;
	case 2:
	    out_channel_layout = AV_CH_LAYOUT_STEREO;
	    break;
	default:
	    out_channel_layout = AV_CH_LAYOUT_SURROUND;
	    break;
    }

    out_nb_channels = av_get_channel_layout_nb_channels (out_channel_layout);
#else
    out_nb_channels = this->getContext ()->ch_layout.nb_channels;
#endif
    ret = av_samples_alloc_array_and_samples (
	&resampled_data, &out_linesize, out_nb_channels, out_nb_samples, this->m_audioContext.getFormat (), 0
    );

    if (ret < 0) {
	sLog.error ("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.");
	return -1;
    }

    // retrieve output samples number taking into account the progressive delay
    out_nb_samples = av_rescale_rnd (
	swr_get_delay (this->m_swrctx, this->getContext ()->sample_rate) + in_nb_samples,
	this->m_audioContext.getSampleRate (), this->getContext ()->sample_rate, AV_ROUND_UP
    );

    // check output samples number was correctly retrieved
    if (out_nb_samples <= 0) {
	sLog.error ("av_rescale_rnd error");
	return -1;
    }

    if (out_nb_samples > max_out_nb_samples) {
	// free memory block and set pointer to NULL
	av_freep (&resampled_data[0]);

	// Allocate a samples buffer for out_nb_samples samples
	ret = av_samples_alloc (
	    resampled_data, &out_linesize, out_nb_channels, out_nb_samples, this->m_audioContext.getFormat (), 1
	);

	// check samples buffer correctly allocated
	if (ret < 0) {
	    sLog.error ("av_samples_alloc failed.");
	    return -1;
	}

	max_out_nb_samples = out_nb_samples;
    }

    // do the actual audio data resampling
    ret = swr_convert (
	this->m_swrctx, resampled_data, max_out_nb_samples, const_cast<const uint8_t**> (this->m_decodeFrame->data),
	this->m_decodeFrame->nb_samples
    );

    // check audio conversion was successful
    if (ret < 0) {
	sLog.error ("swr_convert_error.");
	return -1;
    }

    // Get the required buffer size for the given audio parameters
    resampled_data_size
	= av_samples_get_buffer_size (&out_linesize, out_nb_channels, ret, this->m_audioContext.getFormat (), 1);

    // check audio buffer size
    if (resampled_data_size < 0) {
	sLog.error ("av_samples_get_buffer_size error.");
	return -1;
    }

    // copy the resampled data to the output buffer up to out_size bytes
    memcpy (out_buf, resampled_data[0], std::min (resampled_data_size, out_size));

    // memory cleanup
    if (resampled_data) {
	// free memory block and set pointer to NULL
	av_freep (&resampled_data[0]);
    }

    av_freep (&resampled_data);

    return resampled_data_size;
}

bool AudioStream::shouldDecode () const {
    return this->m_audioContext.getApplicationContext ().state.general.keepRunning && this->isInitialized ()
	&& this->isPlaying ();
}

int AudioStream::decodeQueuedPacket (uint8_t* audioBuffer, const int bufferSize) {
    while (this->m_audioPacketSize > 0 && this->shouldDecode ()) {
	int gotFrame = 0;
	int ret = avcodec_receive_frame (this->getContext (), this->m_decodeFrame);

	if (ret == 0) {
	    gotFrame = 1;
	}
	if (ret == AVERROR (EAGAIN)) {
	    ret = 0;
	}
	if (ret == 0) {
	    ret = avcodec_send_packet (this->getContext (), this->m_decodePacket);
	}
	if (ret < 0 && ret != AVERROR (EAGAIN)) {
	    return -1;
	}

	if (this->m_decodePacket->size < 0) {
	    // if error, skip frame
	    this->m_audioPacketSize = 0;
	    break;
	}

	this->m_audioPacketSize -= this->m_decodePacket->size;
	if (gotFrame == 0) {
	    continue;
	}

	const int dataSize = this->resampleAudio (audioBuffer, bufferSize);
	if (dataSize > 0) {
	    return dataSize;
	}
    }

    return 0;
}

int AudioStream::decodeFrame (uint8_t* audioBuffer, const int bufferSize) {
    if (this->m_decodeMutex == nullptr) {
	return 0;
    }

    SDL_LockMutex (this->m_decodeMutex);
    WallpaperEngine::Data::Utils::ScopeGuard decodeGuard ([this] { SDL_UnlockMutex (this->m_decodeMutex); });

    // block until there's any data in the buffers
    while (this->shouldDecode ()) {
	const int dataSize = this->decodeQueuedPacket (audioBuffer, bufferSize);
	if (dataSize != 0) {
	    return dataSize;
	}
	if (!this->shouldDecode ()) {
	    return 0;
	}

	if (this->m_decodePacket->data) {
	    av_packet_unref (this->m_decodePacket);
	}

	if (!this->dequeuePacket ()) {
	    return 0;
	}

	this->m_audioPacketSize = this->m_decodePacket->size;
    }

    return 0;
}

AudioContext& AudioStream::getAudioContext () const { return this->m_audioContext; }
