#pragma once

#include <map>
#include <string>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "COutput.h"

namespace WallpaperEngine::Render::Drivers::Output
{
    class CWaylandOutput : public COutput
    {
    public:
        CWaylandOutput (CApplicationContext& context, CVideoDriver& driver, Detectors::CFullScreenDetector& detector);
        ~CWaylandOutput ();

        void reset () override;

        bool renderVFlip () const override;
        bool renderMultiple () const override;
        bool haveImageBuffer () const override;
        void* getImageBuffer () const override;
        void updateRender () const override;

    private:
        void updateViewports();

        CVideoDriver& m_driver;
    };
}