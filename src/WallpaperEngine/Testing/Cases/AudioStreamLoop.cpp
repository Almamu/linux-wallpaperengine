#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/AudioPlayingDetector.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Audio/Drivers/SDLAudioDriver.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Data/Utils/MemoryStream.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace {
using WallpaperEngine::Application::ApplicationContext;
using WallpaperEngine::Audio::AudioContext;
using WallpaperEngine::Audio::AudioStream;
using WallpaperEngine::Audio::Drivers::AudioDriver;
using WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector;
using WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder;
using WallpaperEngine::Data::Utils::MemoryStream;
using WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector;

class TestingAudioDriver final : public AudioDriver {
public:
    TestingAudioDriver (
	ApplicationContext& applicationContext, AudioPlayingDetector& detector, PlaybackRecorder& recorder
    ) : AudioDriver (applicationContext, detector, recorder) { }

    int addStream (AudioStream*) override { return 0; }
    void removeStream (int) override { }
    [[nodiscard]] AVSampleFormat getFormat () const override { return AV_SAMPLE_FMT_FLT; }
    [[nodiscard]] int getSampleRate () const override { return 48000; }
    [[nodiscard]] int getChannels () const override { return 2; }
};

void writeU16 (std::vector<char>& output, const size_t offset, const uint16_t value) {
    output[offset] = static_cast<char> (value & 0xff);
    output[offset + 1] = static_cast<char> ((value >> 8) & 0xff);
}

void writeU32 (std::vector<char>& output, const size_t offset, const uint32_t value) {
    output[offset] = static_cast<char> (value & 0xff);
    output[offset + 1] = static_cast<char> ((value >> 8) & 0xff);
    output[offset + 2] = static_cast<char> ((value >> 16) & 0xff);
    output[offset + 3] = static_cast<char> ((value >> 24) & 0xff);
}

std::shared_ptr<MemoryStream> shortPcmWave (const uint16_t sample = 0, const uint32_t sampleRate = 8000,
                                            const uint16_t channelCount = 1) {
    const uint32_t sampleCount = sampleRate / 10;
    constexpr uint16_t bitsPerSample = 16;
    const uint32_t dataSize = sampleCount * channelCount * bitsPerSample / 8;

    std::vector<char> wave (44 + dataSize, 0);
    std::memcpy (wave.data (), "RIFF", 4);
    writeU32 (wave, 4, 36 + dataSize);
    std::memcpy (wave.data () + 8, "WAVEfmt ", 8);
    writeU32 (wave, 16, 16);
    writeU16 (wave, 20, 1);
    writeU16 (wave, 22, channelCount);
    writeU32 (wave, 24, sampleRate);
    writeU32 (wave, 28, sampleRate * channelCount * bitsPerSample / 8);
    writeU16 (wave, 32, channelCount * bitsPerSample / 8);
    writeU16 (wave, 34, bitsPerSample);
    std::memcpy (wave.data () + 36, "data", 4);
    writeU32 (wave, 40, dataSize);
    for (uint32_t index = 0; index < sampleCount * channelCount; index++) {
	writeU16 (wave, 44 + index * sizeof (uint16_t), sample);
    }

    auto buffer = std::make_unique<char[]> (wave.size ());
    std::memcpy (buffer.get (), wave.data (), wave.size ());
    return std::make_shared<MemoryStream> (std::move (buffer), wave.size ());
}
} // namespace

