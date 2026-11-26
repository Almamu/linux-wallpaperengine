#include "VectorAdapter.h"

#include "../ScriptEngine.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Utils/SFINAE.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <memory>
#include <optional>
#include <variant>

using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Scripting::Adapters;

static uint32_t VectorInstanceId = 0;
static uint32_t VectorAdapterInstanceId = 0;
static constexpr int InvalidVectorInstanceId = 0;

// magic value used to ensure the assigned opaque value we got back is valid
#define VEC_OPAQUE_MAGIC 0xdeadbee0
#define VEC_MAGIC_CHECK_EXCEPTION(container, components)                                                               \
    do {                                                                                                               \
	if (!(container) || (container)->magic != (int)(VEC_OPAQUE_MAGIC + (components))) {                            \
	    return JS_ThrowTypeError (ctx, "not a Vec%d object", (int) (components));                                  \
	}                                                                                                              \
	if (!(container)->alive || !*(container)->alive) {                                                              \
	    return JS_ThrowTypeError (ctx, "Vec%d value was destroyed", (int) (components));                            \
	}                                                                                                              \
    } while (0)
#define VEC_MAGIC_CHECK_ERROR(container, components)                                                                   \
    do {                                                                                                               \
	if (!(container) || (container)->magic != (int)(VEC_OPAQUE_MAGIC + (components))) {                            \
	    JS_ThrowTypeError (ctx, "not a Vec%d object", (int) (components));                                         \
	    return -1;                                                                                                 \
	}                                                                                                              \
	if (!(container)->alive || !*(container)->alive) {                                                              \
	    JS_ThrowTypeError (ctx, "Vec%d value was destroyed", (int) (components));                                   \
	    return -1;                                                                                                 \
	}                                                                                                              \
    } while (0)

template <int components> std::map<uint32_t, VectorAdapter<components>&> vectorAdapterInstances;

template <int components> struct VectorOpaqueContainer {
    int magic;
    VectorAdapter<components>& adapter;
    std::unique_ptr<DynamicValue> ownedValue;
    DynamicValue& value;
    std::shared_ptr<const bool> alive;
    uint32_t id;
};

template <int components> auto vector_new () -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if constexpr (components == 2) {
	return glm::vec2 {};
    } else if constexpr (components == 3) {
	return glm::vec3 {};
    } else if constexpr (components == 4) {
	return glm::vec4 {};
    }
}

template auto vector_new<2> () -> decltype (auto);
template auto vector_new<3> () -> decltype (auto);
template auto vector_new<4> () -> decltype (auto);

template <int components> auto vector_new (float value) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if constexpr (components == 2) {
	return glm::vec2 (value);
    } else if constexpr (components == 3) {
	return glm::vec3 (value);
    } else if constexpr (components == 4) {
	return glm::vec4 (value);
    }
}

template auto vector_new<2> (float value) -> decltype (auto);
template auto vector_new<3> (float value) -> decltype (auto);
template auto vector_new<4> (float value) -> decltype (auto);

template <int components> auto vector_get (DynamicValue& value) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if constexpr (components == 2) {
	return value.getVec2 ();
    } else if constexpr (components == 3) {
	return value.getVec3 ();
    } else if constexpr (components == 4) {
	return value.getVec4 ();
    }
}

template <int components> auto vector_get (JSContext* ctx, JSValue source) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    int tag = JS_VALUE_GET_TAG (source);

    if (tag == JS_TAG_INT) {
	int32_t value = 0;

	JS_ToInt32 (ctx, &value, source);

	return vector_new<components> (value);
    }

    if (JS_TAG_IS_FLOAT64 (tag)) {
	double value = 0.0f;

	JS_ToFloat64 (ctx, &value, source);

	return vector_new<components> (static_cast<float> (value));
    }

    if (tag == JS_TAG_OBJECT) {
	// check components, extract x, y, z and w and create the appropriate vector
	JSValue x = JS_GetPropertyStr (ctx, source, "x");
	JSValue y = JS_GetPropertyStr (ctx, source, "y");
	JSValue z = JS_GetPropertyStr (ctx, source, "z");
	JSValue w = JS_GetPropertyStr (ctx, source, "w");
	ScopeGuard fieldsGuard ([&] {
	    JS_FreeValue (ctx, x);
	    JS_FreeValue (ctx, y);
	    JS_FreeValue (ctx, z);
	    JS_FreeValue (ctx, w);
	});

	if (!JS_IsNumber (x) || !JS_IsNumber (y)) {
	    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
	}

	// do not accept bigger vectors
	if (components <= 2 && JS_IsNumber (z)) {
	    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
	}

	if (components <= 3 && JS_IsNumber (w)) {
	    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
	}

	double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f, wVal = 0.0f;

	JS_ToFloat64 (ctx, &xVal, x);
	JS_ToFloat64 (ctx, &yVal, y);

	if (JS_IsNumber (z)) {
	    JS_ToFloat64 (ctx, &zVal, z);
	}

	if (JS_IsNumber (w)) {
	    JS_ToFloat64 (ctx, &wVal, w);
	}

	if constexpr (components == 2) {
	    return glm::vec2 (xVal, yVal);
	} else if constexpr (components == 3) {
	    return glm::vec3 (xVal, yVal, zVal);
	} else if constexpr (components == 4) {
	    return glm::vec4 (xVal, yVal, zVal, wVal);
	}
    }

    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
}

