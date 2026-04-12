#pragma once

#include "Output.h"

#include <string>

namespace WallpaperEngine::Desktop {
template <class T> class VirtualOutput : public Output {
public:
	explicit VirtualOutput (T* output) :
		Output (nullptr, { 0, 0, 0, 0 }), m_wallpaper (nullptr), m_viewport ({ 0, 0, 1, 1 }), m_framebuffer (GL_NONE) {
		this->m_realOutput = output;
	}

	~VirtualOutput () override { }

	void render () override {
		if (this->m_realOutput == nullptr) {
			return;
		}

		this->m_realOutput->render ();
	}

	void setWallpaper (wp_project* wallpaper) override {
		this->m_wallpaper = wallpaper;

		if (this->m_realOutput == nullptr) {
			return;
		}

		this->m_realOutput->setWallpaper (wallpaper);
	}

	void setViewport (glm::vec4 viewport) override {
		this->m_viewport = viewport;

		if (this->m_realOutput == nullptr) {
			return;
		}

		this->m_realOutput->setViewport (viewport);
	}

	void setFramebuffer (GLuint framebuffer) override {
		this->m_framebuffer = framebuffer;

		if (this->m_realOutput == nullptr) {
			return;
		}

		this->m_realOutput->setFramebuffer (framebuffer);
	}

	void setRealOutput (T* output) {
		if (this->m_realOutput != nullptr) {
			// clear wallpaper reference
			this->m_realOutput->setWallpaper (nullptr);
		}

		this->m_realOutput = output;

		if (this->m_realOutput == nullptr) {
			return;
		}

		this->m_realOutput->setWallpaper (this->m_wallpaper);
		this->m_realOutput->setViewport (this->m_viewport);
		this->m_realOutput->setFramebuffer (this->m_framebuffer);
	}

	T* getRealOutput () { return this->m_realOutput; }

private:
	T* m_realOutput;
	wp_project* m_wallpaper;
	glm::vec4 m_viewport;
	GLuint m_framebuffer;
};
}
