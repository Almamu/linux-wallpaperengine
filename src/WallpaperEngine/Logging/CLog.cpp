#include "CLog.h"

#include <cassert>
#include <memory>

using namespace WallpaperEngine::Logging;

CLog::CLog () {
    assert (this->sInstance == nullptr);
}

CLog& CLog::get () {
    if (sInstance == nullptr) {
        sInstance = std::make_unique<CLog> ();
    }

    return *sInstance;
}

void CLog::addOutput (std::ostream* stream) {
    this->mOutputs.push_back (stream);
}

void CLog::addError (std::ostream* stream) {
    this->mErrors.push_back (stream);
}

std::unique_ptr<CLog> CLog::sInstance = nullptr;