TEST_CASE ("looping audio resets its decoder in packet order", "[audio][loop]") {
    char executable[] = "tests";
    char* argv[] = { executable };
    ApplicationContext applicationContext (1, argv);
    applicationContext.state.general.keepRunning = true;
    applicationContext.state.audio.enabled = true;
    applicationContext.state.audio.volume = 0;

    FullScreenDetector fullscreenDetector (applicationContext);
    AudioPlayingDetector audioDetector (applicationContext, fullscreenDetector);
    PlaybackRecorder recorder;
    TestingAudioDriver driver (applicationContext, audioDetector, recorder);
    AudioContext audioContext (driver);
    AudioStream stream (audioContext, shortPcmWave (), true);

    std::array<uint8_t, 192000> decoded {};
    size_t decodedFrames = 0;
    const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (10);

    while (stream.getCompletionCount () < 250 && std::chrono::steady_clock::now () < deadline) {
	const int decodedSize = stream.decodeFrame (decoded.data (), decoded.size ());
	if (decodedSize > 0) {
	    decodedFrames++;
	} else {
	    std::this_thread::yield ();
	}
    }

    REQUIRE (stream.getCompletionCount () >= 250);
    REQUIRE (decodedFrames > 0);

    stream.stop ();
    applicationContext.state.general.keepRunning = false;
}

TEST_CASE ("paused sound streams preserve buffered samples until resumed", "[audio]") {
    const char* oldDriver = SDL_getenv ("SDL_AUDIODRIVER");
    const std::string savedDriver = oldDriver != nullptr ? oldDriver : "";
    SDL_setenv ("SDL_AUDIODRIVER", "dummy", 1);
    WallpaperEngine::Data::Utils::ScopeGuard restoreDriver ([&] {
	if (oldDriver != nullptr) {
	    SDL_setenv ("SDL_AUDIODRIVER", savedDriver.c_str (), 1);
	} else {
	    unsetenv ("SDL_AUDIODRIVER");
	}
    });
    char executable[] = "tests";
    char* argv[] = { executable };
    ApplicationContext applicationContext (1, argv);
    applicationContext.state.general.keepRunning = true;
    applicationContext.state.audio.volume = SDL_MIX_MAXVOLUME;
    FullScreenDetector fullscreenDetector (applicationContext);
    AudioPlayingDetector audioDetector (applicationContext, fullscreenDetector);
    PlaybackRecorder recorder;
    WallpaperEngine::Audio::Drivers::SDLAudioDriver driver (applicationContext, audioDetector, recorder);
    REQUIRE (driver.getSpec ().format == AUDIO_F32);
    AudioContext audioContext (driver);
    AudioStream stream (audioContext, shortPcmWave (), true);
    stream.setPaused (true);
    const int id = driver.addStream (&stream);
    WallpaperEngine::Data::Utils::ScopeGuard removeStream ([&] { driver.removeStream (id); });

    const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (2);
    while (stream.isQueueEmpty () && std::chrono::steady_clock::now () < deadline) {
	std::this_thread::yield ();
    }
    REQUIRE_FALSE (stream.isQueueEmpty ());
    // Hold SDL's recursive mutex to keep the dummy device's callback out while
    // directly exercising the real mixer with a known, partly consumed buffer.
    SDL_LockMutex (driver.getStreamMutex ());
    WallpaperEngine::Data::Utils::ScopeGuard unlock ([&] { SDL_UnlockMutex (driver.getStreamMutex ()); });
    auto* buffer = driver.getStreams ().at (id);
    const std::array<float, 4> samples { 0.125f, 0.25f, 0.5f, 0.75f };
    std::memcpy (buffer->audio_buf, samples.data (), sizeof (samples));
    buffer->audio_buf_size = sizeof (samples);
    buffer->audio_buf_index = sizeof (float);
    std::array<float, 2> output { 1.0f, 1.0f };
    driver.getSpec ().callback (&driver, reinterpret_cast<Uint8*> (output.data ()), sizeof (output));
    REQUIRE (output == std::array<float, 2> { 0.0f, 0.0f });
    REQUIRE (buffer->audio_buf_index == sizeof (float));

    stream.setPaused (false);
    driver.getSpec ().callback (&driver, reinterpret_cast<Uint8*> (output.data ()), sizeof (output));
    REQUIRE (output == std::array<float, 2> { 0.25f, 0.5f });
    REQUIRE (buffer->audio_buf_index == 3 * sizeof (float));
    stream.setPaused (true);
}

