#pragma once

#include "Color.h"

#include <functional>
#include <glm/glm.hpp>
#include <list>
#include <memory>
#include <optional>
#include <string>

namespace WallpaperEngine::Data::Model {
struct ConditionInfo {
    std::string name;
    std::string condition;
};

// TODO: CHANGE THIS TO HOLD A REFERENCE TO THE OBJECT AND EXPOSE THE RIGHT PROPERTY MAP
struct ScriptContext {
    struct Object {
	int id;
	std::string name;
    } object;
};

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
	Boolean = 9,
	String = 10,
	Color = 11,
    };

    enum UpdateSource {
	Initialization = 0,
	Script = 1,
	User = 2,
    };

    DynamicValue () = default;
    explicit DynamicValue (const glm::ivec4& value);
    explicit DynamicValue (const glm::ivec3& value);
    explicit DynamicValue (const glm::ivec2& value);
    explicit DynamicValue (const glm::vec4& value);
    explicit DynamicValue (const glm::vec3& value);
    explicit DynamicValue (const glm::vec2& value);
    explicit DynamicValue (float value);
    explicit DynamicValue (int value);
    explicit DynamicValue (bool value);
    explicit DynamicValue (const std::string& value);
    explicit DynamicValue (const Model::Color& value);
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
    [[nodiscard]] const std::string& getString () const;
    [[nodiscard]] const Model::Color& getColor () const;
    [[nodiscard]] UnderlyingType getType () const;
    [[nodiscard]] virtual std::string toString () const;

    virtual void update (float newValue, UpdateSource source);
    virtual void update (int newValue, UpdateSource source);
    virtual void update (bool newValue, UpdateSource source);
    virtual void update (const glm::vec2& newValue, UpdateSource source);
    virtual void update (const glm::vec3& newValue, UpdateSource source);
    virtual void update (const glm::vec4& newValue, UpdateSource source);
    virtual void update (const glm::ivec2& newValue, UpdateSource source);
    virtual void update (const glm::ivec3& newValue, UpdateSource source);
    virtual void update (const glm::ivec4& newValue, UpdateSource source);
    virtual void update (const std::string& newValue, UpdateSource source);
    virtual void update (const Model::Color& newValue, UpdateSource source);
    virtual void update (const DynamicValue& other, UpdateSource source);
    /**
     * Sets the current value to null
     */
    virtual void update (UpdateSource source);

    /**
     * Registers the given callback to be called when the value changes
     *
     * @param callback
     *
     * @return The de-register function to call when the listener is no longer needed
     */
    std::function<void ()> listen (const std::function<void (const DynamicValue&, UpdateSource)>& callback);
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

    /**
     * Associates a condition with the dynamic value to apply proper checks
     *
     * @param condition
     */
    void attachCondition (const ConditionInfo& condition);
    /**
     * Associates a script source with the dynamic value to allow for dynamic updates
     *
     * @param source The script code to associate to this value
     */
    void setScriptSource (const std::string& source);
    /**
     * Clears the associated script to this dynamic value
     */
    void clearScriptSource ();
    /**
     * @return The current script source associated with this dynamic value
     */
    const std::optional<std::string>& getScriptSource () const;
    /**
     * Associates a script context with the dynamic value to provide the necessary environment for script execution
     *
     * @param context
     */
    void setScriptContext (const ScriptContext& context);
    /**
     * @return The associated script context for this dynamic value
     */
    const std::optional<ScriptContext>& getScriptContext () const;

private:
    /**
     * Notifies any listeners that the value has changed
     */
    void propagate (UpdateSource source) const;

    std::shared_ptr<bool> m_aliveFlag = std::make_shared<bool> (true);
    std::list<std::function<void (const DynamicValue&, UpdateSource)>> m_listeners = {};
    std::vector<std::function<void ()>> m_connections = {};
    std::optional<ScriptContext> m_scriptContext = std::nullopt;
    std::optional<std::string> m_scriptSource = std::nullopt;
    bool m_evaluating = false;

    glm::ivec4 m_ivec4 = {};
    glm::ivec3 m_ivec3 = {};
    glm::ivec2 m_ivec2 = {};
    glm::vec4 m_vec4 = {};
    glm::vec3 m_vec3 = {};
    glm::vec2 m_vec2 = {};
    Model::Color m_color = {};
    float m_float = 0.0f;
    int m_int = 0;
    bool m_bool = false;
    std::string m_string;
    UnderlyingType m_type = Null;
    std::optional<ConditionInfo> m_condition = std::nullopt;
};
}
