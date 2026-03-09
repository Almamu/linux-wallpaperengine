#include "ScriptEngine.h"
#include "WallpaperEngine/Logging/Log.h"

#include <sstream>

using namespace WallpaperEngine::Scripting;
using namespace WallpaperEngine::Data::Model;

static std::unique_ptr<ScriptEngine> sScriptEngine;

ScriptEngine& ScriptEngine::instance () {
	if (!sScriptEngine) {
		sScriptEngine = std::unique_ptr<ScriptEngine> (new ScriptEngine ());
	}
	return *sScriptEngine;
}

ScriptEngine::ScriptEngine () {
	this->m_runtime = JS_NewRuntime ();
	if (!this->m_runtime) {
		sLog.error ("ScriptEngine: Failed to create JS runtime");
		return;
	}

	this->m_context = JS_NewContext (this->m_runtime);
	if (!this->m_context) {
		sLog.error ("ScriptEngine: Failed to create JS context");
		JS_FreeRuntime (this->m_runtime);
		this->m_runtime = nullptr;
		return;
	}
}

ScriptEngine::~ScriptEngine () {
	if (this->m_context) {
		JS_FreeContext (this->m_context);
	}
	if (this->m_runtime) {
		JS_FreeRuntime (this->m_runtime);
	}
}

JSValue ScriptEngine::dynamicValueToJS (const DynamicValue& value) const {
	JSContext* ctx = this->m_context;

	switch (value.getType ()) {
		case DynamicValue::Float:
			return JS_NewFloat64 (ctx, value.getFloat ());
		case DynamicValue::Int:
			return JS_NewInt32 (ctx, value.getInt ());
		case DynamicValue::Boolean:
			return JS_NewBool (ctx, value.getBool ());
		case DynamicValue::Vec2:
			{
				JSValue obj = JS_NewObject (ctx);
				JS_SetPropertyStr (ctx, obj, "x", JS_NewFloat64 (ctx, value.getVec2 ().x));
				JS_SetPropertyStr (ctx, obj, "y", JS_NewFloat64 (ctx, value.getVec2 ().y));
				return obj;
			}
		case DynamicValue::Vec3:
			{
				JSValue obj = JS_NewObject (ctx);
				JS_SetPropertyStr (ctx, obj, "x", JS_NewFloat64 (ctx, value.getVec3 ().x));
				JS_SetPropertyStr (ctx, obj, "y", JS_NewFloat64 (ctx, value.getVec3 ().y));
				JS_SetPropertyStr (ctx, obj, "z", JS_NewFloat64 (ctx, value.getVec3 ().z));
				return obj;
			}
		case DynamicValue::Vec4:
			{
				JSValue obj = JS_NewObject (ctx);
				JS_SetPropertyStr (ctx, obj, "x", JS_NewFloat64 (ctx, value.getVec4 ().x));
				JS_SetPropertyStr (ctx, obj, "y", JS_NewFloat64 (ctx, value.getVec4 ().y));
				JS_SetPropertyStr (ctx, obj, "z", JS_NewFloat64 (ctx, value.getVec4 ().z));
				JS_SetPropertyStr (ctx, obj, "w", JS_NewFloat64 (ctx, value.getVec4 ().w));
				return obj;
			}
		case DynamicValue::IVec2:
			{
				JSValue obj = JS_NewObject (ctx);
				JS_SetPropertyStr (ctx, obj, "x", JS_NewInt32 (ctx, value.getIVec2 ().x));
				JS_SetPropertyStr (ctx, obj, "y", JS_NewInt32 (ctx, value.getIVec2 ().y));
				return obj;
			}
		case DynamicValue::IVec3:
			{
				JSValue obj = JS_NewObject (ctx);
				JS_SetPropertyStr (ctx, obj, "x", JS_NewInt32 (ctx, value.getIVec3 ().x));
				JS_SetPropertyStr (ctx, obj, "y", JS_NewInt32 (ctx, value.getIVec3 ().y));
				JS_SetPropertyStr (ctx, obj, "z", JS_NewInt32 (ctx, value.getIVec3 ().z));
				return obj;
			}
		case DynamicValue::IVec4:
			{
				JSValue obj = JS_NewObject (ctx);
				JS_SetPropertyStr (ctx, obj, "x", JS_NewInt32 (ctx, value.getIVec4 ().x));
				JS_SetPropertyStr (ctx, obj, "y", JS_NewInt32 (ctx, value.getIVec4 ().y));
				JS_SetPropertyStr (ctx, obj, "z", JS_NewInt32 (ctx, value.getIVec4 ().z));
				JS_SetPropertyStr (ctx, obj, "w", JS_NewInt32 (ctx, value.getIVec4 ().w));
				return obj;
			}
		default:
			return JS_UNDEFINED;
	}
}

