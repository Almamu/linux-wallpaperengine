#pragma once

namespace WallpaperEngine::Scripting {
#ifdef WPENGINE_JAVASCRIPTCORE
// Service deferred work on the runtime's owning thread, between scene operations.
// Returns false if no loop is available for safe dispatch.
bool serviceJavaScriptCoreRuntime ();
#else
inline bool serviceJavaScriptCoreRuntime () { return false; }
#endif
}
