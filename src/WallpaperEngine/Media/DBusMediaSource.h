#pragma once

#include "MediaSource.h"
#include <dbus/dbus.h>

namespace WallpaperEngine::Media {
class DBusMediaSource : public MediaSource {
public:
    DBusMediaSource (std::chrono::milliseconds updateInterval);
    ~DBusMediaSource () override;

    void parseMetadata (DBusMessageIter& variant);
    void parsePlaybackStatus (DBusMessageIter& variant);
    void parsePosition (DBusMessageIter& variant);

    void update () override;

protected:
    void performUpdate () override;

    DBusConnection* m_connection;
};
}