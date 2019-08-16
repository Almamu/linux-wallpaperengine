//
// Created by almamu on 17/05/19.
//

#include <SDL_rwops.h>
#include <SDL_mixer.h>
#include "sound.h"
#include "WallpaperEngine/Irrlicht/Irrlicht.h"

namespace WallpaperEngine
{
    sound::sound (json json_data, WallpaperEngine::object* parent) : object3d (object3d::Type::Type_Material, parent)
    {
        json::const_iterator cur = json_data.begin ();
        json::const_iterator end = json_data.end ();

        for (; cur != end; cur ++)
        {
            this->m_filenames.push_back ((*cur).get <std::string> ());
        }

        this->play ();
    }

    void sound::play ()
    {
        std::vector<std::string>::const_iterator cur = this->m_filenames.begin ();
        std::vector<std::string>::const_iterator end = this->m_filenames.end ();

        for (; cur != end; cur ++)
        {
            SDL_RWops* sdlRwops = nullptr;
            Mix_Music* music = nullptr;
            irr::io::IReadFile* readfile = WallpaperEngine::Irrlicht::device->getFileSystem ()->createAndOpenFile ((*cur).c_str ());
            int filesize = readfile->getSize ();
            char* filebuffer = new char [filesize];

            readfile->read (filebuffer, filesize);

            sdlRwops = SDL_RWFromConstMem(filebuffer, filesize);
            music = Mix_LoadMUS_RW (sdlRwops);
            readfile->drop ();

            if (music == nullptr)
            {
                WallpaperEngine::Irrlicht::device->getLogger ()->log ("Cannot load audio", Mix_GetError (), irr::ELL_ERROR);
            }

            this->m_bufferReader.push_back (sdlRwops);
            this->m_soundBuffer.push_back (filebuffer);
            this->m_sdl.push_back (music);
        }

        std::vector<Mix_Music*>::const_iterator mixcur = this->m_sdl.begin ();
        std::vector<Mix_Music*>::const_iterator mixend = this->m_sdl.end ();

        for (; mixcur != mixend; mixcur ++)
        {
            if (Mix_PlayMusic ((*mixcur), -1) == -1)
            {
                WallpaperEngine::Irrlicht::device->getLogger ()->log ("Cannot play audio", Mix_GetError (), irr::ELL_ERROR);
            }
        }
    }
}