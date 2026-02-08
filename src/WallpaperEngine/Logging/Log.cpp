#include "Log.h"

#include <cassert>
#include <memory>

using namespace WallpaperEngine::Logging;

Log::Log () { assert (this->sInstance == nullptr); }

Log& Log::get () {
    if (sInstance == nullptr) {
	sInstance = std::make_unique<Log> ();
    }

    return *sInstance;
}

void Log::addOutput (std::ostream* stream) { this->mOutputs.push_back (stream); }

void Log::addError (std::ostream* stream) { this->mErrors.push_back (stream); }

std::unique_ptr<Log> Log::sInstance = nullptr;