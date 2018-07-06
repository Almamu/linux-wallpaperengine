#include <irrlicht/irrlicht.h>
#include <cstdint>
#include <iostream>

#include "common.h"
#include "wallpaperengine/shaders/compiler.h"
#include "wallpaperengine/fs/fileResolver.h"
#include "wallpaperengine/project.h"
#include "wallpaperengine/irrlicht.h"

irr::io::path _example_base_folder = "../res/";
irr::f32 g_AnimationSpeed = 0.1f;
irr::f32 g_Scale = 2.5f;
irr::f32 g_ScrollSpeed = 0.0f;
irr::f32 g_Direction = 0.0f;
irr::f32 g_Strength = 0.07f;
irr::f32 g_SpecularPower = 1.0f;
irr::f32 g_SpecularStrength = 1.0f;
irr::f32 g_SpecularColor [3] = {1.0f, 1.0f, 1.0f};
irr::f32 g_Texture1Resolution [4] = {1.0f, 1.0f, 1.0f, 1.0f};
irr::f32 g_Texture0 = 0;
irr::f32 g_Texture1 = 1;
irr::f32 g_Texture2 = 2;
irr::f32 g_Time = 0;
irr::core::vector3df _map_size = irr::core::vector3df (21, 11.5f, 16);

class MyShaderCallback : public irr::video::IShaderConstantSetCallBack
{
    virtual void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData)
    {
        irr::video::IVideoDriver* driver = services->getVideoDriver ();

        irr::core::matrix4 worldViewProj;
        worldViewProj = driver->getTransform(irr::video::ETS_PROJECTION);
        worldViewProj *= driver->getTransform(irr::video::ETS_VIEW);
        worldViewProj *= driver->getTransform(irr::video::ETS_WORLD);

        services->setVertexShaderConstant ("g_AnimationSpeed", &g_AnimationSpeed, 1);
        services->setVertexShaderConstant ("g_Scale", &g_Scale, 1);
        services->setVertexShaderConstant ("g_ScrollSpeed", &g_ScrollSpeed, 1);
        services->setVertexShaderConstant ("g_Direction", &g_Direction, 1);
        services->setVertexShaderConstant ("g_Time", &g_Time, 1);
        services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);
        services->setVertexShaderConstant ("g_Texture0Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture1Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture2Resolution", g_Texture1Resolution, 4);

        // TODO: Support up to 7 materials (as wallpaper engine)
        services->setPixelShaderConstant ("g_Strength", &g_Strength, 1);
        services->setPixelShaderConstant ("g_SpecularPower", &g_SpecularPower, 1);
        services->setPixelShaderConstant ("g_SpecularStrength", &g_SpecularStrength, 1);
        services->setPixelShaderConstant ("g_SpecularColor", g_SpecularColor, 3);
        services->setPixelShaderConstant ("g_Texture0", &g_Texture0, 1);
        services->setPixelShaderConstant ("g_Texture1", &g_Texture1, 1);
        services->setPixelShaderConstant ("g_Texture2", &g_Texture2, 1);
    }
};

class QuadSceneNode : public irr::scene::ISceneNode
{
    irr::core::aabbox3d <irr::f32> m_box;
    irr::video::S3DVertex m_vertices [4];
    irr::video::SMaterial m_material;

public:
    QuadSceneNode (irr::scene::ISceneNode* parent, irr::scene::ISceneManager* sceneManager, int32_t id)
            : irr::scene::ISceneNode (parent, sceneManager, id)
    {
        m_material.Wireframe = false;
        m_material.Lighting = false;

        m_vertices[0].Pos = irr::core::vector3df ( _map_size.X,  _map_size.Y, 0.0f); // bottom right
        m_vertices[1].Pos = irr::core::vector3df ( _map_size.X, -_map_size.Y, 0.0f); // top right
        m_vertices[2].Pos = irr::core::vector3df (-_map_size.X, -_map_size.Y, 0.0f); // top left
        m_vertices[3].Pos = irr::core::vector3df (-_map_size.X,  _map_size.Y, 0.0f); // bottom left

        m_vertices[0].TCoords = irr::core::vector2df (1.0f, 0.0f);
        m_vertices[1].TCoords = irr::core::vector2df (1.0f, 1.0f);
        m_vertices[2].TCoords = irr::core::vector2df (0.0f, 1.0f);
        m_vertices[3].TCoords = irr::core::vector2df (0.0f, 0.0f);

        m_vertices[0].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices[1].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices[2].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices[3].Color = irr::video::SColor (255, 255, 255, 255);

        m_box.reset (m_vertices[0].Pos);
        for (int32_t i = 1; i < 4; i ++)
        {
            m_box.addInternalPoint (m_vertices [i].Pos);
        }
    }

