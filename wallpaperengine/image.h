#ifndef WALLENGINE_MODEL_H
#define WALLENGINE_MODEL_H

#include <nlohmann/json.hpp>
#include <irrlicht/path.h>

namespace wp
{
    using json = nlohmann::json;

    class image : public object3d
    {
    public:
        image(json json_data, irr::io::path basepath);

    private:
        bool m_visible;

        irr::io::path m_file;
    };
};

#endif //WALLENGINE_MODEL_H
