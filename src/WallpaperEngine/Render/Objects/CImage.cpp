#include "CImage.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

extern irr::f32 g_Time;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image)
{
    // TODO: INITIALIZE NEEDED EFFECTS AND PROPERLY CALCULATE THESE?
    irr::f32 xsize = this->m_image->getSize ().X;
    irr::f32 ysize = this->m_image->getSize ().Y;
    irr::f32 xscale = this->m_image->getScale ().X;
    irr::f32 yscale = this->m_image->getScale ().Y;

    if (xsize == 0.0f || ysize == 0.0f)
    {
        xsize = 1920.0f;
        ysize = 1080.0f;

        this->getScene ()->getContext ()->getDevice ()->getLogger ()->log (
            "Initializing xsise and ysize as default values 1920 and 1080",
            this->getImage ()->getName ().c_str ()
        );
    }

    irr::core::vector3df cameraCenter = this->getScene ()->getCamera ()->getCenter ();

    // take the orthogonal projection into account
    irr::f32 scene_width = this->getScene ()->getScene ()->getOrthogonalProjection ()->getWidth ();
    irr::f32 scene_height = this->getScene ()->getScene ()->getOrthogonalProjection ()->getHeight ();

    irr::f32 xright     = (-scene_width  / 2.0f + this->m_image->getOrigin ().X + xsize * xscale / 2.0f) + cameraCenter.X;
    irr::f32 xleft      = (-scene_width  / 2.0f + this->m_image->getOrigin ().X - xsize * xscale / 2.0f) + cameraCenter.X;
    irr::f32 ytop       = (-scene_height / 2.0f + this->m_image->getOrigin ().Y + ysize * yscale / 2.0f) + cameraCenter.Y;
    irr::f32 ybottom    = (-scene_height / 2.0f + this->m_image->getOrigin ().Y - ysize * yscale / 2.0f) + cameraCenter.Y;
    irr::f32 z          = this->m_image->getOrigin ().Z;

    // top left
    this->m_vertex [0].Pos = irr::core::vector3df (xleft,  ytop,    z);
    // top right
    this->m_vertex [1].Pos = irr::core::vector3df (xright, ytop,    z);
    // bottom right
    this->m_vertex [2].Pos = irr::core::vector3df (xright, ybottom, z);
    // bottom left
    this->m_vertex [3].Pos = irr::core::vector3df (xleft,  ybottom, z);

    this->m_vertex [0].TCoords = irr::core::vector2df (1.0f, 0.0f);
    this->m_vertex [1].TCoords = irr::core::vector2df (0.0f, 0.0f);
    this->m_vertex [2].TCoords = irr::core::vector2df (0.0f, 1.0f);
    this->m_vertex [3].TCoords = irr::core::vector2df (1.0f, 1.0f);

    this->m_vertex [0].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [1].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [2].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [3].Color = irr::video::SColor (255, 255, 255, 255);

    this->setAutomaticCulling (irr::scene::EAC_OFF);
    this->m_boundingBox = irr::core::aabbox3d<irr::f32>(0, 0, 0, 0, 0, 0);

    // load the texture in the main material
    irr::io::path texturepath = this->getScene ()->getContext ()->resolveMaterials (
        (*(*this->m_image->getMaterial ()->getPasses ().begin ())->getTextures ().begin ())
    );
    irr::video::ITexture *texture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getTexture (texturepath);

    this->m_material = new Render::Objects::Effects::CMaterial (this->getScene ()->getContext (), this, this->m_image->getMaterial (), texture);

    auto effectsCur = this->m_image->getEffects ().begin ();
    auto effectsEnd = this->m_image->getEffects ().end ();

    texture = this->m_material->getOutputTexture ();

    for (; effectsCur != effectsEnd; effectsCur ++)
    {
        CEffect* effect = new CEffect (this, *effectsCur, this->getScene ()->getContext (), texture);

        this->m_effects.push_back (effect);

        texture = effect->getOutputTexture ();
    }

    this->generateMaterial (texture);
}

void CImage::render()
{
    uint16_t indices [] =
    {
        3, 2, 1, 0
    };

    irr::video::IVideoDriver* driver = SceneManager->getVideoDriver ();

    // first render the base material
    this->m_material->render ();

    // now render all the effects, they should already be linked to each other
    auto effectCur = this->m_effects.begin ();
    auto effectEnd = this->m_effects.end ();
    size_t passes = 0;

    for (; effectCur != effectEnd; effectCur ++)
    {
        auto matCur = (*effectCur)->getMaterials ().begin ();
        auto matEnd = (*effectCur)->getMaterials ().end ();

        for (; matCur != matEnd; matCur ++)
            passes += (*matCur)->getPasses ().size ();

        (*effectCur)->render ();
    }

    passes ++;

    // depending on the number of passes we might need to flip the texture
    if (passes % 2 == 0)
    {
        // set render target to the screen
        driver->setRenderTarget (irr::video::ERT_FRAME_BUFFER, false, false);
        // set the material to use
        driver->setMaterial (this->m_irrlichtMaterialInvert);
        // draw it
        driver->drawVertexPrimitiveList (
                this->m_vertex, 4, indices, 1,
            irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
        );
    }
    else
    {
        // set render target to the screen
        driver->setRenderTarget (irr::video::ERT_FRAME_BUFFER, false, false);
        // set the material to use
        driver->setMaterial (this->m_irrlichtMaterial);
        // draw it
        driver->drawVertexPrimitiveList (
                this->m_vertex, 4, indices, 1,
            irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
        );
    }
}

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

const irr::core::aabbox3d<irr::f32>& CImage::getBoundingBox() const
{
    return this->m_boundingBox;
}

const Core::Objects::CImage* CImage::getImage () const
{
    return this->m_image;
}

const std::vector<CEffect*>& CImage::getEffects () const
{
    return this->m_effects;
}

const irr::video::S3DVertex* CImage::getVertex () const
{
    return this->m_vertex;
}

const std::string CImage::Type = "image";