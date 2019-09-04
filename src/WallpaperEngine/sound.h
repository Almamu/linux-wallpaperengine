#ifndef WALLENGINE_SOUND_H
#define WALLENGINE_SOUND_H

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>
#include <SDL_mixer.h>

#include <WallpaperEngine/object3d.h>
#include <WallpaperEngine/object.h>

namespace WallpaperEngine
{
    using json = nlohmann::json;

    class sound : public WallpaperEngine::object3d
    {
    public:
        sound (json json_data, WallpaperEngine::object* parent);

        void play ();

    private:
        std::vector<std::string> m_filenames;
        std::vector <Mix_Music*> m_sdl;
        std::vector <SDL_RWops*> m_bufferReader;
        std::vector <void*> m_soundBuffer;
    };
};


#endif //WALLENGINE_SOUND_H
