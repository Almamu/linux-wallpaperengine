#ifndef WALLENGINE_EFFECT_H
#define WALLENGINE_EFFECT_H

#include <nlohmann/json.hpp>
#include <irrlicht/path.h>

namespace wp
{
    using json = nlohmann::json;

    class effect
    {
    public:
        effect (json json_data, irr::io::path basepath);

    private:
        irr::io::path m_file;
        std::vector<void*> m_passes;
    };
}


#endif //WALLENGINE_EFFECT_H
