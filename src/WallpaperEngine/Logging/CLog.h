#pragma once

#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <vector>

namespace WallpaperEngine::Logging {
/**
 * Singleton class, simplifies logging for the whole app
 */
class CLog {
  public:
    CLog ();

    void addOutput (std::ostream* stream);
    void addError (std::ostream* stream);

    template <typename... Data> void out (Data... data) {
        std::string str = this->buildBuffer (data...);

        // then send it to all the outputs configured
        for (const auto cur : this->mOutputs)
            *cur << str << std::endl;
    }

    template <typename... Data> void debug (Data... data) {
#if (!NDEBUG) && (!ERRORONLY)
        std::string str = this->buildBuffer (data...);

        // then send it to all the outputs configured
        for (const auto cur : this->mOutputs)
            *cur << str << std::endl;
#endif /* DEBUG */
    }

    template <typename... Data> void debugerror (Data... data) {
#if (!NDEBUG) && (ERRORONLY)
        std::string str = this->buildBuffer (data...);

        // then send it to all the outputs configured
        for (const auto cur : this->mOutputs)
            *cur << str << std::endl;
#endif /* DEBUG */
    }

    template <typename... Data> void error (Data... data) {
        std::string str = this->buildBuffer (data...);

        // then send it to all the outputs configured
        for (const auto cur : this->mErrors)
            *cur << str << std::endl;
    }

    template <class EX, typename... Data> [[noreturn]] void exception (Data... data) {
        std::string str = this->buildBuffer (data...);
        // then send it to all the outputs configured
        for (const auto cur : this->mErrors)
            *cur << str << std::endl;

        // now throw the exception
        throw EX (str);
    }

    template <typename... Data> [[noreturn]] void exception (Data... data) {
        this->exception<std::runtime_error> (data...);
    }

    static CLog& get ();

  private:
    template <typename... Data> std::string buildBuffer (Data... data) {
        // buffer the string first
        std::stringbuf buffer;
        std::ostream bufferStream (&buffer);

        ((bufferStream << std::forward<Data> (data)), ...);

        return buffer.str ();
    }

    std::vector<std::ostream*> mOutputs = {};
    std::vector<std::ostream*> mErrors = {};
    static std::unique_ptr<CLog> sInstance;
};
} // namespace WallpaperEngine::Logging

#define sLog (WallpaperEngine::Logging::CLog::get ())