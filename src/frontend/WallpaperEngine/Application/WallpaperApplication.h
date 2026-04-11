#pragma once

#include <chrono>
#include <glad/glad.h>
#include <random>
#include <set>

#include <linux-wallpaperengine/project.h>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Audio/Pulseaudio.h"
#include "WallpaperEngine/Desktop/Environment.h"

namespace WallpaperEngine::Application {
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 */
class WallpaperApplication {
public:
	explicit WallpaperApplication (ApplicationContext& context);

	/**
	 * Prepares the application for rendering.
	 */
	void setup ();
	/**
	 * Renders a frame of the application.
	 */
	void render ();
	/**
	 * Cleans up all the resources used by the application.
	 */
	static void cleanup ();
	/**
	 * Shows the application until it's closed
	 */
	void show ();
	/**
	 * Handles a OS signal sent to this PID
	 *
	 * @param signal
	 */
	void signal (int signal);
	/**
	 * @return Maps screens to loaded backgrounds
	 */
	[[nodiscard]] const std::map<std::string, wp_project*>& getBackgrounds () const;
	/**
	 * @return The current application context
	 */
	[[nodiscard]] ApplicationContext& getContext () const;

private:
	/**
	 * Loads and sets up the desktop environment to use
	 */
	void setupEnvironment ();
	/**
	 * Loads projects based off the settings
	 */
	void loadBackgrounds ();
	/**
	 * Loads the given project
	 *
	 * @param bg
	 * @return
	 */
	[[nodiscard]] wp_project* loadBackground (const std::string& bg) const;
	/**
	 * Prepares all background's values and updates their properties if required
	 */
	void setupProperties ();
	/**
	 * Updates the properties for the given background based on the current context
	 *
	 * @param project
	 */
	void setupPropertiesForProject (wp_project* project);
	/**
	 * Prepares all audio-related things (like detector, output, etc)
	 */
	void setupAudio ();
	/**
	 * Prepares output debugging for all opengl errors
	 */
	static void setupOpenGLDebugging ();
	/**
	 * Takes an screenshot of the background and saves it to the specified path
	 *
	 * @param filename
	 */
	void takeScreenshot (const std::filesystem::path& filename) const;
	/**
	 *
	 * @param screen
	 * @param playlist
	 * @param currentPath
	 */
	void registerPlaylist (
		const std::string& screen, const ApplicationContext::PlaylistDefinition& playlist,
		const std::optional<std::string>& currentPath
	);

	struct ActivePlaylist {
		ApplicationContext::PlaylistDefinition definition;
		std::vector<std::size_t> order;
		std::size_t orderIndex = 0;
		std::chrono::steady_clock::time_point nextSwitch;
		std::chrono::steady_clock::time_point lastUpdate;
		std::set<std::size_t> failedIndices;
	};

	void updatePlaylists ();
	void advancePlaylist (
		const std::string& screen, ActivePlaylist& playlist, const std::chrono::steady_clock::time_point& now
	);
	static bool selectNextCandidate (const ActivePlaylist& playlist, std::size_t& outOrderIndex);
	std::vector<std::size_t> buildPlaylistOrder (const ApplicationContext::PlaylistDefinition& definition);

	/** The application context that contains the current app settings */
	ApplicationContext& m_context;
	/** Maps screens to backgrounds */
	std::map<std::string, wp_project*> m_backgrounds {};
	std::map<std::string, ActivePlaylist> m_activePlaylists {};

	std::unique_ptr<Audio::Pulseaudio> m_playbackRecorder;

	std::mt19937 m_playlistRng { std::random_device {}() };
	bool m_screenShotTaken = false;
	uint32_t m_nextFrameScreenshot = 0;
	std::optional<std::chrono::steady_clock::time_point> m_pauseStart = std::nullopt;

	Desktop::Environment* m_desktopEnvironment;
};
} // namespace WallpaperEngine::Application