template auto vector_get<2> (JSContext* ctx, JSValue source) -> decltype (auto);
template auto vector_get<3> (JSContext* ctx, JSValue source) -> decltype (auto);
template auto vector_get<4> (JSContext* ctx, JSValue source) -> decltype (auto);
template auto vector_get<2> (DynamicValue& value) -> decltype (auto);
template auto vector_get<3> (DynamicValue& value) -> decltype (auto);
template auto vector_get<4> (DynamicValue& value) -> decltype (auto);

/** Converts an authored JS operand without allowing C++ exceptions to cross QuickJS's C ABI. */
template <int components>
auto vector_try_get (JSContext* ctx, JSValue source) -> std::optional<decltype (vector_new<components> ())> {
    try {
	return vector_get<components> (ctx, source);
    } catch (const std::exception& e) {
	JS_ThrowTypeError (ctx, "%s", e.what ());
	return std::nullopt;
    }
}

template <int components> auto vector_get (JSContext* ctx, int argc, JSValueConst* argv) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if (argc == 0 || JS_IsUndefined (argv[0])) {
	return vector_new<components> ();
    }

    // Constructor overloads follow the shipped SceneScript baseclasses.js. Keep
    // these separate from arithmetic operand conversion: partial numeric inputs
    // and space-separated strings have constructor-specific behavior.
    if (JS_IsString (argv[0])) {
	JSValue separator = JS_NewString (ctx, " ");
	const JSValue split = JS_GetPropertyStr (ctx, argv[0], "split");
	const JSValue parts = JS_Call (ctx, split, argv[0], 1, &separator);
	const JSValue global = JS_GetGlobalObject (ctx);
	const JSValue parseFloat = JS_GetPropertyStr (ctx, global, "parseFloat");
	ScopeGuard valuesGuard ([&] {
	    JS_FreeValue (ctx, separator);
	    JS_FreeValue (ctx, split);
	    JS_FreeValue (ctx, parts);
	    JS_FreeValue (ctx, global);
	    JS_FreeValue (ctx, parseFloat);
	});
	if (JS_IsException (parts)) {
	    throw std::runtime_error ("Could not split vector string");
	}
	auto value = vector_new<components> ();
	for (int i = 0; i < components; ++i) {
	    JSValue part = JS_GetPropertyUint32 (ctx, parts, i);
	    const JSValue parsed = JS_Call (ctx, parseFloat, JS_UNDEFINED, 1, &part);
	    ScopeGuard partGuard ([&] {
		JS_FreeValue (ctx, part);
		JS_FreeValue (ctx, parsed);
	    });
	    double number = 0.0;
	    if (JS_IsException (parsed) || JS_ToFloat64 (ctx, &number, parsed) < 0) {
		throw std::runtime_error ("Could not parse vector string component");
	    }
	    value[i] = static_cast<float> (number);
	}
	return value;
    }

    if (JS_IsNumber (argv[0])) {
	double values[4] = {};
	JS_ToFloat64 (ctx, &values[0], argv[0]);
	const bool hasY = argc > 1 && JS_IsNumber (argv[1]);
	const bool hasZ = argc > 2 && JS_IsNumber (argv[2]);
	const bool hasW = argc > 3 && JS_IsNumber (argv[3]);
	values[1] = values[0];
	if (hasY) {
	    JS_ToFloat64 (ctx, &values[1], argv[1]);
	}
	values[2] = hasY ? 0.0 : values[0];
	if (hasZ) {
	    JS_ToFloat64 (ctx, &values[2], argv[2]);
	}
	values[3] = hasZ ? values[2] : (hasY ? 0.0 : values[0]);
	if (hasW) {
	    JS_ToFloat64 (ctx, &values[3], argv[3]);
	}
	auto value = vector_new<components> ();
	for (int i = 0; i < components; ++i) {
	    value[i] = static_cast<float> (values[i]);
	}
	return value;
    }

    if constexpr (components == 2) {
	JSClassID classId = 0;
	const auto* source = static_cast<VectorOpaqueContainer<3>*> (JS_GetAnyOpaque (argv[0], &classId));
	if (source != nullptr && source->magic == static_cast<int> (VEC_OPAQUE_MAGIC + 3)) {
	    if (!source->alive || !*source->alive) {
		throw std::runtime_error ("Vec3 value was destroyed");
	    }
	    return glm::vec2 (source->value.getVec3 ());
	}
    }

    return vector_get<components> (ctx, argv[0]);
}

