#pragma once

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

extern const int FPS;
extern const int FRAME_COUNT;

class RecordingSession {
public:
    RecordingSession () = default;
    ~RecordingSession ();

    bool start (const char* filename, int sourceWidth, int sourceHeight);

    void submitFrame (const uint8_t* rgb);

    void stop ();

    bool isRunning ();

private:
    struct Frame {
	std::vector<uint8_t> rgb;
    };

    void workerLoop ();

    bool m_running = false;

    std::thread m_worker;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::queue<Frame> m_queue;

    static constexpr size_t MAX_QUEUE_SIZE = 10;

    int64_t m_frameCount = 0;

    int m_width = 0;
    int m_height = 0;
    int m_sourceWidth = 0;
    int m_sourceHeight = 0;

    const AVCodec* m_codec = nullptr;
    AVCodecContext* m_codecContext = nullptr;
    AVFormatContext* m_formatContext = nullptr;
    AVStream* m_stream = nullptr;
    SwsContext* m_swsContext = nullptr;

    AVFrame* m_videoFrame = nullptr;
    AVFrame* m_rgbFrame = nullptr;
};