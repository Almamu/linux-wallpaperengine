#pragma once

#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace WallpaperEngine::Data::Model {
/**
 * Class that represents different types of dynamic values
 */
class DynamicValue {
  public:
    enum UnderlyingType {
        Null = 0,
        IVec4 = 1,
        IVec3 = 2,
        IVec2 = 3,
        Vec4 = 4,
        Vec3 = 5,
        Vec2 = 6,
        Float = 7,
        Int = 8,
        Boolean = 9
    };

    DynamicValue () = default;
    explicit DynamicValue(const glm::ivec4& value);
    explicit DynamicValue(const glm::ivec3& value);
    explicit DynamicValue(const glm::ivec2& value);
    explicit DynamicValue(const glm::vec4& value);
    explicit DynamicValue(const glm::vec3& value);
    explicit DynamicValue(const glm::vec2& value);
    explicit DynamicValue(float value);
    explicit DynamicValue(int value);
    explicit DynamicValue(bool value);
    virtual ~DynamicValue ();

    [[nodiscard]] const glm::ivec4& getIVec4 () const;
    [[nodiscard]] const glm::ivec3& getIVec3 () const;
    [[nodiscard]] const glm::ivec2& getIVec2 () const;
    [[nodiscard]] const glm::vec4& getVec4 () const;
    [[nodiscard]] const glm::vec3& getVec3 () const;
    [[nodiscard]] const glm::vec2& getVec2 () const;
    [[nodiscard]] const float& getFloat () const;
    [[nodiscard]] const int& getInt () const;
    [[nodiscard]] const bool& getBool () const;
    [[nodiscard]] UnderlyingType getType () const;
    [[nodiscard]] virtual std::string toString () const;

    void update (float newValue);
    void update (int newValue);
    void update (bool newValue);
    void update (const glm::vec2& newValue);
    void update (const glm::vec3& newValue);
    void update (const glm::vec4& newValue);
    void update (const glm::ivec2& newValue);
    void update (const glm::ivec3& newValue);
    void update (const glm::ivec4& newValue);
    void update (const DynamicValue& newValue);

    /**
     * Registers the given callback to be called when the value changes
     *
     * @param callback
     *
     * @return The de-register function to call when the listener is no longer needed
     */
    std::function<void ()> listen (const std::function<void (const DynamicValue&)>& callback);
    /**
     * Connects the current instance to the given instance, updating this instance's value
     * based on the given instance's value
     *
     * @param other
     *
     * @return The de-register function to call when the listener is no longer needed
     */
    void connect (DynamicValue* other);

    /**
     * Disconnects the current instance from all the connections to stop notifying
     * new value changes
     */
    void disconnect ();

  private:
    /**
     * Notifies any listeners that the value has changed
     */
    void propagate () const;

    std::vector<std::function<void (const DynamicValue&)>> m_listeners = {};
    std::vector<std::function<void ()>> m_connections = {};

    glm::ivec4 m_ivec4 = {};
    glm::ivec3 m_ivec3 = {};
    glm::ivec2 m_ivec2 = {};
    glm::vec4 m_vec4 = {};
    glm::vec3 m_vec3 = {};
    glm::vec2 m_vec2 = {};
    float m_float = 0.0f;
    int m_int = 0;
    bool m_bool = false;
    UnderlyingType m_type = Null;
};
}