TEST_CASE ("sound volume retains authored dynamic values and unity default", "[audio][parser]") {
    using WallpaperEngine::Data::JSON::JSON;
    using WallpaperEngine::Data::Model::Project;
    using WallpaperEngine::Data::Model::Sound;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    const Project project {};
    auto data = JSON::parse (R"({"id":1,"name":"sound","sound":[]})");
    const auto defaults = ObjectParser::parse (data, project);
    REQUIRE (defaults->as<Sound> ()->volume->evaluateFloat (0.0f) == 1.0f);
    data["volume"] = JSON::parse (R"({"value":0.5,"script":"export function update(value) { return value; }"})");
    const auto authored = ObjectParser::parse (data, project);
    REQUIRE (authored->as<Sound> ()->volume->evaluateFloat (0.0f) == 0.5f);
    REQUIRE (authored->as<Sound> ()->volume->value->getScriptSource ().has_value ());
}

TEST_CASE ("sound mixing preserves fractional gain and global volume", "[audio]") {
    const char* oldDriver = SDL_getenv ("SDL_AUDIODRIVER");
    const std::string savedDriver = oldDriver != nullptr ? oldDriver : "";
    SDL_setenv ("SDL_AUDIODRIVER", "dummy", 1);
    WallpaperEngine::Data::Utils::ScopeGuard restoreDriver ([&] {
	if (oldDriver != nullptr) {
	    SDL_setenv ("SDL_AUDIODRIVER", savedDriver.c_str (), 1);
	} else {
	    unsetenv ("SDL_AUDIODRIVER");
	}
    });
    char executable[] = "tests";
    char* argv[] = { executable };
    ApplicationContext applicationContext (1, argv);
    applicationContext.state.general.keepRunning = true;
    applicationContext.state.audio.volume = SDL_MIX_MAXVOLUME / 2;
    FullScreenDetector fullscreenDetector (applicationContext);
    AudioPlayingDetector audioDetector (applicationContext, fullscreenDetector);
    PlaybackRecorder recorder;
    WallpaperEngine::Audio::Drivers::SDLAudioDriver driver (applicationContext, audioDetector, recorder);
    REQUIRE (driver.getSpec ().format == AUDIO_F32SYS);
    AudioContext audioContext (driver);
    AudioStream stream (audioContext, shortPcmWave (), true);
    stream.setPaused (true);
    const int id = driver.addStream (&stream);
    WallpaperEngine::Data::Utils::ScopeGuard removeStream ([&] { driver.removeStream (id); });
    const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (2);
    while (stream.isQueueEmpty () && std::chrono::steady_clock::now () < deadline) {
	std::this_thread::yield ();
    }
    REQUIRE_FALSE (stream.isQueueEmpty ());
    SDL_LockMutex (driver.getStreamMutex ());
    WallpaperEngine::Data::Utils::ScopeGuard unlock ([&] { SDL_UnlockMutex (driver.getStreamMutex ()); });
    auto* buffer = driver.getStreams ().at (id);
    const std::array<float, 2> samples { 0.25f, -0.5f };
    for (const float gain : { 1.0f, 0.25f, 0.0001f, 0.0f }) {
	stream.setGain (gain);
	stream.setPaused (false);
	std::memcpy (buffer->audio_buf, samples.data (), sizeof (samples));
	buffer->audio_buf_size = sizeof (samples);
	buffer->audio_buf_index = 0;
	std::array<float, 2> output {};
	driver.getSpec ().callback (&driver, reinterpret_cast<Uint8*> (output.data ()), sizeof (output));
	REQUIRE (output[0] == Catch::Approx (samples[0] * gain * 0.5f).margin (1e-8));
	REQUIRE (output[1] == Catch::Approx (samples[1] * gain * 0.5f).margin (1e-8));
    }
    stream.setPaused (true);
}

