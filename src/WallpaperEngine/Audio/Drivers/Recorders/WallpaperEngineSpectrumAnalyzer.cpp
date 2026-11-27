#include "WallpaperEngineSpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace WallpaperEngine::Audio::Drivers::Recorders {
namespace {
    constexpr float kPcmScale = 127.0f;
    constexpr float kBandCurveExponent = 0.25f;
    constexpr float kFrequencyWeightCenter = 0.5009999871253967f;
    constexpr float kOutputScale = 0.001f;

    kiss_fft_cpx multiply (const kiss_fft_cpx a, const kiss_fft_cpx b) {
	return { a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r };
    }
} // namespace

WallpaperEngineSpectrumAnalyzer::WallpaperEngineSpectrumAnalyzer (const uint32_t sampleRate) {
    this->configure (sampleRate);
}

WallpaperEngineSpectrumAnalyzer::~WallpaperEngineSpectrumAnalyzer () {
    kiss_fft_free (this->m_inverseFFT);
    kiss_fft_free (this->m_fft);
}

WallpaperEngineSpectrumAnalyzer::Window WallpaperEngineSpectrumAnalyzer::windowForSampleRate (const uint32_t sampleRate) {
    const float rateMultiplier = std::max (static_cast<float> (sampleRate) / 44100.0f, 1.0f);
    const auto transformFrames = static_cast<std::size_t> (rateMultiplier * BandCount * 30.0f);
    const float transformLength = static_cast<float> (transformFrames);
    const auto pcmFrames = static_cast<std::size_t> (transformLength - (10.0f / 30.0f) * transformLength);
    return { transformFrames, pcmFrames };
}

void WallpaperEngineSpectrumAnalyzer::configure (const uint32_t sampleRate) {
    if (sampleRate == this->m_sampleRate && this->m_fft != nullptr) {
	return;
    }

    this->m_sampleRate = sampleRate == 0 ? 44100 : sampleRate;
    this->m_window = windowForSampleRate (this->m_sampleRate);
    const int transformFrames = static_cast<int> (this->m_window.transformFrames);
    const bool direct = kiss_fft_next_fast_size (transformFrames) == transformFrames;
    const int planSize = direct ? transformFrames : kiss_fft_next_fast_size (2 * transformFrames - 1);

    kiss_fft_free (this->m_fft);
    kiss_fft_free (this->m_inverseFFT);
    this->m_fft = kiss_fft_alloc (planSize, 0, nullptr, nullptr);
    this->m_inverseFFT = direct ? nullptr : kiss_fft_alloc (planSize, 1, nullptr, nullptr);
    if (!direct && this->m_inverseFFT == nullptr) {
	kiss_fft_free (this->m_fft);
	this->m_fft = nullptr;
    }
    this->m_fftInput.resize (planSize);
    this->m_fftOutput.resize (planSize);
    this->m_chirp.clear ();
    this->m_chirpSpectrum.clear ();
    if (!direct && this->m_fft != nullptr) {
	this->prepareConvolution ();
    }
}

void WallpaperEngineSpectrumAnalyzer::prepareConvolution () {
    // Bluestein's algorithm uses a padded convolution to keep the exact native FFT
    // length when it has a large prime factor, such as the 2089 frames at 48 kHz.
    const std::size_t size = this->m_fftInput.size ();
    this->m_chirp.resize (this->m_window.transformFrames);
    this->m_chirpSpectrum.resize (size);
    std::fill (this->m_fftInput.begin (), this->m_fftInput.end (), kiss_fft_cpx {});
    for (std::size_t frame = 0; frame < this->m_chirp.size (); ++frame) {
	const double phase = std::numbers::pi * frame * frame / this->m_window.transformFrames;
	const auto real = static_cast<float> (std::cos (phase));
	const auto imaginary = static_cast<float> (std::sin (phase));
	this->m_chirp[frame] = { real, -imaginary };
	this->m_fftInput[frame] = { real, imaginary };
	if (frame != 0) {
	    this->m_fftInput[size - frame] = this->m_fftInput[frame];
	}
    }
    kiss_fft (this->m_fft, this->m_fftInput.data (), this->m_chirpSpectrum.data ());
}

void WallpaperEngineSpectrumAnalyzer::transform (const std::size_t pcmFrames) {
    if (this->m_inverseFFT == nullptr) {
	kiss_fft (this->m_fft, this->m_fftInput.data (), this->m_fftOutput.data ());
	return;
    }

    for (std::size_t frame = 0; frame < pcmFrames; ++frame) {
	this->m_fftInput[frame] = multiply (this->m_fftInput[frame], this->m_chirp[frame]);
    }
    kiss_fft (this->m_fft, this->m_fftInput.data (), this->m_fftOutput.data ());
    for (std::size_t bin = 0; bin < this->m_fftOutput.size (); ++bin) {
	this->m_fftOutput[bin] = multiply (this->m_fftOutput[bin], this->m_chirpSpectrum[bin]);
    }
    kiss_fft (this->m_inverseFFT, this->m_fftOutput.data (), this->m_fftInput.data ());
    const float scale = 1.0f / static_cast<float> (this->m_fftInput.size ());
    for (std::size_t bin = 1; bin < BaseAnalyzedBinCount; ++bin) {
	const auto value = multiply (this->m_fftInput[bin], this->m_chirp[bin]);
	this->m_fftOutput[bin] = { value.r * scale, value.i * scale };
    }
}

