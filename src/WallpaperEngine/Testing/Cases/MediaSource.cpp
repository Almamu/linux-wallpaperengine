#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Media/MediaSource.h"
#include "WallpaperEngine/Media/DBusMediaSource.h"

#include <cstdlib>
#include <cstring>

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

    SECTION ("A request expires without pumping transport or waiting for its reply") {
        REQUIRE (source.startRequest (Request::Players, {}));
        REQUIRE (source.m_pending);
        CHECK_FALSE (source.startRequest (Request::Players, {}));
        source.m_deadline = std::chrono::steady_clock::now () - std::chrono::seconds (1);
        source.pollPending ();
        CHECK (source.m_pending == nullptr);
        CHECK (source.m_request == Request::None);
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
