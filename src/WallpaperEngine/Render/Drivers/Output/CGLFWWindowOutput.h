#pragma once

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "COutput.h"

namespace WallpaperEngine::Render::Drivers::Output
{
    class CGLFWWindowOutput : public COutput
    {
    public:
        CGLFWWindowOutput (CApplicationContext& context, CVideoDriver& driver);

        void reset () override;
        bool renderVFlip () const override;
        bool renderMultiple () const override;
        bool haveImageBuffer () const override;
        void* getImageBuffer () const override;
        void updateRender () const override;

    private:
        void repositionWindow ();
    };
}