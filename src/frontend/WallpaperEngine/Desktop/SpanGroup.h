#pragma once

#include "Output.h"
#include <glm/vec4.hpp>
#include <map>
#include <string>

namespace WallpaperEngine::Desktop {
class SpanGroup {
public:
    SpanGroup () = default;
    ~SpanGroup () = default;

    void registerOutput (const std::string& screen, Output* output);
    void unregisterOutput (const std::string& screen);
    [[nodiscard]] const std::map<std::string, Output*>& getOutputs () const;
    [[nodiscard]] const glm::ivec4& getBounds () const;

    /**
     * Recalculates the bounds of the span group based on the available outputs
     */
    void calculateBounds ();

private:
    std::map<std::string, Output*> m_outputs;
    glm::ivec4 m_bounds;
};
}