template auto vector_get<2> (JSContext* ctx, int argc, JSValueConst* argv) -> decltype (auto);
template auto vector_get<3> (JSContext* ctx, int argc, JSValueConst* argv) -> decltype (auto);
template auto vector_get<4> (JSContext* ctx, int argc, JSValueConst* argv) -> decltype (auto);

template <int components>
JSValue vector_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (obj_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const int index = container->adapter.componentIndex (atom);
    if (index >= 0) {
        return JS_NewFloat64 (ctx, vector_get<components> (container->value)[index]);
    }

    // Component access is exotic, but vector methods live on the ordinary class
    // prototype. Forward unknown names there so calls such as origin.copy() and
    // direction.normalize() are not hidden by the exotic getter.
    JSValue prototype = JS_GetPrototype (ctx, obj_val);
    if (JS_IsException (prototype)) {
	return prototype;
    }
    JSValue result = JS_GetProperty (ctx, prototype, atom);
    JS_FreeValue (ctx, prototype);
    return result;
}

template JSValue vector_property_get<2> (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);
template JSValue vector_property_get<3> (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);
template JSValue vector_property_get<4> (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);

template <int components>
int vector_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    JSClassID classId = 0;
    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (obj_val, &classId));

    VEC_MAGIC_CHECK_ERROR (container, components);

    const int index = container->adapter.componentIndex (atom);
    if (index < 0) {
        const char* name = JS_AtomToCString (ctx, atom);
        if (name == nullptr) return -1;
        JS_ThrowTypeError (ctx, "Vec%d has no writable property '%s'", (int) (components), name);
        JS_FreeCString (ctx, name);
        return -1;
    }
    // Native component setters leave nonnumeric values unchanged. In particular,
    // shared values may still be undefined on a controller's first update.
    const int tag = JS_VALUE_GET_TAG (val);
    if (tag != JS_TAG_INT && !JS_TAG_IS_FLOAT64 (tag)) return 0;
    auto vec = vector_get<components> (container->value);

    double value = 0;

    JS_ToFloat64 (ctx, &value, val);

    vec[index] = static_cast<float> (value);

    container->value.update (vec, DynamicValue::UpdateSource::Script);

    return 0;
}

template int vector_property_set<2> (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);
template int vector_property_set<3> (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);
template int vector_property_set<4> (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);

template <int components> JSValue vector_copy (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    // create a new DynamicValue
    return container->adapter.instantiate (container->value, true);
}

template JSValue vector_copy<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_copy<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_copy<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

bool vector_value_equals (JSContext* ctx, JSValue left, JSValue right) {
    if (JS_TAG_IS_FLOAT64 (JS_VALUE_GET_TAG (left)) || JS_TAG_IS_FLOAT64 (JS_VALUE_GET_TAG (right))) {
	double x1Val = 0.0f, x2Val = 0.0f;

	JS_ToFloat64 (ctx, &x1Val, left);
	JS_ToFloat64 (ctx, &x2Val, right);

	if (std::abs (x1Val - x2Val) > 0.00001) {
	    return false;
	}
    } else {
	if (!JS_IsEqual (ctx, left, right)) {
	    return false;
	}
    }

    return true;
}

