#ifndef __BASIC_SHADER_LOADER_H__
#define __BASIC_SHADER_LOADER_H__

#include <irrlicht/irrlicht.h>
#include <iostream>
#include <vector>
#include <map>

#include <WallpaperEngine/fs/utils.h>

namespace WallpaperEngine
{
    namespace shaders
    {
        /**
         * A basic shader loader that adds basic function definitions to every loaded shader
         */
        class compiler
        {
        public:
            /**
             * Basic struct used to define all the shader variables
             * the compiler will replace in pre-processing time
             * to make sure the shaders compile under OpenGL
             */
            struct VariableReplacement
            {
                const char* original;
                const char* replacement;
            };

            struct TypeName
            {
                const char* name;
                int size;
            };

            struct ShaderParameter
            {
                std::string type;
                std::string variableName;
                std::string identifierName;
                void* defaultValue;
                void* range [2];
            };

            /**
             * Types of shaders
             */
            enum Type
            {
                Type_Vertex = 0,
                Type_Pixel = 1,
            };

            /**
             * List of variables to replace when pre-process is performed
             */
            static std::map<std::string, std::string> sVariableReplacement;
            /**
             * Types of variables the pre-processor understands
             */
            static std::vector<std::string> sTypes;

            /**
             * Compiler constructor, loads the given shader file and prepares
             * the pre-processing and compilation of the shader, adding
             * required definitions if needed
             *
             * @param file The file to load
             * @param type The type of shader
             * @param recursive Whether the compiler should add base definitions or not
             */
            compiler (irr::io::path& file, Type type, std::map<std::string, int>* combos, bool recursive = false);
            /**
             * Performs the actual pre-compilation/pre-processing over the shader files
             * This step is kinda big, replaces variables names on sVariableReplacement,
             * ensures #include directives are correctly handled
             * and takes care of attribute comments for the wallpaper engine specifics
             *
             * @return The shader contents ready to be used by OpenGL
             */
            std::string precompile ();
            /**
             * Searches the list of parameters available for the parameter with the given value
             *
             * @param identifier The identifier to search for
             * @return The shader information
             */
            ShaderParameter* findParameter (std::string identifier);

            /**
             * @return The list of parameters available for this shader with their default values
             */
            std::vector <ShaderParameter*>& getParameters ();

        private:
            /**
             * Checks if there is "str" in the current position without advancing the
             * iterator in use
             *
             * @param str The string to check for
             * @param it  The position to start checking at
             *
             * @return
             */
            bool peekString (std::string str, std::string::const_iterator& it);
            /**
             * Checks for a semicolon as current character, advancing the iterator
             * after finding it, otherwise returns an error
             *
             * @param it The position where to expect the semicolon
             *
             * @return
             */
            bool expectSemicolon (std::string::const_iterator& it);
            /**
             * Ignores contiguous space characters in the string advancing the iterator
             * until the first non-space character
             *
             * @param it The iterator to increase
             */
            void ignoreSpaces (std::string::const_iterator& it);
            /**
             * Ignores all characters until next line-fee (\n) advancing the interator
             *
             * @param it The iterator to increase
             */
            void ignoreUpToNextLineFeed (std::string::const_iterator& it);
            /**
             * Ignores all characters until a block comment end is found, advancing the iterator
             *
             * @param it The iterator to increase
             */
            void ignoreUpToBlockCommentEnd (std::string::const_iterator& it);
            /**
             * Parses the current position as a variable type, extracts it and compares it
             * to the registered types in the pre-processor, returning it's name if valid
             * increasing the iterator at the same time
             *
             * @param it The position to extract it from
             *
             * @return The type name
             */
            std::string extractType (std::string::const_iterator& it);
            /**
             * Parses the current position as a variable name, extractig it's name and
             * increasing the iterator as the name is extracted
             *
             * @param it The position to start extracting the variable name from
             *
             * @return The variable name
             */
            std::string extractName (std::string::const_iterator& it);
            /**
             * Parses the current position as a quoted value, extracting it's value
             * and increasing the iterator at the same time
             *
             * @param it The position to start extracting the value from
             *
             * @return The value
             */
            std::string extractQuotedValue (std::string::const_iterator& it);
            /**
             * Tries to find the given shader file and compile it
             *
             * @param filename The shader's filename
             *
             * @return The compiled contents
             */
            std::string lookupShaderFile (std::string filename);
            /**
             * Searches for the given symbol in the replace table
             *
             * @param symbol The symbol to look for
             *
             * @return The symbol it should be replaced with
             */
            std::string lookupReplaceSymbol (std::string symbol);

            /**
             * @return Whether the character in the current position is a character or not
             */
            bool isChar (std::string::const_iterator& it);
            /**
             * @return Whether the character in the current position is a number or not
             */
            bool isNumeric (std::string::const_iterator& it);
            /**
             * Parses a COMBO value to add the proper define to the code
             *
             * @param content The parameter configuration
             */
            void parseComboConfiguration (const std::string& content);
            /**
             * Parses a parameter extra metadata created by wallpaper engine
             *
             * @param type The type of variable to parse
             * @param name The name of the variable in the shader (for actual variable declaration)
             * @param content The parameter configuration
             */
            void parseParameterConfiguration (const std::string& type, const std::string& name, const std::string& content);
            /**
             * The shader file this instance is loading
             */
            irr::io::path m_file;
            /**
             * The original file content
             */
            std::string m_content;
            /**
             * The final, compiled content ready to be used by OpenGL
             */
            std::string m_compiledContent;
            /**
             * Whether there was any kind of error in the compilation or not
             */
            bool m_error;
            /**
             * Extra information about the error (if any)
             */
            std::string m_errorInfo;
            /**
             * The type of shader
             */
            Type m_type;
            /**
             * The parameters the shader needs
             */
            std::vector <ShaderParameter*> m_parameters;
            /**
             * The combos the shader should be generated with
             */
             std::map <std::string, int>* m_combos;
             /**
              * Whether this compilation is a recursive one or not
              */
             bool m_recursive;
        };
    }
}

#endif /* !__BASIC_SHADER_LOADER_H__ */