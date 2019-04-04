#ifndef WALLENGINE_TEXTURE_H
#define WALLENGINE_TEXTURE_H

#include <irrlicht/irrlicht.h>

namespace wp
{
    class texture
    {
    public:
        texture (irr::io::path& file);

        irr::video::ITexture* getIrrTexture ();

    private:
        irr::video::ITexture* m_texture;
    };
}


#endif //WALLENGINE_TEXTURE_H
