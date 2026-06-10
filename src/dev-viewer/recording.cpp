// this file is horrible, but doesn't need to be anything good as it's only used internally

#include "recording.h"

constexpr int MAX_QUEUE_SIZE = 10;
const int FPS = 30;
const int FRAME_COUNT = FPS * 5;

RecordingSession::~RecordingSession () {
    if (m_running) {
        this->stop();
    }
}

void RecordingSession::submitFrame(const uint8_t* rgb)
{
    if (!m_running)
        return;

    Frame frame;

    size_t size =
        m_sourceWidth *
        m_sourceHeight *
        3;

    frame.rgb.resize(size);

    memcpy(
        frame.rgb.data(),
        rgb,
        size);

    {
        std::lock_guard lock(m_mutex);

        if (m_queue.size() >= MAX_QUEUE_SIZE)
            m_queue.pop();

        m_queue.push(std::move(frame));
    }

    m_cv.notify_one();
}

void RecordingSession::workerLoop()
{
    while (true)
    {
        Frame frame;

        {
            std::unique_lock lock(m_mutex);

            m_cv.wait(lock, [&]
            {
                return !m_queue.empty() || !m_running;
            });

            if (!m_running && m_queue.empty())
                break;

            frame = std::move(m_queue.front());
            m_queue.pop();
        }

        av_image_fill_arrays(
            m_rgbFrame->data,
            m_rgbFrame->linesize,
            frame.rgb.data(),
            AV_PIX_FMT_RGB24,
            m_sourceWidth,
            m_sourceHeight,
            1);

        sws_scale(
            m_swsContext,
            m_rgbFrame->data,
            m_rgbFrame->linesize,
            0,
            m_sourceHeight,
            m_videoFrame->data,
            m_videoFrame->linesize);

        m_videoFrame->pts = m_frameCount++;

        int ret =
            avcodec_send_frame(
                m_codecContext,
                m_videoFrame);

        if (ret < 0)
            continue;

        AVPacket pkt;

        av_init_packet(&pkt);

        while (true)
        {
            ret =
                avcodec_receive_packet(
                    m_codecContext,
                    &pkt);

            if (ret == AVERROR(EAGAIN))
                break;

            if (ret == AVERROR_EOF)
                break;

            if (ret < 0)
                break;

            av_packet_rescale_ts(
                &pkt,
                m_codecContext->time_base,
                m_stream->time_base);

            pkt.stream_index =
                m_stream->index;

            av_interleaved_write_frame(
                m_formatContext,
                &pkt);

            av_packet_unref(&pkt);
        }
    }
}

bool RecordingSession::start (const char* filename, int sourceWidth, int sourceHeight) {
    float factor = 512.0f / (float)sourceWidth;
    this->m_sourceWidth = sourceWidth;
    this->m_sourceHeight = sourceHeight;
    this->m_width = (float)sourceWidth * factor;
    this->m_height = (float)sourceHeight * factor;

    avformat_network_init ();

    // Initialize the output format context
    if (avformat_alloc_output_context2 (&this->m_formatContext, nullptr, "webm", filename) < 0) {
	std::cerr << "Error initializing format context" << std::endl;
	return -1;
    }

    // Video codec: VP8
    this->m_codec = avcodec_find_encoder (AV_CODEC_ID_VP9);
    if (!this->m_codec) {
	std::cerr << "VP8 codec not found!" << std::endl;
	return -1;
    }

    this->m_codecContext = avcodec_alloc_context3 (this->m_codec);
    if (!this->m_codecContext) {
	std::cerr << "Error allocating video codec context" << std::endl;
	return -1;
    }

    this->m_codecContext->bit_rate = 4000000;
    this->m_codecContext->width = this->m_width;
    this->m_codecContext->height = this->m_height;
    this->m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    this->m_codecContext->time_base = (AVRational) { 1, FPS };
    this->m_codecContext->framerate = (AVRational) { FPS, 1 };
    this->m_codecContext->gop_size = 12;
    this->m_codecContext->max_b_frames = 1;
    this->m_codecContext->qmin = 10;
    this->m_codecContext->qmax = 40;

    if (avcodec_open2 (this->m_codecContext, this->m_codec, nullptr) < 0) {
	std::cerr << "Error opening VP8 codec" << std::endl;
	return -1;
    }

    // Create the video stream in the format context
    this->m_stream = avformat_new_stream (this->m_formatContext, this->m_codec);
    if (!this->m_stream) {
	std::cerr << "Error creating video stream" << std::endl;
	return -1;
    }

    // Copy codec parameters from the codec context to the stream
    if (avcodec_parameters_from_context (this->m_stream->codecpar, this->m_codecContext) < 0) {
	std::cerr << "Error copying codec parameters to stream" << std::endl;
	return -1;
    }

    this->m_stream->time_base = this->m_codecContext->time_base;

    // Open output file for writing
    if (avio_open (&this->m_formatContext->pb, filename, AVIO_FLAG_WRITE) < 0) {
	std::cerr << "Error opening output file" << std::endl;
	return -1;
    }

    // Write file header
    if (avformat_write_header (this->m_formatContext, nullptr) < 0) {
	std::cerr << "Error writing file header" << std::endl;
	return -1;
    }

    // Allocate video frame
    this->m_videoFrame = av_frame_alloc ();
    this->m_videoFrame->format = AV_PIX_FMT_YUV420P;
    this->m_videoFrame->width = this->m_width;
    this->m_videoFrame->height = this->m_height;
    av_frame_get_buffer (this->m_videoFrame, 0);

    this->m_rgbFrame = av_frame_alloc ();

    this->m_rgbFrame->format = AV_PIX_FMT_RGB24;
    this->m_rgbFrame->width = this->m_sourceWidth;
    this->m_rgbFrame->height = this->m_sourceHeight;

    // Set up YUV conversion context (RGB to YUV)
    this->m_swsContext = sws_getContext (
        // source
	this->m_sourceWidth, this->m_sourceHeight, AV_PIX_FMT_RGB24,
	// destination
	this->m_width, this->m_height, AV_PIX_FMT_YUV420P,
	SWS_BICUBIC, nullptr, nullptr,
	nullptr
    );

    m_running = true;

    m_worker =
        std::thread(
            &RecordingSession::workerLoop,
            this);

    return true;
}

void RecordingSession::stop()
{
    if (!m_running)
        return;

    {
        std::lock_guard lock(m_mutex);
        m_running = false;
    }

    m_cv.notify_one();

    m_worker.join();

    //
    // flush encoder
    //

    avcodec_send_frame(m_codecContext, nullptr);

    AVPacket pkt;

    av_init_packet(&pkt);

    while (avcodec_receive_packet(m_codecContext, &pkt) == 0)
    {
        av_packet_rescale_ts(
            &pkt,
            m_codecContext->time_base,
            m_stream->time_base);

        pkt.stream_index =
            m_stream->index;

        av_interleaved_write_frame(
            m_formatContext,
            &pkt);

        av_packet_unref(&pkt);
    }

    av_write_trailer(m_formatContext);

    av_frame_free(&m_videoFrame);
    av_frame_free(&m_rgbFrame);

    sws_freeContext(m_swsContext);

    avcodec_free_context(&m_codecContext);

    if (m_formatContext)
    {
        avio_closep(&m_formatContext->pb);
        avformat_free_context(m_formatContext);
    }
}

bool RecordingSession::isRunning () {
    return m_running;
}