#pragma once

#include "MediaSource.h"
#include <dbus/dbus.h>

namespace WallpaperEngine::Media {
class DBusMediaSource : public MediaSource {
public:
    explicit DBusMediaSource (std::chrono::milliseconds updateInterval);
    ~DBusMediaSource () override;

    static std::unique_ptr<MediaSource> createOrFallback (std::chrono::milliseconds updateInterval);

    void parseMetadata (DBusMessageIter& variant);
    void parsePlaybackStatus (DBusMessageIter& variant, const char* sender);
    void parsePosition (DBusMessageIter& variant);
    DBusHandlerResult handleMessage (DBusMessage* message);

protected:
    void dispatchPendingEvents () override;
    void performUpdate () override;
    void fetchMetadata ();
    void detectPlayer ();
    void clearPlayer ();

    DBusMessage* dbusMessage (
	const char* bus_name, const char* path, const char* interface, const char* method, const char* iface = nullptr,
	const char* prop = nullptr
    );

    std::optional<std::string> m_currentPlayer = std::nullopt;
    bool m_playerListChanged = false;

    DBusConnection* m_connection;

private:
    void disconnect () noexcept;
};
}
