#include <SDL.h>
#include <SDL_rwops.h>
#include <SDL_mixer.h>

#include "CSound.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (CScene* scene, Core::Objects::CSound* sound) :
    CObject (scene, Type, sound),
    m_sound (sound)
{
    this->load ();
    this->play ();
}

void CSound::load ()
{
    if (SDL_WasInit (SDL_INIT_AUDIO) != SDL_INIT_AUDIO)
    {
        return;
    }

    auto cur = this->m_sound->getSounds ().begin ();
    auto end = this->m_sound->getSounds ().end ();

    for (; cur != end; cur ++)
    {
        uint32_t filesize = 0;
        void* filebuffer = this->getContainer ()->readFile ((*cur), &filesize);

        SDL_RWops* sdlRwops = SDL_RWFromConstMem (filebuffer, filesize);
        Mix_Music* music = Mix_LoadMUS_RW (sdlRwops);

        if (music == nullptr)
            throw std::runtime_error ("cannot load audio");

        this->m_bufferReader.push_back (sdlRwops);
        this->m_soundBuffer.push_back (filebuffer);
        this->m_sdl.push_back (music);
    }
}
void CSound::play ()
{
    if (SDL_WasInit (SDL_INIT_AUDIO) != SDL_INIT_AUDIO)
    {
        return;
    }

    auto mixcur = this->m_sdl.begin ();
    auto mixend = this->m_sdl.end ();

    for (; mixcur != mixend; mixcur ++)
    {
        if (Mix_PlayMusic ((*mixcur), -1) == -1)
            throw std::runtime_error ("cannot play audio");
    }
}

void CSound::render ()
{

}

const std::string CSound::Type = "sound";