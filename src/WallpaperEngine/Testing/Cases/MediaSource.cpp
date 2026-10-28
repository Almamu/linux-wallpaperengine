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
    using DBusMediaSource::m_metadataNeeded;
    using DBusMediaSource::m_pending;
    using DBusMediaSource::m_request;
    using DBusMediaSource::m_requestPlayer;
    using DBusMediaSource::isExcluded;
    using DBusMediaSource::nextCandidate;
    using DBusMediaSource::pollPending;
    using DBusMediaSource::pollPlayer;
    using DBusMediaSource::selectPlayer;
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
    Connection player (dbus_bus_get_private (DBUS_BUS_SESSION, nullptr), [] (DBusConnection* connection) {
        dbus_connection_close (connection);
        dbus_connection_unref (connection);
    });
    REQUIRE (player);
    dbus_connection_set_exit_on_disconnect (player.get (), false);
    source.prepareReplyTest ();
    const char* owner = dbus_bus_get_unique_name (player.get ());
    if (requestKind == DBusTestSource::Request::Position) {
        source.selectPlayer (owner);
        source.m_metadataNeeded = false;
        source.pollPlayer ();
    } else {
        REQUIRE (source.startRequest (requestKind, owner, "PlaybackStatus"));
    }
    REQUIRE (source.m_pending);

    Message request (nullptr, dbus_message_unref);
    const auto until = std::chrono::steady_clock::now () + std::chrono::seconds (2);
    while (std::chrono::steady_clock::now () < until) {
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
    // Test-only barrier: the bus routes the reply/signals before answering this round trip.
    char* id = dbus_bus_get_id (player.get (), nullptr);
    REQUIRE (id);
    dbus_free (id);
    source.waitForReply ();
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