template <int components> JSValue vector_equals (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc == 0) {
	return JS_FALSE;
    }

    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue other = argv[0];
    auto* otherContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (other, &classId));

    if (otherContainer == nullptr || otherContainer->magic != static_cast<int> (VEC_OPAQUE_MAGIC + components)) {
	return JS_FALSE;
    }
    VEC_MAGIC_CHECK_EXCEPTION (otherContainer, components);

    const auto vector = vector_get<components> (container->value);
    const auto otherVector = vector_get<components> (otherContainer->value);

    for (int i = 0; i < components; ++i) {
	// Native equals uses an absolute epsilon, and NaN/Infinity do not compare equal.
	if (!(std::abs (static_cast<double> (vector[i]) - otherVector[i]) < 0.00001)) {
	    return JS_FALSE;
	}
    }

    return JS_TRUE;
}

template JSValue vector_equals<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_equals<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_equals<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_length (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    return JS_NewFloat64 (ctx, glm::length (vector_get<components> (container->value)));
}

template JSValue vector_length<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_length<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_length<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_length_sqr (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto vector = vector_get<components> (container->value);
    return JS_NewFloat64 (ctx, glm::dot (vector, vector));
}

template <int components>
JSValue vector_constructor (JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv, int magic) {
    auto it = vectorAdapterInstances<components>.find (magic);

    if (it == vectorAdapterInstances<components>.end ()) {
	return JS_ThrowInternalError (ctx, "Vec%d adapter instance not found", (int) (components));
    }

    JSValue result = it->second.instantiate ();
    JSClassID classId = 0;
    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (result, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    try {
	container->value.update (
	    vector_get<components> (ctx, argc, argv), DynamicValue::UpdateSource::Initialization
	);
    } catch (const std::exception& e) {
	// Never unwind a C++ exception through QuickJS's C callback boundary. An authored
	// script type error belongs to that module and must not abort construction of the scene.
	JS_FreeValue (ctx, result);
	return JS_ThrowTypeError (ctx, "%s", e.what ());
    }

    return result;
}

template JSValue vector_constructor<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
template JSValue vector_constructor<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
template JSValue vector_constructor<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

template <int components> void vector_finalizer (JSRuntime* rt, JSValueConst val) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (val, &classId));

    if (!container || container->magic != (int)(VEC_OPAQUE_MAGIC + components)) {
	return;
    }

    delete container;
}

template void vector_finalizer<2> (JSRuntime* rt, JSValueConst val);
template void vector_finalizer<3> (JSRuntime* rt, JSValueConst val);
template void vector_finalizer<4> (JSRuntime* rt, JSValueConst val);

template <int components>
JSValue vector_normalize (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate (container->value, true);

    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    newContainer->value.update (
	glm::normalize (vector_get<components> (container->value)), DynamicValue::UpdateSource::Script
    );

    return newVector;
}

