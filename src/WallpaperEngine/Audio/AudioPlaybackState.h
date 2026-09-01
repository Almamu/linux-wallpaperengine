#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace WallpaperEngine::Audio {
class AudioPlaybackState {
public:
    enum class State {
	Playing,
	Paused,
	Stopped,
    };

    void play () { this->m_state.store (State::Playing); }
    void pause () { this->m_state.store (State::Paused); }

    std::uint64_t stop () {
	this->m_state.store (State::Stopped);
	return this->m_generation.fetch_add (1) + 1;
    }

    [[nodiscard]] bool isPlaying () const {
	return this->m_state.load () == State::Playing
	    && this->m_completedGeneration.load () == this->m_generation.load ();
    }

    [[nodiscard]] std::uint64_t generation () const { return this->m_generation.load (); }
    [[nodiscard]] std::uint64_t completedGeneration () const { return this->m_completedGeneration.load (); }
    [[nodiscard]] bool resetPending () const { return this->completedGeneration () != this->generation (); }

    void markResetComplete (std::uint64_t generation) { this->m_completedGeneration.store (generation); }

    void setVolume (float volume) { this->m_volume.store (std::clamp (volume, 0.0f, 1.0f)); }
    [[nodiscard]] float volume () const { return this->m_volume.load (); }

private:
    std::atomic<State> m_state = State::Playing;
    std::atomic<float> m_volume = 1.0f;
    std::atomic<std::uint64_t> m_generation = 0;
    std::atomic<std::uint64_t> m_completedGeneration = 0;
};
} // namespace WallpaperEngine::Audio