TEST_CASE ("sound layers retain initial silence with an audible default", "[audio][parser]") {
    using WallpaperEngine::Data::JSON::JSON;
    using WallpaperEngine::Data::Model::Project;
    using WallpaperEngine::Data::Model::Sound;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    const Project project {};
    auto data = JSON::parse (R"({"id":1,"name":"sound","sound":[]})");
    const auto defaults = ObjectParser::parse (data, project);
    REQUIRE_FALSE (defaults->as<Sound> ()->startSilent);
    data["startsilent"] = true;
    const auto authored = ObjectParser::parse (data, project);
    REQUIRE (authored->as<Sound> ()->startSilent);
}

TEST_CASE ("sound random intervals retain native defaults and authored values", "[audio][parser]") {
    using WallpaperEngine::Data::JSON::JSON;
    using WallpaperEngine::Data::Model::Project;
    using WallpaperEngine::Data::Model::Sound;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    const Project project {};
    auto data = JSON::parse (R"({"id":1,"name":"sound","sound":[]})");
    const auto defaults = ObjectParser::parse (data, project);
    REQUIRE (defaults->as<Sound> ()->minTime->evaluateFloat (0.0f) == 1.0f);
    REQUIRE (defaults->as<Sound> ()->maxTime->evaluateFloat (0.0f) == 5.0f);
    data["mintime"] = 0.25;
    data["maxtime"] = 0.75;
    const auto authored = ObjectParser::parse (data, project);
    REQUIRE (authored->as<Sound> ()->minTime->evaluateFloat (0.0f) == 0.25f);
    REQUIRE (authored->as<Sound> ()->maxTime->evaluateFloat (0.0f) == 0.75f);
}

TEST_CASE ("single audio completion waits for decoded and resampled output tails", "[audio]") {
    const char* oldDriver = SDL_getenv ("SDL_AUDIODRIVER");
    const std::string savedDriver = oldDriver != nullptr ? oldDriver : "";
    SDL_setenv ("SDL_AUDIODRIVER", "dummy", 1);
    WallpaperEngine::Data::Utils::ScopeGuard restoreDriver ([&] {
	if (oldDriver != nullptr) {
	    SDL_setenv ("SDL_AUDIODRIVER", savedDriver.c_str (), 1);
	} else {
	    unsetenv ("SDL_AUDIODRIVER");
	}
    });
    char executable[] = "tests";
    char* argv[] = { executable };
    ApplicationContext applicationContext (1, argv);
    applicationContext.state.general.keepRunning = true;
    applicationContext.state.audio.volume = SDL_MIX_MAXVOLUME;
    FullScreenDetector fullscreenDetector (applicationContext);
    AudioPlayingDetector audioDetector (applicationContext, fullscreenDetector);
    PlaybackRecorder recorder;
    WallpaperEngine::Audio::Drivers::SDLAudioDriver driver (applicationContext, audioDetector, recorder);
    AudioContext audioContext (driver);
    AudioStream stream (audioContext, shortPcmWave (4096), false);
    stream.setPaused (true);
    const int id = driver.addStream (&stream);
    WallpaperEngine::Data::Utils::ScopeGuard removeStream ([&] { driver.removeStream (id); });
    const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (2);
    while (stream.getCompletionCount () == 0 && std::chrono::steady_clock::now () < deadline) {
	std::this_thread::yield ();
    }
    REQUIRE (stream.getCompletionCount () == 1);
    REQUIRE (stream.getPlaybackCompletionCount () == 0);
    REQUIRE (stream.getDuration () == Catch::Approx (0.1));
    SDL_LockMutex (driver.getStreamMutex ());
    WallpaperEngine::Data::Utils::ScopeGuard unlock ([&] { SDL_UnlockMutex (driver.getStreamMutex ()); });
    stream.setPaused (false);
    double sum = 0.0;
    size_t samples = 0;
    // Odd callback sizes force the last decoded buffer to span callbacks after
    // the packet queue has emptied. 8 kHz input also requires a resampler tail.
    std::array<float, 34> output {};
    while (stream.getPlaybackCompletionCount () == 0 && samples < 12000) {
	driver.getSpec ().callback (&driver, reinterpret_cast<Uint8*> (output.data ()), sizeof (output));
	for (const float value : output) {
	    sum += value;
	}
	samples += output.size ();
	if (samples < 9600) {
	    REQUIRE (stream.getPlaybackCompletionCount () == 0);
	}
    }
    REQUIRE (stream.getPlaybackCompletionCount () == 1);
    // Mono-to-stereo conversion uses the default -3 dB matrix per output channel.
    REQUIRE (sum == Catch::Approx (9600.0 * 0.125 / std::sqrt (2.0)).margin (0.1));
    driver.getSpec ().callback (&driver, reinterpret_cast<Uint8*> (output.data ()), sizeof (output));
    REQUIRE (stream.getPlaybackCompletionCount () == 1);
    REQUIRE (output == std::array<float, 34> {});
    stream.setPaused (true);
}

