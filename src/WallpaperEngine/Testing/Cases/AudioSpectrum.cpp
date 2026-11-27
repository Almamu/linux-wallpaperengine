#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/WallpaperEngineSpectrumAnalyzer.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <numbers>
#include <string_view>
#include <thread>
#include <vector>

namespace WallpaperEngine::Audio::Drivers::Recorders {
void pa_server_info_cb (pa_context* context, const pa_server_info* info, void* userdata);
}

using WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder;
using WallpaperEngine::Audio::Drivers::Recorders::PulseAudioPlaybackRecorder;
using WallpaperEngine::Audio::Drivers::Recorders::WallpaperEngineSpectrumAnalyzer;

TEST_CASE ("PulseAudio capture retains its in-flight stream for repeated server info", "[.pulse]") {
    const char* server = std::getenv ("LWE_TEST_PULSE_SERVER");
    if (server == nullptr) {
	SKIP ("Set LWE_TEST_PULSE_SERVER to the socket of a private PulseAudio server with a null sink");
    }
    REQUIRE (std::string_view (server).starts_with ("unix:/tmp/"));

    std::unique_ptr<pa_mainloop, decltype (&pa_mainloop_free)> mainloop (pa_mainloop_new (), pa_mainloop_free);
    REQUIRE (mainloop != nullptr);
    std::unique_ptr<pa_context, decltype (&pa_context_unref)> context (
	pa_context_new (pa_mainloop_get_api (mainloop.get ()), "test repeated monitor selection"), pa_context_unref
    );
    REQUIRE (context != nullptr);
    PulseAudioPlaybackRecorder::PulseAudioData data;
    const WallpaperEngine::Data::Utils::ScopeGuard disconnect ([&] {
	pa_context_disconnect (context.get ());
	if (data.captureStream != nullptr) {
	    pa_stream_unref (data.captureStream);
	}
    });

    const auto waitFor = [&] (const auto& ready) {
	using namespace std::chrono_literals;
	const auto deadline = std::chrono::steady_clock::now () + 3s;
	while (!ready () && std::chrono::steady_clock::now () < deadline) {
	    REQUIRE (pa_mainloop_iterate (mainloop.get (), 0, nullptr) >= 0);
	    std::this_thread::sleep_for (1ms);
	}
	REQUIRE (ready ());
    };
    REQUIRE (pa_context_connect (context.get (), server, PA_CONTEXT_NOAUTOSPAWN, nullptr) == 0);
    waitFor ([&] { return pa_context_get_state (context.get ()) == PA_CONTEXT_READY; });

    auto* request = pa_context_get_server_info (
	context.get (), [] (pa_context* ready, const pa_server_info* info, void* userdata) {
	    // Feed the same default monitor during one dispatch so the first stream is still creating.
	    for (int attempt = 0; attempt < 5; ++attempt) {
		WallpaperEngine::Audio::Drivers::Recorders::pa_server_info_cb (ready, info, userdata);
	    }
	}, &data
    );
    REQUIRE (request != nullptr);
    pa_operation_unref (request);
    waitFor ([&] { return data.captureStream != nullptr && pa_stream_get_state (data.captureStream) == PA_STREAM_READY; });
    REQUIRE (data.formatGeneration == 1);
    REQUIRE (data.frameCount == WallpaperEngineSpectrumAnalyzer::windowForSampleRate (data.sampleRate).pcmFrames);
}

TEST_CASE ("Wallpaper Engine spectrum uses the recovered frequency range and band curve") {
    WallpaperEngineSpectrumAnalyzer analyzer44100 (44100);
    REQUIRE (analyzer44100.frameCount () == 1920);
    REQUIRE (WallpaperEngineSpectrumAnalyzer::windowForSampleRate (44100).pcmFrames == 1280);
    REQUIRE (analyzer44100.analyzedBinCount () == 640);
    REQUIRE (analyzer44100.frequencyForBin (1) == Catch::Approx (22.96875f));
    REQUIRE (analyzer44100.frequencyForBin (639) == Catch::Approx (14677.03125f));
    REQUIRE (analyzer44100.bandForBin (1) == 0);
    REQUIRE (analyzer44100.bandForBin (17) == 16);
    REQUIRE (analyzer44100.bandForBin (639) == 63);

    WallpaperEngineSpectrumAnalyzer analyzer48000 (48000);
    REQUIRE (analyzer48000.frameCount () == 2089);
    REQUIRE (WallpaperEngineSpectrumAnalyzer::windowForSampleRate (48000).pcmFrames == 1392);
    REQUIRE (analyzer48000.analyzedBinCount () == 640);
    REQUIRE (analyzer48000.frequencyForBin (639) == Catch::Approx (14682.623f));

    WallpaperEngineSpectrumAnalyzer analyzer96000 (96000);
    REQUIRE (analyzer96000.frameCount () == 4179);
    REQUIRE (WallpaperEngineSpectrumAnalyzer::windowForSampleRate (96000).pcmFrames == 2786);
    REQUIRE (analyzer96000.analyzedBinCount () == 640);
    REQUIRE (analyzer96000.frequencyForBin (639) == Catch::Approx (14679.110f));
}

