#include "DynamicValue.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Model;

DynamicValue::DynamicValue (const glm::ivec4& value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (const glm::ivec3& value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (const glm::ivec2& value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (const glm::vec4& value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (const glm::vec3& value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (const glm::vec2& value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (float value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (int value) { this->DynamicValue::update (value); }

DynamicValue::DynamicValue (bool value) { this->DynamicValue::update (value); }

DynamicValue::~DynamicValue () {
    if (this->m_aliveFlag) {
	*this->m_aliveFlag = false;
    }
    this->disconnect ();
    this->m_listeners.clear ();
}

const glm::ivec4& DynamicValue::getIVec4 () const { return this->m_ivec4; }

const glm::ivec3& DynamicValue::getIVec3 () const { return this->m_ivec3; }

const glm::ivec2& DynamicValue::getIVec2 () const { return this->m_ivec2; }

const glm::vec4& DynamicValue::getVec4 () const { return this->m_vec4; }

const glm::vec3& DynamicValue::getVec3 () const { return this->m_vec3; }

const glm::vec2& DynamicValue::getVec2 () const { return this->m_vec2; }

const float& DynamicValue::getFloat () const { return this->m_float; }

const int& DynamicValue::getInt () const { return this->m_int; }

const bool& DynamicValue::getBool () const { return this->m_bool; }

const std::string& DynamicValue::getString () const { return this->m_string; }

DynamicValue::UnderlyingType DynamicValue::getType () const { return this->m_type; }

std::string DynamicValue::toString () const {
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
	    return std::to_string (this->m_vec3.x) + ", " + std::to_string (this->m_vec3.y) + ", "
		+ std::to_string (this->m_vec3.z);
	case UnderlyingType::Vec4:
	    return std::to_string (this->m_vec4.x) + ", " + std::to_string (this->m_vec4.y) + ", "
		+ std::to_string (this->m_vec4.z) + ", " + std::to_string (this->m_vec4.w);
	case UnderlyingType::IVec2:
	    return std::to_string (this->m_ivec2.x) + ", " + std::to_string (this->m_ivec2.y);
	case UnderlyingType::IVec3:
	    return std::to_string (this->m_ivec3.x) + ", " + std::to_string (this->m_ivec3.y) + ", "
		+ std::to_string (this->m_ivec3.z);
	case UnderlyingType::IVec4:
	    return std::to_string (this->m_ivec4.x) + ", " + std::to_string (this->m_ivec4.y) + ", "
		+ std::to_string (this->m_ivec4.z) + ", " + std::to_string (this->m_ivec4.w);
	case UnderlyingType::String:
	    return this->m_string;
	default:
	    return "Unknown conversion for dynamic value of type: " + std::to_string (static_cast<int> (this->m_type));
    }
}

void DynamicValue::update (const float newValue) {
    this->m_ivec4 = glm::ivec4 (static_cast<int> (newValue));
    this->m_ivec3 = glm::ivec3 (static_cast<int> (newValue));
    this->m_ivec2 = glm::ivec2 (static_cast<int> (newValue));
    this->m_vec4 = glm::vec4 (newValue);
    this->m_vec3 = glm::vec3 (newValue);
    this->m_vec2 = glm::vec2 (newValue);
    this->m_float = newValue;
    this->m_int = static_cast<int> (newValue);
    this->m_bool = static_cast<int> (newValue) != 0;
    this->m_string = "";
    this->m_type = UnderlyingType::Float;

    this->propagate ();
}

void DynamicValue::update (const int newValue) {
    this->m_ivec4 = glm::ivec4 (newValue);
    this->m_ivec3 = glm::ivec3 (newValue);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec4 = glm::vec4 (static_cast<float> (newValue));
    this->m_vec3 = glm::vec3 (static_cast<float> (newValue));
    this->m_vec2 = glm::vec2 (static_cast<float> (newValue));
    this->m_float = static_cast<float> (newValue);
    this->m_int = newValue;
    this->m_bool = newValue != 0;
    this->m_string = "";
    this->m_type = UnderlyingType::Int;

    this->propagate ();
}

void DynamicValue::update (const bool newValue) {
    this->m_ivec4 = glm::ivec4 (newValue);
    this->m_ivec3 = glm::ivec3 (newValue);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec4 = glm::vec4 (newValue);
    this->m_vec3 = glm::vec3 (newValue);
    this->m_vec2 = glm::vec2 (newValue);
    this->m_float = newValue;
    this->m_int = newValue;
    this->m_bool = newValue;
    this->m_string = "";
    this->m_type = UnderlyingType::Boolean;

    this->propagate ();
}

void DynamicValue::update (const glm::vec2& newValue) {
    this->m_ivec4 = glm::ivec4 (newValue, 0, 0);
    this->m_ivec3 = glm::ivec3 (newValue, 0);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec2 = newValue;
    this->m_vec3 = glm::vec3 (newValue, 0.0f);
    this->m_vec4 = glm::vec4 (newValue, 0.0f, 0.0f);
    this->m_float = newValue.x;
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0.0f;
    this->m_string = "";
    this->m_type = UnderlyingType::Vec2;

    this->propagate ();
}

void DynamicValue::update (const glm::vec3& newValue) {
    this->m_ivec4 = glm::ivec4 (newValue, 0);
    this->m_ivec3 = glm::ivec3 (newValue);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec2 = glm::vec2 (newValue);
    this->m_vec3 = newValue;
    this->m_vec4 = glm::vec4 (newValue, 0.0f);
    this->m_float = newValue.x;
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0.0f;
    this->m_string = "";
    this->m_type = UnderlyingType::Vec3;

    this->propagate ();
}

void DynamicValue::update (const glm::vec4& newValue) {
    this->m_ivec4 = glm::ivec4 (newValue);
    this->m_ivec3 = glm::ivec3 (newValue);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec2 = glm::vec2 (newValue);
    this->m_vec3 = glm::vec3 (newValue);
    this->m_vec4 = newValue;
    this->m_float = newValue.x;
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0.0f;
    this->m_string = "";
    this->m_type = UnderlyingType::Vec4;

    this->propagate ();
}

void DynamicValue::update (const glm::ivec2& newValue) {
    this->m_ivec4 = glm::ivec4 (newValue, 0, 0);
    this->m_ivec3 = glm::ivec3 (newValue, 0);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec2 = newValue;
    this->m_vec3 = glm::vec3 (newValue, 0.0f);
    this->m_vec4 = glm::vec4 (newValue, 0.0f, 0.0f);
    this->m_float = static_cast<float> (newValue.x);
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0;
    this->m_string = "";
    this->m_type = UnderlyingType::IVec2;

    this->propagate ();
}

void DynamicValue::update (const glm::ivec3& newValue) {
    this->m_ivec4 = glm::ivec4 (newValue, 0);
    this->m_ivec3 = glm::ivec3 (newValue);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec2 = glm::vec2 (newValue);
    this->m_vec3 = newValue;
    this->m_vec4 = glm::vec4 (newValue, 0.0f);
    this->m_float = static_cast<float> (newValue.x);
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0;
    this->m_string = "";
    this->m_type = UnderlyingType::IVec3;

    this->propagate ();
}

void DynamicValue::update (const glm::ivec4& newValue) {
    this->m_ivec4 = glm::ivec4 (newValue);
    this->m_ivec3 = glm::ivec3 (newValue);
    this->m_ivec2 = glm::ivec2 (newValue);
    this->m_vec2 = glm::vec2 (newValue);
    this->m_vec3 = glm::vec3 (newValue);
    this->m_vec4 = newValue;
    this->m_float = static_cast<float> (newValue.x);
    this->m_int = static_cast<int> (newValue.x);
    this->m_bool = newValue.x != 0;
    this->m_string = "";
    this->m_type = UnderlyingType::IVec4;

    this->propagate ();
}

void DynamicValue::update (const std::string& newValue) {
    this->m_ivec4 = glm::ivec4 (0);
    this->m_ivec3 = glm::ivec3 (0);
    this->m_ivec2 = glm::ivec2 (0);
    this->m_vec2 = glm::vec2 (0.0f);
    this->m_vec3 = glm::vec3 (0.0f);
    this->m_vec4 = glm::vec4 (0.0f);
    this->m_float = 0.0f;
    this->m_int = 0;
    this->m_bool = false;
    this->m_string = newValue;
    this->m_type = UnderlyingType::String;

    if (this->m_condition.has_value ()) {
	this->m_bool = this->m_condition.value ().condition == newValue;
    }

    this->propagate ();
}

void DynamicValue::update (const DynamicValue& other) {
    this->m_ivec4 = other.getIVec4 ();
    this->m_ivec3 = other.getIVec3 ();
    this->m_ivec2 = other.getIVec2 ();
    this->m_vec2 = other.getVec2 ();
    this->m_vec3 = other.getVec3 ();
    this->m_vec4 = other.getVec4 ();
    this->m_float = other.getFloat ();
    this->m_int = other.getInt ();
    this->m_bool = other.getBool ();
    this->m_type = other.getType ();

    if (this->m_condition.has_value () && other.getType () == UnderlyingType::String) {
	// TODO: DOES THIS NEED TO HAPPEN WITH OTHER TYPES TOO?
	this->m_bool = this->m_condition.value ().condition == other.getString ();
    }

    this->propagate ();
}

void DynamicValue::update () {
    this->m_ivec4 = glm::ivec4 (0);
    this->m_ivec3 = glm::ivec3 (0);
    this->m_ivec2 = glm::ivec2 (0);
    this->m_vec2 = glm::vec2 (0.0f);
    this->m_vec3 = glm::vec3 (0.0f);
    this->m_vec4 = glm::vec4 (0.0f);
    this->m_float = 0.0f;
    this->m_int = 0;
    this->m_bool = false;
    this->m_type = UnderlyingType::Null;

    this->propagate ();
}

std::function<void ()> DynamicValue::listen (const std::function<void (const DynamicValue&)>& callback) {
    const auto it = this->m_listeners.insert (this->m_listeners.end (), callback);
    auto alive = this->m_aliveFlag;

    return [this, it, alive] {
	if (!alive || !*alive) {
	    return;
	}
	this->m_listeners.erase (it);
    };
}

void DynamicValue::connect (DynamicValue* other) {
    const auto lambda = [this] (const DynamicValue& other) {
	// null is a special case, copying everything to 0 is different,
	// so calling the update without parameters is required
	if (other.getType () == UnderlyingType::Null) {
	    this->update ();
	} else {
	    this->update (other);
	}
    };

    const auto deregisterFunction = other->listen (lambda);

    // same update cycle has to happen as in the lambda, so trigger it
    lambda (*other);

    this->m_connections.push_back (deregisterFunction);
}

void DynamicValue::disconnect () {
    for (const auto& deregister : this->m_connections) {
	if (!deregister) {
	    continue;
	}
	try {
	    deregister ();
	} catch (...) {
	    sLog.error ("Exception during listener deregistration");
	}
    }

    this->m_connections.clear ();
}

void DynamicValue::attachCondition (const ConditionInfo& condition) { this->m_condition = condition; }

void DynamicValue::propagate () const {
    for (const auto& callback : this->m_listeners) {
	callback (*this);
    }
}
