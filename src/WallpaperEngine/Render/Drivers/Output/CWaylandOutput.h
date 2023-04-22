#pragma once

#include <map>
#include <string>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "COutput.h"

namespace WallpaperEngine::Render::Drivers
{
    class CWaylandOpenGLDriver;

    namespace Output
    {
        class CWaylandOutput : public COutput
        {
        public:
            CWaylandOutput (CApplicationContext& context, CWaylandOpenGLDriver& driver);
            ~CWaylandOutput ();

            void reset () override;

            bool renderVFlip () const override;
            bool renderMultiple () const override;
            bool haveImageBuffer () const override;
            void* getImageBuffer () const override;
            void updateRender () const override;

        private:
            void updateViewports();
        };
    }
}