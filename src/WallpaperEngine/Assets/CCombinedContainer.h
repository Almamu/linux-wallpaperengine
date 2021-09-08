//
// Created by almamu on 8/8/21.
//

#pragma once

#include <vector>
#include <stdexcept>

#include "CContainer.h"

namespace WallpaperEngine::Assets
{
    class CCombinedContainer : public CContainer
    {
    public:
        CCombinedContainer () {}
        ~CCombinedContainer () {}

        /**
         * Adds a container to the list
         *
         * @param container
         */
        void add (CContainer* container);

        void* readFile (std::string filename, uint32_t* length) override;

    private:
        std::vector<CContainer*> m_containers;
    };
};