TEST_CASE ("Wallpaper Engine spectrum keeps stereo channels independent at each input gain") {
    WallpaperEngineSpectrumAnalyzer analyzer (44100);
    std::vector<float> samples (analyzer.frameCount () * 2);
    const float leftFrequency = analyzer.frequencyForBin (17);
    const float rightFrequency = analyzer.frequencyForBin (300);

    for (std::size_t frame = 0; frame < analyzer.frameCount (); frame++) {
	const float time = static_cast<float> (frame) / 44100.0f;
	samples[frame * 2] = std::sin (2.0f * std::numbers::pi_v<float> * leftFrequency * time) * 0.1f;
	samples[frame * 2 + 1] = std::sin (2.0f * std::numbers::pi_v<float> * rightFrequency * time) * 0.1f;
    }

    std::array<float, 64> left {};
    std::array<float, 64> right {};
    analyzer.processInterleaved (samples.data (), analyzer.frameCount (), 2, left.data (), right.data ());

    const std::size_t leftPeak = std::distance (left.begin (), std::max_element (left.begin (), left.end ()));
    const std::size_t rightPeak = std::distance (right.begin (), std::max_element (right.begin (), right.end ()));
    REQUIRE (leftPeak == analyzer.bandForBin (17));
    REQUIRE (rightPeak == analyzer.bandForBin (300));
    REQUIRE (leftPeak != rightPeak);
    // Independent DFT of the recovered padded window, weighting and output scale.
    REQUIRE (left[leftPeak] == Catch::Approx (0.32432690f).margin (0.0001f));
    REQUIRE (right[rightPeak] == Catch::Approx (3.63815174f).margin (0.0001f));

    const auto unityLeft = left;
    const auto unityRight = right;
    // Native capture gain is linear and its zero setting mutes both raw spectra.
    for (const float gain : { 0.0f, 0.5f, 2.0f, 4.0f }) {
	CAPTURE (gain);
	analyzer.setInputVolume (gain);
	analyzer.processInterleaved (samples.data (), analyzer.frameCount (), 2, left.data (), right.data ());
	for (std::size_t band = 0; band < left.size (); band++) {
	    CAPTURE (band);
	    REQUIRE (left[band] == Catch::Approx (unityLeft[band] * gain));
	    REQUIRE (right[band] == Catch::Approx (unityRight[band] * gain));
	}
    }
}

TEST_CASE ("48 kHz stereo tones retain the native 16-band sidelobes") {
    WallpaperEngineSpectrumAnalyzer analyzer (48000);
    PlaybackRecorder recorder;
    std::vector<float> samples (analyzer.frameCount () * 2);
    for (std::size_t frame = 0; frame < analyzer.frameCount (); ++frame) {
	const double phase = 2.0 * std::numbers::pi * frame / 48000;
	samples[frame * 2] = std::round (0.1 * 32768 * std::sin (750 * phase)) / 32768;
	samples[frame * 2 + 1] = std::round (0.1 * 32768 * std::sin (3000 * phase)) / 32768;
    }

    std::array<float, 64> left {};
    std::array<float, 64> right {};
    analyzer.processInterleaved (samples.data (), analyzer.frameCount (), 2, left.data (), right.data ());
    for (int frame = 0; frame < 240; ++frame) {
	recorder.setRawSpectrum (left.data (), right.data (), 1.0f / 24);
    }

    // Stable SceneScript readback from the native Sun capture at unity gain,
    // with the two fixed-phase stereo tones above. The native reciprocal estimate
    // differs slightly from the fork's floating-point division.
    constexpr std::array<float, 16> expectedLeft {
	0.00764926f, 0.00982265f, 0.01190261f, 0.01488575f, 0.02587325f, 0.03686618f, 0.05805245f, 0.89863783f,
	0.06603922f, 0.02605979f, 0.01893271f, 0.01636123f, 0.01484112f, 0.01370158f, 0.01255407f, 0.01142866f,
    };
    constexpr std::array<float, 16> expectedRight {
	0.00311676f, 0.00328091f, 0.00349941f, 0.00406866f, 0.00448307f, 0.00489391f, 0.00583692f, 0.00817295f,
	0.01502740f, 0.03851938f, 1.00003517f, 0.03723350f, 0.02554669f, 0.01437250f, 0.00953569f, 0.00668465f,
    };
    for (std::size_t band = 0; band < expectedLeft.size (); ++band) {
	CAPTURE (band);
	REQUIRE (recorder.audio16Left[band] == Catch::Approx (expectedLeft[band]).margin (0.00015f));
	REQUIRE (recorder.audio16Right[band] == Catch::Approx (expectedRight[band]).margin (0.00015f));
    }
}

