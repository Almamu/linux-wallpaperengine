#include <algorithm>
#include "CDynamicValue.h"

using namespace WallpaperEngine::Core::DynamicValues;

CDynamicValue::~CDynamicValue() {
    for (auto* connection : this->m_incomingConnections) {
        connection->destroyOutgoingConnection (this);
    }
}

void CDynamicValue::update(float newValue) {
    this->m_ivec4 = glm::ivec4(static_cast<int> (newValue));
    this->m_ivec3 = glm::ivec3(static_cast<int> (newValue));
    this->m_ivec2 = glm::ivec2(static_cast<int> (newValue));
    this->m_vec4 = glm::vec4(newValue);
    this->m_vec3 = glm::vec3(newValue);
    this->m_vec2 = glm::vec2(newValue);
    this->m_float = newValue;
    this->m_int = static_cast<int> (newValue);
    this->m_bool = static_cast<int> (newValue) != 0;
    this->m_type = UnderlyingType::Float;

    this->propagate ();
}

void CDynamicValue::update(int newValue) {
    this->m_ivec4 = glm::ivec4(newValue);
    this->m_ivec3 = glm::ivec3(newValue);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec4 = glm::vec4(static_cast<float> (newValue));
    this->m_vec3 = glm::vec3(static_cast<float> (newValue));
    this->m_vec2 = glm::vec2(static_cast<float> (newValue));
    this->m_float = static_cast<float> (newValue);
    this->m_int = newValue;
    this->m_bool = newValue != 0;
    this->m_type = UnderlyingType::Int;

    this->propagate ();
}

void CDynamicValue::update(bool newValue) {
    this->m_ivec4 = glm::ivec4(newValue);
    this->m_ivec3 = glm::ivec3(newValue);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec4 = glm::vec4(newValue);
    this->m_vec3 = glm::vec3(newValue);
    this->m_vec2 = glm::vec2(newValue);
    this->m_float = newValue;
    this->m_int = newValue;
    this->m_bool = newValue;
    this->m_type = UnderlyingType::Boolean;

    this->propagate ();
}

void CDynamicValue::update(const glm::vec2& newValue) {
    this->m_ivec4 = glm::ivec4(newValue, 0, 0);
    this->m_ivec3 = glm::ivec3(newValue, 0);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec2 = newValue;
    this->m_vec3 = glm::vec3(newValue, 0.0f);
    this->m_vec4 = glm::vec4(newValue, 0.0f, 0.0f);
    this->m_float = newValue.x;
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0.0f;
    this->m_type = UnderlyingType::Vec2;

    this->propagate ();
}

void CDynamicValue::update(const glm::vec3& newValue) {
    this->m_ivec4 = glm::ivec4(newValue, 0);
    this->m_ivec3 = glm::ivec3(newValue);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec2 = glm::vec2(newValue);
    this->m_vec3 = newValue;
    this->m_vec4 = glm::vec4(newValue, 0.0f);
    this->m_float = newValue.x;
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0.0f;
    this->m_type = UnderlyingType::Vec3;

    this->propagate ();
}

void CDynamicValue::update(const glm::vec4& newValue) {
    this->m_ivec4 = glm::ivec4(newValue);
    this->m_ivec3 = glm::ivec3(newValue);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec2 = glm::vec2(newValue);
    this->m_vec3 = glm::vec3(newValue);
    this->m_vec4 = newValue;
    this->m_float = newValue.x;
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0.0f;
    this->m_type = UnderlyingType::Vec4;

    this->propagate ();
}

void CDynamicValue::update(const glm::ivec2& newValue) {
    this->m_ivec4 = glm::ivec4(newValue, 0, 0);
    this->m_ivec3 = glm::ivec3(newValue, 0);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec2 = newValue;
    this->m_vec3 = glm::vec3(newValue, 0.0f);
    this->m_vec4 = glm::vec4(newValue, 0.0f, 0.0f);
    this->m_float = static_cast<float> (newValue.x);
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0;
    this->m_type = UnderlyingType::IVec2;

    this->propagate ();
}

void CDynamicValue::update(const glm::ivec3& newValue) {
    this->m_ivec4 = glm::ivec4(newValue, 0);
    this->m_ivec3 = glm::ivec3(newValue);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec2 = glm::vec2(newValue);
    this->m_vec3 = newValue;
    this->m_vec4 = glm::vec4(newValue, 0.0f);
    this->m_float = static_cast<float> (newValue.x);
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0;
    this->m_type = UnderlyingType::IVec3;

    this->propagate ();
}