TEST_CASE ("sound distance attenuation retains authored fields and native defaults", "[audio][spatial][parser]") {
    using WallpaperEngine::Data::JSON::JSON;
    using WallpaperEngine::Data::Model::Project;
    using WallpaperEngine::Data::Model::Sound;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    const Project project {};
    auto data = JSON::parse (R"({"id":1,"name":"sound","sound":[]})");
    const auto defaults = ObjectParser::parse (data, project);
    REQUIRE_FALSE (defaults->as<Sound> ()->spatialization->value->getBool ());
    REQUIRE (defaults->as<Sound> ()->attenuation->evaluateFloat (0.0f) == 1.0f);
    REQUIRE (defaults->as<Sound> ()->minDistance->evaluateFloat (0.0f) == 1.0f);
    data["spatialization"] = JSON::parse (R"({"value":true})");
    data["attenuation"] = 0.25;
    data["mindistance"] = 3.0;
    const auto authored = ObjectParser::parse (data, project);
    REQUIRE (authored->as<Sound> ()->spatialization->value->getBool ());
    REQUIRE (authored->as<Sound> ()->attenuation->evaluateFloat (0.0f) == 0.25f);
    REQUIRE (authored->as<Sound> ()->minDistance->evaluateFloat (0.0f) == 3.0f);
}

TEST_CASE ("sound distance attenuation follows the native listener space", "[audio][spatial]") {
    using WallpaperEngine::Render::Objects::CSound;
    const glm::vec3 listener (10, 20, 30);
    const glm::vec2 canvas (800, 400);
    // Reference distance is a unity-gain plateau, and rolloff zero disables fading.
    REQUIRE (CSound::calculateDistanceGain ({ 10, 20, 32 }, listener, false, canvas, 0.25f, 3.0f, 1) == 1.0f);
    REQUIRE (CSound::calculateDistanceGain ({ 10, 20, 33 }, listener, false, canvas, 0.25f, 3.0f, 1) == 1.0f);
    REQUIRE (CSound::calculateDistanceGain ({ 10, 20, 45 }, listener, false, canvas, 0.25f, 3.0f, 1)
             == Catch::Approx (0.5f));
    REQUIRE (CSound::calculateDistanceGain ({ 10, 20, 45 }, listener, false, canvas, 0.0f, 3.0f, 1) == 1.0f);
    // 3-4-5 offset proves all world axes participate in perspective distance.
    REQUIRE (CSound::calculateDistanceGain ({ 13, 24, 30 }, listener, false, canvas, 1.0f, 1.0f, 1)
             == Catch::Approx (0.2f));
    // Stereo and multichannel sources stay nonspatial with native defaults.
    REQUIRE (CSound::calculateDistanceGain ({ 13, 24, 30 }, listener, false, canvas, 1.0f, 1.0f, 2) == 1.0f);
    REQUIRE (CSound::calculateDistanceGain ({ 13, 24, 30 }, listener, false, canvas, 1.0f, 1.0f, 6) == 1.0f);
    // Orthographic X/Y do not change distance; negative source depth moves it away.
    REQUIRE (CSound::calculateDistanceGain ({ 50, 80, 0 }, listener, true, canvas, 1.0f, 1.0f, 1) == 1.0f);
    REQUIRE (CSound::calculateDistanceGain ({ 5000, -4000, -300 }, listener, true, canvas, 1.0f, 1.0f, 1)
             == Catch::Approx (0.5f));
    REQUIRE (CSound::calculateDistanceGain ({ 50, 80, 600 }, listener, true, canvas, 1.0f, 1.0f, 1) == 1.0f);
    REQUIRE (CSound::calculateDistanceGain ({ 10, 20, 45 }, listener, false, canvas, 1.0f, 0.0f, 1) == 1.0f);
    REQUIRE (CSound::calculateDistanceGain (listener, listener, false, canvas, 1.0f, 0.0f, 1) == 1.0f);
}

