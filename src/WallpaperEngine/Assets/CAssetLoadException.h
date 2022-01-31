#pragma once

#include <exception>
#include <string>

namespace WallpaperEngine::Assets
{
    class CAssetLoadException : public std::exception
    {
    public:
        CAssetLoadException(const std::string& filename, const std::string& extrainfo = "");
        const char* what() const noexcept override;

    private:
        std::string m_message;
    };
};