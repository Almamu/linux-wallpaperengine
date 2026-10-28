#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Media/MediaSource.h"
#include "WallpaperEngine/Media/DBusMediaSource.h"

#include <cstdlib>
#include <cstring>
#include <poll.h>

namespace {
class CountingSource : public WallpaperEngine::Media::MediaSource {
public:
    CountingSource () : MediaSource (std::chrono::hours (1)) { }

    int calls = 0;

    void expireInterval () { m_nextUpdate = std::chrono::steady_clock::now () - std::chrono::seconds (1); }

protected:
    void performUpdate () override { ++calls; }
};
}

TEST_CASE ("Media sources poll once per interval and resume after it expires") {
    CountingSource source;
    source.update ();
    REQUIRE (source.calls == 1);

    for (int frame = 0; frame < 1000; ++frame) {
	source.update ();
    }
    CHECK (source.calls == 1);

    source.expireInterval ();
    source.update ();
    CHECK (source.calls == 2);
    source.update ();
    CHECK (source.calls == 2);
}

namespace {
class DBusTestSource : public WallpaperEngine::Media::DBusMediaSource {
public:
    DBusTestSource () : DBusMediaSource (std::chrono::hours (1)) { }
    using DBusMediaSource::Request;
    using DBusMediaSource::m_candidates;
    using DBusMediaSource::m_currentPlayer;
    using DBusMediaSource::m_deadline;
    using DBusMediaSource::m_failedPlayers;
    using DBusMediaSource::m_playerOwners;
    using DBusMediaSource::m_metadataNeeded;
    using DBusMediaSource::m_pending;
    using DBusMediaSource::m_request;
    using DBusMediaSource::m_requestPlayer;
    using DBusMediaSource::isExcluded;
    using DBusMediaSource::nextCandidate;
    using DBusMediaSource::pollPending;
    using DBusMediaSource::pollPlayer;
    using DBusMediaSource::selectPlayer;
    using DBusMediaSource::serviceForOwner;
    using DBusMediaSource::startRequest;

    void pumpOutgoing () {
        dbus_connection_read_write (m_connection, 0);
        REQUIRE_FALSE (dbus_connection_has_messages_to_send (m_connection));
    }

    void prepareReplyTest () {
        // A private-bus round trip drains startup traffic before polling for the test reply.
        char* id = dbus_bus_get_id (m_connection, nullptr);
        REQUIRE (id);
        dbus_free (id);
        while (dbus_connection_get_dispatch_status (m_connection) == DBUS_DISPATCH_DATA_REMAINS) {
            dbus_connection_dispatch (m_connection);
        }
        m_nextUpdate = std::chrono::steady_clock::now () + std::chrono::hours (1);
    }

    void waitForReply () {
        int socket = -1;
        REQUIRE (dbus_connection_get_unix_fd (m_connection, &socket));
        pollfd descriptor { socket, POLLIN, 0 };
        REQUIRE (poll (&descriptor, 1, 2000) == 1);
        REQUIRE ((descriptor.revents & POLLIN) != 0);
    }

    void receiveReply () {
        dbus_connection_read_write (m_connection, 0);
        for (int i = 0; i < 16
             && dbus_connection_get_dispatch_status (m_connection) == DBUS_DISPATCH_DATA_REMAINS; ++i) {
            dbus_connection_dispatch (m_connection);
        }
        REQUIRE (dbus_pending_call_get_completed (m_pending));
    }

