#include "CLog.h"

#include <cassert>

using namespace WallpaperEngine::Logging;

CLog::CLog()
{
    assert (this->sInstance == nullptr);
}

CLog& CLog::get ()
{
    if (sInstance == nullptr)
        sInstance.reset (new CLog ());

    return *sInstance;
}

void CLog::addOutput (std::ostream* stream)
{
    this->mOutputs.push_back (stream);
}

void CLog::addError (std::ostream* stream)
{
    this->mErrors.push_back (stream);
}

std::shared_ptr <CLog> CLog::sInstance = nullptr;