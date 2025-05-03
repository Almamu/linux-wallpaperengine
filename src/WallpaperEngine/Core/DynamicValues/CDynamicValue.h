#pragma once

#include <functional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace WallpaperEngine::Data::Parsers {
class UserSettingParser;
}

namespace WallpaperEngine::Data::Builders {
class UserSettingBuilder;
}

namespace WallpaperEngine::Core::DynamicValues {
class CDynamicValue {
    //TODO: THIS SHOULD BE CHANGED ONCE THINGS ARE FINISHED
    friend class WallpaperEngine::Data::Parsers::UserSettingParser;
    friend class WallpaperEngine::Data::Builders::UserSettingBuilder;
  public:
    virtual ~CDynamicValue ();

    [[nodiscard]] const glm::ivec4& getIVec4 () const;
    [[nodiscard]] const glm::ivec3& getIVec3 () const;
    [[nodiscard]] const glm::ivec2& getIVec2 () const;
    [[nodiscard]] const glm::vec4& getVec4 () const;
    [[nodiscard]] const glm::vec3& getVec3 () const;
    [[nodiscard]] const glm::vec2& getVec2 () const;
    [[nodiscard]] const float& getFloat () const;
    [[nodiscard]] const int& getInt () const;
    [[nodiscard]] const bool& getBool () const;

    /**
     * Connects the current instance to the given instance, updating it's values
     * based on current instance's changes
     *
     * @param value
     */
    void connectOutgoing (CDynamicValue* value) const;

  protected:
    void update (float newValue);
    void update (int newValue);
    void update (bool newValue);
    void update (const glm::vec2& newValue);
    void update (const glm::vec3& newValue);
    void update (const glm::vec4& newValue);
    void update (const glm::ivec2& newValue);
    void update (const glm::ivec3& newValue);
    void update (const glm::ivec4& newValue);
    /**
     * Registers an incoming connection (another CDynamicValue affecting the current instance's value)
     * Useful mainly for destroying the connection on delete
     *
     * @param value
     */
    void connectIncoming (const CDynamicValue* value) const;
    void destroyOutgoingConnection (CDynamicValue* value) const;
    /**
     * Propagates the current value to all it's connections
     */
    virtual void propagate () const;

  private:
    mutable std::vector<CDynamicValue*> m_outgoingConnections = {};
    mutable std::vector<const CDynamicValue*> m_incomingConnections = {};
    // different values that we will be casted to automagically
    glm::ivec4 m_ivec4 = {};
    glm::ivec3 m_ivec3 = {};
    glm::ivec2 m_ivec2 = {};
    glm::vec4 m_vec4 = {};
    glm::vec3 m_vec3 = {};
    glm::vec2 m_vec2 = {};
    float m_float = 0.0f;
    int m_int = 0;
    bool m_bool = false;
};
};