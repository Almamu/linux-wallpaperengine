#if DEMOMODE

// this file is horrible, but doesn't need to be anything good as it's only used internally

#include "recording.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

const int FPS = 30;
const int FRAME_COUNT = 150;
int WIDTH = 0;
int HEIGHT = 0;
int SOURCE_WIDTH = 0;
int SOURCE_HEIGHT = 0;

int64_t frame_count = 0;

// Global variables to hold encoder context and output stream
const AVCodec *video_codec = nullptr;
AVCodecContext *video_codec_context = nullptr;
AVFormatContext *format_context = nullptr;
AVStream *video_stream = nullptr;
SwsContext *sws_context = nullptr;
AVFrame *video_frame = nullptr;
AVFrame* rgb_frame = nullptr;

int init_encoder(const char *output_file, int sourceWidth, int sourceHeight) {
    float factor = 512.0f / (float) sourceWidth;
    SOURCE_WIDTH = sourceWidth;
    SOURCE_HEIGHT = sourceHeight;
    WIDTH = (float) sourceWidth * factor;
    HEIGHT = (float) sourceHeight * factor;

    avformat_network_init();

    // Initialize the output format context
    if (avformat_alloc_output_context2(&format_context, nullptr, "webm", output_file) < 0) {
        std::cerr << "Error initializing format context" << std::endl;
        return -1;
    }

    // Video codec: VP8
    video_codec = avcodec_find_encoder(AV_CODEC_ID_VP9);
    if (!video_codec) {
        std::cerr << "VP8 codec not found!" << std::endl;
        return -1;
    }

    video_codec_context = avcodec_alloc_context3(video_codec);
    if (!video_codec_context) {
        std::cerr << "Error allocating video codec context" << std::endl;
        return -1;
    }

    video_codec_context->bit_rate = 4000000;
    video_codec_context->width = WIDTH;
    video_codec_context->height = HEIGHT;
    video_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    video_codec_context->time_base = (AVRational){1, FPS};
    video_codec_context->framerate = (AVRational){FPS, 1};
    video_codec_context->gop_size = 12;
    video_codec_context->max_b_frames = 1;
    video_codec_context->qmin = 10;
    video_codec_context->qmax = 40;

    if (avcodec_open2(video_codec_context, video_codec, nullptr) < 0) {
        std::cerr << "Error opening VP8 codec" << std::endl;
        return -1;
    }

    // Create the video stream in the format context
    video_stream = avformat_new_stream(format_context, video_codec);
    if (!video_stream) {
        std::cerr << "Error creating video stream" << std::endl;
        return -1;
    }

    // Copy codec parameters from the codec context to the stream
    if (avcodec_parameters_from_context(video_stream->codecpar, video_codec_context) < 0) {
        std::cerr << "Error copying codec parameters to stream" << std::endl;
        return -1;
    }

    video_stream->time_base = video_codec_context->time_base;

    // Open output file for writing
    if (avio_open(&format_context->pb, output_file, AVIO_FLAG_WRITE) < 0) {
        std::cerr << "Error opening output file" << std::endl;
        return -1;
    }

    // Write file header
    if (avformat_write_header(format_context, nullptr) < 0) {
        std::cerr << "Error writing file header" << std::endl;
        return -1;
    }

    // Allocate video frame
    video_frame = av_frame_alloc();
    video_frame->format = AV_PIX_FMT_YUV420P;
    video_frame->width = WIDTH;
    video_frame->height = HEIGHT;
    av_frame_get_buffer(video_frame, 0);

    rgb_frame = av_frame_alloc();

    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = SOURCE_WIDTH;
    rgb_frame->height = SOURCE_HEIGHT;

    // Set up YUV conversion context (RGB to YUV)
    sws_context = sws_getContext(SOURCE_WIDTH, SOURCE_HEIGHT, AV_PIX_FMT_RGB24,
                                 WIDTH, HEIGHT, AV_PIX_FMT_YUV420P,
                                 SWS_BICUBIC, nullptr, nullptr, nullptr);

    return 0;
}

int write_video_frame(const uint8_t *rgb_data) {
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_data, AV_PIX_FMT_RGB24, SOURCE_WIDTH, SOURCE_HEIGHT, 1);

    sws_scale(sws_context, rgb_frame->data, rgb_frame->linesize, 0, SOURCE_HEIGHT, video_frame->data, video_frame->linesize);

    // Send the frame to the encoder
    int ret = avcodec_send_frame(video_codec_context, video_frame);
    if (ret < 0) {
        std::cerr << "Error sending video frame: " << ret << std::endl;
        return -1;
    }

    AVPacket packet;
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;

    // Receive the encoded packet from the encoder
    ret = avcodec_receive_packet(video_codec_context, &packet);
    if (ret < 0) {
        std::cerr << "Error receiving video packet: " << ret << std::endl;
        return -1;
    }

    packet.stream_index = video_stream->index;
    // Set the PTS and DTS values
    packet.pts = av_rescale_q(frame_count, video_codec_context->time_base, video_stream->time_base);
    packet.dts = packet.pts;  // For simplicity, you can set DTS equal to PTS for now
    packet.duration = av_rescale_q(1, video_codec_context->time_base, video_stream->time_base);

    // Increment frame counter
    frame_count++;
    // Write the encoded video packet to the file
    ret = av_interleaved_write_frame(format_context, &packet);
    if (ret < 0) {
        std::cerr << "Error writing video packet: " << ret << std::endl;
        return -1;
    }

    // Ensure that the packet is freed
    av_packet_unref(&packet);

    return 0;
}

int close_encoder() {
    // Write any remaining frames (flush encoder)
    avcodec_flush_buffers(video_codec_context);

    // Write the trailer
    av_write_trailer(format_context);

    // Clean up
    avcodec_free_context(&video_codec_context);
    avformat_free_context(format_context);
    av_frame_free(&video_frame);
    sws_freeContext(sws_context);

    return 0;
}

#endif /* DEMOMODE */