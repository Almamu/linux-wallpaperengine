#include "DBusMediaSource.h"

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cstring>
#include <string_view>

using namespace WallpaperEngine::Media;

namespace {
constexpr auto requestTimeout = std::chrono::milliseconds (250);
constexpr auto failedPlayerCooldown = std::chrono::seconds (10);

bool hasSignature (DBusMessageIter& value, const char* signature) {
    char* actual = dbus_message_iter_get_signature (&value);
    WallpaperEngine::Data::Utils::ScopeGuard guard ([actual] { dbus_free (actual); });
    return actual && std::strcmp (actual, signature) == 0;
}

bool propertyReply (DBusMessage* reply, const char* signature, DBusMessageIter& value) {
    if (!reply || dbus_message_get_type (reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature (reply, "v")) {
        return false;
    }
    DBusMessageIter body;
    dbus_message_iter_init (reply, &body);
    dbus_message_iter_recurse (&body, &value);
    return hasSignature (value, signature);
}
}

DBusHandlerResult DBusMediaSource::messageFilter (DBusConnection*, DBusMessage* message, void* userData) {
    auto& source = *static_cast<DBusMediaSource*> (userData);
    if (!dbus_message_is_signal (message, "org.freedesktop.DBus.Properties", "PropertiesChanged")
        || !dbus_message_has_signature (message, "sa{sv}as")) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    const char* sender = dbus_message_get_sender (message);
    if (!sender || source.isExcluded (sender)) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    DBusMessageIter body;
    dbus_message_iter_init (message, &body);
    const char* interface = nullptr;
    dbus_message_iter_get_basic (&body, &interface);
    if (std::strcmp (interface, "org.mpris.MediaPlayer2.Player") != 0) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_message_iter_next (&body);
    DBusMessageIter changed;
    dbus_message_iter_recurse (&body, &changed);
    while (dbus_message_iter_get_arg_type (&changed) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry, value;
        dbus_message_iter_recurse (&changed, &entry);
        const char* key = nullptr;
        dbus_message_iter_get_basic (&entry, &key);
        dbus_message_iter_next (&entry);
        dbus_message_iter_recurse (&entry, &value);
        if (std::strcmp (key, "PlaybackStatus") == 0) {
            source.parsePlaybackStatus (value, sender);
        } else if (std::strcmp (key, "Metadata") == 0
                   && source.m_currentPlayer == sender
                   && hasSignature (value, "a{sv}")) {
            if (source.m_request == Request::Metadata) {
                source.cancelPending ();
            }
            source.parseMetadata (value);
            source.m_metadataNeeded = false;
        }
        dbus_message_iter_next (&changed);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

DBusMediaSource::DBusMediaSource (std::chrono::milliseconds updateInterval) : MediaSource (updateInterval) {
    DBusError err;
    dbus_error_init (&err);
    Data::Utils::ScopeGuard errorGuard ([&err] { dbus_error_free (&err); });

    // Registration is startup-only. Private ownership prevents other users of
    // libdbus in this process from blocking this connection's I/O mutex.
    m_connection = dbus_bus_get_private (DBUS_BUS_SESSION, &err);
    if (!m_connection) {
        sLog.exception ("Could not connect to DBus: ", err.message);
    }
    dbus_connection_set_exit_on_disconnect (m_connection, false);
    if (!dbus_connection_add_filter (m_connection, messageFilter, this, nullptr)) {
        dbus_connection_close (m_connection);
        dbus_connection_unref (m_connection);
        m_connection = nullptr;
        sLog.exception ("Cannot allocate DBus media filter");
    }
    // A null error queues AddMatch without waiting. update() pumps transmission.
    dbus_bus_add_match (m_connection,
        "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'", nullptr);
}

DBusMediaSource::~DBusMediaSource () {
    cancelPending ();
    dbus_connection_remove_filter (m_connection, messageFilter, this);
    dbus_connection_close (m_connection);
    dbus_connection_unref (m_connection);
}

void DBusMediaSource::parseMetadata (DBusMessageIter& variant) {
    if (!hasSignature (variant, "a{sv}")) {
        return;
    }
    DBusMessageIter dict;
    dbus_message_iter_recurse (&variant, &dict);

    bool metadataUpdate = false;
    bool albumUpdate = false;

    while (dbus_message_iter_get_arg_type (&dict) == DBUS_TYPE_DICT_ENTRY) {
	DBusMessageIter entry;
	dbus_message_iter_recurse (&dict, &entry);

	const char* key = nullptr;
	dbus_message_iter_get_basic (&entry, &key);

	std::string keyStr = key ?: "";

	DBusMessageIter value;
	dbus_message_iter_next (&entry);
	dbus_message_iter_recurse (&entry, &value);

	if (keyStr == "xesam:title" && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING) {
	    const char* title = nullptr;
	    dbus_message_iter_get_basic (&value, &title);

	    std::string titleStr = title ?: "";

	    if (this->m_mediaInfo.title != titleStr) {
		this->m_mediaInfo.title = titleStr;
		metadataUpdate = true;
	    }
	} else if (keyStr == "xesam:artist" && hasSignature (value, "as")) {
	    DBusMessageIter arr;

	    dbus_message_iter_recurse (&value, &arr);

	    if (dbus_message_iter_get_arg_type (&arr) == DBUS_TYPE_STRING) {
		const char* artist = nullptr;
		dbus_message_iter_get_basic (&arr, &artist);

		std::string artistStr = artist ?: "";

		if (this->m_mediaInfo.artist != artistStr) {
		    this->m_mediaInfo.artist = artistStr;
		    metadataUpdate = true;
		}
	    }
	} else if (keyStr == "xesam:album" && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING) {
	    const char* album = nullptr;
	    dbus_message_iter_get_basic (&value, &album);

	    std::string albumStr = album ?: "";

	    if (this->m_mediaInfo.album != albumStr) {
		this->m_mediaInfo.album = albumStr;
		albumUpdate = true;
	    }
	} else if (keyStr == "mpris:artUrl" && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING) {
	    const char* artUrl = nullptr;
	    dbus_message_iter_get_basic (&value, &artUrl);

	    std::string artUrlStr = artUrl ?: "";

	    if (artUrlStr.empty () && this->m_mediaInfo.url.has_value ()) {
		this->m_mediaInfo.url.reset ();
		albumUpdate = true;
	    } else if (this->m_mediaInfo.url.has_value () && artUrlStr != *this->m_mediaInfo.url) {
		this->m_mediaInfo.url = artUrlStr;
		albumUpdate = true;
	    } else if (!this->m_mediaInfo.url.has_value ()) {
		this->m_mediaInfo.url = artUrlStr;
		albumUpdate = true;
	    }
	} else if (keyStr == "mpris:length" && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_INT64) {
	    int64_t length = 0;
	    dbus_message_iter_get_basic (&value, &length);

	    if (this->m_mediaInfo.duration != length) {
		this->m_mediaInfo.duration = length;
		metadataUpdate = true;
	    }
	}

	dbus_message_iter_next (&dict);
    }

    sLog.debug (
	"Player metadata received: title=", this->m_mediaInfo.title, ",artist=", this->m_mediaInfo.artist,
	",album=", this->m_mediaInfo.album, ",url=", this->m_mediaInfo.url.has_value () ? *this->m_mediaInfo.url : ""
    );

    if (metadataUpdate) {
	this->fireMetadataListeners ();
    }

    if (albumUpdate) {
	this->fireAlbumArtListeners ();
    }
}

void DBusMediaSource::parsePlaybackStatus (DBusMessageIter& variant, const char* sender) {
    if (!sender || isExcluded (sender) || dbus_message_iter_get_arg_type (&variant) != DBUS_TYPE_STRING) {
        return;
    }
    const char* status = nullptr;
    dbus_message_iter_get_basic (&variant, &status);
    PlaybackState newState;
    if (std::strcmp (status, "Playing") == 0) {
        selectPlayer (sender);
        newState = PlaybackState::Playing;
    } else if (m_currentPlayer != sender) {
        return;
    } else if (std::strcmp (status, "Paused") == 0) {
        newState = PlaybackState::Paused;
    } else if (std::strcmp (status, "Stopped") == 0) {
        newState = PlaybackState::Stopped;
    } else {
        return;
    }
    if (newState != m_mediaInfo.playbackState) {
        m_mediaInfo.playbackState = newState;
        fireMetadataListeners ();
    }
}

void DBusMediaSource::parsePosition (DBusMessageIter& variant) {
    if (dbus_message_iter_get_arg_type (&variant) != DBUS_TYPE_INT64) {
        return;
    }
    dbus_int64_t position = 0;
    dbus_message_iter_get_basic (&variant, &position);
    if (m_mediaInfo.position != position) {
        m_mediaInfo.position = position;
        fireMetadataListeners ();
    }
}

void DBusMediaSource::update () {
    const auto now = Clock::now ();
    for (auto it = m_failedPlayers.begin (); it != m_failedPlayers.end ();) {
        if (it->second <= now) {
            it = m_failedPlayers.erase (it);
        } else {
            ++it;
        }
    }

    // A frame pause can outlast the deadline while a reply is waiting in the socket.
    dbus_connection_read_write (m_connection, 0);
    for (int dispatched = 0; dispatched < 16
         && dbus_connection_get_dispatch_status (m_connection) == DBUS_DISPATCH_DATA_REMAINS; ++dispatched) {
        dbus_connection_dispatch (m_connection);
    }
    pollPending ();
    MediaSource::update ();
}

bool DBusMediaSource::startRequest (Request request, const std::string& player, const char* property) {
    // Canceling a pending call does not remove its queued message. Do not build
    // another queue of requests if the bus stops reading.
    if (m_pending || !dbus_connection_get_is_connected (m_connection)
        || dbus_connection_has_messages_to_send (m_connection)) {
        return false;
    }

    DBusMessage* message = request == Request::Players
        ? dbus_message_new_method_call (DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "ListNames")
        : dbus_message_new_method_call (player.c_str (), "/org/mpris/MediaPlayer2",
                                        "org.freedesktop.DBus.Properties", "Get");
    if (!message) {
        return false;
    }
    Data::Utils::ScopeGuard messageGuard ([message] { dbus_message_unref (message); });
    if (request != Request::Players) {
        const char* interface = "org.mpris.MediaPlayer2.Player";
        if (!property || !dbus_message_append_args (message, DBUS_TYPE_STRING, &interface,
                                                    DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) {
            return false;
        }
    }

    const auto started = Clock::now ();
    if (!dbus_connection_send_with_reply (m_connection, message, &m_pending, requestTimeout.count ())
        || !m_pending) {
        cancelPending ();
        return false;
    }
    m_request = request;
    m_requestPlayer = player;
    m_deadline = started + requestTimeout;
    dbus_connection_read_write (m_connection, 0);
    return true;
}

void DBusMediaSource::cancelPending () {
    if (m_pending) {
        dbus_pending_call_cancel (m_pending);
        dbus_pending_call_unref (m_pending);
        m_pending = nullptr;
    }
    m_request = Request::None;
    m_requestPlayer.clear ();
}

void DBusMediaSource::pollPending () {
    if (!m_pending) {
        return;
    }
    const bool completed = dbus_pending_call_get_completed (m_pending);
    if (!completed && Clock::now () < m_deadline) {
        return;
    }

    const auto request = m_request;
    const auto player = m_requestPlayer;
    DBusMessage* reply = completed ? dbus_pending_call_steal_reply (m_pending) : nullptr;
    cancelPending ();
    Data::Utils::ScopeGuard replyGuard ([reply] {
        if (reply) {
            dbus_message_unref (reply);
        }
    });
    completeRequest (request, player, reply);
}

void DBusMediaSource::completeRequest (Request request, const std::string& player, DBusMessage* reply) {
    DBusMessageIter value;
    if (request == Request::Players) {
        m_candidates.clear ();
        m_candidateIndex = 0;
        if (!reply || dbus_message_get_type (reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN
            || !dbus_message_has_signature (reply, "as")) {
            return;
        }
        DBusMessageIter body;
        dbus_message_iter_init (reply, &body);
        dbus_message_iter_recurse (&body, &value);
        while (dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING) {
            const char* name = nullptr;
            dbus_message_iter_get_basic (&value, &name);
            if (std::string_view (name).starts_with ("org.mpris.MediaPlayer2.")) {
                m_candidates.emplace_back (name);
            }
            dbus_message_iter_next (&value);
        }
        nextCandidate ();
    } else if (request == Request::Status) {
        if (propertyReply (reply, "s", value)) {
            const char* status = nullptr;
            dbus_message_iter_get_basic (&value, &status);
            const char* owner = dbus_message_get_sender (reply);
            if (std::strcmp (status, "Playing") == 0 && owner && owner[0] == ':'
                && !isExcluded (player) && !isExcluded (owner)) {
                selectPlayer (owner, player);
                parsePlaybackStatus (value, owner);
                pollPlayer ();
                return;
            }
        } else {
            m_failedPlayers[player] = Clock::now () + failedPlayerCooldown;
        }
        nextCandidate ();
    } else if (m_currentPlayer == player) {
        if (request == Request::Metadata) {
            if (propertyReply (reply, "a{sv}", value)) {
                parseMetadata (value);
                m_metadataNeeded = false;
            }
            // Keep metadata pending after an error, but also check player health.
            startRequest (Request::Position, player, "Position");
        } else if (request == Request::Position) {
            if (propertyReply (reply, "x", value)) {
                parsePosition (value);
            } else {
                failPlayer ();
            }
        }
    }
}

void DBusMediaSource::nextCandidate () {
    while (m_candidateIndex < m_candidates.size ()) {
        const auto player = m_candidates[m_candidateIndex++];
        if (isExcluded (player)) {
            continue;
        }
        if (!startRequest (Request::Status, player, "PlaybackStatus")) {
            m_candidates.clear ();
        }
        return;
    }
    m_candidates.clear ();
}

void DBusMediaSource::pollPlayer () {
    if (m_currentPlayer) {
        startRequest (m_metadataNeeded ? Request::Metadata : Request::Position, *m_currentPlayer,
                      m_metadataNeeded ? "Metadata" : "Position");
    }
}

void DBusMediaSource::selectPlayer (const std::string& player, const std::string& service) {
    if (m_currentPlayer == player) {
        return;
    }
    cancelPending ();
    m_candidates.clear ();
    m_currentPlayer = player;
    m_currentService = service;
    m_metadataNeeded = true;
}

void DBusMediaSource::failPlayer () {
    const auto retryAt = Clock::now () + failedPlayerCooldown;
    if (m_currentPlayer) {
        m_failedPlayers[*m_currentPlayer] = retryAt;
    }
    if (!m_currentService.empty ()) {
        m_failedPlayers[m_currentService] = retryAt;
    }
    m_currentPlayer.reset ();
    m_currentService.clear ();
    m_metadataNeeded = true;
}

bool DBusMediaSource::isExcluded (const std::string& player) const {
    const auto it = m_failedPlayers.find (player);
    return it != m_failedPlayers.end () && Clock::now () < it->second;
}

void DBusMediaSource::performUpdate () {
    if (m_pending) {
        return;
    }
    if (m_currentPlayer) {
        pollPlayer ();
    } else {
        startRequest (Request::Players, {});
    }
}
