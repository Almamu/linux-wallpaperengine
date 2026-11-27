#pragma once

#include "kiss_fft.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace WallpaperEngine::Audio::Drivers::Recorders {
/**
 * The raw 64-band capture processor used by Wallpaper Engine build 23967692.
 *
 * The transform is 30 * 64 frames at 44.1 kHz and scales with higher mix rates.
 * The last third is padded with silence. The first 10 * 64 bins are mapped through a
 * fourth-root curve, keeping the strongest value assigned to each output band.
 */
class WallpaperEngineSpectrumAnalyzer final {
public:
    static constexpr std::size_t BandCount = 64;
    static constexpr std::size_t BaseFrameCount = 30 * BandCount;
    static constexpr std::size_t BaseAnalyzedBinCount = 10 * BandCount;
    static constexpr std::size_t BasePcmFrameCount = 20 * BandCount;

    struct Window {
	std::size_t transformFrames;
	std::size_t pcmFrames;
    };

    [[nodiscard]] static Window windowForSampleRate (uint32_t sampleRate);

    explicit WallpaperEngineSpectrumAnalyzer (uint32_t sampleRate = 44100);
    ~WallpaperEngineSpectrumAnalyzer ();

    WallpaperEngineSpectrumAnalyzer (const WallpaperEngineSpectrumAnalyzer&) = delete;
    WallpaperEngineSpectrumAnalyzer& operator= (const WallpaperEngineSpectrumAnalyzer&) = delete;

    void configure (uint32_t sampleRate);
    void setInputVolume (float volume);
    void processInterleaved (
	const float* samples, std::size_t frameCount, unsigned int channelCount, float* left, float* right
    );

    [[nodiscard]] uint32_t sampleRate () const;
    [[nodiscard]] std::size_t frameCount () const;
    [[nodiscard]] std::size_t analyzedBinCount () const;
    [[nodiscard]] float frequencyForBin (std::size_t bin) const;
    [[nodiscard]] std::size_t bandForBin (std::size_t bin) const;

private:
    void prepareConvolution ();
    void transform (std::size_t pcmFrames);
    void processChannel (
	const float* samples, std::size_t frameCount, unsigned int channelCount, unsigned int channel, float* output
    );

    uint32_t m_sampleRate = 0;
    Window m_window {};
    float m_inputVolume = 1.0f;
    kiss_fft_cfg m_fft = nullptr;
    kiss_fft_cfg m_inverseFFT = nullptr;
    std::vector<kiss_fft_cpx> m_fftInput;
    std::vector<kiss_fft_cpx> m_fftOutput;
    std::vector<kiss_fft_cpx> m_chirp;
    std::vector<kiss_fft_cpx> m_chirpSpectrum;
};
} // namespace WallpaperEngine::Audio::Drivers::Recorders
