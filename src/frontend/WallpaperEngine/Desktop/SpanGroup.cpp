#include "SpanGroup.h"

#include <climits>
#include <ranges>

using namespace WallpaperEngine::Desktop;

void SpanGroup::registerOutput (const std::string& screen, Output* output) {
    this->m_outputs[screen] = output;
    this->calculateBounds ();
}

void SpanGroup::unregisterOutput (const std::string& screen) {
    this->m_outputs.erase (screen);
    this->calculateBounds ();
}

const std::map<std::string, Output*>& SpanGroup::getOutputs () const { return this->m_outputs; }

const glm::ivec4& SpanGroup::getBounds () const { return this->m_bounds; }

void SpanGroup::calculateBounds () {
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;

    // calculate positions based on all the screens and set the framebuffer bounds
    for (const auto& spanOutput : this->m_outputs | std::views::values) {
	glm::ivec2 globalPosition = spanOutput->getGlobalPosition ();
	glm::ivec2 logicalSize = spanOutput->getLogicalSize ();

	minX = std::min (minX, globalPosition.x);
	minY = std::min (minY, globalPosition.y);
	maxX = std::max (maxX, globalPosition.x + logicalSize.x);
	maxY = std::max (maxY, globalPosition.y + logicalSize.y);
    }

    this->m_bounds = { minX, minY, maxX - minX, maxY - minY };
}