TEST_CASE ("scene spectra use native time-based normalization and peak reduction") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left[3] = 2.0f;
    right[6] = 1.0f;

    recorder.setRawSpectrum (left.data (), right.data (), 0.05f);

    // The first audible block resets the envelopes to one. Left rises to 1.05; right stays at one.
    // dt*20 and dt*40 both saturate, so the final slew-limited values reach one.
    REQUIRE (recorder.audio64Left[3] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio64Right[6] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio64Average[3] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio32Left[1] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio16Left[0] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio16Right[1] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio16Average[0] == Catch::Approx (0.5f));
}

TEST_CASE ("scene spectrum starts at the envelope floor for a below-threshold signal") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left[37] = 0.00005f;

    // A fresh envelope is zero. This signal is too quiet to trigger the restart to one.
    recorder.setRawSpectrum (left.data (), right.data (), 0.05f);
    REQUIRE (recorder.audio64Left[37] == Catch::Approx (0.05f));
    REQUIRE (recorder.rawAudio64Left[37] == left[37]);
}

TEST_CASE ("scene spectrum uses the minimum native frame interval") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left[37] = 0.1f;

    // At 0.1 ms the envelope is 0.99995, and the intermediate advances by 0.002.
    recorder.setRawSpectrum (left.data (), right.data (), 0.0f);
    REQUIRE (recorder.audio64Left[37] == Catch::Approx (0.00020001f));
}

TEST_CASE ("scene spectrum caps the envelope step after a long frame") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left[37] = 1.0f;
    recorder.setRawSpectrum (left.data (), right.data (), 0.05f);

    // The maximum 0.25 s interval only lowers the envelope to 0.875.
    left[37] = 0.1f;
    recorder.setRawSpectrum (left.data (), right.data (), 0.8f);
    REQUIRE (recorder.audio64Left[37] == Catch::Approx (0.11428571f));
}

TEST_CASE ("scene normalization keeps quiet frequency groups relative to the stereo peak") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left[37] = 3.0f;
    left[7] = 0.0004f;
    right[5] = 0.3f;
    right[55] = 0.0006f;

    // Let the envelopes settle: independently normalizing quiet groups used to
    // turn this tiny leakage into bars at 40% and 60% height.
    for (int frame = 0; frame < 1200; frame++) {
	recorder.setRawSpectrum (left.data (), right.data (), 1.0f / 60.0f);
    }

    REQUIRE (recorder.audio64Left[37] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio64Left[7] < 0.001f);
    REQUIRE (recorder.audio64Right[55] < 0.001f);
    // The native floor is shared across channels: the weaker tone retains its level.
    REQUIRE (recorder.audio64Right[5] == Catch::Approx (0.3f / (3.0f * 0.333f)));
    REQUIRE (recorder.rawAudio64Left[7] == left[7]);
    REQUIRE (recorder.rawAudio64Right[55] == right[55]);
    REQUIRE (recorder.audio32Left[3] < 0.001f);
    REQUIRE (recorder.audio16Right[13] < 0.001f);
}