template JSValue vector_normalize<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_normalize<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_normalize<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_add (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	*operand + vector_get<components> (container->value),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_add<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_add<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_add<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_subtract (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    // a.subtract(b) is a - b, not b - a
    newContainer->value.update (
	vector_get<components> (container->value) - *operand,
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_subtract<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_subtract<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_subtract<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_multiply (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	*operand * vector_get<components> (container->value),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_multiply<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_multiply<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_multiply<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_divide (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    // a.divide(b) is a / b, not b / a -- scripts normalize with `v.divide(v.length())`
    newContainer->value.update (
	vector_get<components> (container->value) / *operand,
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_divide<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_divide<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_divide<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_dot (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    return JS_NewFloat64 (ctx, glm::dot (*operand, vector_get<components> (container->value)));
}

template JSValue vector_dot<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_dot<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_dot<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_cross (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	// a.cross(b) is cross(a, b); the reversed order returns the negated normal
	glm::cross (vector_get<components> (container->value), *operand),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_cross<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_mix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 2) {
	return JS_ThrowTypeError (ctx, "mix expects 2 arguments");
    }

    if (!JS_IsNumber (argv[1])) {
	return JS_ThrowTypeError (ctx, "mix amount must be a number");
    }

    double amount = 0.0f;

    JS_ToFloat64 (ctx, &amount, argv[1]);

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	// a.mix(b, t) interpolates from a toward b; the reversed order interpolates at 1 - t
	glm::mix (vector_get<components> (container->value), *operand, amount),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_mix<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_mix<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_mix<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_min (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::min (*operand, vector_get<components> (container->value)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_min<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_min<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_min<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_max (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto operand = vector_try_get<components> (ctx, argv[0]);
    if (!operand.has_value ()) {
	return JS_EXCEPTION;
    }

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::max (*operand, vector_get<components> (container->value)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_max<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_max<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_max<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_abs (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::abs (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_abs<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_abs<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_abs<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_sign (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::sign (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_sign<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_sign<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_sign<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_round (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::round (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_round<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_round<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_round<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_floor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::floor (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_floor<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_floor<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_floor<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_ceil (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));
    const auto vector = vector_get<components> (container->value);

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (glm::ceil (vector), DynamicValue::UpdateSource::Initialization);

    return newVector;
}

template JSValue vector_ceil<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_ceil<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_ceil<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_toString (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    return JS_NewString (ctx, container->value.toString ().c_str ());
}

template JSValue vector_toString<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_toString<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_toString<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
VectorAdapter<components>::VectorAdapter (ScriptEngine& engine) :
    ObjectAdapter (engine), m_instanceId (++VectorAdapterInstanceId), m_name ("Vec" + std::to_string (components)),
    m_exoticMethods (
	{
	    .get_property = vector_property_get<components>,
	    .set_property = vector_property_set<components>,
	}
    ) {
    // Atoms belong to this runtime. Keep component lookup allocation-free in
    // physics scripts, which read and write vectors millions of times per frame.
    constexpr const char* names[] = { "x", "y", "z", "w" };
    for (int index = 0; index < components; ++index) {
        m_componentAtoms[index] = JS_NewAtom (this->m_engine.getContext (), names[index]);
    }
    vectorAdapterInstances<components>.emplace (this->m_instanceId, *this);
    this->registerType (
	{
	    .class_name = this->m_name.c_str (),
	    .finalizer = vector_finalizer<components>,
	    .exotic = &this->m_exoticMethods,
	}
    );

    // build the prototype for the Vector and assign the required methods
    m_prototype = JS_NewObject (this->m_engine.getContext ());

    JS_DupValue (this->m_engine.getContext (), m_prototype);

    JSValue ctor = JS_NewCFunctionMagic (
	this->m_engine.getContext (), vector_constructor<components>, this->m_name.c_str (), 1,
	JS_CFUNC_constructor_magic, this->m_instanceId
    );

    JS_SetConstructor (this->m_engine.getContext (), ctor, m_prototype);
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_engine.getGlobalThis (), this->m_name.c_str (),
	JS_DupValue (this->m_engine.getContext (), ctor), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "copy",
	JS_NewCFunction (this->m_engine.getContext (), vector_copy<components>, "copy", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "equals",
	JS_NewCFunction (this->m_engine.getContext (), vector_equals<components>, "equals", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "length",
	JS_NewCFunction (this->m_engine.getContext (), vector_length<components>, "length", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "lengthSqr",
	JS_NewCFunction (this->m_engine.getContext (), vector_length_sqr<components>, "lengthSqr", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "normalize",
	JS_NewCFunction (this->m_engine.getContext (), vector_normalize<components>, "normalize", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "add",
	JS_NewCFunction (this->m_engine.getContext (), vector_add<components>, "add", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "subtract",
	JS_NewCFunction (this->m_engine.getContext (), vector_subtract<components>, "subtract", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "multiply",
	JS_NewCFunction (this->m_engine.getContext (), vector_multiply<components>, "multiply", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "divide",
	JS_NewCFunction (this->m_engine.getContext (), vector_divide<components>, "divide", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "dot",
	JS_NewCFunction (this->m_engine.getContext (), vector_dot<components>, "dot", 1), JS_PROP_ENUMERABLE
    );
    if constexpr (components == 3) {
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "cross",
	    JS_NewCFunction (this->m_engine.getContext (), vector_cross<components>, "cross", 1), JS_PROP_ENUMERABLE
	);
    }
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "mix",
	JS_NewCFunction (this->m_engine.getContext (), vector_mix<components>, "mix", 2), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "min",
	JS_NewCFunction (this->m_engine.getContext (), vector_min<components>, "min", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "max",
	JS_NewCFunction (this->m_engine.getContext (), vector_max<components>, "max", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "abs",
	JS_NewCFunction (this->m_engine.getContext (), vector_abs<components>, "abs", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "sign",
	JS_NewCFunction (this->m_engine.getContext (), vector_sign<components>, "sign", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "round",
	JS_NewCFunction (this->m_engine.getContext (), vector_round<components>, "round", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "floor",
	JS_NewCFunction (this->m_engine.getContext (), vector_floor<components>, "floor", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "ceil",
	JS_NewCFunction (this->m_engine.getContext (), vector_ceil<components>, "ceil", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "toString",
	JS_NewCFunction (this->m_engine.getContext (), vector_toString<components>, "toString", 0), JS_PROP_ENUMERABLE
    );

    JS_SetClassProto (this->m_engine.getContext (), this->m_classId, m_prototype);
    JS_FreeValue (this->m_engine.getContext (), ctor);
}

template <int components> VectorAdapter<components>::~VectorAdapter () {
    for (JSAtom atom : m_componentAtoms) JS_FreeAtom (this->m_engine.getContext (), atom);
    vectorAdapterInstances<components>.erase (this->m_instanceId);

    JS_FreeValue (this->m_engine.getContext (), m_prototype);
    JS_FreeValue (this->m_engine.getContext (), m_scriptConstructor);
}

template <int components> void VectorAdapter<components>::useScriptConstructor () {
    auto* ctx = m_engine.getContext ();
    m_scriptConstructor = JS_GetPropertyStr (ctx, m_engine.getGlobalThis (), m_name.c_str ());
    JS_FreeValue (ctx, m_prototype);
    m_prototype = JS_GetPropertyStr (ctx, m_scriptConstructor, "prototype");
    JS_SetClassProto (ctx, m_classId, JS_DupValue (ctx, m_prototype));
}

template <int components> bool VectorAdapter<components>::isInstance (JSValueConst value) const {
    return JS_GetClassID (value) == m_classId
        || (!JS_IsUndefined (m_scriptConstructor) && JS_IsObject (value)
            && JS_IsInstanceOf (m_engine.getContext (), value, m_scriptConstructor) > 0);
}

template <int components> JSValue VectorAdapter<components>::instantiate (ScriptableObject& object) {
    throw std::runtime_error ("Cannot create a vector instance from a ScriptableObject");
}

template <int components> JSValue VectorAdapter<components>::instantiate (DynamicValue& value) {
    JSValue result = this->ObjectAdapter::instantiate (value);
    JS_SetOpaque (
	result,
	new VectorOpaqueContainer<components> {
	    .magic = VEC_OPAQUE_MAGIC + components,
	    .adapter = *this,
	    .ownedValue = nullptr,
	    .value = value,
	    .alive = value.getAliveFlag (),
	    .id = InvalidVectorInstanceId,
	}
    );

    return result;
}

template <int components> JSValue VectorAdapter<components>::instantiate (DynamicValue& source, bool temporal) {
    if (!JS_IsUndefined (m_scriptConstructor)) {
        auto* ctx = m_engine.getContext ();
        const auto vector = vector_get<components> (source);
        JSValue args[components];
        for (int i = 0; i < components; ++i) args[i] = JS_NewFloat64 (ctx, vector[i]);
        return JS_CallConstructor (ctx, m_scriptConstructor, components, args);
    }
    auto value = std::make_unique<DynamicValue> (source);
    auto& valueRef = *value;
    uint32_t id = ++VectorInstanceId;
    JSValue result = this->ObjectAdapter::instantiate (valueRef);
    JS_SetOpaque (
	result,
	new VectorOpaqueContainer<components> {
	    .magic = VEC_OPAQUE_MAGIC + components,
	    .adapter = *this,
	    .ownedValue = std::move (value),
	    .value = valueRef,
	    .alive = valueRef.getAliveFlag (),
	    .id = id,
	}
    );

    return result;
}

template <int components> JSValue VectorAdapter<components>::instantiate () {
    if (!JS_IsUndefined (m_scriptConstructor)) {
        return JS_CallConstructor (m_engine.getContext (), m_scriptConstructor, 0, nullptr);
    }
    auto value = std::make_unique<DynamicValue> (vector_new<components> ());
    auto& valueRef = *value;
    uint32_t id = ++VectorInstanceId;
    JSValue result = this->ObjectAdapter::instantiate (valueRef);
    JS_SetOpaque (
	result,
	new VectorOpaqueContainer<components> {
	    .magic = VEC_OPAQUE_MAGIC + components,
	    .adapter = *this,
	    .ownedValue = std::move (value),
	    .value = valueRef,
	    .alive = valueRef.getAliveFlag (),
	    .id = id,
	}
    );

    return result;
}

namespace WallpaperEngine::Scripting::Adapters {
template class VectorAdapter<2>;
template class VectorAdapter<3>;
template class VectorAdapter<4>;
} // namespace WallpaperEngine::Scripting::Adapters
