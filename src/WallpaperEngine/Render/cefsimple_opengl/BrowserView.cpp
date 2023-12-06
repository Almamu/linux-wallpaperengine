// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui

#include "BrowserView.hpp"
#include "GLCore.hpp"

//------------------------------------------------------------------------------
BrowserView::RenderHandler::RenderHandler(glm::vec4 const& viewport)
    : m_viewport(viewport)
{}

//------------------------------------------------------------------------------
BrowserView::RenderHandler::~RenderHandler()
{
    // Free GPU memory
    GLCore::deleteProgram(m_prog);
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

//------------------------------------------------------------------------------
bool BrowserView::RenderHandler::init()
{
    // Dummy texture data
    const unsigned char data[] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255,
    };

    // Compile vertex and fragment shaders
    m_prog = GLCore::createShaderProgram("shaders/tex.vert", "shaders/tex.frag");
    if (m_prog == 0)
    {
        std::cerr << "shader compile failed" << std::endl;
        return false;
    }

    // Get locations of shader variables (attributes and uniforms)
    m_pos_loc = GLCHECK(glGetAttribLocation(m_prog, "position"));
    m_tex_loc = GLCHECK(glGetUniformLocation(m_prog, "tex"));
    m_mvp_loc = GLCHECK(glGetUniformLocation(m_prog, "mvp"))

    // Square vertices (texture positions are computed directly inside the shader)
    float coords[] = {-1.0,-1.0,-1.0,1.0,1.0,-1.0,1.0,-1.0,-1.0,1.0,1.0,1.0};

    // See https://learnopengl.com/Getting-started/Textures
    GLCHECK(glGenVertexArrays(1, &m_vao));
    GLCHECK(glBindVertexArray(m_vao));
    GLCHECK(glGenBuffers(1, &m_vbo));
    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GLCHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(coords), coords, GL_STATIC_DRAW));
    GLCHECK(glEnableVertexAttribArray(m_pos_loc));
    GLCHECK(glVertexAttribPointer(m_pos_loc, 2, GL_FLOAT, GL_FALSE, 0, 0));

    GLCHECK(glGenTextures(1, &m_tex));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, m_tex));
    GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GLCHECK(glBindVertexArray(0));

    return true;
}

//------------------------------------------------------------------------------
void BrowserView::RenderHandler::draw(glm::vec4 const& viewport, bool fixed)
{
    // Where to paint on the OpenGL window
    GLCHECK(glViewport(viewport[0],
                       viewport[1],
                       GLsizei(viewport[2] * m_width),
                       GLsizei(viewport[3] * m_height)));

    // Apply a rotation
    glm::mat4 trans = glm::mat4(1.0f); // Identity matrix
    if (!fixed)
    {
        trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
        trans = glm::rotate(trans, (float)glfwGetTime() / 5.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // See https://learnopengl.com/Getting-started/Textures
    GLCHECK(glUseProgram(m_prog));
    GLCHECK(glBindVertexArray(m_vao));

    GLCHECK(glUniformMatrix4fv(m_mvp_loc, 1, GL_FALSE, glm::value_ptr(trans)));
    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GLCHECK(glActiveTexture(GL_TEXTURE0));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, m_tex));
    GLCHECK(glDrawArrays(GL_TRIANGLES, 0, 6));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GLCHECK(glBindVertexArray(0));
    GLCHECK(glUseProgram(0));
}

//------------------------------------------------------------------------------
void BrowserView::RenderHandler::reshape(int w, int h)
{
    m_width = w;
    m_height = h;
}

bool BrowserView::viewport(float x, float y, float w, float h)
{
    if (!(x >= 0.0f) && (x < 1.0f))
        return false;

    if (!(x >= 0.0f) && (y < 1.0f))
        return false;

    if (!(w > 0.0f) && (w <= 1.0f))
        return false;

    if (!(h > 0.0f) && (h <= 1.0f))
        return false;

    if (x + w > 1.0f)
        return false;

    if (y + h > 1.0f)
        return false;

    m_viewport[0] = x;
    m_viewport[1] = y;
    m_viewport[2] = w;
    m_viewport[3] = h;

    return true;
}

//------------------------------------------------------------------------------
void BrowserView::RenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
    rect = CefRect(m_viewport[0], m_viewport[1], m_viewport[2] * m_width, m_viewport[3] * m_height);
}

//------------------------------------------------------------------------------
void BrowserView::RenderHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                         const RectList &dirtyRects, const void *buffer,
                                         int width, int height)
{
    //std::cout << "BrowserView::RenderHandler::OnPaint" << std::endl;
    GLCHECK(glActiveTexture(GL_TEXTURE0));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, m_tex));
    GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT,
                         GL_UNSIGNED_BYTE, (unsigned char*)buffer));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));
}

//------------------------------------------------------------------------------
BrowserView::BrowserView(const std::string &url)
    : m_mouse_x(0), m_mouse_y(0), m_viewport(0.0f, 0.0f, 1.0f, 1.0f)
{
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);

    m_render_handler = new RenderHandler(m_viewport);
    m_initialized = m_render_handler->init();
    m_render_handler->reshape(128, 128); // initial size

    CefBrowserSettings browserSettings;
    browserSettings.windowless_frame_rate = 60; // 30 is default

    m_client = new BrowserClient(m_render_handler);
    m_browser = CefBrowserHost::CreateBrowserSync(window_info, m_client.get(),
                                                  url, browserSettings,
                                                  nullptr, nullptr);
}

//------------------------------------------------------------------------------
BrowserView::~BrowserView()
{
    CefDoMessageLoopWork();
    m_browser->GetHost()->CloseBrowser(true);

    m_browser = nullptr;
    m_client = nullptr;
}

//------------------------------------------------------------------------------
void BrowserView::load(const std::string &url)
{
    assert(m_initialized);
    m_browser->GetMainFrame()->LoadURL(url);
}

//------------------------------------------------------------------------------
void BrowserView::draw()
{
    CefDoMessageLoopWork();
    m_render_handler->draw(m_viewport, m_fixed);
}

//------------------------------------------------------------------------------
void BrowserView::reshape(int w, int h)
{
    m_render_handler->reshape(w, h);
    GLCHECK(glViewport(m_viewport[0],
                       m_viewport[1],
                       GLsizei(m_viewport[2] * w),
                       GLsizei(m_viewport[3] * h)));
    m_browser->GetHost()->WasResized();
}

//------------------------------------------------------------------------------
void BrowserView::mouseMove(int x, int y)
{
    m_mouse_x = x;
    m_mouse_y = y;

    CefMouseEvent evt;
    evt.x = x;
    evt.y = y;

    bool mouse_leave = false; // TODO
    m_browser->GetHost()->SendMouseMoveEvent(evt, mouse_leave);
}

//------------------------------------------------------------------------------
void BrowserView::mouseClick(CefBrowserHost::MouseButtonType btn, bool mouse_up)
{
    CefMouseEvent evt;
    evt.x = m_mouse_x;
    evt.y = m_mouse_y;

    int click_count = 1; // TODO
    m_browser->GetHost()->SendMouseClickEvent(evt, btn, mouse_up, click_count);
}

//------------------------------------------------------------------------------
void BrowserView::keyPress(int key, bool pressed)
{
    CefKeyEvent evt;
    evt.character = key;
    evt.native_key_code = key;
    evt.type = pressed ? KEYEVENT_CHAR : KEYEVENT_KEYUP;

    m_browser->GetHost()->SendKeyEvent(evt);
}
