// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"

using namespace WallpaperEngine::Render;

CWeb::CWeb (Core::CWeb* web, CRenderContext& context, CAudioContext& audioContext) :
    CWallpaper (web, Type, context, audioContext),
    m_width (context.getOutput ().getFullWidth ()),
    m_height (context.getOutput ().getFullHeight ()),
    m_browser(),
    m_client()
{
    // setup framebuffers
    this->setupFramebuffers();

    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);

    this->m_render_handler = new RenderHandler(this);

    CefBrowserSettings browserSettings;
    //Documentaion says said that 60 fps is maximum value
    browserSettings.windowless_frame_rate = std::max(60,context.getApp().getContext().settings.render.maximumFPS);

    m_client = new BrowserClient(m_render_handler);
    std::filesystem::path htmlpath = this->getWeb ()->getProject ().getContainer ()->resolveRealFile (this->getWeb ()->getFilename ());
    //To open local file in browser URL must be "file:///path/to/file.html"
    const std::string htmlURL = std::string("file:///") + htmlpath.c_str();
    m_browser = CefBrowserHost::CreateBrowserSync(window_info, m_client.get(),
                                                  htmlURL, browserSettings,
                                                  nullptr, nullptr);
}

void CWeb::setSize (int64_t width, int64_t height)
{
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0)
        return;

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture());
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth(), this->getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Notify cef that it was resized(maybe it's not even needed)
    m_browser->GetHost()->WasResized();
}

void CWeb::renderFrame (glm::ivec4 viewport)
{
    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);
    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    //Cef processes all messages, including OnPaint, which renders frame
    //If there is no OnPaint in message loop, we will not update(render) frame
    // This means some frames will not have OnPaint call in cef messageLoop
    // Because of that glClear will result in flickering on higher fps
    // Do not use glClear until some method to control rendering with cef is supported
    //We might actually try to use cef to execute javascript, and not using off-screen rendering at all
    //But for now let it be like this
    // glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CefDoMessageLoopWork();
}
void CWeb::updateMouse (glm::ivec4 viewport)
{
    // update virtual mouse position first
    glm::dvec2 position = this->getContext ().getInputContext ().getMouseInput ().position();

    CefMouseEvent evt;
    // Set mouse current position. Maybe clamps are not needed
    evt.x = std::clamp(int(position.x - viewport.x),0,viewport.z);
    evt.y = std::clamp(int(position.y - viewport.y),0,viewport.w);
    // Send mouse position to cef
    m_browser->GetHost()->SendMouseMoveEvent(evt, false);
}

CWeb::~CWeb(){
    CefDoMessageLoopWork();
    m_browser->GetHost()->CloseBrowser(true);
}

//TODO Remove
GLuint compileShaderFromCode(GLenum shader_type, const char *src)
{
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    // shader compile fail!
    fprintf(stderr, "SHADER COMPILE ERROR\n");

    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 1) {
        char *info_log = (char*)malloc(sizeof(char) * info_len);
        glGetShaderInfoLog(shader, info_len, NULL, info_log);
        fprintf(stderr, "Error compiling shader: \n%s\n", info_log);
        free(info_log);
    }
    glDeleteShader(shader);
    return 0;
}

GLuint createShaderProgram(GLuint vert, GLuint frag)
{
    GLuint program = glCreateProgram();

    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDetachShader(program, vert);
    glDetachShader(program, frag);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked) {
        return program;
    }

    // fail...
    GLint info_len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 1) {
        char *info_log = (char*)malloc(sizeof(char) * info_len);
        glGetProgramInfoLog(program, info_len, NULL, info_log);
        fprintf(stderr, "Error linking program: \n%s\n", info_log);
        free(info_log);
    }

    glDeleteProgram(program);
    return 0;
}

CWeb::RenderHandler::RenderHandler(CWeb* webdata):
    m_webdata(webdata)
{
    // m_prog = GLCore::createShaderProgram("shaders/tex.vert", "shaders/tex.frag");

    //GLuint vertShader = compileShaderFromFile(GL_VERTEX_SHADER, vert);


    // const char *vertCode = R"(
    //     #version 150

    //     uniform mat4 mvp;
    //     in vec2 position;
    //     out vec2 Texcoord;

    //     void main() {
    //     Texcoord = (vec2(position.x + 1.0f, position.y - 1.0f) * 0.5);
    //     Texcoord.y *= -1.0f;
    //     gl_Position = mvp * vec4(position.x, position.y, 0.0f, 1.0f);
    //     })";
    // GLuint vertShader = compileShaderFromCode(GL_VERTEX_SHADER, vertCode);

    // const char *fragCode = R"(
    //     #version 150

    //     in vec2 Texcoord;

    //     out vec4 outputColor;

    //     uniform sampler2D tex;

    //     void main() {
    //     outputColor = texture2D(tex, Texcoord);
    //     if (outputColor.a < 0.1)
    //     {
    //         discard;
    //     }
    //     }
    // )";
    // GLuint fragShader = compileShaderFromCode(GL_FRAGMENT_SHADER, fragCode);

    // if (vertShader == 0 || fragShader == 0) {
    //     sLog.exception("Can't compile vert or frag shader in Web");
    // }

    // this->m_prog = createShaderProgram(vertShader, fragShader);
    // if(this->m_prog==0){
    //     sLog.exception("Can't create program from shaders in Web");
    // }

}
CWeb::RenderHandler::~RenderHandler(){
    // glDeleteProgram(this->m_prog);
    // glDeleteBuffers(1, &this->m_vbo);
    // glDeleteVertexArrays(1, &this->m_vao);
}
//Required by CEF
void CWeb::RenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
    rect = CefRect(0, 0, this->m_webdata->getWidth(), this->m_webdata->getHeight());
}
//Will be executed in CEF message loop
void CWeb::RenderHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                         const RectList &dirtyRects, const void *buffer,
                                         int width, int height)
{
    //sLog.debug("BrowserView::RenderHandler::OnPaint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->texture());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT,
                         GL_UNSIGNED_BYTE, (unsigned char*)buffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    // GLCHECK(glActiveTexture(GL_TEXTURE0));
    // GLCHECK(glBindTexture(GL_TEXTURE_2D, this->texture()));
    // GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT,
    //                      GL_UNSIGNED_BYTE, (unsigned char*)buffer));
    // GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

}

const std::string CWeb::Type = "web";