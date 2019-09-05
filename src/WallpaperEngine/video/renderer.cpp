#include <WallpaperEngine/video/renderer.h>
#include <WallpaperEngine/Irrlicht/CContext.h>

extern WallpaperEngine::Irrlicht::CContext* IrrlichtContext;

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
            s_camera = IrrlichtContext->getDevice ()->getSceneManager ()->addCameraSceneNode (0, position, lookat);
            s_camera->setProjectionMatrix (orthoProjection);

            IrrlichtContext->getDevice ()->getVideoDriver ()->setTransform (irr::video::ETS_PROJECTION, orthoProjection);
            IrrlichtContext->getDevice ()->getVideoDriver ()->setTransform (irr::video::ETS_VIEW, identity);
            IrrlichtContext->getDevice ()->getVideoDriver ()->setTransform (irr::video::ETS_WORLD, identity);
        }

        void renderer::render ()
        {
            if (IrrlichtContext->getDevice ()->getVideoDriver () == nullptr) return;

            IrrlichtContext->getDevice ()->getVideoDriver ()->beginScene(true, true, irr::video::SColor(0, 0, 0, 0));

            std::vector<node*>::const_iterator cur = s_nodes.begin ();
            std::vector<node*>::const_iterator end = s_nodes.end ();

            for(; cur != end; cur ++)
            {
                (*cur)->render ();
            }

            IrrlichtContext->getDevice ()->getVideoDriver ()->endScene ();
        }

        std::vector<node*> renderer::s_nodes;
        irr::scene::ICameraSceneNode* renderer::s_camera;
    }
};