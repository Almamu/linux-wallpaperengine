#pragma once

#include "MediaSource.h"
#include <dbus/dbus.h>
#include <vector>

namespace WallpaperEngine::Media {
class DBusMediaSource : public MediaSource {
public:
    explicit DBusMediaSource (std::chrono::milliseconds updateInterval);
    ~DBusMediaSource () override;

    void parseMetadata (DBusMessageIter& variant);
    void parsePlaybackStatus (DBusMessageIter& variant, const char* sender);
    void parsePosition (DBusMessageIter& variant);
    void update () override;

protected:
    enum class Request { None, Players, Owner, Status, Metadata, Position };
    using Clock = std::chrono::steady_clock;

    void performUpdate () override;
    /** Queue one request without flushing; its deadline starts before enqueueing. */
    bool startRequest (Request request, const std::string& player, const char* property = nullptr);
    /** Complete or cancel work from update(); never wait for a reply. */
    void pollPending ();
    void cancelPending ();
    void completeRequest (Request request, const std::string& player, DBusMessage* reply);
    void nextOwner ();
    void nextCandidate ();
    void pollPlayer ();
    void selectPlayer (const std::string& player, const std::string& service = {});
    void failPlayer ();
    bool isExcluded (const std::string& player) const;
    std::string serviceForOwner (const std::string& owner) const;
    void updateOwner (const std::string& service, const std::string& owner);
    void resetUnownedPlayer ();

    static DBusHandlerResult messageFilter (DBusConnection*, DBusMessage*, void*);

    std::optional<std::string> m_currentPlayer;
    std::string m_currentService;
    bool m_metadataNeeded = true;
    std::map<std::string, Clock::time_point> m_failedPlayers;
    std::map<std::string, std::string> m_playerOwners;
    std::vector<std::string> m_candidates;
    std::size_t m_candidateIndex = 0;

    DBusConnection* m_connection = nullptr;
    DBusPendingCall* m_pending = nullptr;
    Request m_request = Request::None;
    std::string m_requestPlayer;
    Clock::time_point m_deadline;
};
}