TEST_CASE ("sound decoding preserves supported channel layouts and every partial frame", "[audio][spatial]") {
    char executable[] = "tests";
    char* argv[] = { executable };
    ApplicationContext applicationContext (1, argv);
    applicationContext.state.general.keepRunning = true;
    FullScreenDetector fullscreenDetector (applicationContext);
    AudioPlayingDetector audioDetector (applicationContext, fullscreenDetector);
    PlaybackRecorder recorder;
    TestingAudioDriver driver (applicationContext, audioDetector, recorder);
    AudioContext audioContext (driver);
    const std::array<std::pair<unsigned, uint32_t>, 7> layouts {{
        { 1, AV_CH_LAYOUT_MONO }, { 2, AV_CH_LAYOUT_STEREO }, { 4, AV_CH_LAYOUT_QUAD },
        { 6, AV_CH_LAYOUT_5POINT1 }, { 7, AV_CH_LAYOUT_6POINT1 }, { 8, AV_CH_LAYOUT_7POINT1 },
        { 3, AV_CH_LAYOUT_SURROUND }
    }};
    for (const auto& [channels, mask] : layouts) {
        INFO ("channels=" << channels);
        constexpr unsigned frames = 37;
        const unsigned dataSize = frames * channels * sizeof (uint16_t);
        std::vector<char> wave (68 + dataSize);
        std::memcpy (wave.data (), "RIFF", 4); writeU32 (wave, 4, wave.size () - 8);
        std::memcpy (wave.data () + 8, "WAVEfmt ", 8); writeU32 (wave, 16, 40);
        writeU16 (wave, 20, 0xfffe); writeU16 (wave, 22, channels);
        writeU32 (wave, 24, 48000); writeU32 (wave, 28, 48000 * channels * 2);
        writeU16 (wave, 32, channels * 2); writeU16 (wave, 34, 16);
        writeU16 (wave, 36, 22); writeU16 (wave, 38, 16); writeU32 (wave, 40, mask);
        const uint8_t pcmGuid[] { 1, 0, 0, 0, 0, 0, 16, 0, 128, 0, 0, 170, 0, 56, 155, 113 };
        std::memcpy (wave.data () + 44, pcmGuid, sizeof (pcmGuid));
        std::memcpy (wave.data () + 60, "data", 4); writeU32 (wave, 64, dataSize);
        for (unsigned frame = 0; frame < frames; ++frame) for (unsigned channel = 0; channel < channels; ++channel)
            writeU16 (wave, 68 + (frame * channels + channel) * 2, (channel + 1) * 1024);
        auto data = std::make_unique<char[]> (wave.size ());
        std::memcpy (data.get (), wave.data (), wave.size ());
        AudioStream stream (audioContext, std::make_shared<MemoryStream> (std::move (data), wave.size ()), false, true);
        const bool supported = channels != 3;
        REQUIRE (stream.usesSpatialMixer () == supported);
        const unsigned outputChannels = supported ? channels : 2;
        REQUIRE (stream.getOutputChannels () == outputChannels);
        std::vector<float> block (7 * outputChannels), output;
        bool ended = false;
        const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (2);
        while (!ended && std::chrono::steady_clock::now () < deadline) {
            const int bytes = stream.decodeFrame (reinterpret_cast<uint8_t*> (block.data ()), block.size () * sizeof (float) - 1);
            if (bytes == AVERROR_EOF) ended = true;
            else if (bytes > 0) output.insert (output.end (), block.begin (), block.begin () + bytes / sizeof (float));
            else std::this_thread::yield ();
        }
        REQUIRE (ended);
        REQUIRE (output.size () == frames * outputChannels);
        if (supported) for (unsigned i = 0; i < output.size (); ++i)
            REQUIRE (output[i] == Catch::Approx (float (i % channels + 1) / 32.0f));
        stream.stop ();
    }

    AudioStream fallback (audioContext, shortPcmWave (4096), false, true);
    REQUIRE (fallback.usesSpatialMixer ());
    REQUIRE (fallback.getOutputSampleRate () == 8000);
    fallback.useLegacyOutput ();
    REQUIRE_FALSE (fallback.usesSpatialMixer ());
    REQUIRE (fallback.getOutputChannels () == 2);
    REQUIRE (fallback.getOutputSampleRate () == 48000);
    fallback.stop ();
}

