#include <algorithm>
#include <stdexcept>
#include "CDynamicValue.h"

using namespace WallpaperEngine::Core::DynamicValues;

CDynamicValue::CDynamicValue() :
    m_ivec4(new glm::ivec4()),
    m_ivec3(new glm::ivec3()),
    m_ivec2(new glm::ivec2()),
    m_vec4 (new glm::vec4()),
    m_vec3 (new glm::vec3()),
    m_vec2 (new glm::vec2()),
    m_float (new float()),
    m_int (new int()),
    m_bool (new bool()),
    m_outgoingConnections (),
    m_incomingConnections () {}

CDynamicValue::~CDynamicValue() {
    for (auto* connection : this->m_incomingConnections) {
        connection->destroyOutgoingConnection (this);
    }

    delete this->m_ivec4;
    delete this->m_ivec3;
    delete this->m_ivec2;
    delete this->m_vec4;
    delete this->m_vec3;
    delete this->m_vec2;
    delete this->m_float;
    delete this->m_int;
    delete this->m_bool;
}

void CDynamicValue::update(float newValue) {
    *this->m_ivec4 = glm::ivec4(static_cast<int> (newValue));
    *this->m_ivec3 = glm::ivec3(static_cast<int> (newValue));
    *this->m_ivec2 = glm::ivec2(static_cast<int> (newValue));
    *this->m_vec4 = glm::vec4(newValue);
    *this->m_vec3 = glm::vec3(newValue);
    *this->m_vec2 = glm::vec2(newValue);
    *this->m_float = newValue;
    *this->m_int = static_cast<int> (newValue);
    *this->m_bool = static_cast<int> (newValue);

    this->propagate ();
}

void CDynamicValue::update(int newValue) {
    *this->m_ivec4 = glm::ivec4(newValue);
    *this->m_ivec3 = glm::ivec3(newValue);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec4 = glm::vec4(static_cast<float> (newValue));
    *this->m_vec3 = glm::vec3(static_cast<float> (newValue));
    *this->m_vec2 = glm::vec2(static_cast<float> (newValue));
    *this->m_float = static_cast<float> (newValue);
    *this->m_int = newValue;
    *this->m_bool = newValue;

    this->propagate ();
}

void CDynamicValue::update(bool newValue) {
    *this->m_ivec4 = glm::ivec4(newValue);
    *this->m_ivec3 = glm::ivec3(newValue);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec4 = glm::vec4(newValue);
    *this->m_vec3 = glm::vec3(newValue);
    *this->m_vec2 = glm::vec2(newValue);
    *this->m_float = newValue;
    *this->m_int = newValue;
    *this->m_bool = newValue;

    this->propagate ();
}

void CDynamicValue::update(const glm::vec2& newValue) {
    *this->m_ivec4 = glm::ivec4(newValue, 0, 0);
    *this->m_ivec3 = glm::ivec3(newValue, 0);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec2 = newValue;
    *this->m_vec3 = glm::vec3(newValue, 0.0f);
    *this->m_vec4 = glm::vec4(newValue, 0.0f, 0.0f);
    *this->m_float = newValue.x;
    *this->m_int = static_cast<int> (newValue.x);
    *this->m_bool = newValue.x != 0.0f;

    this->propagate ();
}

void CDynamicValue::update(const glm::vec3& newValue) {
    *this->m_ivec4 = glm::ivec4(newValue, 0);
    *this->m_ivec3 = glm::ivec3(newValue);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec2 = glm::vec2(newValue);
    *this->m_vec3 = newValue;
    *this->m_vec4 = glm::vec4(newValue, 0.0f);
    *this->m_float = newValue.x;
    *this->m_int = static_cast<int> (newValue.x);
    *this->m_bool = newValue.x != 0.0f;

    this->propagate ();
}

void CDynamicValue::update(const glm::vec4& newValue) {
    *this->m_ivec4 = glm::ivec4(newValue);
    *this->m_ivec3 = glm::ivec3(newValue);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec2 = glm::vec2(newValue);
    *this->m_vec3 = glm::vec3(newValue);
    *this->m_vec4 = newValue;
    *this->m_float = newValue.x;
    *this->m_int = static_cast<int> (newValue.x);
    *this->m_bool = newValue.x != 0.0f;

    this->propagate ();
}

void CDynamicValue::update(const glm::ivec2& newValue) {
    *this->m_ivec4 = glm::ivec4(newValue, 0, 0);
    *this->m_ivec3 = glm::ivec3(newValue, 0);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec2 = newValue;
    *this->m_vec3 = glm::vec3(newValue, 0.0f);
    *this->m_vec4 = glm::vec4(newValue, 0.0f, 0.0f);
    *this->m_float = static_cast<float> (newValue.x);
    *this->m_int = static_cast<int> (newValue.x);
    *this->m_bool = newValue.x;

    this->propagate ();
}

void CDynamicValue::update(const glm::ivec3& newValue) {
    *this->m_ivec4 = glm::ivec4(newValue, 0);
    *this->m_ivec3 = glm::ivec3(newValue);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec2 = glm::vec2(newValue);
    *this->m_vec3 = newValue;
    *this->m_vec4 = glm::vec4(newValue, 0.0f);
    *this->m_float = static_cast<float> (newValue.x);
    *this->m_int = static_cast<int> (newValue.x);
    *this->m_bool = newValue.x;

    this->propagate ();
}

void CDynamicValue::update(const glm::ivec4& newValue) {
    *this->m_ivec4 = glm::ivec4(newValue);
    *this->m_ivec3 = glm::ivec3(newValue);
    *this->m_ivec2 = glm::ivec2(newValue);
    *this->m_vec2 = glm::vec2(newValue);
    *this->m_vec3 = glm::vec3(newValue);
    *this->m_vec4 = newValue;
    *this->m_float = static_cast<float> (newValue.x);
    *this->m_int = static_cast<int> (newValue.x);
    *this->m_bool = newValue.x;

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
        *cur->m_bool = *this->m_bool;
        *cur->m_int = *this->m_int;
        *cur->m_float = *this->m_float;
        *cur->m_ivec2 = *this->m_ivec2;
        *cur->m_ivec3 = *this->m_ivec3;
        *cur->m_ivec4 = *this->m_ivec4;
        *cur->m_vec2 = *this->m_vec2;
        *cur->m_vec3 = *this->m_vec3;
        *cur->m_vec4 = *this->m_vec4;
    }
}

const glm::ivec4& CDynamicValue::getIVec4 () const {
    return *this->m_ivec4;
}

const glm::ivec3& CDynamicValue::getIVec3 () const {
    return *this->m_ivec3;
}

const glm::ivec2& CDynamicValue::getIVec2 () const {
    return *this->m_ivec2;
}

const glm::vec4& CDynamicValue::getVec4 () const {
    return *this->m_vec4;
}

const glm::vec3& CDynamicValue::getVec3 () const {
    return *this->m_vec3;
}

const glm::vec2& CDynamicValue::getVec2 () const {
    return *this->m_vec2;
}

const float& CDynamicValue::getFloat () const {
    return *this->m_float;
}

const int& CDynamicValue::getInt () const {
    return *this->m_int;
}

const bool& CDynamicValue::getBool () const {
    return *this->m_bool;
}