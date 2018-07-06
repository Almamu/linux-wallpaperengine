#include "effect.h"

namespace wp
{
    effect::effect (json json_data, irr::io::path basepath)
    {
        json::const_iterator file = json_data.find ("file");
        json::const_iterator pass = json_data.find ("passes");

        if (file != json_data.end () && (*file).is_string () == true)
        {
            std::string file_s = *file;

            this->m_file = basepath + "/" + file_s.c_str ();
        }
    }
};