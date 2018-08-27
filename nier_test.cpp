#include <irrlicht/irrlicht.h>
#include <cstdint>
#include <iostream>
#include <wallpaperengine/config.h>
#include <wallpaperengine/video/renderer.h>
#include <wallpaperengine/video/material.h>

#include "common.h"
#include "wallpaperengine/shaders/compiler.h"
#include "wallpaperengine/project.h"
#include "wallpaperengine/irrlicht.h"
#include "nier_test.h"

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

int nier_test ()
{
    do_decompress ();
    irr::io::path _wp_engine_folder = "/home/almamu/Development/tmp/nier__automata_-_become_as_gods_edition/";

    // set our working directory
    wp::fs::resolver.changeWorkingDirectory (_wp_engine_folder);
    wp::project* wp_project = new wp::project ();
    /*wp::fs::resolver.changeWorkingDirectory (wp::config::path::resources);

    irr::io::path _water_example = wp::fs::resolver.resolve ("materials/water-intact.png");
    irr::io::path _mud_example = wp::fs::resolver.resolve ("materials/plant-on-water.png");
    irr::io::path _background_example = wp::fs::resolver.resolve ("materials/top-part.png");
    irr::io::path _waterripple_normal = wp::fs::resolver.resolve ("materials/effects/waterripplenormal.png");
    irr::io::path _waterripple_frag_shader = wp::fs::resolver.resolve ("shaders/effects/waterripple_opengl.frag");
    irr::io::path _waterripple_vert_shader = wp::fs::resolver.resolve ("shaders/effects/waterripple_opengl.vert");
    irr::io::path _white = wp::fs::resolver.resolve ("materials/white.png");

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
    irr::scene::ISceneManager* sceneManager = wp::irrlicht::device->getSceneManager ();*/

    if (wp_project->getScene ()->isOrthogonal() == true)
    {
        wp::video::renderer::setupOrthographicCamera (
                wp_project->getScene ()->getProjectionWidth (),
                wp_project->getScene ()->getProjectionHeight (),
                wp_project->getScene ()->getCamera ()->getCenter (),
                wp_project->getScene ()->getCamera ()->getEye (),
                wp_project->getScene ()->getCamera ()->getUp ().X,
                wp_project->getScene ()->getCamera ()->getUp ().Y
        );
    }
    else
    {
        wp::irrlicht::device->getLogger ()->log ("Non-orthogonal cameras not supported yet!!");
        return 0;
    }

/*    wp::video::material* waterNode = new wp::video::material (irr::core::vector3df (960.0f, 540.0f, 0.0f), wp_project->getScene ());
    wp::video::material* backgroundNode = new wp::video::material (irr::core::vector3df (960.0f, 540.0f, 0.0f), wp_project->getScene ());
    wp::video::material* mudNode = new wp::video::material (irr::core::vector3df (960.0f, 540.0f, 0.0f), wp_project->getScene ());

    backgroundNode->getMaterial ().setTexture (0, backgroundExample);
    backgroundNode->setType (irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    mudNode->getMaterial ().setTexture (0, mudTexture);
    mudNode->setType (irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    waterNode->setFlag (irr::video::EMF_LIGHTING, false);
    waterNode->setFlag (irr::video::EMF_BLEND_OPERATION, true);

    waterNode->getMaterial ().setTexture (0, waterTexture);
    waterNode->getMaterial ().setTexture (1, waterRippleNormalTexture);
    waterNode->getMaterial ().setTexture (2, whiteTexture);
    waterNode->setType ( (irr::video::E_MATERIAL_TYPE) materialType1);*/

    // register nodes
    wp::video::renderer::queueNode (wp_project->getScene ());
    // wp::video::renderer::queueNode (backgroundNode);
    // wp::video::renderer::queueNode (mudNode);
    // wp::video::renderer::queueNode (waterNode);

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
                wp::video::renderer::render ();
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