TEST_CASE ("a sustained 2000 Hz tone keeps the scene sidelobes near the expected peak") {
    for (const uint32_t sampleRate : { 44100u, 48000u }) {
	for (const double amplitude : { 0.1, 0.9 }) {
	    CAPTURE (sampleRate, amplitude);
	    WallpaperEngineSpectrumAnalyzer analyzer (sampleRate);
	    PlaybackRecorder recorder;
	    std::vector<float> samples (analyzer.frameCount ());
	    std::array<float, 64> left {};
	    std::array<float, 64> right {};

	    // 44.1 kHz deliberately places the tone between FFT bins. A quiet,
	    // bin-aligned tone alone hid the normalization bug in earlier tests.
	    for (std::size_t block = 0; block < 300; block++) {
		for (std::size_t frame = 0; frame < samples.size (); frame++) {
		    const double time = static_cast<double> (block * samples.size () + frame) / sampleRate;
		    samples[frame] = static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * 2000.0 * time));
		}
		analyzer.processInterleaved (samples.data (), samples.size (), 1, left.data (), right.data ());
		recorder.setRawSpectrum (
		    left.data (), right.data (), static_cast<float> (samples.size ()) / sampleRate
		);
	    }

	    const auto peak = std::distance (
		recorder.audio64Left, std::max_element (recorder.audio64Left, recorder.audio64Left + 64)
	    );
	    const auto expectedBin = static_cast<std::size_t> (std::lround (2000.0 * samples.size () / sampleRate));
	    REQUIRE (peak == analyzer.bandForBin (expectedBin));
	    REQUIRE (recorder.audio64Left[peak] > 0.8f);
	    float sidelobePeak = 0.0f;
	    float distantPeak = 0.0f;
	    for (int band = 0; band < 64; band++) {
		if (std::abs (band - peak) > 1) {
		    sidelobePeak = std::max (sidelobePeak, recorder.audio64Left[band]);
		}
		if (std::abs (band - peak) > 16) {
		    distantPeak = std::max (distantPeak, recorder.audio64Left[band]);
		}
	    }
	    // Across input phases, the independent padded DFT predicts about 0.11 to
	    // 0.13 outside the immediate neighbors, and at most 0.032 far from the tone.
	    REQUIRE (sidelobePeak > 0.10f);
	    REQUIRE (sidelobePeak < 0.14f);
	    REQUIRE (distantPeak < 0.034f);
	}
    }
}

TEST_CASE ("scene normalization restarts its envelope when audio returns after silence") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    for (int frame = 0; frame < 600; frame++) {
	recorder.setRawSpectrum (left.data (), right.data (), 1.0f / 60.0f);
    }
    left[37] = 0.1f;
    recorder.setRawSpectrum (left.data (), right.data (), 1.0f / 60.0f);
    REQUIRE (recorder.audio64Left[37] > 0.03f);
    REQUIRE (recorder.audio64Left[37] < 0.05f);
    REQUIRE (recorder.audio64Right[37] == 0.0f);
}

TEST_CASE ("reduced scene Average pools stereo pairs before frequency groups") {
    PlaybackRecorder recorder;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left[0] = 1.0f;
    right[1] = 1.0f;
    left[8] = 1.0f;
    right[11] = 1.0f;
    left[20] = right[20] = 1.0f;
    left[32] = 1.0f;

    // Saturated interpolation and slew make these processed peaks exactly one.
    recorder.setRawSpectrum (left.data (), right.data (), 0.05f);

    // Separate left/right peaks must not combine into a stronger Average band.
    REQUIRE (recorder.audio64Average[0] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio64Average[1] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio32Left[0] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio32Right[0] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio32Average[0] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio16Average[0] == Catch::Approx (0.5f));

    // The 16-band reduction also spans peaks in different 32-band groups.
    REQUIRE (recorder.audio32Average[4] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio32Average[5] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio16Left[2] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio16Right[2] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio16Average[2] == Catch::Approx (0.5f));

    // Coincident stereo peaks, one-channel peaks and silence retain their levels.
    REQUIRE (recorder.audio32Average[10] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio16Average[5] == Catch::Approx (1.0f));
    REQUIRE (recorder.audio32Average[16] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio16Average[8] == Catch::Approx (0.5f));
    REQUIRE (recorder.audio32Average[8] == 0.0f);
    REQUIRE (recorder.audio16Average[4] == 0.0f);
    REQUIRE (recorder.rawAudio64Left[0] == left[0]);
    REQUIRE (recorder.rawAudio64Right[11] == right[11]);
}
