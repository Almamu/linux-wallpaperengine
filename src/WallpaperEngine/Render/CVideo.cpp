#include "CVideo.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CVideo::CVideo (Core::CVideo* video, CContainer* container, CContext* context) :
    CWallpaper (video, Type, container, context)
{
    if (avformat_open_input (&m_formatCtx, video->getFilename ().c_str (), NULL, NULL) < 0)
        throw std::runtime_error ("Failed to open video file");

    if (avformat_find_stream_info (m_formatCtx, NULL) < 0)
        throw std::runtime_error ("Failed to get stream info");

    // Find first video stream
    for (int i = 0; i < m_formatCtx->nb_streams; i++)
    {
        if (m_formatCtx->streams [i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStream = i;
            break;
        }
    }

    // Find first audio stream
    for (int i = 0; i < m_formatCtx->nb_streams; i++)
    {
        if (m_formatCtx->streams [i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStream = i; 
            break;
        }
    }

    // Only video stream is required
    if (m_videoStream == -1)
        throw std::runtime_error ("Failed to find video stream");

    const AVCodec* codec = avcodec_find_decoder (m_formatCtx->streams [m_videoStream]->codecpar->codec_id);
    if (codec == nullptr)
        throw std::runtime_error ("Failed to find codec");

    m_codecCtx = avcodec_alloc_context3 (codec);
    if (avcodec_parameters_to_context (m_codecCtx, m_formatCtx->streams [m_videoStream]->codecpar))
        throw std::runtime_error ("Failed to copy codec parameters");

    if (avcodec_open2 (m_codecCtx, codec, NULL) < 0)
        throw std::runtime_error ("Failed to open codec");

    // initialize audio if there's any audio stream
    if (m_audioStream != -1)
    {
        const AVCodec* audioCodec = avcodec_find_decoder (m_formatCtx->streams [m_audioStream]->codecpar->codec_id);
        if (audioCodec == nullptr)
            throw std::runtime_error ("Failed to find codec");

        AVCodecContext* audioContext = avcodec_alloc_context3 (audioCodec);
        if (avcodec_parameters_to_context (audioContext, m_formatCtx->streams [m_audioStream]->codecpar))
            throw std::runtime_error ("Failed to copy codec parameters");

        if (avcodec_open2 (audioContext, audioCodec, NULL) < 0)
            throw std::runtime_error ("Failed to open codec");

        this->m_audio = new Audio::CAudioStream (audioContext);
    }

    m_videoFrame = av_frame_alloc ();
    m_videoFrameRGB = av_frame_alloc ();
    if (m_videoFrameRGB == nullptr)
        throw std::runtime_error ("Failed to allocate video frame");

    GLfloat texCoords [] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    // inverted positions so the final texture is rendered properly
    GLfloat position [] = {
        -1.0f, 1.0f, 0.0f,
        1.0, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    glGenBuffers (1, &this->m_texCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_positionBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);

    // setup gl things to render the background
    glGenTextures (1, &this->m_texture);
    // configure the texture
    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    // set filtering parameters, otherwise the texture is not rendered
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // set texture basic data
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, m_codecCtx->width, m_codecCtx->height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    this->setupShaders ();
    this->setupFramebuffers ();
}

void CVideo::setSize (int width, int height)
{
    if (m_buffer != nullptr)
        av_free (m_buffer);

    if (m_swsCtx != nullptr)
        sws_freeContext (m_swsCtx);

    int numBytes = av_image_get_buffer_size (AV_PIX_FMT_RGB24, width, height, 1);
    m_buffer = (uint8_t*) av_malloc (numBytes * sizeof (uint8_t));

    av_image_fill_arrays (m_videoFrameRGB->data, m_videoFrameRGB->linesize, m_buffer, AV_PIX_FMT_RGB24, width, height, 1);

    m_swsCtx = sws_getContext (m_codecCtx->width, m_codecCtx->height,
                    m_codecCtx->pix_fmt,
                    width, height,
                    AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, NULL, NULL, NULL);
}

void CVideo::renderFrame (glm::ivec4 viewport)
{
    // do not render using the CWallpaper function, just use this one
    this->setSize (m_codecCtx->width, m_codecCtx->height);
    getNextFrame ();
    writeFrameToImage ();

    glViewport (0, 0, this->getWidth (), this->getHeight ());

    // do the actual rendering
    // write to default's framebuffer
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer());

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    // do not use any shader
    glUseProgram (this->m_shader);
    // activate scene texture
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    // set uniforms and attribs
    glEnableVertexAttribArray (this->a_TexCoord);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glVertexAttribPointer (this->a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glEnableVertexAttribArray (this->a_Position);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glVertexAttribPointer (this->a_Position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform1i (this->g_Texture0, 0);
    // write the framebuffer as is to the screen
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glDrawArrays (GL_TRIANGLES, 0, 6);
}

void CVideo::getNextFrame ()
{
    bool eof = false;
    AVPacket packet;
    packet.data = nullptr;
    
    // Find video streams packet
    do
    {
        if (packet.data != nullptr)
            av_packet_unref (&packet);

        int readError = av_read_frame (m_formatCtx, &packet);
        if (readError == AVERROR_EOF)
        {
            eof = true;
            break;
        }
        else if (readError < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            throw std::runtime_error (av_make_error_string (err, AV_ERROR_MAX_STRING_SIZE, readError));
        }
        
    } while (packet.stream_index != m_videoStream /*&& packet.stream_index != m_audioStream*/);

    if (!eof && packet.stream_index == m_videoStream)
    {
        // Send video stream packet to codec
        if (avcodec_send_packet (m_codecCtx, &packet) < 0)
            return;

        // Receive frame from codec
        if (avcodec_receive_frame (m_codecCtx, m_videoFrame) < 0)
            return;

        sws_scale (m_swsCtx, (uint8_t const* const*) m_videoFrame->data, m_videoFrame->linesize,
                   0, m_codecCtx->height, m_videoFrameRGB->data, m_videoFrameRGB->linesize);

    }
    /*else if (packet.stream_index == m_audioStream)
    {
        this->m_audio->queuePacket (&packet);
    }*/

    av_packet_unref (&packet);

    if (eof)
        restartStream ();
}

void CVideo::writeFrameToImage ()
{
    uint8_t* frameData = m_videoFrameRGB->data [0];

    if (frameData == nullptr)
        return;

    // bind the texture
    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    // give openGL the new image's data
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, m_codecCtx->width, m_codecCtx->height, GL_RGB, GL_UNSIGNED_BYTE, frameData);
}

void CVideo::restartStream ()
{
    av_seek_frame (m_formatCtx, m_videoStream, 0, AVSEEK_FLAG_FRAME);
    avcodec_flush_buffers (m_codecCtx);
}

Core::CVideo* CVideo::getVideo ()
{
    return this->getWallpaperData ()->as<Core::CVideo> ();
}


void CVideo::setupShaders ()
{
    // reserve shaders in OpenGL
    GLuint vertexShaderID = glCreateShader (GL_VERTEX_SHADER);

    // give shader's source code to OpenGL to be compiled
    const char* sourcePointer = "#version 120\n"
                                "attribute vec3 a_Position;\n"
                                "attribute vec2 a_TexCoord;\n"
                                "varying vec2 v_TexCoord;\n"
                                "void main () {\n"
                                "gl_Position = vec4 (a_Position, 1.0);\n"
                                "v_TexCoord = a_TexCoord;\n"
                                "}";

    glShaderSource (vertexShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (vertexShaderID);

    GLint result = GL_FALSE;
    int infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (vertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetShaderInfoLog (vertexShaderID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        throw std::runtime_error (message);
    }

    // reserve shaders in OpenGL
    GLuint fragmentShaderID = glCreateShader (GL_FRAGMENT_SHADER);

    // give shader's source code to OpenGL to be compiled
    sourcePointer = "#version 120\n"
                    "uniform sampler2D g_Texture0;\n"
                    "varying vec2 v_TexCoord;\n"
                    "void main () {\n"
                    "gl_FragColor = texture2D (g_Texture0, v_TexCoord);\n"
                    "}";

    glShaderSource (fragmentShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (fragmentShaderID);

    result = GL_FALSE;
    infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetShaderInfoLog (fragmentShaderID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        throw std::runtime_error (message);
    }

    // create the final program
    this->m_shader = glCreateProgram ();
    // link the shaders together
    glAttachShader (this->m_shader, vertexShaderID);
    glAttachShader (this->m_shader, fragmentShaderID);
    glLinkProgram (this->m_shader);
    // check that the shader was properly linked
    result = GL_FALSE;
    infoLogLength = 0;

    glGetProgramiv (this->m_shader, GL_LINK_STATUS, &result);
    glGetProgramiv (this->m_shader, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetProgramInfoLog (this->m_shader, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        throw std::runtime_error (message);
    }

    // after being liked shaders can be dettached and deleted
    glDetachShader (this->m_shader, vertexShaderID);
    glDetachShader (this->m_shader, fragmentShaderID);

    glDeleteShader (vertexShaderID);
    glDeleteShader (fragmentShaderID);

    // get textures
    this->g_Texture0 = glGetUniformLocation (this->m_shader, "g_Texture0");
    this->a_Position = glGetAttribLocation (this->m_shader, "a_Position");
    this->a_TexCoord = glGetAttribLocation (this->m_shader, "a_TexCoord");
}

int CVideo::getWidth ()
{
    return this->m_codecCtx->width;
}

int CVideo::getHeight ()
{
    return this->m_codecCtx->height;
}

const std::string CVideo::Type = "video";
