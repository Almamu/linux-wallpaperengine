#include <SDL_rwops.h>
#include <SDL_mixer.h>

#include "CSound.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (CScene* scene, Core::Objects::CSound* sound) :
    CObject (scene, Type, sound),
    m_sound (sound)
{
    this->setAutomaticCulling (irr::scene::EAC_OFF);
    this->m_boundingBox = irr::core::aabbox3d<irr::f32>(0, 0, 0, 0, 0, 0);

    this->load ();
    this->play ();
}

void CSound::load ()
{
    std::vector<std::string>::const_iterator cur = this->m_sound->getSounds ()->begin ();
    std::vector<std::string>::const_iterator end = this->m_sound->getSounds ()->end ();

    for (; cur != end; cur ++)
    {
        SDL_RWops* sdlRwops = nullptr;
        Mix_Music* music = nullptr;
        irr::io::IReadFile* readfile = this->getScene ()->getContext ()->getDevice ()->getFileSystem ()->createAndOpenFile ((*cur).c_str ());
        int filesize = readfile->getSize ();
        char* filebuffer = new char [filesize];

        readfile->read (filebuffer, filesize);

        sdlRwops = SDL_RWFromConstMem(filebuffer, filesize);
        music = Mix_LoadMUS_RW (sdlRwops);
        readfile->drop ();

        if (music == nullptr)
        {
            this->getScene ()->getContext ()->getDevice ()->getLogger ()->log (
                "cannot load audio", Mix_GetError (), irr::ELL_ERROR
            );

            continue;
        }

        this->m_bufferReader.push_back (sdlRwops);
        this->m_soundBuffer.push_back (filebuffer);
        this->m_sdl.push_back (music);
    }
}
void CSound::play ()
{
    std::vector<Mix_Music*>::const_iterator mixcur = this->m_sdl.begin ();
    std::vector<Mix_Music*>::const_iterator mixend = this->m_sdl.end ();

    for (; mixcur != mixend; mixcur ++)
    {
        if (Mix_PlayMusic ((*mixcur), -1) == -1)
        {
            this->getScene ()->getContext ()->getDevice ()->getLogger ()->log (
                "cannot play audio", Mix_GetError (), irr::ELL_ERROR
            );
        }
    }
}

void CSound::render ()
{

}

const irr::core::aabbox3d<irr::f32>& CSound::getBoundingBox() const
{
    return this->m_boundingBox;
}

const std::string CSound::Type = "sound";