#pragma once

#include <glm/vec3.hpp>
#include <random>

namespace WallpaperEngine::Maths {
float randomFloat (std::mt19937& rng, float min, float max);
glm::vec3 randomVec3 (std::mt19937& rng, const glm::vec3& min, const glm::vec3& max);
float lerp (float t, float a, float b);
float fadeValue (float life, float startTime, float endTime, float startValue, float endValue);
}