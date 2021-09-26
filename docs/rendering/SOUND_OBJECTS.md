# Sound objects
These are the simplest ones. They contain a simple list of sounds to play (one after another? needs to be confirmed) and simple information like volume, playmode, etc...

## Specific json entries
- sound: An array of music files to play
- volume: The volume to play the music at. 0.0f - 1.0f
- mintime: The minimum delay between background rendering starting and sound starting
- maxtime: The maximum delay between background rendering starting and sound starting
- playbackmode: Whether the sound has to be looped or played oneshot...

## Editor json entries
- muteineditor: Whether the sound has to be muted by default