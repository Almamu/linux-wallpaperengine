#include <wallpaperengine/fs/utils.h>
#include "effect.h"

namespace wp
{
    effect::effect (json json_data)
    {
        json::const_iterator file = json_data.find ("file");
        json::const_iterator pass = json_data.find ("passes");

        if (file != json_data.end () && (*file).is_string () == true)
        {
            this->m_file = ("effects/" + (*file).get <std::string> ()).c_str ();
        }
    }
};