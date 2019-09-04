#include <WallpaperEngine/Irrlicht/Irrlicht.h>
#include <WallpaperEngine/video/renderer.h>

namespace WallpaperEngine
{
    namespace video
    {
        void renderer::queueNode (node* node)
        {
            s_nodes.push_back (node);
        }

        void renderer::setupOrthographicCamera (WallpaperEngine::scene* scene)
        {
            setupOrthographicCamera (
                    scene->getProjectionWidth (),
                    scene->getProjectionHeight (),
                    scene->getCamera ()->getCenter (),
                    scene->getCamera ()->getEye (),
                    scene->getCamera ()->getUp ().X,
                    scene->getCamera ()->getUp ().Y
            );
        }

        void renderer::setupOrthographicCamera (irr::f32 width, irr::f32 height, irr::core::vector3df position, irr::core::vector3df lookat, irr::f32 znear, irr::f32 zfar)
        {
            irr::core::matrix4 identity; identity.makeIdentity ();
            irr::core::matrix4 orthoProjection; orthoProjection.buildProjectionMatrixOrthoLH(
                    width,
                    height,
                    znear,
                    zfar
            );
            WallpaperEngine::Irrlicht::camera = WallpaperEngine::Irrlicht::device->getSceneManager ()->addCameraSceneNode (0, position, lookat);
            WallpaperEngine::Irrlicht::camera->setProjectionMatrix (orthoProjection);

            WallpaperEngine::Irrlicht::driver->setTransform (irr::video::ETS_PROJECTION, orthoProjection);
            WallpaperEngine::Irrlicht::driver->setTransform (irr::video::ETS_VIEW, identity);
            WallpaperEngine::Irrlicht::driver->setTransform (irr::video::ETS_WORLD, identity);
        }

        void renderer::render ()
        {
            if (WallpaperEngine::Irrlicht::driver == nullptr) return;

            WallpaperEngine::Irrlicht::driver->beginScene(true, true, irr::video::SColor(0, 0, 0, 0));

            std::vector<node*>::const_iterator cur = s_nodes.begin ();
            std::vector<node*>::const_iterator end = s_nodes.end ();

            for(; cur != end; cur ++)
            {
                (*cur)->render ();
            }

            WallpaperEngine::Irrlicht::driver->endScene ();
        }

        std::vector<node*> renderer::s_nodes;
    }
};