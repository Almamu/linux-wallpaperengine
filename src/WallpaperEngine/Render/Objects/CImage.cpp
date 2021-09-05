#include "CImage.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image)
{
    auto projection = this->getScene ()->getScene ()->getOrthogonalProjection ();

    // get scene width and height to calculate positions
    auto scene_width = static_cast <float> (projection->getWidth ());
    auto scene_height = static_cast <float> (projection->getHeight ());

    float xleft = 0.0f;
    float ytop = 0.0f;
    float xright = 0.0f;
    float ybottom = 0.0f;

    // TODO: TAKE INTO ACCOUNT SCALE
    // depending on the alignment these values might change, for now just support center
    if (this->getImage ()->getAlignment () == "center")
    {
        // calculate the real position of the image
        xleft = this->getImage ()->getOrigin ().x - (this->getImage ()->getSize ().x / 2);
        xright = this->getImage ()->getOrigin ().x + (this->getImage ()->getSize ().x / 2);
        ytop = this->getImage ()->getOrigin ().y - (this->getImage ()->getSize ().y / 2);
        ybottom = this->getImage ()->getOrigin ().y + (this->getImage ()->getSize ().y / 2);
    }
    else
    {
        throw std::runtime_error ("Only centered images are supported for now!");
    }

    // load image from the .tex file
    uint32_t textureSize = 0;

    // get the first texture on the first pass (this one represents the image assigned to this object)
    this->m_texture = this->getScene ()->getContainer ()->readTexture (
            (*(*this->m_image->getMaterial ()->getPasses ().begin ())->getTextures ().begin ())
    );

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat data [] = {
        xleft, ytop, 0.0f,
        xright, ytop, 0.0f,
        xleft, ybottom, 0.0f,
        xleft, ybottom, 0.0f,
        xright, ytop, 0.0f,
        xright, ybottom, 0.0f
    };

    memcpy (this->m_vertexList, data, sizeof (data));

    GLfloat data1 [] = {
        0.0f, 0.0f, 0.0f,
        scene_width, 0.0f, 0.0f,
        0.0f, scene_height, 0.0f,
        0.0f, scene_height, 0.0f,
        scene_width, 0.0f, 0.0f,
        scene_width, scene_height, 0.0f
    };

    memcpy (this->m_passesVertexList, data1, sizeof (data1));

    float width = 1.0f;
    float height = 1.0f;

    // calculate the correct texCoord limits for the texture based on the texture screen size and real size
    if (this->getTexture ()->getHeader ()->textureWidth != this->getTexture ()->getHeader ()->width ||
        this->getTexture ()->getHeader ()->textureHeight != this->getTexture ()->getHeader ()->height)
    {
        uint32_t x = 1;
        uint32_t y = 1;

        while (x < this->getImage ()->getSize ().x) x <<= 1;
        while (y < this->getImage ()->getSize ().y) y <<= 1;

        width = this->getImage ()->getSize ().x / x;
        height = this->getImage ()->getSize ().y / y;
    }

    GLfloat data2 [] = {
        0.0f, 0.0f,
        width, 0.0f,
        0.0f, height,
        0.0f, height,
        width, 0.0f,
        width, height
    };

    memcpy (this->m_texCoordList, data2, sizeof (data2));

    GLfloat data3 [] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    memcpy (this->m_passTexCoordList, data3, sizeof (data3));

    // bind vertex list to the openGL buffers
    glGenBuffers (1, &this->m_vertexBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_vertexBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_vertexList), this->m_vertexList, GL_STATIC_DRAW);

    // bind pass' vertex list to the openGL buffers
    glGenBuffers (1, &this->m_passesVertexBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_passesVertexBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_passesVertexList), this->m_passesVertexList, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_texCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_texCoordList), this->m_texCoordList, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_passTexCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_passTexCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_passTexCoordList), this->m_passTexCoordList, GL_STATIC_DRAW);

    // generate the main material used to render the image
    this->m_material = new Effects::CMaterial (this, this->m_image->getMaterial ());

    // generate the effects used by this material
    auto cur = this->getImage ()->getEffects ().begin ();
    auto end = this->getImage ()->getEffects ().end ();

    for (; cur != end; cur ++)
        this->m_effects.emplace_back (new CEffect (this, *cur));
}

void CImage::render ()
{
    // ensure this image is visible first
    if (this->getImage ()->isVisible () == false)
        return;

    GLuint drawTo = this->getScene()->getWallpaperFramebuffer();
    GLuint inputTexture = this->m_texture->getTextureID ();

    // pinpong current buffer
    this->getScene ()->pinpongFramebuffer (&drawTo, nullptr);
    // render all the other materials
    auto cur = this->getEffects ().begin ();
    auto end = this->getEffects ().end ();
    auto begin = this->getEffects ().begin ();

    for (; cur != end; cur ++)
    {
        if (cur != begin)
            // pinpong current buffer
            this->getScene ()->pinpongFramebuffer (&drawTo, &inputTexture);

        // render now
        (*cur)->render (drawTo, inputTexture);
    }

    if (this->getEffects ().size () > 0)
        this->getScene ()->pinpongFramebuffer (nullptr, &inputTexture);

    // render the main material
    this->m_material->render (this->getScene()->getWallpaperFramebuffer(), inputTexture);
/*
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    // set the viewport, for now use the scene width/height but we might want to use image's size TODO: INVESTIGATE THAT
    glViewport (0, 0, projection->getWidth (), projection->getHeight ());
    // bind the texture to the first slot
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, inputTexture);
    // now render the last pass (to screen)
    glBindBuffer (GL_ARRAY_BUFFER, *this->m_material->getImage ()->getVertexBuffer ());
    glDrawArrays (GL_TRIANGLES, 0, 6);*/
}

