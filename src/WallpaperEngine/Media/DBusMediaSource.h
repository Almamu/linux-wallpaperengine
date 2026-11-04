#pragma once

#include "MediaSource.h"

#include <dbus/dbus.h>
#include <memory>

namespace WallpaperEngine::Media {
class DBusMediaSource : public MediaSource {
public:
    explicit DBusMediaSource (std::chrono::milliseconds updateInterval);
    ~DBusMediaSource () override;

    /**
     * @return A DBusMediaSource, or a NullMediaSource if the session bus can't be reached, so the
     * application keeps running without media information
     */
    static std::unique_ptr<MediaSource> createOrFallback (std::chrono::milliseconds updateInterval);

    void parseMetadata (DBusMessageIter& variant);
    void parsePlaybackStatus (DBusMessageIter& variant, const char* sender);
    void parsePosition (DBusMessageIter& variant);

    void update () override;

protected:
    void performUpdate () override;
    void initialStatusFetch ();
    void detectPlayer ();
    /** Undoes the constructor's setup on the shared connection and drops our reference to it */
    void disconnect ();

    DBusMessage* dbusMessage (
	const char* bus_name, const char* path, const char* interface, const char* method, const char* iface = nullptr,
	const char* prop = nullptr
    );

    std::optional<std::string> m_currentPlayer = std::nullopt;

    DBusConnection* m_connection;
};
}