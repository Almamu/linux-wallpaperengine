#include "Maths.h"

using namespace WallpaperEngine::Maths;

float WallpaperEngine::Maths::randomFloat (std::mt19937& rng, float min, float max) {
    if (max < min) {
        std::swap (min, max);
    }
    std::uniform_real_distribution<float> dist (min, max);
    return dist (rng);
}

glm::vec3 WallpaperEngine::Maths::randomVec3 (std::mt19937& rng, const glm::vec3& min, const glm::vec3& max) {
    return glm::vec3 (
        randomFloat (rng, min.x, max.x),
        randomFloat (rng, min.y, max.y),
        randomFloat (rng, min.z, max.z)
    );
}

// Helper: Linear interpolation
float WallpaperEngine::Maths::lerp (float t, float a, float b) { return a + t * (b - a); }

// Helper: Fade value change over lifetime
float WallpaperEngine::Maths::fadeValue (float life, float startTime, float endTime, float startValue, float endValue) {
    if (life <= startTime) {
        return startValue;
    } else if (life >= endTime) {
        return endValue;
    } else {
        float t = (life - startTime) / (endTime - startTime);
        return lerp (t, startValue, endValue);
    }
}