DynamicValueUniquePtr ScriptEngine::jsToDynamicValue (JSValue val, DynamicValue::UnderlyingType hint) const {
	JSContext* ctx = this->m_context;
	auto result = std::make_unique<DynamicValue> ();

	if (JS_IsException (val)) {
		return result;
	}

	// scalar types returned directly
	int tag = JS_VALUE_GET_TAG (val);
	if (tag == JS_TAG_INT) {
		int32_t i;
		JS_ToInt32 (ctx, &i, val);
		if (hint == DynamicValue::Float) {
			result->update (static_cast<float> (i));
		} else {
			result->update (static_cast<int> (i));
		}
		return result;
	}
	if (tag == JS_TAG_BOOL) {
		result->update (static_cast<bool> (JS_ToBool (ctx, val)));
		return result;
	}
	if (JS_TAG_IS_FLOAT64 (tag)) {
		double d;
		JS_ToFloat64 (ctx, &d, val);
		result->update (static_cast<float> (d));
		return result;
	}

	// object - extract x/y/z/w properties based on the hint type
	if (tag == JS_TAG_OBJECT) {
		auto readFloat = [&] (const char* prop) -> float {
			JSValue v = JS_GetPropertyStr (ctx, val, prop);
			double d = 0.0;
			if (!JS_IsException (v) && !JS_IsUndefined (v)) {
				JS_ToFloat64 (ctx, &d, v);
			}
			JS_FreeValue (ctx, v);
			return static_cast<float> (d);
		};

		auto readInt = [&] (const char* prop) -> int {
			JSValue v = JS_GetPropertyStr (ctx, val, prop);
			int32_t i = 0;
			if (!JS_IsException (v) && !JS_IsUndefined (v)) {
				JS_ToInt32 (ctx, &i, v);
			}
			JS_FreeValue (ctx, v);
			return static_cast<int> (i);
		};

		switch (hint) {
			case DynamicValue::Vec2:
				result->update (glm::vec2 (readFloat ("x"), readFloat ("y")));
				break;
			case DynamicValue::Vec3:
				result->update (glm::vec3 (readFloat ("x"), readFloat ("y"), readFloat ("z")));
				break;
			case DynamicValue::Vec4:
				result->update (glm::vec4 (readFloat ("x"), readFloat ("y"), readFloat ("z"), readFloat ("w")));
				break;
			case DynamicValue::IVec2:
				result->update (glm::ivec2 (readInt ("x"), readInt ("y")));
				break;
			case DynamicValue::IVec3:
				result->update (glm::ivec3 (readInt ("x"), readInt ("y"), readInt ("z")));
				break;
			case DynamicValue::IVec4:
				result->update (glm::ivec4 (readInt ("x"), readInt ("y"), readInt ("z"), readInt ("w")));
				break;
			default:
				{
					// try to read as vec3 by default (most common for origin/angles)
					float x = readFloat ("x");
					float y = readFloat ("y");
					float z = readFloat ("z");
					result->update (glm::vec3 (x, y, z));
					break;
				}
		}
		return result;
	}

	// fallback: try as float
	double d;
	if (JS_ToFloat64 (ctx, &d, val) == 0) {
		result->update (static_cast<float> (d));
	}
	return result;
}

/// Helper to check for and log JS exceptions
static void logJSException (JSContext* ctx, const char* context) {
	JSValue exc = JS_GetException (ctx);
	if (!JS_IsNull (exc) && !JS_IsUndefined (exc)) {
		const char* str = JS_ToCString (ctx, exc);
		if (str) {
			sLog.error ("ScriptEngine [", context, "]: ", str);
			JS_FreeCString (ctx, str);
		}
	}
	JS_FreeValue (ctx, exc);
}

