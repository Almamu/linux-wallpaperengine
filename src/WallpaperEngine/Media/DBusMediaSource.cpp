#include "DBusMediaSource.h"

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"

#include <stdexcept>

using namespace WallpaperEngine::Media;

namespace {
constexpr const char* mediaMatchRules[] = {
    "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'",
    "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',"
    "member='NameOwnerChanged',arg0namespace='org.mpris.MediaPlayer2'",
};

class UnavailableMediaSource final : public MediaSource {
public:
    using MediaSource::MediaSource;

protected:
    void performUpdate () override { }
};
}

DBusHandlerResult dbus_message_filter (DBusConnection* connection, DBusMessage* message, void* user_data) {
    return static_cast<DBusMediaSource*> (user_data)->handleMessage (message);
}

DBusHandlerResult DBusMediaSource::handleMessage (DBusMessage* message) {
    if (dbus_message_is_signal (message, "org.freedesktop.DBus", "NameOwnerChanged")) {
	const char* name = nullptr;
	const char* oldOwner = nullptr;
	const char* newOwner = nullptr;
	if (dbus_message_get_args (
		message, nullptr, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &oldOwner, DBUS_TYPE_STRING, &newOwner,
		DBUS_TYPE_INVALID
	    )
	    && std::string_view (name).starts_with ("org.mpris.MediaPlayer2.")) {
	    this->m_playerListChanged = true;
	}
	return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!dbus_message_is_signal (message, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* interface = nullptr;
    DBusMessageIter iter;

    if (!dbus_message_iter_init (message, &iter) || dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_message_iter_get_basic (&iter, &interface);

    std::string iface = interface ?: "";

    if (iface != "org.mpris.MediaPlayer2.Player") {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusMessageIter changed;

    if (!dbus_message_iter_next (&iter) || dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_ARRAY) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_message_iter_recurse (&iter, &changed);

    const char* sender = dbus_message_get_sender (message);
    const bool selected = sender != nullptr && this->m_currentPlayer == sender;

    while (dbus_message_iter_get_arg_type (&changed) == DBUS_TYPE_DICT_ENTRY) {
	DBusMessageIter entry;
	dbus_message_iter_recurse (&changed, &entry);

	const char* key = nullptr;
	dbus_message_iter_get_basic (&entry, &key);

	std::string keyStr = key ?: "";

	DBusMessageIter value;
	dbus_message_iter_next (&entry);
	dbus_message_iter_recurse (&entry, &value);

	if (keyStr == "Metadata") {
	    // Loading or clearing a paused track can change player eligibility without
	    // changing PlaybackStatus, including on a player we previously ignored.
	    this->m_playerListChanged = true;
	    if (selected) {
		this->parseMetadata (value);
	    }
	} else if (keyStr == "PlaybackStatus") {
	    // A different player may have started, or the selected player may have
	    // paused. Re-evaluate after dispatch, without borrowing its metadata/state.
	    this->m_playerListChanged = true;
	    if (selected) {
		this->parsePlaybackStatus (value, sender);
	    }
	}

	dbus_message_iter_next (&changed);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

std::unique_ptr<MediaSource> DBusMediaSource::createOrFallback (std::chrono::milliseconds updateInterval) {
    try {
	return std::make_unique<DBusMediaSource> (updateInterval);
    } catch (const std::exception& error) {
	sLog.error ("Continuing without media information (MPRIS): ", error.what ());
	return std::make_unique<UnavailableMediaSource> (updateInterval);
    }
}

DBusMediaSource::DBusMediaSource (std::chrono::milliseconds updateInterval) : MediaSource (updateInterval) {
    DBusError err;

    dbus_error_init (&err);
    Data::Utils::ScopeGuard errorGuard ([&err] { dbus_error_free (&err); });

    this->m_connection = dbus_bus_get (DBUS_BUS_SESSION, &err);

    if (!this->m_connection) {
	throw std::runtime_error (err.message ? err.message : "Could not connect to the D-Bus session bus");
    }

    if (!dbus_connection_add_filter (this->m_connection, dbus_message_filter, this, nullptr)) {
	dbus_connection_unref (this->m_connection);
	throw std::runtime_error ("Could not register the D-Bus media message filter");
    }
    Data::Utils::ScopeGuard cleanup ([this] { this->disconnect (); });

    for (const auto* rule : mediaMatchRules) {
	dbus_bus_add_match (this->m_connection, rule, nullptr);
    }
    dbus_connection_flush (this->m_connection);

    this->detectPlayer ();
    this->fetchMetadata ();
    cleanup.cancel ();
}

DBusMediaSource::~DBusMediaSource () { this->disconnect (); }

void DBusMediaSource::disconnect () noexcept {
    dbus_connection_remove_filter (this->m_connection, dbus_message_filter, this);
    for (const auto* rule : mediaMatchRules) {
	dbus_bus_remove_match (this->m_connection, rule, nullptr);
    }
    dbus_connection_flush (this->m_connection);
    dbus_connection_unref (this->m_connection);
}

void DBusMediaSource::parseMetadata (DBusMessageIter& variant) {
    if (dbus_message_iter_get_arg_type (&variant) != DBUS_TYPE_ARRAY) {
	return;
    }
    // MPRIS Metadata is a complete dictionary, including when delivered inside
    // PropertiesChanged. Omitted fields belong to the new track and must clear.
    auto updated = this->m_mediaInfo;
    updated.title.clear ();
    updated.artist.clear ();
    updated.album.clear ();
    updated.url.reset ();
    updated.duration = 0.0;

    DBusMessageIter dict;
    dbus_message_iter_recurse (&variant, &dict);
    while (dbus_message_iter_get_arg_type (&dict) == DBUS_TYPE_DICT_ENTRY) {
	DBusMessageIter entry;
	dbus_message_iter_recurse (&dict, &entry);
	const char* key = nullptr;
	dbus_message_iter_get_basic (&entry, &key);
	if (!dbus_message_iter_next (&entry) || dbus_message_iter_get_arg_type (&entry) != DBUS_TYPE_VARIANT) {
	    dbus_message_iter_next (&dict);
	    continue;
	}
	DBusMessageIter value;
	dbus_message_iter_recurse (&entry, &value);
	const std::string_view keyStr = key;
	const int type = dbus_message_iter_get_arg_type (&value);
	if (type == DBUS_TYPE_STRING) {
	    const char* text = nullptr;
	    dbus_message_iter_get_basic (&value, &text);
	    if (keyStr == "xesam:title") {
		updated.title = text;
	    } else if (keyStr == "xesam:album") {
		updated.album = text;
	    } else if (keyStr == "mpris:artUrl" && *text != '\0') {
		updated.url = text;
	    }
	} else if (keyStr == "xesam:artist" && type == DBUS_TYPE_ARRAY) {
	    DBusMessageIter artists;
	    dbus_message_iter_recurse (&value, &artists);
	    if (dbus_message_iter_get_arg_type (&artists) == DBUS_TYPE_STRING) {
		const char* artist = nullptr;
		dbus_message_iter_get_basic (&artists, &artist);
		updated.artist = artist;
	    }
	} else if (keyStr == "mpris:length" && type == DBUS_TYPE_INT64) {
	    dbus_int64_t duration = 0;
	    dbus_message_iter_get_basic (&value, &duration);
	    updated.duration = duration;
	}
	dbus_message_iter_next (&dict);
    }

    const bool metadataUpdate = updated.title != this->m_mediaInfo.title || updated.artist != this->m_mediaInfo.artist
	|| updated.album != this->m_mediaInfo.album || updated.duration != this->m_mediaInfo.duration;
    const bool albumUpdate = updated.url != this->m_mediaInfo.url;
    this->m_mediaInfo = std::move (updated);
    if (metadataUpdate) {
	this->fireMetadataListeners ();
    }
    if (albumUpdate) {
	this->fireAlbumArtListeners ();
    }
}

void DBusMediaSource::parsePlaybackStatus (DBusMessageIter& variant, const char* sender) {
    if ((sender != nullptr && this->m_currentPlayer != sender)
	|| dbus_message_iter_get_arg_type (&variant) != DBUS_TYPE_STRING) {
	return;
    }
    const char* status = nullptr;
    dbus_message_iter_get_basic (&variant, &status);
    std::string statusStr = status ?: "";
    PlaybackState newState = this->m_mediaInfo.playbackState;

    if (statusStr == "Playing") {
	newState = PlaybackState::Playing;
    } else if (statusStr == "Paused") {
	newState = PlaybackState::Paused;
    } else {
	newState = PlaybackState::Stopped;
    }

    if (newState != this->m_mediaInfo.playbackState) {
	this->m_mediaInfo.playbackState = newState;
	this->fireMetadataListeners ();
    }
}

void DBusMediaSource::parsePosition (DBusMessageIter& variant) {
    int64_t position = 0;
    dbus_message_iter_get_basic (&variant, &position);

    if (this->m_mediaInfo.position != position) {
	this->m_mediaInfo.position = position;
	this->fireMetadataListeners ();
    }
}

void DBusMediaSource::dispatchPendingEvents () {
    // drain any dbus events
    dbus_connection_read_write (this->m_connection, 0);

    while (dbus_connection_dispatch (this->m_connection) == DBUS_DISPATCH_DATA_REMAINS)
	;

    if (this->m_playerListChanged) {
	this->m_playerListChanged = false;
	this->detectPlayer ();
	this->fetchMetadata ();
    }
}

DBusMessage* DBusMediaSource::dbusMessage (
    const char* bus_name, const char* path, const char* interface, const char* method, const char* iface,
    const char* prop
) {
    DBusError err;
    dbus_error_init (&err);
    Data::Utils::ScopeGuard errorGuard ([&err] { dbus_error_free (&err); });

    DBusMessage* msg = dbus_message_new_method_call (bus_name, path, interface, method);
    Data::Utils::ScopeGuard guard ([msg] { dbus_message_unref (msg); });

    if (iface != nullptr && prop != nullptr) {
	dbus_message_append_args (msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
    }

    DBusMessage* reply = dbus_connection_send_with_reply_and_block (m_connection, msg, -1, &err);

    if (reply == nullptr) {
	sLog.error ("DBus error: ", err.message, " (", err.name, ")");
	return nullptr;
    }

    return reply;
}

void DBusMediaSource::detectPlayer () {
    DBusMessage* reply
	= this->dbusMessage ("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

    if (reply == nullptr) {
	return;
    }

    Data::Utils::ScopeGuard guard ([reply] { dbus_message_unref (reply); });
    std::vector<std::string> players;
    DBusMessageIter iter;
    dbus_message_iter_init (reply, &iter);

    if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_ARRAY) {
	return;
    }

    DBusMessageIter array;
    dbus_message_iter_recurse (&iter, &array);

    while (dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID) {
	char* name;
	dbus_message_iter_get_basic (&array, &name);

	std::string service = name;

	if (service.starts_with ("org.mpris.MediaPlayer2.")) {
	    players.push_back (service);
	}

	dbus_message_iter_next (&array);
    }

    std::optional<std::string> selectedPlayer;
    PlaybackState selectedState = PlaybackState::Stopped;
    int selectedPriority = -1;
    for (const auto& player : players) {
	// also get playback status
	reply = this->dbusMessage (
	    player.c_str (), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get",
	    "org.mpris.MediaPlayer2.Player", "PlaybackStatus"
	);

	if (reply == nullptr) {
	    continue;
	}

	Data::Utils::ScopeGuard guard2 ([reply] { dbus_message_unref (reply); });

	DBusMessageIter outer;
	if (!dbus_message_iter_init (reply, &outer) || dbus_message_iter_get_arg_type (&outer) != DBUS_TYPE_VARIANT) {
	    continue;
	}
	DBusMessageIter variant;
	dbus_message_iter_recurse (&outer, &variant);
	if (dbus_message_iter_get_arg_type (&variant) != DBUS_TYPE_STRING) {
	    continue;
	}
	const char* status = nullptr;
	dbus_message_iter_get_basic (&variant, &status);
	const PlaybackState state = std::string_view (status) == "Playing" ? PlaybackState::Playing
	    : std::string_view (status) == "Paused"                        ? PlaybackState::Paused
									   : PlaybackState::Stopped;
	if (state != PlaybackState::Playing) {
	    // Idle controllers can advertise Paused/Stopped with no current track.
	    // Keep real paused tracks, and allow playing streams without metadata.
	    DBusMessage* metadataReply = this->dbusMessage (
		player.c_str (), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get",
		"org.mpris.MediaPlayer2.Player", "Metadata"
	    );
	    if (metadataReply == nullptr) {
		continue;
	    }
	    Data::Utils::ScopeGuard metadataGuard ([metadataReply] { dbus_message_unref (metadataReply); });
	    DBusMessageIter metadataOuter;
	    if (!dbus_message_iter_init (metadataReply, &metadataOuter)
		|| dbus_message_iter_get_arg_type (&metadataOuter) != DBUS_TYPE_VARIANT) {
		continue;
	    }
	    DBusMessageIter metadata;
	    dbus_message_iter_recurse (&metadataOuter, &metadata);
	    if (dbus_message_iter_get_arg_type (&metadata) != DBUS_TYPE_ARRAY) {
		continue;
	    }
	    DBusMessageIter fields;
	    dbus_message_iter_recurse (&metadata, &fields);
	    if (dbus_message_iter_get_arg_type (&fields) != DBUS_TYPE_DICT_ENTRY) {
		continue;
	    }
	}
	const int priority = state == PlaybackState::Playing ? 2 : state == PlaybackState::Paused ? 1 : 0;
	// Replies and signals carry the unique bus owner, whereas ListNames returns
	// well-known MPRIS names. Keep one identity for both polling and filtering.
	const char* owner = dbus_message_get_sender (reply);
	if (owner != nullptr
	    && (priority > selectedPriority || (priority == selectedPriority && this->m_currentPlayer == owner))) {
	    selectedPlayer = owner;
	    selectedState = state;
	    selectedPriority = priority;
	}
    }

    if (!selectedPlayer.has_value ()) {
	this->clearPlayer ();
	return;
    }
    const bool changed = this->m_currentPlayer != selectedPlayer;
    const bool hadArt = this->m_mediaInfo.url.has_value ();
    const bool stateChanged = this->m_mediaInfo.playbackState != selectedState;
    if (changed) {
	this->m_mediaInfo = {};
    }
    this->m_currentPlayer = std::move (selectedPlayer);
    this->m_mediaInfo.playbackState = selectedState;
    this->m_mediaInfo.available = true;
    if (changed || stateChanged) {
	this->fireMetadataListeners ();
    }
    if (changed && hadArt) {
	this->fireAlbumArtListeners ();
    }
}

void DBusMediaSource::clearPlayer () {
    const bool hadPlayer = this->m_currentPlayer.has_value ();
    const bool hadArt = this->m_mediaInfo.url.has_value ();
    this->m_currentPlayer.reset ();
    this->m_mediaInfo = {};
    if (hadPlayer) {
	this->fireMetadataListeners ();
    }
    if (hadArt) {
	this->fireAlbumArtListeners ();
    }
}

void DBusMediaSource::fetchMetadata () {
    if (this->m_currentPlayer.has_value () == false) {
	return;
    }

    DBusMessage* reply = this->dbusMessage (
	this->m_currentPlayer.value ().c_str (), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get",
	"org.mpris.MediaPlayer2.Player", "Metadata"
    );

    if (reply == nullptr) {
	return;
    }

    Data::Utils::ScopeGuard guard ([reply] { dbus_message_unref (reply); });
    DBusMessageIter outer;
    dbus_message_iter_init (reply, &outer);

    DBusMessageIter variant;
    dbus_message_iter_recurse (&outer, &variant);

    this->parseMetadata (variant);
}

void DBusMediaSource::performUpdate () {
    // 'available' reflects whether a media player is currently selected (it is otherwise
    // never set, leaving consumers unable to tell whether there is now-playing info).
    this->m_mediaInfo.available = this->m_currentPlayer.has_value ();
    // nothing to do if no player is detected
    if (!this->m_currentPlayer.has_value ()) {
	return;
    }

    // Re-poll the track metadata (title/artist/album/art). The constructor's one-shot
    // fetch races player detection and PropertiesChanged signals don't always carry the
    // full metadata, so without this an already-playing track delivers no title/artist.
    this->fetchMetadata ();

    DBusMessage* reply = this->dbusMessage (
	this->m_currentPlayer.value ().c_str (), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get",
	"org.mpris.MediaPlayer2.Player", "Position"
    );

    if (reply == nullptr) {
	return;
    }

    Data::Utils::ScopeGuard guard ([reply] { dbus_message_unref (reply); });
    DBusMessageIter outer;
    dbus_message_iter_init (reply, &outer);

    DBusMessageIter variant;
    dbus_message_iter_recurse (&outer, &variant);

    dbus_int64_t position = 0;
    dbus_message_iter_get_basic (&variant, &position);

    if (this->m_mediaInfo.position != position) {
	this->m_mediaInfo.position = position;
	this->fireMetadataListeners ();
    }
}
