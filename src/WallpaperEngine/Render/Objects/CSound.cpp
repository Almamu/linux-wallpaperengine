#include "CSound.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CSound::CSound (CScene* scene, Core::Objects::CSound* sound) :
    Render::CObject (scene, Type, sound),
    m_sound (sound)
{
    this->m_boundingBox = irr::core::aabbox3d<irr::f32>(0, 0, 0, 0, 0, 0);

    this->play ();
}

void CSound::play ()
{
    std::vector<std::string>* sounds = this->m_sound->getSounds ();
    std::vector<std::string>::const_iterator cur = sounds->begin ();
    std::vector<std::string>::const_iterator end = sounds->end ();

    for (; cur != end; cur ++)
    {
        SDL_RWops* rwops = nullptr;
        Mix_Music* music = nullptr;

        // open the sound file and read it fully
        irr::io::IReadFile* readfile = this->getScene ()->getContext ()->getDevice ()->getFileSystem ()->createAndOpenFile ((*cur).c_str ());
        long filesize = readfile->getSize ();
        char* buffer = new char [filesize];

        // TODO: IMPLEMENT A MAXIMUM FILESIZE TO PREVENT CRAZY ALLOCATIONS

        readfile->read (buffer, filesize);

        rwops = SDL_RWFromConstMem (buffer, filesize);
        music = Mix_LoadMUS_RW (rwops);

        // free the file reader
        readfile->drop ();

        if (music == nullptr)
        {
            this->getScene ()->getContext ()->getDevice ()->getLogger ()->log ("Cannot load audio", Mix_GetError (), irr::ELL_ERROR);
        }

        this->m_bufferReader.push_back (rwops);
        this->m_mixSdl.push_back (music);
        this->m_soundBuffer.push_back (buffer);
    }

    // after all the sounds are loaded, play them all
    std::vector<Mix_Music*>::const_iterator mixcur = this->m_mixSdl.begin ();
    std::vector<Mix_Music*>::const_iterator mixend = this->m_mixSdl.end ();

    for (; mixcur != mixend; mixcur ++)
    {
        if (Mix_PlayMusic ((*mixcur), -1) == -1)
        {
            this->getScene ()->getContext ()->getDevice ()->getLogger ()->log ("Cannot play audio", Mix_GetError (), irr::ELL_ERROR);
        }
    }
}

const irr::core::aabbox3d<irr::f32>& CSound::getBoundingBox () const
{
    return this->m_boundingBox;
}

const std::string CSound::Type = "sound";