DynamicValueUniquePtr ScriptEngine::evaluate (
	const std::string& scriptSource, const std::map<std::string, DynamicValue*>& scriptProperties,
	const DynamicValue& currentValue
) {
	if (!this->m_context) {
		sLog.error ("ScriptEngine: No JS context available");
		auto fallback = std::make_unique<DynamicValue> ();
		fallback->update (currentValue);
		return fallback;
	}

	JSContext* ctx = this->m_context;

	// Build the scriptProperties object that createScriptProperties() will return
	JSValue propsObj = JS_NewObject (ctx);
	for (const auto& [name, dynVal] : scriptProperties) {
		if (dynVal) {
			JS_SetPropertyStr (ctx, propsObj, name.c_str (), this->dynamicValueToJS (*dynVal));
		}
	}

	// We need to rewrite the ES6 module into a regular script that we can evaluate
	// and extract the update() function from, because QuickJS module evaluation
	// via JS_EVAL_TYPE_MODULE doesn't easily let us get at exported functions
	// from C in a straightforward way.
	//
	// Strategy: Replace the ES6 module pattern with a plain script that:
	// 1. Has createScriptProperties() available as a global
	// 2. Defines update() in global scope
	// 3. We call update() with the value object
	//
	// The script pattern is always:
	//   'use strict';
	//   export var scriptProperties = createScriptProperties()...finish();
	//   export function update(value) { ... }
	//
	// We transform this to a self-contained IIFE that we evaluate directly.

	// Set createScriptProperties as a global that returns a builder
	// The builder supports .addSlider({...}).addCheckbox({...}).finish()
	// and returns an object with the property values

	// Create the builder as a JS object with fluent methods
	// that ultimately resolves to the propsObj
	std::ostringstream wrapper;
	wrapper << "(function() {\n"
			<< "  var __props = globalThis.__scriptProps;\n"
			<< "  function createScriptProperties() {\n"
			<< "    var builder = {\n"
			<< "      addSlider: function(opts) {\n"
			<< "        if (!(opts.name in __props)) __props[opts.name] = opts.value;\n"
			<< "        return builder;\n"
			<< "      },\n"
			<< "      addCheckbox: function(opts) {\n"
			<< "        if (!(opts.name in __props)) __props[opts.name] = opts.value;\n"
			<< "        return builder;\n"
			<< "      },\n"
			<< "      addCombo: function(opts) {\n"
			<< "        if (!(opts.name in __props)) __props[opts.name] = opts.value;\n"
			<< "        return builder;\n"
			<< "      },\n"
			<< "      addColor: function(opts) {\n"
			<< "        if (!(opts.name in __props)) __props[opts.name] = opts.value;\n"
			<< "        return builder;\n"
			<< "      },\n"
			<< "      addText: function(opts) {\n"
			<< "        if (!(opts.name in __props)) __props[opts.name] = opts.value;\n"
			<< "        return builder;\n"
			<< "      },\n"
			<< "      finish: function() { return __props; }\n"
			<< "    };\n"
			<< "    return builder;\n"
			<< "  }\n";

	// Strip 'use strict'; and export keywords, embed the script body
	std::string body = scriptSource;

	// Remove 'use strict'; declarations
	size_t pos;
	while ((pos = body.find ("'use strict';")) != std::string::npos) {
		body.erase (pos, 13);
	}
	while ((pos = body.find ("\"use strict\";")) != std::string::npos) {
		body.erase (pos, 13);
	}

	// Remove export keywords (export var ..., export function ...)
	while ((pos = body.find ("export ")) != std::string::npos) {
		body.erase (pos, 7);
	}

	wrapper << body << "\n"
			<< "  return update(globalThis.__currentValue);\n"
			<< "})();\n";

	std::string evalScript = wrapper.str ();

	// Set globals: __scriptProps and __currentValue
	JSValue globalObj = JS_GetGlobalObject (ctx);
	JS_SetPropertyStr (ctx, globalObj, "__scriptProps", JS_DupValue (ctx, propsObj));
	JS_SetPropertyStr (ctx, globalObj, "__currentValue", this->dynamicValueToJS (currentValue));

	// Evaluate
	JSValue result = JS_Eval (ctx, evalScript.c_str (), evalScript.size (), "<script>", JS_EVAL_TYPE_GLOBAL);

	// Clean up globals
	JS_SetPropertyStr (ctx, globalObj, "__scriptProps", JS_UNDEFINED);
	JS_SetPropertyStr (ctx, globalObj, "__currentValue", JS_UNDEFINED);
	JS_FreeValue (ctx, globalObj);
	JS_FreeValue (ctx, propsObj);

	if (JS_IsException (result)) {
		logJSException (ctx, "evaluate");
		JS_FreeValue (ctx, result);
		auto fallback = std::make_unique<DynamicValue> ();
		fallback->update (currentValue);
		return fallback;
	}

	auto dynResult = this->jsToDynamicValue (result, currentValue.getType ());
	JS_FreeValue (ctx, result);
	return dynResult;
}
