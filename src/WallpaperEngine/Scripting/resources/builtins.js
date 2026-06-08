globalThis.__intervals = Object.create(null);
globalThis.localStorage = globalThis.localStorage || {
  __data: Object.create(null),
  get(key) {
    key = String(key);
    return Object.prototype.hasOwnProperty.call(this.__data, key) ? this.__data[key] : null;
  },
  set(key, value) { this.__data[String(key)] = String(value); },
  remove(key) { delete this.__data[String(key)]; },
  clear() { this.__data = Object.create(null); }
};
globalThis.MediaPlaybackEvent = globalThis.MediaPlaybackEvent || {
  PLAYBACK_STOPPED: 0,
  PLAYBACK_PLAYING: 1,
  PLAYBACK_PAUSED: 2
};
