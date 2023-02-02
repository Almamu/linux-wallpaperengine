#pragma once

#include <memory>
#include <vector>
#include <ostream>
#include <sstream>

namespace WallpaperEngine::Logging
{
    /**
     * Singleton class, simplifies logging for the whole app
     */
    class CLog
    {
    public:
        CLog ();

        void addOutput (std::ostream* stream);
        void addError (std::ostream* stream);

        template<typename... Data>
        void out (Data... data)
        {
            // buffer the string first
            std::stringbuf buffer;
            std::ostream bufferStream (&buffer);

            ((bufferStream << std::forward<Data>(data)), ...);

            // then send it to all the outputs configured
            for (auto cur : this->mOutputs)
                *cur << buffer.str () << std::endl;
        }

        template<typename... Data>
        void debug (Data... data)
        {
#if DEBUG && !ERRORONLY
            // buffer the string first
            std::stringbuf buffer;
            std::ostream bufferStream (&buffer);

            ((bufferStream << std::forward<Data> (data)), ...);

            // then send it to all the outputs configured
            for (auto cur : this->mOutputs)
                *cur << buffer.str () << std::endl;
#endif /* DEBUG */
        }

        template<typename... Data>
        void debugerror (Data... data)
        {
#if DEBUG && ERRORONLY
            // buffer the string first
            std::stringbuf buffer;
            std::ostream bufferStream (&buffer);

            ((bufferStream << std::forward<Data>(data)), ...);

            // then send it to all the outputs configured
            for (auto cur : this->mOutputs)
                *cur << buffer.str () << std::endl;
#endif /* DEBUG */
        }

        template<typename... Data>
        void error (Data... data)
        {
            // buffer the string first
            std::stringbuf buffer;
            std::ostream bufferStream (&buffer);

            ((bufferStream << std::forward<Data>(data)), ...);

            // then send it to all the outputs configured
            for (auto cur : this->mErrors)
                *cur << buffer.str () << std::endl;
        }

        template<class EX, typename... Data>
        [[noreturn]] void exception (Data... data)
        {
            // buffer the string first
            std::stringbuf buffer;
            std::ostream bufferStream (&buffer);

            ((bufferStream << std::forward<Data>(data)), ...);

            // then send it to all the outputs configured
            for (auto cur : this->mErrors)
                *cur << buffer.str () << std::endl;

            // now throw the exception
            throw EX (buffer.str ());
        }

        template<typename... Data>
        [[noreturn]] void exception (Data... data)
        {
            this->exception <std::runtime_error> (data...);
        }

        static CLog& get ();

    private:
        std::vector <std::ostream*> mOutputs;
        std::vector <std::ostream*> mErrors;
        static std::shared_ptr<CLog> sInstance;
    };
}

#define sLog (WallpaperEngine::Logging::CLog::get ())