/*
void CImage::generateMaterial (irr::video::ITexture* resultTexture)
{
    this->m_irrlichtMaterial.setTexture (0, resultTexture);
    this->m_irrlichtMaterial.MaterialType = irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_irrlichtMaterial.Wireframe = false;
    this->m_irrlichtMaterial.Lighting = false;

    /// TODO: XXXHACK: This material is used to flip textures upside down based on the amount of passes
    /// TODO: XXXHACK: This fixes an issue with opengl render that I had no better idea of how to solve
    /// TODO: XXXHACK: For the love of god, If you have a better fix, please LET ME KNOW!
    std::string vertex =
            "#define mul(x, y) (y * x)\n"
            "uniform mat4 g_ModelViewProjectionMatrix;\n"
            "// Pass to fragment shader with the same name\n"
            "varying vec2 v_texcoord;\n"
            "void main(void)\n"
            "{\n"
            "    gl_Position = mul(vec4(gl_Vertex.xyz, 1.0), g_ModelViewProjectionMatrix);\n"
            "    \n"
            "    // The origin of the texture coordinates locates at bottom-left \n"
            "    // corner rather than top-left corner as defined on screen quad.\n"
            "    // Instead of using texture coordinates passed in by OpenGL, we\n"
            "    // calculate TexCoords based on vertex position as follows.\n"
            "    //\n"
            "    // Vertex[0] (-1, -1) to (0, 0)\n"
            "    // Vertex[1] (-1,  1) to (0, 1)\n"
            "    // Vertex[2] ( 1,  1) to (1, 1)\n"
            "    // Vertex[3] ( 1, -1) to (1, 0)\n"
            "    // \n"
            "    // Texture coordinate system in OpenGL operates differently from \n"
            "    // DirectX 3D. It is not necessary to offset TexCoords to texel \n"
            "    // center by adding 1.0 / TextureSize / 2.0\n"
            "    \n"
            "    v_texcoord = vec2(gl_MultiTexCoord0.x, gl_MultiTexCoord0.y);"
            "}";

    std::string fragment =
            "// Texture sampler\n"
            "uniform sampler2D TextureSampler;\n"
            "\n"
            "// TexCoords from vertex shader\n"
            "varying vec2 v_texcoord;\n"
            "\n"
            "void main (void)\n"
            "{\n"
            "    gl_FragColor = texture2D(TextureSampler, v_texcoord);\n"
            "}";

    this->m_irrlichtMaterialInvert.setTexture (0, resultTexture);
    this->m_irrlichtMaterialInvert.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_irrlichtMaterialInvert.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_irrlichtMaterialInvert.Wireframe = false;
    this->m_irrlichtMaterialInvert.Lighting = false;
    this->m_irrlichtMaterialInvert.MaterialType = (irr::video::E_MATERIAL_TYPE)
    this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getGPUProgrammingServices ()->addHighLevelShaderMaterial (
        vertex.c_str (), "main", irr::video::EVST_VS_2_0,
        fragment.c_str (), "main", irr::video::EPST_PS_2_0,
        this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
    );
}

void CImage::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    irr::s32 g_Texture0 = 0;

    irr::video::IVideoDriver* driver = services->getVideoDriver ();

    irr::core::matrix4 worldViewProj;
    worldViewProj = driver->getTransform (irr::video::ETS_PROJECTION);
    worldViewProj *= driver->getTransform (irr::video::ETS_VIEW);
    worldViewProj *= driver->getTransform (irr::video::ETS_WORLD);


    services->setPixelShaderConstant ("TextureSampler", &g_Texture0, 1);
    services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);
}
*/

const CTexture* CImage::getTexture () const
{
    return this->m_texture;
}

const Core::Objects::CImage* CImage::getImage () const
{
    return this->m_image;
}

const std::vector<CEffect*>& CImage::getEffects () const
{
    return this->m_effects;
}

const GLfloat* CImage::getVertex () const
{
    return this->m_vertexList;
}

const GLuint* CImage::getVertexBuffer () const
{
    return &this->m_vertexBuffer;
}

const GLuint* CImage::getPassVertexBuffer () const
{
    return &this->m_passesVertexBuffer;
}

const GLuint* CImage::getTexCoordBuffer () const
{
    return &this->m_texCoordBuffer;
}

const GLuint* CImage::getPassTexCoordBuffer () const
{
    return &this->m_passTexCoordBuffer;
}

const std::string CImage::Type = "image";