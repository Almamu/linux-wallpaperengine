#pragma once

#include <exception>
#include <string>

namespace WallpaperEngine::Assets
{
    class CPackageLoadException : public std::exception
    {
    public:
        CPackageLoadException(const std::string& message, const std::string& extrainfo = "");
        const char* what() const noexcept override;

    private:
        std::string m_message;
    };
}