#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::objects::particles
{
    using json = nlohmann::json;

    class initializer
    {
    public:
        std::string& getName ();
        irr::u32 getId ();
    protected:
        friend class particle;

        static initializer* fromJSON (json data);

        initializer (irr::u32 id, std::string name);
    private:
        irr::u32 m_id;
        std::string m_name;
    };
};