    virtual void OnRegisterSceneNode ()
    {
        if (IsVisible)
            SceneManager->registerNodeForRendering (this);

        ISceneNode::OnRegisterSceneNode ();
    }

    virtual void render ()
    {
        uint16_t indices[] = {
                0, 1, 2, 3
        };

        irr::video::IVideoDriver* driver = SceneManager->getVideoDriver ();

        driver->setMaterial (m_material);
        driver->drawVertexPrimitiveList (m_vertices, 4, indices, 1, irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT);
    }

    virtual const irr::core::aabbox3d <irr::f32>& getBoundingBox () const
    {
        return m_box;
    }

    virtual uint32_t getMaterialCount () const
    {
        return 1;
    }

    virtual irr::video::SMaterial& getMaterial (uint32_t i)
    {
        return m_material;
    }
};

int nier_test ()
{
    irr::io::path _wp_engine_folder = "/home/almamu/Downloads/nier__automata_-_become_as_gods_edition/";

    // set our working directory
    wp::fs::resolver.changeWorkingDirectory (_wp_engine_folder);
    // also append base folder for resources
    wp::fs::resolver.appendEnvironment ("");

    wp::project* wp_project = new wp::project (_wp_engine_folder);
    wp::fs::resolver.changeWorkingDirectory ("../res");

    irr::io::path _water_example = wp::fs::resolver.resolve ("materials/water-intact.png");
    irr::io::path _mud_example = wp::fs::resolver.resolve ("materials/plant-on-water.png");
    irr::io::path _background_example = wp::fs::resolver.resolve ("materials/top-part.png");
    irr::io::path _waterripple_normal = wp::fs::resolver.resolve ("materials/effects/waterripplenormal.png");
    irr::io::path _waterripple_frag_shader = wp::fs::resolver.resolve ("shaders/effects/waterripple_opengl.frag");
    irr::io::path _waterripple_vert_shader = wp::fs::resolver.resolve ("shaders/effects/waterripple_opengl.vert");
    irr::io::path _white = wp::fs::resolver.resolve ("materials/white.png");
    // irr::io::path _water_example = _example_base_folder; _water_example += "materials/water-intact.png";
    // irr::io::path _mud_example = _example_base_folder; _mud_example += "materials/plant-on-water.png";
    // irr::io::path _background_example = _example_base_folder; _background_example += "materials/top-part.png";
    // irr::io::path _waterripple_normal = _example_base_folder; _waterripple_normal += "materials/effects/waterripplenormal.png";
    // irr::io::path _waterripple_frag_shader = _example_base_folder; _waterripple_frag_shader += "shaders/effects/waterripple_opengl.frag";
    // irr::io::path _waterripple_vert_shader = _example_base_folder; _waterripple_vert_shader += "shaders/effects/waterripple_opengl.vert";
    // irr::io::path _white = _example_base_folder; _white += "materials/white.png";

    /*irr::video::E_DRIVER_TYPE driverType = irr::video::E_DRIVER_TYPE::EDT_OPENGL;
    device = irr::createDevice (driverType, irr::core::dimension2d<uint32_t>(1280, 720));

    device->setWindowCaption (L"Wallpaper engine simulation v0.1");
    driver = device->getVideoDriver ();*/

    // check for ps and vs support
    if (wp::irrlicht::driver->queryFeature (irr::video::EVDF_PIXEL_SHADER_1_1) == false && wp::irrlicht::driver->queryFeature (irr::video::EVDF_ARB_FRAGMENT_PROGRAM_1) == false)
    {
        wp::irrlicht::device->getLogger ()->log ("WARNING: Pixel shaders disabled because of missing driver/hardware support");
        _waterripple_frag_shader = "";
    }

    if (wp::irrlicht::driver->queryFeature (irr::video::EVDF_VERTEX_SHADER_1_1) == false && wp::irrlicht::driver->queryFeature (irr::video::EVDF_ARB_VERTEX_PROGRAM_1) == false)
    {
        wp::irrlicht::device->getLogger ()->log ("WARNING: Vertex shaders disabled because of missing driver/hardware support");
        _waterripple_vert_shader = "";
    }

    irr::video::IGPUProgrammingServices* gpuProgrammingServices = wp::irrlicht::driver->getGPUProgrammingServices ();

    int32_t materialType1 = 0;

    if (gpuProgrammingServices)
    {
        MyShaderCallback* shader = new MyShaderCallback ();

        wp::shaders::compiler _vert(_waterripple_vert_shader, wp::shaders::compiler::Type::Type_Vertex);
        wp::shaders::compiler _frag(_waterripple_frag_shader, wp::shaders::compiler::Type::Type_Pixel);

        materialType1 = gpuProgrammingServices->addHighLevelShaderMaterial(
                _vert.precompile ().c_str (), "vertexMain", irr::video::EVST_VS_2_0,
                _frag.precompile ().c_str (), "pixelMain", irr::video::EPST_PS_2_0,
                shader, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
        );

        shader->drop ();
    }

    // load some basic textures
    irr::video::ITexture*	waterTexture = wp::irrlicht::driver->getTexture (_water_example.c_str ());
    irr::video::ITexture*	mudTexture = wp::irrlicht::driver->getTexture (_mud_example.c_str ());
    irr::video::ITexture*	backgroundExample = wp::irrlicht::driver->getTexture (_background_example.c_str ());
    irr::video::ITexture*	waterRippleNormalTexture = wp::irrlicht::driver->getTexture (_waterripple_normal.c_str ());
    irr::video::ITexture*	whiteTexture = wp::irrlicht::driver->getTexture (_white.c_str ());

    // get scene manager
    irr::scene::ISceneManager* sceneManager = wp::irrlicht::device->getSceneManager ();

    sceneManager->addCameraSceneNode (0, irr::core::vector3df (0.0f, 0.0f, -_map_size.Z), irr::core::vector3df (0.0f, 0.0f, _map_size.Z));

    QuadSceneNode* backgroundNode = new QuadSceneNode (sceneManager->getRootSceneNode (), sceneManager, 666);
    QuadSceneNode* waterNode = new QuadSceneNode (sceneManager->getRootSceneNode (), sceneManager, 667);
    QuadSceneNode* mudNode = new QuadSceneNode (sceneManager->getRootSceneNode (), sceneManager, 668);

    backgroundNode->getMaterial (0).setTexture (0, backgroundExample);
    backgroundNode->setMaterialType (irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    mudNode->getMaterial (0).setTexture (0, mudTexture);
    mudNode->setMaterialType (irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    waterNode->setMaterialFlag(irr::video::EMF_LIGHTING, false);
    waterNode->setMaterialFlag(irr::video::EMF_BLEND_OPERATION, true);

    waterNode->getMaterial (0).setTexture (0, waterTexture);
    waterNode->getMaterial (0).setTexture (1, waterRippleNormalTexture);
    waterNode->getMaterial (0).setTexture (2, whiteTexture);
    waterNode->setMaterialType ( (irr::video::E_MATERIAL_TYPE) materialType1);

    irr::core::matrix4 identity; identity.makeIdentity ();
    irr::core::matrix4 orthoProjection; orthoProjection.buildProjectionMatrixOrthoLH (1.0f, 1.0f, 0.0f, 1.0f);

    wp::irrlicht::driver->setTransform (irr::video::ETS_PROJECTION, orthoProjection);
    wp::irrlicht::driver->setTransform (irr::video::ETS_VIEW, identity);
    wp::irrlicht::driver->setTransform (irr::video::ETS_WORLD, identity);

    int32_t lastTime = 0;
    int32_t minimumTime = 1000 / 90;
    int32_t currentTime = 0;

    while (wp::irrlicht::device->run () && wp::irrlicht::driver)
    {
        // if (device->isWindowActive ())
        {
            currentTime = wp::irrlicht::device->getTimer ()->getTime ();
            g_Time = currentTime / 1000.0f;

            if (currentTime - lastTime > minimumTime)
            {
                wp::irrlicht::driver->beginScene (true, true, irr::video::SColor(0, 0, 0, 0));
                sceneManager->drawAll ();
                wp::irrlicht::driver->endScene ();

                lastTime = currentTime;
            }
            else
            {
                wp::irrlicht::device->sleep (1, false);
            }
        }
    }

    return 0;
}