#pragma once

#include <string>
#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/CObject.h"

#include "CScene.h"

namespace WallpaperEngine::Render
{
    class CScene;

    class CObject : public irr::scene::ISceneNode
    {
    public:
        template<class T> const T* As () const { assert (Is<T> ()); return (const T*) this; }
        template<class T> T* As () { assert (Is<T> ()); return (T*) this; }

        template<class T> bool Is () { return this->m_type == T::Type; }

    protected:
        CObject (CScene* scene, std::string type, Core::CObject *object);
        ~CObject () override;

        void OnRegisterSceneNode () override;

        CScene* getScene () const;

    private:
        std::string m_type;

        CScene* m_scene;
        Core::CObject* m_object;
    };
}