TEST_CASE ("sound source rates and scalar fallback drain across repeated boundaries", "[audio][spatial][loop][source-rate]") {
    char executable[] = "tests";
    char* argv[] = { executable };
    ApplicationContext applicationContext (1, argv);
    applicationContext.state.general.keepRunning = true;
    FullScreenDetector fullscreenDetector (applicationContext);
    AudioPlayingDetector audioDetector (applicationContext, fullscreenDetector);
    PlaybackRecorder recorder;
    TestingAudioDriver driver (applicationContext, audioDetector, recorder);
    AudioContext audioContext (driver);
    for (const unsigned sourceChannels : { 1, 2 }) for (const bool native : { true, false }) {
        AudioStream stream (audioContext, shortPcmWave (4096, 32000, sourceChannels), true, true);
        if (!native) stream.useLegacyOutput ();
        const unsigned channels = native ? sourceChannels : 2;
        const unsigned rate = native ? 32000 : 48000;
        REQUIRE (stream.usesSpatialMixer () == native);
        REQUIRE (stream.getOutputSampleRate () == rate);
        REQUIRE (stream.getOutputChannels () == channels);
        std::vector<float> block (7 * channels);
        unsigned completed = 0, frames = 0;
        double sum = 0;
        const auto deadline = std::chrono::steady_clock::now () + std::chrono::seconds (2);
        while (completed < 3 && std::chrono::steady_clock::now () < deadline) {
            const int bytes = stream.decodeFrame (reinterpret_cast<uint8_t*> (block.data ()), block.size () * sizeof (float) - 1);
            if (bytes == AVERROR_EOF) {
                REQUIRE (frames == rate / 10);
                const double gain = native || sourceChannels == 2 ? 1.0 : 1.0 / std::sqrt (2.0);
                REQUIRE (sum == Catch::Approx (rate / 10 * channels * .125 * gain).margin (0.02));
                frames = 0; sum = 0; ++completed;
            } else if (bytes > 0) {
                REQUIRE (bytes % (channels * sizeof (float)) == 0);
                frames += bytes / (channels * sizeof (float));
                for (unsigned i = 0; i < bytes / sizeof (float); ++i) sum += block[i];
            } else std::this_thread::yield ();
        }
        REQUIRE (completed == 3);
        stream.stop ();
    }
}
