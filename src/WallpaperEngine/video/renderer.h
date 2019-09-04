#ifndef WALLENGINE_RENDER_H
#define WALLENGINE_RENDER_H

#include <vector>

#include <WallpaperEngine/video/node.h>
#include <WallpaperEngine/scene.h>

namespace WallpaperEngine
{
    namespace video
    {
        class renderer
        {
        public:
            static void queueNode (node* node);
            static void setupOrthographicCamera (WallpaperEngine::scene* scene);
            static void setupOrthographicCamera (irr::f32 width, irr::f32 height, irr::core::vector3df position, irr::core::vector3df lookat, irr::f32 znear, irr::f32 zfar);
            static void render ();

        private:
            static std::vector<node*> s_nodes;
        };
    }
}


#endif //WALLENGINE_RENDER_H