void WallpaperEngineSpectrumAnalyzer::setInputVolume (const float volume) {
    this->m_inputVolume = std::isfinite (volume) && volume >= 0.0f ? volume : 1.0f;
}

uint32_t WallpaperEngineSpectrumAnalyzer::sampleRate () const { return this->m_sampleRate; }

std::size_t WallpaperEngineSpectrumAnalyzer::frameCount () const { return this->m_window.transformFrames; }

std::size_t WallpaperEngineSpectrumAnalyzer::analyzedBinCount () const { return BaseAnalyzedBinCount; }

float WallpaperEngineSpectrumAnalyzer::frequencyForBin (const std::size_t bin) const {
    return static_cast<float> (bin) * static_cast<float> (this->m_sampleRate) / static_cast<float> (this->frameCount ());
}

std::size_t WallpaperEngineSpectrumAnalyzer::bandForBin (const std::size_t requestedBin) const {
    if (requestedBin == 0) {
	return 0;
    }

    std::size_t previousBand = 0;
    const std::size_t lastBin = std::min (requestedBin, BaseAnalyzedBinCount - 1);
    for (std::size_t bin = 1; bin <= lastBin; bin++) {
	const float position = static_cast<float> (bin - 1) / static_cast<float> (BaseAnalyzedBinCount - 1);
	const std::size_t curvedBand = static_cast<std::size_t> (std::pow (position, kBandCurveExponent) * BandCount);
	previousBand = std::min ({ curvedBand, previousBand + 1, BandCount - 1 });
    }
    return previousBand;
}

void WallpaperEngineSpectrumAnalyzer::processInterleaved (
    const float* samples, const std::size_t frameCount, const unsigned int channelCount, float* left, float* right
) {
    if (this->m_fft == nullptr || samples == nullptr || channelCount == 0) {
	std::fill_n (left, BandCount, 0.0f);
	std::fill_n (right, BandCount, 0.0f);
	return;
    }

    this->processChannel (samples, frameCount, channelCount, 0, left);
    if (channelCount < 2) {
	std::memcpy (right, left, BandCount * sizeof (float));
    } else {
	this->processChannel (samples, frameCount, channelCount, 1, right);
    }
}

void WallpaperEngineSpectrumAnalyzer::processChannel (
    const float* samples, const std::size_t frameCount, const unsigned int channelCount, const unsigned int channel,
    float* output
) {
    const std::size_t pcmFrames = std::min (frameCount, this->m_window.pcmFrames);
    for (std::size_t frame = 0; frame < pcmFrames; frame++) {
	const float transformed = samples[frame * channelCount + channel] * kPcmScale + kPcmScale;
	this->m_fftInput[frame] = { transformed - kPcmScale, 1.0f / transformed - 1.0f / kPcmScale };
    }
    // Subtract the encoded silence from the entire window. It contributes only
    // to the discarded DC bin, and the padded silence then becomes an exact zero.
    std::fill (this->m_fftInput.begin () + pcmFrames, this->m_fftInput.end (), kiss_fft_cpx {});

    this->transform (pcmFrames);
    std::fill_n (output, BandCount, 0.0f);

    std::size_t previousBand = 0;
    for (std::size_t bin = 1; bin < BaseAnalyzedBinCount; bin++) {
	const float real = this->m_fftOutput[bin].r;
	const float imaginary = this->m_fftOutput[bin].i;
	float magnitudeSquared = real * real + imaginary * imaginary;
	if (!std::isfinite (magnitudeSquared)) {
	    magnitudeSquared = 0.0f;
	}

	const float position = static_cast<float> (bin - 1) / static_cast<float> (BaseAnalyzedBinCount - 1);
	const std::size_t curvedBand = static_cast<std::size_t> (std::pow (position, kBandCurveExponent) * BandCount);
	const std::size_t band = std::min ({ curvedBand, previousBand + 1, BandCount - 1 });
	previousBand = band;

	const float phase = position * std::numbers::pi_v<float>;
	const float weight = kFrequencyWeightCenter - std::cos (phase) * (1.0f - kFrequencyWeightCenter);
	const float value = std::sqrt (weight * magnitudeSquared);
	output[band] = std::max (output[band], value);
    }

    const float scale = this->m_inputVolume * kOutputScale
	* (static_cast<float> (BaseAnalyzedBinCount) / (static_cast<float> (this->frameCount ()) * 0.5f));
    for (std::size_t band = 0; band < BandCount; band++) {
	output[band] *= scale;
    }
}
} // namespace WallpaperEngine::Audio::Drivers::Recorders
