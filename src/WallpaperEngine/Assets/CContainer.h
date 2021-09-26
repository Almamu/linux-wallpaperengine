#pragma once

#include <string>
#include "WallpaperEngine/Assets/ITexture.h"

namespace WallpaperEngine::Assets
{
    class CContainer
    {
    public:
        /**
         * Reads the given file from the container and returns it's data
         * Additionally sets a length parameter to return back the file's length
         *
         * @param filename The file to read
         * @param length The file's length after it's been read, null for not getting anything back
         *
         * @return
         */
        virtual void* readFile (std::string filename, uint32_t* length = nullptr) = 0;

        /**
         * Wrapper for readFile, appends the texture extension at the end of the filename
         *
         * @param filename The texture name (without the .tex)
         *
         * @return
         */
        ITexture* readTexture (std::string filename);

        /**
         * Wrapper for readFile, appends the .vert extension at the end and opens the given shader file
         *
         * @param filename
         *
         * @return The shader code as an string to be used
         */
        std::string readVertexShader (const std::string& filename);

        /**
         * Wrapper for readFile, appends the .frag extension at the end and opens the given shader file
         *
         * @param filename
         *
         * @return The shader code as an string to be used
         */
        std::string readFragmentShader (const std::string& filename);

        /**
         * Wrapper for readFile, appends the .h extension at the end and opens the given shader file
         *
         * @param filename
         *
         * @return The shader code as an string to be used
         */
        std::string readIncludeShader (const std::string& filename);

    private:
        /**
         * Reads a file as string
         *
         * @param filename
         *
         * @return The file's contents as string
         */
        std::string readFileAsString (std::string filename);
    };
}