    void finish (Request request, const std::string& player, DBusMessage* reply) {
        cancelPending ();
        completeRequest (request, player, reply);
    }
};

using Message = std::unique_ptr<DBusMessage, decltype (&dbus_message_unref)>;

Message propertyMessage (const char* signature, const char* sender) {
    Message message (dbus_message_new (DBUS_MESSAGE_TYPE_METHOD_RETURN), dbus_message_unref);
    REQUIRE (message);
    REQUIRE (dbus_message_set_sender (message.get (), sender));
    DBusMessageIter body, value;
    dbus_message_iter_init_append (message.get (), &body);
    REQUIRE (dbus_message_iter_open_container (&body, DBUS_TYPE_VARIANT, signature, &value));
    if (std::strcmp (signature, "s") == 0) {
        const char* status = "Playing";
        REQUIRE (dbus_message_iter_append_basic (&value, DBUS_TYPE_STRING, &status));
    } else if (std::strcmp (signature, "x") == 0) {
        dbus_int64_t position = 42000000;
        REQUIRE (dbus_message_iter_append_basic (&value, DBUS_TYPE_INT64, &position));
    } else {
        DBusMessageIter metadata;
        REQUIRE (dbus_message_iter_open_container (&value, DBUS_TYPE_ARRAY, "{sv}", &metadata));
        REQUIRE (dbus_message_iter_close_container (&value, &metadata));
    }
    REQUIRE (dbus_message_iter_close_container (&body, &value));
    return message;
}

using Connection = std::unique_ptr<DBusConnection, void (*) (DBusConnection*)>;

Connection newPeer () {
    Connection peer (dbus_bus_get_private (DBUS_BUS_SESSION, nullptr), [] (DBusConnection* connection) {
        dbus_connection_close (connection);
        dbus_connection_unref (connection);
    });
    REQUIRE (peer);
    dbus_connection_set_exit_on_disconnect (peer.get (), false);
    return peer;
}

void waitForDelivery (DBusTestSource& source, DBusConnection* peer) {
    // Test-only barrier: the bus routes previous messages before answering this round trip.
    char* id = dbus_bus_get_id (peer, nullptr);
    REQUIRE (id);
    dbus_free (id);
    source.waitForReply ();
}

Message playbackSignal (const char* status) {
    Message message (dbus_message_new_signal ("/org/mpris/MediaPlayer2",
        "org.freedesktop.DBus.Properties", "PropertiesChanged"), dbus_message_unref);
    REQUIRE (message);
    DBusMessageIter body, changed, entry, value, invalidated;
    dbus_message_iter_init_append (message.get (), &body);
    const char* interface = "org.mpris.MediaPlayer2.Player";
    const char* key = "PlaybackStatus";
    REQUIRE (dbus_message_iter_append_basic (&body, DBUS_TYPE_STRING, &interface));
    REQUIRE (dbus_message_iter_open_container (&body, DBUS_TYPE_ARRAY, "{sv}", &changed));
    REQUIRE (dbus_message_iter_open_container (&changed, DBUS_TYPE_DICT_ENTRY, nullptr, &entry));
    REQUIRE (dbus_message_iter_append_basic (&entry, DBUS_TYPE_STRING, &key));
    REQUIRE (dbus_message_iter_open_container (&entry, DBUS_TYPE_VARIANT, "s", &value));
    REQUIRE (dbus_message_iter_append_basic (&value, DBUS_TYPE_STRING, &status));
    REQUIRE (dbus_message_iter_close_container (&entry, &value));
    REQUIRE (dbus_message_iter_close_container (&changed, &entry));
    REQUIRE (dbus_message_iter_close_container (&body, &changed));
    REQUIRE (dbus_message_iter_open_container (&body, DBUS_TYPE_ARRAY, "s", &invalidated));
    REQUIRE (dbus_message_iter_close_container (&body, &invalidated));
    return message;
}

Connection readyPropertyReply (DBusTestSource& source, DBusTestSource::Request requestKind,
                               const char* status = nullptr, bool signalFirst = false) {
    auto player = newPeer ();
    source.prepareReplyTest ();
    const char* owner = dbus_bus_get_unique_name (player.get ());
    static unsigned int sequence = 0;
    const auto service = "org.mpris.MediaPlayer2.regression" + std::to_string (++sequence);
    REQUIRE (dbus_bus_request_name (player.get (), service.c_str (), DBUS_NAME_FLAG_DO_NOT_QUEUE, nullptr)
        == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
    if (requestKind == DBusTestSource::Request::Position) {
        source.selectPlayer (owner, service);
        source.m_metadataNeeded = false;
        source.pollPlayer ();
    } else if (requestKind == DBusTestSource::Request::Players) {
        source.prepareReplyTest ();
        source.m_playerOwners.clear (); // Require startup GetNameOwner discovery, not the earlier signal.
        REQUIRE (source.startRequest (requestKind, {}));
    } else {
        REQUIRE (source.startRequest (requestKind, service, "PlaybackStatus"));
    }
    REQUIRE (source.m_pending);

    Message request (nullptr, dbus_message_unref);
    const auto until = std::chrono::steady_clock::now () + std::chrono::seconds (2);
    while (std::chrono::steady_clock::now () < until) {
        source.update ();
        dbus_connection_read_write (player.get (), 50);
        request.reset (dbus_connection_pop_message (player.get ()));
        if (request && dbus_message_is_method_call (request.get (), "org.freedesktop.DBus.Properties", "Get")) {
            break;
        }
        request.reset ();
    }
    REQUIRE (request);
    auto reply = propertyMessage (requestKind == DBusTestSource::Request::Position ? "x" : "s", owner);
    REQUIRE (dbus_message_set_reply_serial (reply.get (), dbus_message_get_serial (request.get ())));
    REQUIRE (dbus_message_set_destination (reply.get (), dbus_message_get_sender (request.get ())));
    if (status && signalFirst) {
        auto signal = playbackSignal (status);
        REQUIRE (dbus_connection_send (player.get (), signal.get (), nullptr));
    }
    REQUIRE (dbus_connection_send (player.get (), reply.get (), nullptr));
    if (status && !signalFirst) {
        auto signal = playbackSignal (status);
        REQUIRE (dbus_connection_send (player.get (), signal.get (), nullptr));
    }
    waitForDelivery (source, player.get ());
    return player;
}
}

// Explicit opt-in prevents the fixture from connecting to a desktop session bus.
// Run with: dbus-run-session -- env LWE_TEST_PRIVATE_BUS=1 ./tests "[dbus-media]"
TEST_CASE ("D-Bus media requests recover without waiting for replies", "[.][dbus-media]") {
    const char* isolated = std::getenv ("LWE_TEST_PRIVATE_BUS");
    REQUIRE (isolated);
    REQUIRE (std::strcmp (isolated, "1") == 0);
    DBusTestSource source;
    source.pumpOutgoing ();
    using Request = DBusTestSource::Request;
    const std::string playerA = ":1.101";
    const std::string playerB = ":1.102";
    const std::string serviceA = "org.mpris.MediaPlayer2.testA";
    const std::string serviceB = "org.mpris.MediaPlayer2.testB";

    SECTION ("An incomplete request expires without waiting for its reply") {
        REQUIRE (source.startRequest (Request::Players, {}));
        REQUIRE (source.m_pending);
        CHECK_FALSE (source.startRequest (Request::Players, {}));
        source.m_deadline = std::chrono::steady_clock::now () - std::chrono::seconds (1);
        source.pollPending ();
        CHECK (source.m_pending == nullptr);
        CHECK (source.m_request == Request::None);
    }

    SECTION ("An unrelated peer cannot select itself or invent a bus ownership announcement") {
        auto peer = newPeer ();
        source.prepareReplyTest ();
        Message forged (dbus_message_new_signal (DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "NameOwnerChanged"),
                        dbus_message_unref);
        REQUIRE (forged);
        const char* service = "org.mpris.MediaPlayer2.forged";
        const char* previous = "";
        const char* owner = dbus_bus_get_unique_name (peer.get ());
        REQUIRE (dbus_message_append_args (forged.get (), DBUS_TYPE_STRING, &service,
            DBUS_TYPE_STRING, &previous, DBUS_TYPE_STRING, &owner, DBUS_TYPE_INVALID));
        REQUIRE (dbus_connection_send (peer.get (), forged.get (), nullptr));
        auto signal = playbackSignal ("Playing");
        REQUIRE (dbus_connection_send (peer.get (), signal.get (), nullptr));
        waitForDelivery (source, peer.get ());
        source.update ();
        CHECK_FALSE (source.m_currentPlayer.has_value ());
        CHECK (source.m_playerOwners.empty ());
        CHECK (source.m_pending == nullptr);
        CHECK (source.getMediaInfo ().playbackState == WallpaperEngine::Media::MediaSource::Stopped);
    }

    SECTION ("Startup discovery resolves the owner before accepting its status reply") {
        auto player = readyPropertyReply (source, Request::Players);
        const char* owner = dbus_bus_get_unique_name (player.get ());
        REQUIRE_FALSE (source.serviceForOwner (owner).empty ());
        REQUIRE_FALSE (source.m_currentPlayer.has_value ());
        source.update ();
        CHECK (source.m_currentPlayer == owner);
        CHECK (source.getMediaInfo ().playbackState == WallpaperEngine::Media::MediaSource::Playing);
    }

    SECTION ("Losing an MPRIS name revokes the old peer and accepts its replacement") {
        auto player = readyPropertyReply (source, Request::Status);
        source.update ();
        const char* oldOwner = dbus_bus_get_unique_name (player.get ());
        REQUIRE (source.m_currentPlayer == oldOwner);
        const auto service = source.serviceForOwner (oldOwner);
        REQUIRE_FALSE (service.empty ());
        REQUIRE (dbus_bus_release_name (player.get (), service.c_str (), nullptr)
            == DBUS_RELEASE_NAME_REPLY_RELEASED);
        auto signal = playbackSignal ("Playing");
        REQUIRE (dbus_connection_send (player.get (), signal.get (), nullptr));
        waitForDelivery (source, player.get ());
        source.update ();
        CHECK_FALSE (source.m_currentPlayer.has_value ());
        CHECK (source.serviceForOwner (oldOwner).empty ());
        CHECK (source.m_pending == nullptr);
        CHECK (source.getMediaInfo ().playbackState == WallpaperEngine::Media::MediaSource::Stopped);

        auto replacement = newPeer ();
        REQUIRE (dbus_bus_request_name (replacement.get (), service.c_str (), DBUS_NAME_FLAG_DO_NOT_QUEUE, nullptr)
            == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
        auto replacementSignal = playbackSignal ("Playing");
        REQUIRE (dbus_connection_send (replacement.get (), replacementSignal.get (), nullptr));
        waitForDelivery (source, replacement.get ());
        source.update ();
        CHECK (source.m_currentPlayer == dbus_bus_get_unique_name (replacement.get ()));
        CHECK (source.getMediaInfo ().playbackState == WallpaperEngine::Media::MediaSource::Playing);
    }

    SECTION ("A completed Position reply survives an expired application deadline") {
        auto player = readyPropertyReply (source, Request::Position);
        source.receiveReply ();
        source.m_deadline = std::chrono::steady_clock::now () - std::chrono::seconds (1);
        source.pollPending ();
        CHECK (source.getMediaInfo ().position == 42000000);
        CHECK (source.m_currentPlayer.has_value ());
        CHECK (source.m_failedPlayers.empty ());
        CHECK (source.m_pending == nullptr);
    }

    SECTION ("A buffered Position reply is read before a delayed frame applies its deadline") {
        auto player = readyPropertyReply (source, Request::Position);
        REQUIRE_FALSE (dbus_pending_call_get_completed (source.m_pending));
        source.m_deadline = std::chrono::steady_clock::now () - std::chrono::seconds (1);
        source.update ();
        CHECK (source.getMediaInfo ().position == 42000000);
        CHECK (source.m_currentPlayer.has_value ());
        CHECK (source.m_failedPlayers.empty ());
        CHECK (source.m_pending == nullptr);
    }

    SECTION ("Metadata is retried after failure even when Position succeeds") {
        source.selectPlayer (playerA, serviceA);
        source.finish (Request::Metadata, playerA, nullptr);
        REQUIRE (source.m_metadataNeeded);
        auto position = propertyMessage ("x", playerA.c_str ());
        source.finish (Request::Position, playerA, position.get ());
        CHECK (source.getMediaInfo ().position == 42000000);
        source.pumpOutgoing ();
        source.pollPlayer ();
        REQUIRE (source.m_request == Request::Metadata);
        auto metadata = propertyMessage ("a{sv}", playerA.c_str ());
        source.finish (Request::Metadata, playerA, metadata.get ());
        CHECK_FALSE (source.m_metadataNeeded);
        CHECK (source.m_currentPlayer == playerA);
    }

    SECTION ("A discovery reply is applied before a following Paused or Stopped signal") {
        for (const char* status : { "Paused", "Stopped" }) {
            DBusTestSource ordered;
            auto player = readyPropertyReply (ordered, Request::Status, status);
            ordered.update ();
            CHECK (ordered.m_currentPlayer == dbus_bus_get_unique_name (player.get ()));
            CHECK (ordered.getMediaInfo ().playbackState == (std::strcmp (status, "Paused") == 0
                ? WallpaperEngine::Media::MediaSource::Paused : WallpaperEngine::Media::MediaSource::Stopped));
            CHECK (ordered.m_failedPlayers.empty ());
        }
    }

    SECTION ("An earlier signal does not expire a Position reply still queued behind it") {
        auto player = readyPropertyReply (source, Request::Position, "Paused", true);
        source.m_deadline = std::chrono::steady_clock::now () - std::chrono::seconds (1);
        source.update ();
        CHECK (source.getMediaInfo ().position == 42000000);
        CHECK (source.getMediaInfo ().playbackState == WallpaperEngine::Media::MediaSource::Paused);
        CHECK (source.m_currentPlayer.has_value ());
        CHECK (source.m_failedPlayers.empty ());
    }

    SECTION ("A failed Position excludes A and discovery reaches healthy B") {
        source.m_playerOwners = { { serviceA, playerA }, { serviceB, playerB } };
        source.selectPlayer (playerA, serviceA);
        source.finish (Request::Position, playerA, nullptr);
        CHECK_FALSE (source.m_currentPlayer.has_value ());
        REQUIRE (source.isExcluded (playerA));
        REQUIRE (source.isExcluded (serviceA));
        source.m_candidates = { serviceA, serviceB };
        source.nextCandidate ();
        REQUIRE (source.m_requestPlayer == serviceB);
        auto status = propertyMessage ("s", playerB.c_str ());
        source.finish (Request::Status, serviceB, status.get ());
        CHECK (source.m_currentPlayer == playerB);
        source.m_failedPlayers[playerA] = std::chrono::steady_clock::now () - std::chrono::seconds (1);
        CHECK_FALSE (source.isExcluded (playerA));
    }

    SECTION ("Switching players cancels work and ignores the old metadata reply") {
        source.selectPlayer (playerA, serviceA);
        source.pollPlayer ();
        REQUIRE (source.m_pending);
        source.selectPlayer (playerB, serviceB);
        CHECK (source.m_pending == nullptr);
        auto metadata = propertyMessage ("a{sv}", playerA.c_str ());
        source.finish (Request::Metadata, playerA, metadata.get ());
        CHECK (source.m_currentPlayer == playerB);
        CHECK (source.m_metadataNeeded);
    }
}