void CDynamicValue::update(const glm::ivec4& newValue) {
    this->m_ivec4 = glm::ivec4(newValue);
    this->m_ivec3 = glm::ivec3(newValue);
    this->m_ivec2 = glm::ivec2(newValue);
    this->m_vec2 = glm::vec2(newValue);
    this->m_vec3 = glm::vec3(newValue);
    this->m_vec4 = newValue;
    this->m_float = static_cast<float> (newValue.x);
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0;
    this->m_type = UnderlyingType::IVec4;

    this->propagate ();
}

void CDynamicValue::connectOutgoing (CDynamicValue* value) const {
    this->m_outgoingConnections.push_back (value);

    // ensure that new connection has the right value
    this->propagate ();
    // ensure the other value keeps track of our connection too
    value->connectIncoming (this);
}

void CDynamicValue::connectIncoming (const CDynamicValue* value) const {
    this->m_incomingConnections.push_back (value);
}

void CDynamicValue::destroyOutgoingConnection (CDynamicValue* value) const {
    this->m_outgoingConnections.erase (
        std::remove (this->m_outgoingConnections.begin (), this->m_outgoingConnections.end (), value), this->m_outgoingConnections.end ()
    );
}

void CDynamicValue::propagate () const {
    for (auto* cur : this->m_outgoingConnections) {
        cur->m_bool = this->m_bool;
        cur->m_int = this->m_int;
        cur->m_float = this->m_float;
        cur->m_ivec2 = this->m_ivec2;
        cur->m_ivec3 = this->m_ivec3;
        cur->m_ivec4 = this->m_ivec4;
        cur->m_vec2 = this->m_vec2;
        cur->m_vec3 = this->m_vec3;
        cur->m_vec4 = this->m_vec4;
    }
}

const glm::ivec4& CDynamicValue::getIVec4 () const {
    return this->m_ivec4;
}

const glm::ivec3& CDynamicValue::getIVec3 () const {
    return this->m_ivec3;
}

const glm::ivec2& CDynamicValue::getIVec2 () const {
    return this->m_ivec2;
}

const glm::vec4& CDynamicValue::getVec4 () const {
    return this->m_vec4;
}

const glm::vec3& CDynamicValue::getVec3 () const {
    return this->m_vec3;
}

const glm::vec2& CDynamicValue::getVec2 () const {
    return this->m_vec2;
}

const float& CDynamicValue::getFloat () const {
    return this->m_float;
}

const int& CDynamicValue::getInt () const {
    return this->m_int;
}

const bool& CDynamicValue::getBool () const {
    return this->m_bool;
}

CDynamicValue::UnderlyingType CDynamicValue::getType () const {
    return this->m_type;
}

std::string CDynamicValue::toString () const {
    switch (this->m_type) {
        case UnderlyingType::Float:
            return std::to_string (this->m_float);
        case UnderlyingType::Int:
            return std::to_string (this->m_int);
        case UnderlyingType::Boolean:
            return std::to_string (this->m_bool);
        case UnderlyingType::Vec2:
            return std::to_string (this->m_vec2.x) + ", " + std::to_string (this->m_vec2.y);
        case UnderlyingType::Vec3:
            return std::to_string (this->m_vec3.x) + ", " + std::to_string (this->m_vec3.y) + ", " + std::to_string (this->m_vec3.z);
        case UnderlyingType::Vec4:
            return std::to_string (this->m_vec4.x) + ", " + std::to_string (this->m_vec4.y) + ", " + std::to_string (this->m_vec4.z) + ", " + std::to_string (this->m_vec4.w);
        case UnderlyingType::IVec2:
            return std::to_string (this->m_ivec2.x) + ", " + std::to_string (this->m_ivec2.y);
        case UnderlyingType::IVec3:
            return std::to_string (this->m_ivec3.x) + ", " + std::to_string (this->m_ivec3.y) + ", " + std::to_string (this->m_ivec3.z);
        case UnderlyingType::IVec4:
            return std::to_string (this->m_ivec4.x) + ", " + std::to_string (this->m_ivec4.y) + ", " + std::to_string (this->m_ivec4.z) + ", " + std::to_string (this->m_ivec4.w);
        default:
            return "Unknown conversion for dynamic value of type: " + std::to_string (static_cast<int> (this->m_type));
    }
}