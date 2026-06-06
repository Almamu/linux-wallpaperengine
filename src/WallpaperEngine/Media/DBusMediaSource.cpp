//
// Created by almamu on 4/6/26.
//

#include "DBusMediaSource.h"

#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Media;

DBusHandlerResult dbus_message_filter (DBusConnection* connection, DBusMessage* message, void* user_data) {
    const auto mediaSource = static_cast<DBusMediaSource*> (user_data);

    if (!dbus_message_is_signal (message, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* interface = nullptr;
    DBusMessageIter iter;

    dbus_message_iter_init (message, &iter);
    dbus_message_iter_get_basic (&iter, &interface);

    std::string iface = interface ?: "";

    if (iface != "org.mpris.MediaPlayer2.Player") {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusMessageIter changed;

    dbus_message_iter_next (&iter);
    dbus_message_iter_recurse (&iter, &changed);

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
	    mediaSource->parseMetadata (value);
	} else if (keyStr == "PlaybackStatus") {
	    mediaSource->parsePlaybackStatus (value);
	}

	dbus_message_iter_next (&changed);
    }

    const char* sender = dbus_message_get_sender (message);
    std::string service = sender ?: "";

    return DBUS_HANDLER_RESULT_HANDLED;
}

DBusMediaSource::DBusMediaSource (std::chrono::milliseconds updateInterval) : MediaSource (updateInterval) {
    DBusError err;

    dbus_error_init (&err);

    this->m_connection = dbus_bus_get (DBUS_BUS_SESSION, &err);

    if (!this->m_connection) {
	sLog.exception ("Could not connect to DBus: ", err.message);
    }

    dbus_connection_add_filter (this->m_connection, dbus_message_filter, this, nullptr);

    dbus_bus_add_match (
	this->m_connection, "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'",
	nullptr
    );

    dbus_connection_flush (this->m_connection);
}

DBusMediaSource::~DBusMediaSource () {
    dbus_connection_remove_filter (this->m_connection, dbus_message_filter, this);

    dbus_connection_unref (this->m_connection);
}

void DBusMediaSource::parseMetadata (DBusMessageIter& variant) {
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

	if (keyStr == "xesam:title") {
	    const char* title = nullptr;
	    dbus_message_iter_get_basic (&value, &title);

	    this->m_mediaInfo.title = title ?: "";
	    metadataUpdate = true;
	} else if (keyStr == "xesam:artist") {
	    DBusMessageIter arr;

	    dbus_message_iter_recurse (&value, &arr);

	    if (dbus_message_iter_get_arg_type (&arr) == DBUS_TYPE_STRING) {
		const char* artist = nullptr;
		dbus_message_iter_get_basic (&arr, &artist);
		this->m_mediaInfo.artist = artist ?: "";
		metadataUpdate = true;
	    }
	} else if (keyStr == "xesam:album") {
	    const char* album = nullptr;
	    dbus_message_iter_get_basic (&value, &album);

	    this->m_mediaInfo.album = album ?: "";
	    metadataUpdate = true;
	} else if (keyStr == "mpris:artUrl") {
	    const char* artUrl = nullptr;
	    dbus_message_iter_get_basic (&value, &artUrl);

	    this->m_mediaInfo.url = artUrl ?: "";
	    albumUpdate = true;
	} else if (keyStr == "mpris:length") {
	    int64_t length = 0;
	    dbus_message_iter_get_basic (&value, &length);

	    this->m_mediaInfo.duration = length;
	    metadataUpdate = true;
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

void DBusMediaSource::parsePlaybackStatus (DBusMessageIter& variant) {
    const char* status = nullptr;
    dbus_message_iter_get_basic (&variant, &status);
    std::string statusStr = status ?: "";

    if (statusStr == "Playing") {
	this->m_mediaInfo.playbackState = PlaybackState::Playing;
    } else if (statusStr == "Paused") {
	this->m_mediaInfo.playbackState = PlaybackState::Paused;
    } else {
	this->m_mediaInfo.playbackState = PlaybackState::Stopped;
    }

    this->fireMetadataListeners ();
}

void DBusMediaSource::parsePosition (DBusMessageIter& variant) {
    int64_t position = 0;
    dbus_message_iter_get_basic (&variant, &position);
    this->m_mediaInfo.position = position;

    this->fireMetadataListeners ();
}

void DBusMediaSource::update () {
    this->MediaSource::update ();

    // drain any dbus events
    dbus_connection_read_write (this->m_connection, 0);

    while (dbus_connection_dispatch (this->m_connection) == DBUS_DISPATCH_DATA_REMAINS)
	;
}

void DBusMediaSource::performUpdate () {
    // send message to get position
    DBusMessage* msg = dbus_message_new_method_call (
	"org.mpris.MediaPlayer2.Player", "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get"
    );

    const char* iface = "org.mpris.MediaPlayer2.Player";
    const char* prop = "Position";

    dbus_message_append_args (msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

    DBusError err;
    dbus_error_init (&err);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block (this->m_connection, msg, -1, &err);

    dbus_message_unref (msg);

    if (reply == nullptr) {
	return;
    }

    DBusMessageIter outer;
    dbus_message_iter_init (reply, &outer);

    DBusMessageIter variant;
    dbus_message_iter_recurse (&outer, &variant);

    dbus_int64_t position = 0;
    dbus_message_iter_get_basic (&variant, &position);

    this->m_mediaInfo.position = position;

    dbus_message_unref (reply);

    this->fireMetadataListeners ();
}