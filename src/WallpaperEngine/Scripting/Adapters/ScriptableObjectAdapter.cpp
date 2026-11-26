#include "ScriptableObjectAdapter.h"
#include "VideoTextureAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#undef GLM_ENABLE_EXPERIMENTAL
#include <utility>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CModel.h"
#include "WallpaperEngine/Render/Objects/CParticle.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Objects/CText.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Scripting::Adapters;

#define SCRIPTABLE_OPAQUE_MAGIC 0xdeadbeef

struct OpaqueScriptableObjectAdapter {
    unsigned int magic;
    ScriptableObjectAdapter& adapter;
    WallpaperEngine::Scripting::ScriptableObject& object;
};

static void scriptableobject_finalizer (JSRuntime* runtime, JSValueConst value) {
    JSClassID classId = 0;
    delete static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (value, &classId));
}

enum AnimationCommand {
    AnimationCommand_Play,
    AnimationCommand_Pause,
    AnimationCommand_Stop,
    AnimationCommand_IsPlaying,
    AnimationCommand_GetFrame,
    AnimationCommand_SetFrame,
    AnimationCommand_GetRate,
    AnimationCommand_SetRate,
    AnimationCommand_GetFps,
    AnimationCommand_GetFrameCount,
    AnimationCommand_GetDuration,
    AnimationCommand_GetBlend,
    AnimationCommand_SetBlend,
    AnimationCommand_GetVisible,
    AnimationCommand_SetVisible,
};

enum TextureAnimationCommand {
    TextureAnimationCommand_Play,
    TextureAnimationCommand_Pause,
    TextureAnimationCommand_Stop,
    TextureAnimationCommand_IsPlaying,
    TextureAnimationCommand_GetFrame,
    TextureAnimationCommand_SetFrame,
    TextureAnimationCommand_Join,
};

enum TextureAnimationProperty {
    TextureAnimationProperty_FrameCount,
    TextureAnimationProperty_Duration,
    TextureAnimationProperty_Rate,
};

static OpaqueScriptableObjectAdapter* scriptable_container (JSValueConst value) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (value, &classId));
    return container != nullptr && container->magic == SCRIPTABLE_OPAQUE_MAGIC ? container : nullptr;
}

static WallpaperEngine::Render::Objects::CImage* scriptable_image (JSValueConst value) {
    OpaqueScriptableObjectAdapter* container = scriptable_container (value);
    return container != nullptr ? dynamic_cast<WallpaperEngine::Render::Objects::CImage*> (&container->object)
				: nullptr;
}

static WallpaperEngine::Render::Objects::CModel* scriptable_model (JSValueConst value) {
    auto* container = scriptable_container (value);
    return container != nullptr ? dynamic_cast<WallpaperEngine::Render::Objects::CModel*> (&container->object) : nullptr;
}

static WallpaperEngine::Render::Objects::CImage*
scriptable_animation_image (JSContext* ctx, JSValueConst* functionData) {
    return scriptable_image (functionData[0]);
}

static JSValue texture_animation_command (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* functionData
) {
    auto* image = scriptable_animation_image (ctx, functionData);
    if (image == nullptr) {
	return JS_UNDEFINED;
    }

    switch (magic) {
	case TextureAnimationCommand_Play:
	    image->playTextureAnimation ();
	    return JS_UNDEFINED;
	case TextureAnimationCommand_Pause:
	    image->pauseTextureAnimation ();
	    return JS_UNDEFINED;
	case TextureAnimationCommand_Stop:
	    image->stopTextureAnimation ();
	    return JS_UNDEFINED;
	case TextureAnimationCommand_IsPlaying:
	    return JS_NewBool (ctx, image->isTextureAnimationPlaying ());
	case TextureAnimationCommand_GetFrame:
	    return JS_NewInt64 (ctx, static_cast<int64_t> (image->getTextureAnimationFrame ()));
	case TextureAnimationCommand_SetFrame:
	    {
		int64_t frame = 0;
		if (argc < 1 || JS_ToInt64 (ctx, &frame, argv[0]) < 0) {
		    return JS_ThrowTypeError (ctx, "setFrame() expects a frame number");
		}
		image->setTextureAnimationFrame (static_cast<size_t> (std::max<int64_t> (0, frame)));
		return JS_UNDEFINED;
	    }
	case TextureAnimationCommand_Join:
	    image->joinTextureAnimation ();
	    return JS_UNDEFINED;
	default:
	    return JS_UNDEFINED;
    }
}

static JSValue texture_animation_property_get (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* functionData
) {
    auto* image = scriptable_animation_image (ctx, functionData);
    if (image == nullptr) {
	return JS_UNDEFINED;
    }

    switch (magic) {
	case TextureAnimationProperty_FrameCount:
	    return JS_NewInt64 (ctx, static_cast<int64_t> (image->getTextureAnimationFrameCount ()));
	case TextureAnimationProperty_Duration:
	    return JS_NewFloat64 (ctx, image->getTextureAnimationDuration ());
	case TextureAnimationProperty_Rate:
	    return JS_NewFloat64 (ctx, image->getTextureAnimationRate ());
	default:
	    return JS_UNDEFINED;
    }
}

static JSValue texture_animation_rate_set (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* functionData
) {
    auto* image = scriptable_animation_image (ctx, functionData);
    double rate = 1.0;
    if (image == nullptr || argc < 1 || JS_ToFloat64 (ctx, &rate, argv[0]) < 0) {
	return JS_ThrowTypeError (ctx, "rate expects a number");
    }

    image->setTextureAnimationRate (static_cast<float> (rate));
    return JS_UNDEFINED;
}

static JSValue
scriptable_texture_animation_controller (JSContext* ctx, JSValueConst owner) {
    JSValue functionData[] = { JS_DupValue (ctx, owner) };
    JSValue result = JS_NewObject (ctx);

    const auto defineReadOnly = [&] (const char* name, const int property) {
	JS_DefinePropertyGetSet (
	    ctx, result, JS_NewAtom (ctx, name),
	    JS_NewCFunctionData (ctx, texture_animation_property_get, 0, property, 1, functionData), JS_UNDEFINED,
	    JS_PROP_ENUMERABLE
	);
    };
    defineReadOnly ("frameCount", TextureAnimationProperty_FrameCount);
    defineReadOnly ("duration", TextureAnimationProperty_Duration);
    JS_DefinePropertyGetSet (
	ctx, result, JS_NewAtom (ctx, "rate"),
	JS_NewCFunctionData (ctx, texture_animation_property_get, 0, TextureAnimationProperty_Rate, 1, functionData),
	JS_NewCFunctionData (ctx, texture_animation_rate_set, 1, TextureAnimationProperty_Rate, 1, functionData),
	JS_PROP_ENUMERABLE
    );

    const auto defineMethod = [&] (const char* name, const int command, const int length) {
	JS_SetPropertyStr (
	    ctx, result, name, JS_NewCFunctionData (ctx, texture_animation_command, length, command, 1, functionData)
	);
    };
    defineMethod ("play", TextureAnimationCommand_Play, 0);
    defineMethod ("pause", TextureAnimationCommand_Pause, 0);
    defineMethod ("stop", TextureAnimationCommand_Stop, 0);
    defineMethod ("isPlaying", TextureAnimationCommand_IsPlaying, 0);
    defineMethod ("getFrame", TextureAnimationCommand_GetFrame, 0);
    defineMethod ("setFrame", TextureAnimationCommand_SetFrame, 1);
    defineMethod ("join", TextureAnimationCommand_Join, 0);

    JS_FreeValue (ctx, functionData[0]);
    return result;
}

static JSValue scriptable_get_texture_animation (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* image = scriptable_image (thisVal);
    return image != nullptr && image->hasTextureAnimation () ? scriptable_texture_animation_controller (ctx, thisVal)
							     : JS_UNDEFINED;
}

static JSValue make_script_mat4 (JSContext* ctx, const glm::mat4& matrix) {
    JSValue values = JS_NewArray (ctx);
    uint32_t index = 0;
    for (int column = 0; column < 4; column++) {
	for (int row = 0; row < 4; row++) {
	    JS_SetPropertyUint32 (ctx, values, index++, JS_NewFloat64 (ctx, matrix[column][row]));
	}
    }

    JSValue global = JS_GetGlobalObject (ctx);
    JSValue constructor = JS_GetPropertyStr (ctx, global, "Mat4");
    JSValue result = JS_CallConstructor (ctx, constructor, 1, &values);
    JS_FreeValue (ctx, constructor);
    JS_FreeValue (ctx, global);
    JS_FreeValue (ctx, values);
    return result;
}

static JSValue make_script_mat3 (JSContext* ctx, const glm::mat3& matrix) {
    JSValue values = JS_NewArray (ctx);
    uint32_t index = 0;
    for (int column = 0; column < 3; column++) {
        for (int row = 0; row < 3; row++) {
            JS_SetPropertyUint32 (ctx, values, index++, JS_NewFloat64 (ctx, matrix[column][row]));
        }
    }
    JSValue global = JS_GetGlobalObject (ctx);
    JSValue constructor = JS_GetPropertyStr (ctx, global, "Mat3");
    JSValue result = JS_CallConstructor (ctx, constructor, 1, &values);
    JS_FreeValue (ctx, constructor);
    JS_FreeValue (ctx, global);
    JS_FreeValue (ctx, values);
    return result;
}

static bool read_script_vec3 (JSContext* ctx, JSValueConst value, glm::vec3& result) {
    if (!JS_IsObject (value)) {
	return false;
    }

    JSValue fields[] = {
	JS_GetPropertyStr (ctx, value, "x"),
	JS_GetPropertyStr (ctx, value, "y"),
	JS_GetPropertyStr (ctx, value, "z"),
    };
    const bool valid = JS_IsNumber (fields[0]) && JS_IsNumber (fields[1]);
    double components[] = { 0.0, 0.0, 0.0 };
    if (valid) {
	JS_ToFloat64 (ctx, &components[0], fields[0]);
	JS_ToFloat64 (ctx, &components[1], fields[1]);
	if (JS_IsNumber (fields[2])) {
	    JS_ToFloat64 (ctx, &components[2], fields[2]);
	}
    }
    for (const auto& field : fields) {
	JS_FreeValue (ctx, field);
    }

    result = glm::vec3 (components[0], components[1], components[2]);
    return valid;
}

static JSValue scriptable_get_transform_matrix (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    const auto* container = scriptable_container (thisVal);
    return container != nullptr ? make_script_mat4 (ctx, container->object.resolveWorldMatrix ()) : JS_UNDEFINED;
}

static JSValue scriptable_get_parent (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr || !container->object.getObject ().parent.has_value ()) {
	return JS_UNDEFINED;
    }

    const auto* parent = container->object.getScene ().getObject (*container->object.getObject ().parent);
    const auto* scriptable = dynamic_cast<const WallpaperEngine::Scripting::ScriptableObject*> (parent);
    return scriptable != nullptr ? container->adapter.getEngine ().getAdapters ().object->instantiate (
				       const_cast<WallpaperEngine::Scripting::ScriptableObject&> (*scriptable)
				   )
				 : JS_UNDEFINED;
}

static JSValue scriptable_get_children (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    JSValue result = JS_NewArray (ctx);
    if (container == nullptr) {
	return result;
    }

    uint32_t index = 0;
    for (auto* child : container->object.getScene ().getObjectsByRenderOrder ()) {
	if (child == nullptr || !child->getObject ().parent.has_value ()
	    || *child->getObject ().parent != container->object.getId ()) {
	    continue;
	}
	if (auto* scriptable = dynamic_cast<WallpaperEngine::Scripting::ScriptableObject*> (child);
	    scriptable != nullptr) {
	    JS_SetPropertyUint32 (
		ctx, result, index++, container->adapter.getEngine ().getAdapters ().object->instantiate (*scriptable)
	    );
	}
    }
    return result;
}

static std::optional<std::string>
resolve_attachment_name (JSContext* ctx, const WallpaperEngine::Render::CObject& object, JSValueConst value) {
    if (JS_IsNumber (value)) {
	int32_t index = -1;
	if (JS_ToInt32 (ctx, &index, value) < 0 || index < 0) {
	    return std::nullopt;
	}
	return object.getAttachmentName (static_cast<size_t> (index));
    }

    const char* name = JS_ToCString (ctx, value);
    if (name == nullptr) {
	return std::nullopt;
    }
    std::string result (name);
    JS_FreeCString (ctx, name);
    return result;
}

static glm::mat4 attachment_world_matrix (
    JSContext* ctx, const WallpaperEngine::Scripting::ScriptableObject& object, int argc, JSValueConst* argv
) {
    const glm::mat4 world = object.resolveWorldMatrix ();
    if (argc < 1 || JS_IsNull (argv[0]) || JS_IsUndefined (argv[0])) {
	return world;
    }

    const auto name = resolve_attachment_name (ctx, object, argv[0]);
    if (!name.has_value ()) {
	return world;
    }
    const auto local = object.getAttachmentTransform (*name);
    return local.has_value () ? world * *local : world;
}

static JSValue scriptable_get_attachment_index (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr || argc < 1) {
	return JS_NewInt32 (ctx, -1);
    }

    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) {
	return JS_NewInt32 (ctx, -1);
    }
    const auto index = container->object.getAttachmentIndex (name);
    JS_FreeCString (ctx, name);
    return JS_NewInt64 (ctx, index.has_value () ? static_cast<int64_t> (*index) : -1);
}

static JSValue scriptable_get_attachment_matrix (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    return container != nullptr ? make_script_mat4 (ctx, attachment_world_matrix (ctx, container->object, argc, argv))
				: JS_UNDEFINED;
}

static JSValue scriptable_get_bone_index (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr || argc < 1) return JS_NewInt32 (ctx, -1);
    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) return JS_EXCEPTION;
    const auto index = container->object.getBoneIndex (name);
    JS_FreeCString (ctx, name);
    return JS_NewInt64 (ctx, index.has_value () ? static_cast<int64_t> (*index) : -1);
}

static JSValue scriptable_get_bone_transform (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr || argc < 1) return JS_UNDEFINED;
    std::optional<size_t> index;
    if (JS_IsNumber (argv[0])) {
	int32_t number = -1;
	if (JS_ToInt32 (ctx, &number, argv[0]) < 0) return JS_EXCEPTION;
	if (number >= 0) index = static_cast<size_t> (number);
    } else {
	const char* name = JS_ToCString (ctx, argv[0]);
	if (name == nullptr) return JS_EXCEPTION;
	index = container->object.getBoneIndex (name);
	JS_FreeCString (ctx, name);
    }
    const auto transform = index.has_value () ? container->object.getBoneTransform (*index) : std::nullopt;
    return transform.has_value ()
	? make_script_mat4 (ctx, *transform)
	: JS_UNDEFINED;
}

static JSValue scriptable_get_attachment_origin (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr) {
	return JS_UNDEFINED;
    }
    DynamicValue origin (glm::vec3 (attachment_world_matrix (ctx, container->object, argc, argv)[3]));
    return container->adapter.getEngine ().getAdapters ().vec3->instantiate (origin, true);
}

static JSValue scriptable_get_attachment_angles (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr) {
	return JS_UNDEFINED;
    }

    glm::vec3 scale, translation, skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose (
	attachment_world_matrix (ctx, container->object, argc, argv), scale, orientation, translation, skew,
	perspective
    );
    DynamicValue angles (glm::degrees (glm::eulerAngles (glm::normalize (orientation))));
    return container->adapter.getEngine ().getAdapters ().vec3->instantiate (angles, true);
}

static WallpaperEngine::Scripting::ScriptableObject*
resolve_scriptable_parent (JSContext* ctx, OpaqueScriptableObjectAdapter& container, JSValueConst value) {
    if (JS_IsNull (value) || JS_IsUndefined (value)) {
	return nullptr;
    }
    if (auto* scriptable = ScriptableObjectAdapter::fromJS (value); scriptable != nullptr) {
	return scriptable;
    }

    auto& scene = container.object.getScene ();
    if (JS_IsNumber (value)) {
	int32_t id = -1;
	if (JS_ToInt32 (ctx, &id, value) >= 0) {
	    if (auto* object = const_cast<WallpaperEngine::Render::CObject*> (scene.getObject (id));
		object != nullptr) {
		return dynamic_cast<WallpaperEngine::Scripting::ScriptableObject*> (object);
	    }
	}
    }

    const char* requestedName = JS_ToCString (ctx, value);
    if (requestedName == nullptr) {
	return nullptr;
    }
    WallpaperEngine::Scripting::ScriptableObject* result = nullptr;
    for (auto* object : scene.getObjectsByRenderOrder ()) {
	if (object != nullptr && object->getObject ().name == requestedName) {
	    result = dynamic_cast<WallpaperEngine::Scripting::ScriptableObject*> (object);
	    break;
	}
    }
    JS_FreeCString (ctx, requestedName);
    return result;
}

static JSValue scriptable_transform_attachment_to_texture (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv
) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr || argc < 2) return JS_UNDEFINED;
    const auto* source = resolve_scriptable_parent (ctx, *container, argv[0]);
    if (source == nullptr) return JS_UNDEFINED;
    const glm::mat4 attachmentWorld = attachment_world_matrix (ctx, *source, 1, argv + 1);
    std::optional<glm::mat3> matrix;
    if (auto* image = scriptable_image (thisVal)) {
        matrix = image->transformAttachmentToTexture (*source, attachmentWorld);
    } else if (auto* text = dynamic_cast<WallpaperEngine::Render::Objects::CText*> (&container->object)) {
        matrix = text->transformAttachmentToTexture (*source, attachmentWorld);
    }
    return matrix.has_value () ? make_script_mat3 (ctx, *matrix) : JS_UNDEFINED;
}

static bool would_create_parent_cycle (
    const WallpaperEngine::Scripting::ScriptableObject& child,
    const WallpaperEngine::Scripting::ScriptableObject& parent
) {
    constexpr int kMaxParentDepth = 64;
    const auto* current = &parent;
    for (int depth = 0; current != nullptr && depth < kMaxParentDepth; depth++) {
	if (current->getId () == child.getId ()) {
	    return true;
	}
	if (!current->getObject ().parent.has_value ()) {
	    return false;
	}
	const auto* next = current->getScene ().getObject (*current->getObject ().parent);
	current = dynamic_cast<const WallpaperEngine::Scripting::ScriptableObject*> (next);
    }
    return current != nullptr;
}

static void apply_local_transform (WallpaperEngine::Scripting::ScriptableObject& object, const glm::mat4& local) {
    glm::vec3 scale (1.0f);
    glm::quat orientation (1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 translation (0.0f);
    glm::vec3 skew (0.0f);
    glm::vec4 perspective (0.0f);
    if (!glm::decompose (local, scale, orientation, translation, skew, perspective)) {
	return;
    }

    object.getProperty ("origin").update (translation, DynamicValue::UpdateSource::User);
    object.getProperty ("scale").update (scale, DynamicValue::UpdateSource::User);
    object.getProperty ("angles").update (
	glm::eulerAngles (glm::normalize (orientation)), DynamicValue::UpdateSource::User
    );
}

static JSValue scriptable_set_parent (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    if (container == nullptr || argc < 1) {
	return JS_ThrowTypeError (ctx, "setParent() expects a parent layer or undefined");
    }

    const bool clearParent = JS_IsNull (argv[0]) || JS_IsUndefined (argv[0]);
    auto* parent = resolve_scriptable_parent (ctx, *container, argv[0]);
    if (!clearParent && parent == nullptr) {
	return JS_ThrowTypeError (ctx, "setParent() could not resolve the parent layer");
    }
    if (parent != nullptr && would_create_parent_cycle (container->object, *parent)) {
	return JS_ThrowTypeError (ctx, "setParent() would create a parent cycle");
    }

    std::optional<std::string> attachment;
    bool adjustTransforms = false;
    if (argc >= 2 && JS_IsBool (argv[1])) {
	adjustTransforms = JS_ToBool (ctx, argv[1]) != 0;
    } else if (argc >= 2 && !JS_IsNull (argv[1]) && !JS_IsUndefined (argv[1])) {
	if (parent != nullptr) {
	    attachment = resolve_attachment_name (ctx, *parent, argv[1]);
	}
	if (argc >= 3) {
	    adjustTransforms = JS_ToBool (ctx, argv[2]) != 0;
	}
    }

    const glm::mat4 oldWorld = container->object.resolveWorldMatrix ();
    auto& object = const_cast<Object&> (container->object.getObject ());
    object.parent = parent != nullptr ? std::optional<int> (parent->getId ()) : std::nullopt;
    object.attachment = parent != nullptr ? attachment : std::nullopt;

    if (adjustTransforms) {
	glm::mat4 parentWorld (1.0f);
	if (parent != nullptr) {
	    parentWorld = parent->resolveWorldMatrix ();
	    if (attachment.has_value ()) {
		if (const auto attachmentTransform = parent->getAttachmentTransform (*attachment);
		    attachmentTransform.has_value ()) {
		    parentWorld *= *attachmentTransform;
		}
	    }
	}
	apply_local_transform (container->object, glm::inverse (parentWorld) * oldWorld);
    }
    return JS_UNDEFINED;
}

static JSValue scriptable_rotate_object_space (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    glm::vec3 degrees (0.0f);
    if (container == nullptr || argc < 1 || !read_script_vec3 (ctx, argv[0], degrees)) {
	return JS_ThrowTypeError (ctx, "rotateObjectSpace() expects a Vec3");
    }

    auto& angles = container->object.getProperty ("angles");
    const glm::quat current = glm::quat (angles.getVec3 ());
    const glm::quat delta = glm::quat (glm::radians (degrees));
    angles.update (glm::eulerAngles (glm::normalize (current * delta)), DynamicValue::UpdateSource::User);
    return JS_UNDEFINED;
}

static void
set_look_at_orientation (WallpaperEngine::Scripting::ScriptableObject& object, glm::vec3 direction, glm::vec3 up) {
    if (glm::dot (direction, direction) < 1e-8f || glm::dot (up, up) < 1e-8f) {
	return;
    }
    direction = glm::normalize (direction);
    up = glm::normalize (up);
    if (glm::dot (glm::cross (direction, up), glm::cross (direction, up)) < 1e-8f) {
	up = std::abs (direction.y) < 0.999f ? glm::vec3 (0.0f, 1.0f, 0.0f) : glm::vec3 (1.0f, 0.0f, 0.0f);
    }

    glm::quat worldOrientation = glm::quatLookAt (direction, up);
    if (object.getObject ().parent.has_value ()) {
	if (const auto* parent = object.getScene ().getObject (*object.getObject ().parent); parent != nullptr) {
	    glm::vec3 scale, translation, skew;
	    glm::vec4 perspective;
	    glm::quat parentOrientation;
	    if (glm::decompose (
		    parent->resolveWorldMatrix (), scale, parentOrientation, translation, skew, perspective
		)) {
		worldOrientation = glm::inverse (glm::normalize (parentOrientation)) * worldOrientation;
	    }
	}
    }
    object.getProperty ("angles").update (
	glm::eulerAngles (glm::normalize (worldOrientation)), DynamicValue::UpdateSource::User
    );
}

static JSValue
scriptable_look_at_common (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, const bool yawOnly) {
    auto* container = scriptable_container (thisVal);
    glm::vec3 center (0.0f);
    glm::vec3 up (0.0f, 1.0f, 0.0f);
    if (container == nullptr || argc < 1 || !read_script_vec3 (ctx, argv[0], center)
	|| (argc >= 2 && !read_script_vec3 (ctx, argv[1], up))) {
	return JS_ThrowTypeError (
	    ctx, yawOnly ? "lookAtYaw() expects a center Vec3" : "lookAt() expects a center Vec3"
	);
    }

    const glm::vec3 origin = glm::vec3 (container->object.resolveWorldMatrix ()[3]);
    glm::vec3 direction = center - origin;
    if (yawOnly && glm::dot (up, up) > 1e-8f) {
	const glm::vec3 normalizedUp = glm::normalize (up);
	direction -= normalizedUp * glm::dot (direction, normalizedUp);
    }
    set_look_at_orientation (container->object, direction, up);
    return JS_UNDEFINED;
}

static JSValue scriptable_look_at (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    return scriptable_look_at_common (ctx, thisVal, argc, argv, false);
}

static JSValue scriptable_look_at_yaw (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    return scriptable_look_at_common (ctx, thisVal, argc, argv, true);
}

static JSValue scriptable_animation_command (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* functionData
) {
    int32_t layerIndex = -1;
    if (JS_ToInt32 (ctx, &layerIndex, functionData[1]) < 0) {
	return JS_EXCEPTION;
    }

    auto* image = scriptable_image (functionData[0]);
    if (image == nullptr) {
	return JS_UNDEFINED;
    }
    const std::optional<size_t> index
	= layerIndex >= 0 ? std::optional<size_t> (static_cast<size_t> (layerIndex)) : std::nullopt;

    switch (magic) {
	case AnimationCommand_Play:
	    image->playPuppetAnimationLayer (index);
	    return JS_UNDEFINED;
	case AnimationCommand_Pause:
	    image->pausePuppetAnimationLayer (index);
	    return JS_UNDEFINED;
	case AnimationCommand_Stop:
	    image->stopPuppetAnimationLayer (index);
	    return JS_UNDEFINED;
	case AnimationCommand_IsPlaying:
	    return JS_NewBool (ctx, image->isPuppetAnimationLayerPlaying (index));
	case AnimationCommand_GetFrame:
	    return index.has_value () ? JS_NewFloat64 (ctx, image->getPuppetAnimationLayerFrame (*index)) : JS_UNDEFINED;
	case AnimationCommand_SetFrame:
	    {
		double frame = 0.0;
		if (argc < 1 || JS_ToFloat64 (ctx, &frame, argv[0]) < 0 || !std::isfinite (frame)) {
		    return JS_ThrowTypeError (ctx, "Animation frame expects a finite number");
		}
		if (index.has_value ()) image->setPuppetAnimationLayerFrame (*index, static_cast<float> (frame));
		return JS_UNDEFINED;
	    }
	default:
	    return JS_UNDEFINED;
    }
}

static JSValue scriptable_animation_controller (
    JSContext* ctx, JSValueConst owner, const std::optional<size_t> index
) {
    const int32_t layerIndex = index.has_value () ? static_cast<int32_t> (*index) : -1;
    JSValue functionData[] = {
	JS_DupValue (ctx, owner),
	JS_NewInt32 (ctx, layerIndex),
    };
    JSValue result = JS_NewObject (ctx);

    JS_SetPropertyStr (
	ctx, result, "play",
	JS_NewCFunctionData (ctx, scriptable_animation_command, 0, AnimationCommand_Play, 2, functionData)
    );
    JS_SetPropertyStr (
	ctx, result, "pause",
	JS_NewCFunctionData (ctx, scriptable_animation_command, 0, AnimationCommand_Pause, 2, functionData)
    );
    JS_SetPropertyStr (
	ctx, result, "stop",
	JS_NewCFunctionData (ctx, scriptable_animation_command, 0, AnimationCommand_Stop, 2, functionData)
    );
    JS_SetPropertyStr (
	ctx, result, "isPlaying",
	JS_NewCFunctionData (ctx, scriptable_animation_command, 0, AnimationCommand_IsPlaying, 2, functionData)
    );
    if (index.has_value ()) {
	JS_SetPropertyStr (ctx, result, "getFrame",
	    JS_NewCFunctionData (ctx, scriptable_animation_command, 0, AnimationCommand_GetFrame, 2, functionData));
	JS_SetPropertyStr (ctx, result, "setFrame",
	    JS_NewCFunctionData (ctx, scriptable_animation_command, 1, AnimationCommand_SetFrame, 2, functionData));
    }
    JS_FreeValue (ctx, functionData[0]);
    JS_FreeValue (ctx, functionData[1]);
    return result;
}

static JSValue property_animation_command (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* functionData
) {
    auto* container = scriptable_container (functionData[0]);
    const char* name = JS_ToCString (ctx, functionData[1]);
    if (container == nullptr || name == nullptr) {
	JS_FreeCString (ctx, name);
	return JS_UNDEFINED;
    }
    const auto& animations = container->object.getAnimations ();
    const auto found = animations.find (name);
    auto* model = scriptable_model (functionData[0]);
    auto* modelLayer = model != nullptr ? model->findAnimationLayer (name) : nullptr;
    JS_FreeCString (ctx, name);
    if (found == animations.end ()) {
	return JS_UNDEFINED;
    }
    auto& animation = *found->second;
    const double time = container->object.getScene ().getElapsedTime ();
    double value = 0.0;
    switch (magic) {
	case AnimationCommand_Play: animation.play (time); break;
	case AnimationCommand_Pause: animation.pause (time); break;
	case AnimationCommand_Stop: animation.stop (time); break;
	case AnimationCommand_IsPlaying: return JS_NewBool (ctx, animation.isPlaying (time));
	case AnimationCommand_GetFrame: return JS_NewFloat64 (ctx, animation.frameAt (time));
	case AnimationCommand_GetRate: return JS_NewFloat64 (ctx, animation.rate);
	case AnimationCommand_GetFps: return JS_NewFloat64 (ctx, animation.fps);
	case AnimationCommand_GetFrameCount: return JS_NewFloat64 (ctx, animation.length);
	case AnimationCommand_GetDuration:
	    return JS_NewFloat64 (ctx, animation.fps > 0.0f ? animation.length / animation.fps : 0.0f);
	case AnimationCommand_GetBlend:
	    return modelLayer == nullptr ? JS_UNDEFINED : JS_NewFloat64 (ctx, modelLayer->settings != nullptr
		? modelLayer->settings->blend->value->getFloat () : modelLayer->blend);
	case AnimationCommand_GetVisible:
	    return modelLayer == nullptr ? JS_UNDEFINED : JS_NewBool (ctx, modelLayer->settings != nullptr
		? modelLayer->settings->visible->value->getBool () : modelLayer->visible);
	case AnimationCommand_SetVisible:
	    if (modelLayer == nullptr || argc < 1) return JS_UNDEFINED;
	    modelLayer->visible = JS_ToBool (ctx, argv[0]) != 0;
	    if (modelLayer->settings != nullptr) modelLayer->settings->visible->value->update (
		modelLayer->visible, DynamicValue::UpdateSource::User);
	    break;
	case AnimationCommand_SetFrame:
	case AnimationCommand_SetRate:
	case AnimationCommand_SetBlend:
	    if (argc < 1 || JS_ToFloat64 (ctx, &value, argv[0]) < 0 || !std::isfinite (value)) {
		return JS_ThrowTypeError (ctx, "Animation frame/rate expects a finite number");
	    }
	    if (magic == AnimationCommand_SetFrame) animation.setFrame (static_cast<float> (value), time);
	    else if (magic == AnimationCommand_SetRate) animation.setRate (static_cast<float> (value), time);
	    else if (modelLayer != nullptr) {
		modelLayer->blend = std::clamp (static_cast<float> (value), 0.0f, 1.0f);
		if (modelLayer->settings != nullptr) modelLayer->settings->blend->value->update (
		    modelLayer->blend, DynamicValue::UpdateSource::User);
	    }
	    break;
	default: break;
    }
    if (model != nullptr) model->animationControlsChanged (animation);
    return JS_UNDEFINED;
}

static JSValue property_animation_controller (JSContext* ctx, JSValueConst layer, JSValueConst name);

static JSValue model_layer_get_animation (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data
) {
    auto* model = scriptable_model (data[0]);
    if (model == nullptr || argc < 1 || !JS_IsString (argv[0])) return JS_UNDEFINED;
    const char* key = JS_ToCString (ctx, data[1]);
    if (key == nullptr) return JS_EXCEPTION;
    auto* layer = model->findAnimationLayer (key);
    JS_FreeCString (ctx, key);
    if (layer == nullptr || layer->settings == nullptr) return JS_UNDEFINED;
    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) return JS_EXCEPTION;
    ScopeGuard releaseName ([&] { JS_FreeCString (ctx, name); });
    const auto prefix = "animation_" + std::to_string (layer->settings->id) + "_";
    for (const auto& [property, entry] : model->getProperties ()) {
	if (property.starts_with (prefix) && entry.setting != nullptr && entry.setting->animation != nullptr
	    && entry.setting->animation->name == name) {
	    JSValue animationKey = JS_NewString (ctx, property.c_str ());
	    JSValue result = property_animation_controller (ctx, data[0], animationKey);
	    JS_FreeValue (ctx, animationKey);
	    return result;
	}
    }
    return JS_UNDEFINED;
}

static JSValue property_animation_controller (JSContext* ctx, JSValueConst layer, JSValueConst name) {
    JSValue data[] = { layer, name };
    JSValue result = JS_NewObject (ctx);
    const auto method = [&] (const char* methodName, const int command, const int length = 0) {
	JS_SetPropertyStr (ctx, result, methodName,
	    JS_NewCFunctionData (ctx, property_animation_command, length, command, 2, data));
    };
    method ("play", AnimationCommand_Play);
    method ("pause", AnimationCommand_Pause);
    method ("stop", AnimationCommand_Stop);
    method ("isPlaying", AnimationCommand_IsPlaying);
    method ("getFrame", AnimationCommand_GetFrame);
    method ("setFrame", AnimationCommand_SetFrame, 1);
    if (auto* model = scriptable_model (layer); model != nullptr) {
	const char* key = JS_ToCString (ctx, name);
	const bool modelLayer = key != nullptr && model->findAnimationLayer (key) != nullptr;
	JS_FreeCString (ctx, key);
	if (modelLayer) JS_SetPropertyStr (ctx, result, "getAnimation",
	    JS_NewCFunctionData (ctx, model_layer_get_animation, 1, 0, 2, data));
    }
    const JSAtom rate = JS_NewAtom (ctx, "rate");
    JS_DefinePropertyGetSet (ctx, result, rate,
	JS_NewCFunctionData (ctx, property_animation_command, 0, AnimationCommand_GetRate, 2, data),
	JS_NewCFunctionData (ctx, property_animation_command, 1, AnimationCommand_SetRate, 2, data),
	JS_PROP_ENUMERABLE);
    JS_FreeAtom (ctx, rate);
    for (const auto& [field, command] : std::initializer_list<std::pair<const char*, int>> {
	{ "fps", AnimationCommand_GetFps }, { "frameCount", AnimationCommand_GetFrameCount },
	{ "duration", AnimationCommand_GetDuration }, { "blend", AnimationCommand_GetBlend },
	{ "visible", AnimationCommand_GetVisible } }) {
	const JSAtom atom = JS_NewAtom (ctx, field);
	const int setter = command == AnimationCommand_GetBlend ? AnimationCommand_SetBlend
	    : command == AnimationCommand_GetVisible ? AnimationCommand_SetVisible : -1;
	JS_DefinePropertyGetSet (ctx, result, atom,
	    JS_NewCFunctionData (ctx, property_animation_command, 0, command, 2, data),
	    setter >= 0 ? JS_NewCFunctionData (ctx, property_animation_command, 1, setter, 2, data) : JS_UNDEFINED,
	    JS_PROP_ENUMERABLE);
	JS_FreeAtom (ctx, atom);
    }
    return result;
}

static const std::vector<ImageEffectUniquePtr>* scriptable_effects (JSValueConst owner) {
    const auto* container = scriptable_container (owner);
    if (container == nullptr) return nullptr;
    const auto& object = container->object.getObject ();
    if (object.is<Image> ()) return &object.as<Image> ()->effects;
    if (object.is<Text> ()) return &object.as<Text> ()->effects;
    if (object.is<Shape> ()) return &object.as<Shape> ()->effects;
    return nullptr;
}

static JSValue effect_material_get_animation (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data
) {
    const auto* effects = scriptable_effects (data[0]);
    int32_t effectIndex = -1, passIndex = -1;
    if (effects == nullptr || argc < 1 || !JS_IsString (argv[0])
	|| JS_ToInt32 (ctx, &effectIndex, data[1]) < 0 || JS_ToInt32 (ctx, &passIndex, data[2]) < 0
	|| effectIndex < 0 || static_cast<size_t> (effectIndex) >= effects->size ()) return JS_UNDEFINED;
    const auto& effect = *(*effects)[effectIndex];
    if (passIndex < 0 || static_cast<size_t> (passIndex) >= effect.passOverrides.size ()) return JS_UNDEFINED;
    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) return JS_EXCEPTION;
    ScopeGuard releaseName ([&] { JS_FreeCString (ctx, name); });
    for (const auto& [property, setting] : effect.passOverrides[passIndex]->constants) {
	if (setting->animation == nullptr || setting->animation->name != name) continue;
	const auto key = "effect_" + std::to_string (effect.id) + "_pass" + std::to_string (passIndex) + "_" + property;
	JSValue animationKey = JS_NewString (ctx, key.c_str ());
	JSValue result = property_animation_controller (ctx, data[0], animationKey);
	JS_FreeValue (ctx, animationKey);
	return result;
    }
    return JS_UNDEFINED;
}

static JSValue effect_get_material (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data
) {
    const auto* effects = scriptable_effects (data[0]);
    int32_t effectIndex = -1, passIndex = -1;
    if (effects == nullptr || argc < 1 || !JS_IsNumber (argv[0])
	|| JS_ToInt32 (ctx, &effectIndex, data[1]) < 0 || JS_ToInt32 (ctx, &passIndex, argv[0]) < 0
	|| effectIndex < 0 || static_cast<size_t> (effectIndex) >= effects->size ()) return JS_UNDEFINED;
    const auto& effect = *(*effects)[effectIndex];
    if (passIndex < 0 || static_cast<size_t> (passIndex) >= effect.passOverrides.size ()) return JS_UNDEFINED;
    JSValue materialData[] = { data[0], data[1], argv[0] };
    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "getAnimation",
	JS_NewCFunctionData (ctx, effect_material_get_animation, 1, 0, 3, materialData));
    return result;
}

struct OpaqueEffect {
    JSValue owner;
    int32_t index;
};

static void effect_finalizer (JSRuntime* runtime, JSValueConst value) {
    JSClassID classId = 0;
    auto* effect = static_cast<OpaqueEffect*> (JS_GetAnyOpaque (value, &classId));
    if (effect == nullptr) return;
    JS_FreeValueRT (runtime, effect->owner);
    delete effect;
}

static void effect_mark (JSRuntime* runtime, JSValueConst value, JS_MarkFunc* mark) {
    JSClassID classId = 0;
    const auto* effect = static_cast<const OpaqueEffect*> (JS_GetAnyOpaque (value, &classId));
    if (effect != nullptr) JS_MarkValue (runtime, effect->owner, mark);
}

static JSValue effect_execute_material_function (
    JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic
) {
    const auto* receiver = static_cast<const OpaqueEffect*> (JS_GetOpaque (thisVal, magic));
    if (receiver == nullptr || argc < 1 || !JS_IsString (argv[0])) return JS_UNDEFINED;
    const auto* effects = scriptable_effects (receiver->owner);
    if (effects == nullptr || receiver->index < 0
	|| static_cast<size_t> (receiver->index) >= effects->size ()) return JS_UNDEFINED;
    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) return JS_EXCEPTION;
    ScopeGuard releaseName ([&] { JS_FreeCString (ctx, name); });
    scriptable_container (receiver->owner)->object.executeMaterialFunction (*(*effects)[receiver->index], name);
    return JS_UNDEFINED;
}

static JSValue scriptable_get_effect (JSContext* ctx, JSValueConst owner, int argc, JSValueConst* argv) {
    const auto* effects = scriptable_effects (owner);
    if (effects == nullptr || argc < 1) return JS_UNDEFINED;
    int32_t index = -1;
    if (JS_IsNumber (argv[0])) {
	if (JS_ToInt32 (ctx, &index, argv[0]) < 0) return JS_EXCEPTION;
    } else if (JS_IsString (argv[0])) {
	const char* name = JS_ToCString (ctx, argv[0]);
	if (name == nullptr) return JS_EXCEPTION;
	for (size_t i = 0; i < effects->size (); ++i) {
	    if ((*effects)[i]->name == name) { index = static_cast<int32_t> (i); break; }
	}
	JS_FreeCString (ctx, name);
    }
    if (index < 0 || static_cast<size_t> (index) >= effects->size ()) return JS_UNDEFINED;
    return scriptable_container (owner)->adapter.instantiateEffect (owner, index);
}

JSValue ScriptableObjectAdapter::instantiateEffect (JSValueConst owner, const int32_t index) {
    JSContext* ctx = m_engine.getContext ();
    JSValue result = JS_NewObjectClass (ctx, m_effectClassId);
    if (JS_IsException (result)) return result;
    JS_SetOpaque (result, new OpaqueEffect { JS_DupValue (ctx, owner), index });
    JSValue data[] = { owner, JS_NewInt32 (ctx, index) };
    JS_SetPropertyStr (ctx, result, "getMaterial", JS_NewCFunctionData (ctx, effect_get_material, 1, 0, 2, data));
    JS_SetPropertyStr (ctx, result, "executeMaterialFunction",
	JS_NewCFunctionMagic (ctx, effect_execute_material_function, "executeMaterialFunction", 1,
	    JS_CFUNC_generic_magic, m_effectClassId));
    JS_FreeValue (ctx, data[1]);
    return result;
}

static JSValue scriptable_get_animation (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (auto* model = scriptable_model (this_val); model != nullptr && argc > 0) {
	WallpaperEngine::Render::Objects::CModel::AnimationLayer* layer = nullptr;
	if (JS_IsString (argv[0])) {
	    const char* name = JS_ToCString (ctx, argv[0]);
	    if (name == nullptr) return JS_EXCEPTION;
	    layer = model->getAnimationLayer (name);
	    JS_FreeCString (ctx, name);
	} else if (JS_IsNumber (argv[0])) {
	    int32_t index = -1;
	    if (JS_ToInt32 (ctx, &index, argv[0]) < 0) return JS_EXCEPTION;
	    if (index >= 0) layer = model->getAnimationLayer (static_cast<size_t> (index));
	}
	if (layer != nullptr) {
	    JSValue key = JS_NewString (ctx, layer->key.c_str ());
	    JSValue result = property_animation_controller (ctx, this_val, key);
	    JS_FreeValue (ctx, key);
	    return result;
	}
    }
    if (auto* container = scriptable_container (this_val); container != nullptr && argc > 0 && JS_IsString (argv[0])) {
	const char* name = JS_ToCString (ctx, argv[0]);
	if (name == nullptr) return JS_EXCEPTION;
	const bool found = container->object.getAnimations ().contains (name);
	JS_FreeCString (ctx, name);
	if (found) return property_animation_controller (ctx, this_val, argv[0]);
    }
    auto* image = scriptable_image (this_val);
    if (image == nullptr) {
	return JS_UNDEFINED;
    }
    if (argc < 1) {
	return scriptable_animation_controller (ctx, this_val, std::nullopt);
    }

    std::optional<size_t> index;
    if (JS_IsNumber (argv[0])) {
	int32_t numericIndex = -1;
	if (JS_ToInt32 (ctx, &numericIndex, argv[0]) < 0) {
	    return JS_EXCEPTION;
	}
	index = image->findPuppetAnimationLayer (numericIndex);
    } else if (JS_IsString (argv[0])) {
	const char* name = JS_ToCString (ctx, argv[0]);
	if (name == nullptr) {
	    return JS_EXCEPTION;
	}
	index = image->findPuppetAnimationLayer (name);
	JS_FreeCString (ctx, name);
    }

    return index.has_value () ? scriptable_animation_controller (ctx, this_val, index) : JS_UNDEFINED;
}

static JSValue scriptable_play_single_animation (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* model = scriptable_model (thisVal);
    if (model == nullptr) return JS_UNDEFINED;
    if (argc < 1 || !JS_IsString (argv[0])) return JS_ThrowTypeError (ctx, "playSingleAnimation expects an animation name");
    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) return JS_EXCEPTION;
    ScopeGuard releaseName ([&] { JS_FreeCString (ctx, name); });
    WallpaperEngine::Render::Objects::CModel::SingleAnimationConfig config;
    if (argc >= 2 && JS_IsObject (argv[1])) {
	for (const auto& [field, target] : std::initializer_list<std::pair<const char*, bool*>> {
	    { "blendin", &config.blendIn }, { "blendout", &config.blendOut } }) {
	    JSValue value = JS_GetPropertyStr (ctx, argv[1], field);
	    if (!JS_IsUndefined (value)) *target = JS_ToBool (ctx, value) != 0;
	    JS_FreeValue (ctx, value);
	}
	JSValue value = JS_GetPropertyStr (ctx, argv[1], "blendtime");
	double seconds = config.blendTime;
	const bool valid = JS_IsUndefined (value) || (JS_ToFloat64 (ctx, &seconds, value) == 0 && std::isfinite (seconds));
	JS_FreeValue (ctx, value);
	if (!valid) return JS_ThrowTypeError (ctx, "Animation blendtime expects a finite number");
	config.blendTime = static_cast<float> (seconds);
    }
    auto* layer = model->playSingleAnimation (name, config);
    if (layer == nullptr) return JS_UNDEFINED;
    JSValue key = JS_NewString (ctx, layer->key.c_str ());
    JSValue result = property_animation_controller (ctx, thisVal, key);
    JS_FreeValue (ctx, key);
    return result;
}

static JSValue
scriptable_object_animation_command (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    auto* container = scriptable_container (this_val);
    if (auto* model = scriptable_model (this_val)) {
	bool playing = false;
	const double time = model->getScene ().getElapsedTime ();
	for (size_t i = 0; i < model->getAnimationLayerCount (); ++i) {
	    auto& animation = model->getAnimationLayer (i)->timeline;
	    switch (magic) {
		case AnimationCommand_Play: animation.play (time); break;
		case AnimationCommand_Pause: animation.pause (time); break;
		case AnimationCommand_Stop: animation.stop (time); break;
		case AnimationCommand_IsPlaying: playing |= animation.isPlaying (time); break;
		default: break;
	    }
	    if (magic != AnimationCommand_IsPlaying) model->animationControlsChanged (animation);
	}
	return magic == AnimationCommand_IsPlaying ? JS_NewBool (ctx, playing) : JS_UNDEFINED;
    }
    if (auto* particle = container != nullptr
	    ? dynamic_cast<WallpaperEngine::Render::Objects::CParticle*> (&container->object) : nullptr) {
	switch (magic) {
	    case AnimationCommand_Play: particle->play (); break;
	    case AnimationCommand_Pause: particle->pause (); break;
	    case AnimationCommand_Stop: particle->stop (); break;
	    case AnimationCommand_IsPlaying: return JS_NewBool (ctx, particle->isPlaying ());
	    default: break;
	}
	return JS_UNDEFINED;
    }
    if (auto* sound = container != nullptr
	    ? dynamic_cast<WallpaperEngine::Render::Objects::CSound*> (&container->object)
	    : nullptr) {
	switch (magic) {
	    case AnimationCommand_Play:
		sound->play ();
		return JS_UNDEFINED;
	    case AnimationCommand_Pause:
		sound->pause ();
		return JS_UNDEFINED;
	    case AnimationCommand_Stop:
		sound->stop ();
		return JS_UNDEFINED;
	    case AnimationCommand_IsPlaying:
		return JS_NewBool (ctx, sound->isPlaying ());
	    default:
		return JS_UNDEFINED;
	}
    }
    auto* image = scriptable_image (this_val);
    if (image == nullptr) {
	return magic == AnimationCommand_IsPlaying ? JS_FALSE : JS_UNDEFINED;
    }

    switch (magic) {
	case AnimationCommand_Play:
	    image->playPuppetAnimationLayer ();
	    return JS_UNDEFINED;
	case AnimationCommand_Pause:
	    image->pausePuppetAnimationLayer ();
	    return JS_UNDEFINED;
	case AnimationCommand_Stop:
	    image->stopPuppetAnimationLayer ();
	    return JS_UNDEFINED;
	case AnimationCommand_IsPlaying:
	    return JS_NewBool (ctx, image->isPuppetAnimationLayerPlaying ());
	default:
	    return JS_UNDEFINED;
    }
}

static JSValue
scriptable_get_animation_layer_count (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (const auto* model = scriptable_model (this_val)) return JS_NewInt64 (ctx, model->getAnimationLayerCount ());
    auto* image = scriptable_image (this_val);
    return JS_NewInt64 (ctx, image != nullptr ? static_cast<int64_t> (image->getPuppetAnimationLayerCount ()) : 0);
}

static JSValue scriptable_emit_particles (JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    auto* container = scriptable_container (thisVal);
    auto* particle = container != nullptr
	? dynamic_cast<WallpaperEngine::Render::Objects::CParticle*> (&container->object) : nullptr;
    if (particle == nullptr) return JS_UNDEFINED;
    double count = 1.0;
    if (argc > 0 && !JS_IsUndefined (argv[0])
	&& (JS_ToFloat64 (ctx, &count, argv[0]) < 0 || !std::isfinite (count) || count < 0.0)) {
	return JS_ThrowTypeError (ctx, "emitParticles expects a nonnegative finite count");
    }
    particle->emitParticles (static_cast<uint32_t> (std::min (count, static_cast<double> (UINT32_MAX))));
    return JS_UNDEFINED;
}

static JSValue scriptable_get_video_texture (JSContext* ctx, JSValueConst thisVal, int, JSValueConst*) {
    const auto* image = scriptable_image (thisVal);
    return image != nullptr
	? image->getScene ().getScriptEngine ().getAdapters ().video->instantiate (image->getTexture ())
	: JS_UNDEFINED;
}

static JSValue create_particle_instance (JSContext* ctx, JSValueConst owner);

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_ThrowTypeError (ctx, "invalid receiver for property access");
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    if (container->object.getObject ().is<Text> ()) {
	if (std::strcmp (name, "font") == 0) {
	    return JS_NewString (ctx, container->object.getObject ().as<Text> ()->font.c_str ());
	}
	if (std::strcmp (name, "pointsize") == 0) name = "pointSize";
    }
    if (std::strcmp (name, "size") == 0) {
	if (const auto* image = dynamic_cast<WallpaperEngine::Render::Objects::CImage*> (&container->object)) {
	    DynamicValue size (image->getSize ());
	    return container->adapter.getEngine ().getAdapters ().vec2->instantiate (size, true);
	}
	if (auto* text = dynamic_cast<WallpaperEngine::Render::Objects::CText*> (&container->object)) {
	    DynamicValue size (text->getSize ());
	    return container->adapter.getEngine ().getAdapters ().vec2->instantiate (size, true);
	}
    }
    if (std::strcmp (name, "instance") == 0
	&& dynamic_cast<WallpaperEngine::Render::Objects::CParticle*> (&container->object) != nullptr) {
	return create_particle_instance (ctx, obj_val);
    }
    if (std::strcmp (name, "name") == 0) {
	return JS_NewString (ctx, container->object.getObject ().name.c_str ());
    }

    if (std::strcmp (name, "play") == 0) {
	return JS_NewCFunctionMagic (
	    ctx, scriptable_object_animation_command, name, 0, JS_CFUNC_generic_magic, AnimationCommand_Play
	);
    }
    if (std::strcmp (name, "pause") == 0) {
	return JS_NewCFunctionMagic (
	    ctx, scriptable_object_animation_command, name, 0, JS_CFUNC_generic_magic, AnimationCommand_Pause
	);
    }
    if (std::strcmp (name, "stop") == 0) {
	return JS_NewCFunctionMagic (
	    ctx, scriptable_object_animation_command, name, 0, JS_CFUNC_generic_magic, AnimationCommand_Stop
	);
    }
    if (std::strcmp (name, "isPlaying") == 0) {
	return JS_NewCFunctionMagic (
	    ctx, scriptable_object_animation_command, name, 0, JS_CFUNC_generic_magic, AnimationCommand_IsPlaying
	);
    }
    if (std::strcmp (name, "getTransformMatrix") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_transform_matrix, name, 0);
    }
    if (std::strcmp (name, "rotateObjectSpace") == 0) {
	return JS_NewCFunction (ctx, scriptable_rotate_object_space, name, 1);
    }
    if (std::strcmp (name, "lookAt") == 0) {
	return JS_NewCFunction (ctx, scriptable_look_at, name, 2);
    }
    if (std::strcmp (name, "lookAtYaw") == 0) {
	return JS_NewCFunction (ctx, scriptable_look_at_yaw, name, 2);
    }
    if (std::strcmp (name, "setParent") == 0) {
	return JS_NewCFunction (ctx, scriptable_set_parent, name, 3);
    }
    if (std::strcmp (name, "getParent") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_parent, name, 0);
    }
    if (std::strcmp (name, "getChildren") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_children, name, 0);
    }
    if (std::strcmp (name, "transformAttachmentToTexture") == 0
        && (scriptable_image (obj_val) != nullptr
            || dynamic_cast<WallpaperEngine::Render::Objects::CText*> (&container->object) != nullptr)) {
        return JS_NewCFunction (ctx, scriptable_transform_attachment_to_texture, name, 2);
    }
    if (std::strcmp (name, "getAttachmentIndex") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_attachment_index, name, 1);
    }
    if (std::strcmp (name, "getAttachmentMatrix") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_attachment_matrix, name, 1);
    }
    if (std::strcmp (name, "getAttachmentOrigin") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_attachment_origin, name, 1);
    }
    if (std::strcmp (name, "getAttachmentAngles") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_attachment_angles, name, 1);
    }
    if (std::strcmp (name, "getBoneIndex") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_bone_index, name, 1);
    }
    if (std::strcmp (name, "getBoneTransform") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_bone_transform, name, 1);
    }

    if (std::strcmp (name, "getAnimation") == 0 || std::strcmp (name, "getAnimationLayer") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_animation, name, 1);
    }
    if (std::strcmp (name, "getEffect") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_effect, name, 1);
    }
    if (std::strcmp (name, "getTextureAnimation") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_texture_animation, name, 0);
    }
    if (std::strcmp (name, "getVideoTexture") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_video_texture, name, 0);
    }
    if (std::strcmp (name, "getAnimationLayerCount") == 0) {
	return JS_NewCFunction (ctx, scriptable_get_animation_layer_count, name, 0);
    }
    if (std::strcmp (name, "playSingleAnimation") == 0 && scriptable_model (obj_val) != nullptr) {
	return JS_NewCFunction (ctx, scriptable_play_single_animation, name, 2);
    }
    if (std::strcmp (name, "emitParticles") == 0
	&& dynamic_cast<WallpaperEngine::Render::Objects::CParticle*> (&container->object) != nullptr) {
	return JS_NewCFunction (ctx, scriptable_emit_particles, name, 1);
    }

    // Scripts probe optional layer fields every frame. A missing field has
    // always returned undefined here; avoid logging and throwing just to catch
    // it below, which floods the log for unsupported fields such as solid.
    if (!container->object.getProperties ().contains (name)) return JS_UNDEFINED;
    try {
	// find the property inside, otherwise return undefined
	auto& property = container->object.getProperty (name);
	const auto* setting = container->object.getPropertySetting (name);
	if (setting != nullptr && std::string_view (name).starts_with ("instance.controlpoint")) {
	    DynamicValue sampled (setting->evaluateVec3 (container->object.getScene ().getElapsedTime ()));
	    return container->adapter.getEngine ().getAdapters ().vec3->instantiate (sampled, true);
	}
	if (setting != nullptr && setting->animation != nullptr) {
	    // A camera-follow script reads the animated layer pose, not the base
	    // value stored in scene.json. Use owned snapshots so sampling relative
	    // curves neither mutates their base nor leaves dangling vector adapters.
	    const double time = container->object.getScene ().getElapsedTime ();
	    auto& adapters = container->adapter.getEngine ().getAdapters ();
	    switch (property.getType ()) {
		case DynamicValue::Float:
		case DynamicValue::Int: return JS_NewFloat64 (ctx, setting->evaluateFloat (time));
		case DynamicValue::Vec2: {
		    DynamicValue sampled (glm::vec2 (setting->evaluateVec3 (time)));
		    return adapters.vec2->instantiate (sampled, true);
		}
		case DynamicValue::Vec3: {
		    glm::vec3 value = setting->evaluateVec3 (time);
		    if (std::strcmp (name, "angles") == 0) value = glm::degrees (value);
		    DynamicValue sampled (value);
		    return adapters.vec3->instantiate (sampled, true);
		}
		case DynamicValue::Vec4: {
		    glm::vec4 value = property.getVec4 ();
		    for (int i = 0; i < 4; ++i) {
			value[i] = setting->animation->relative
			    ? value[i] + setting->animation->evaluateChannel (i, time, 0.0f)
			    : setting->animation->evaluateChannel (i, time, value[i]);
		    }
		    DynamicValue sampled (value);
		    return adapters.vec4->instantiate (sampled, true);
		}
		default: break;
	    }
	}

	// SceneScript exposes layer Euler angles in degrees even though scene.json
	// and the renderer store them in radians.
	if (std::strcmp (name, "angles") == 0 && property.getType () == DynamicValue::Vec3) {
	    DynamicValue degrees (glm::degrees (property.getVec3 ()));
	    return container->adapter.getEngine ().getAdapters ().vec3->instantiate (degrees, true);
	}

	// Layer vectors are values. Scripts commonly retain an initial origin or
	// scale and assign offsets from it each frame. A borrowed adapter would
	// mutate that saved value on every assignment, accumulating the offset
	// until models such as Sonic's rail (3759507080) leave the camera entirely.
	auto& adapters = container->adapter.getEngine ().getAdapters ();
	switch (property.getType ()) {
	    case DynamicValue::Vec2: return adapters.vec2->instantiate (property, true);
	    case DynamicValue::Vec3: return adapters.vec3->instantiate (property, true);
	    case DynamicValue::Vec4: return adapters.vec4->instantiate (property, true);
	    default: return container->adapter.getEngine ().dynamicToJs (property);
	}
    } catch (const std::exception& e) {
	return JS_UNDEFINED;
    }
}

int scriptableobject_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	JS_ThrowTypeError (ctx, "invalid receiver for property assignment");
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    if (std::strcmp (name, "pointsize") == 0 && container->object.getObject ().is<Text> ()) name = "pointSize";
    if (std::strcmp (name, "name") == 0 && JS_IsString (val)) {
	const char* text = JS_ToCString (ctx, val);
	if (text == nullptr) return -1;
	const_cast<Object&> (container->object.getObject ()).name = text;
	JS_FreeCString (ctx, text);
	return 1;
    }
    if (!container->object.getProperties ().contains (name)) {
	// Keep native image/text size separate until its engine behavior is implemented.
	if (std::strcmp (name, "size") == 0
	    && (container->object.getObject ().is<Image> () || container->object.getObject ().is<Text> ())) return 1;
	const JSValue current = scriptableobject_property_get (ctx, obj_val, atom, receiver);
	if (JS_IsException (current)) return -1;
	const bool nativeProperty = !JS_IsUndefined (current);
	JS_FreeValue (ctx, current);
	if (nativeProperty) return 1;
	return JS_DefinePropertyValueStr (ctx, obj_val, name, JS_DupValue (ctx, val), JS_PROP_C_W_E);
    }
    try {
	auto& property = container->object.getProperty (name);

	if (JS_IsBool (val)) {
	    property.update (JS_ToBool (ctx, val) != 0, DynamicValue::UpdateSource::User);
	} else if (JS_IsString (val)) {
	    const char* text = JS_ToCString (ctx, val);
	    if (text == nullptr) {
		return -1;
	    }
	    property.update (std::string (text), DynamicValue::UpdateSource::Script);
	    JS_FreeCString (ctx, text);
	} else if (JS_IsNumber (val)) {
	    double number = 0.0;
	    JS_ToFloat64 (ctx, &number, val);
	    property.update (static_cast<float> (number), DynamicValue::UpdateSource::User);
	} else if (JS_IsObject (val)) {
	    const auto component = [ctx, &val] (const char* key) -> float {
		JSValue field = JS_GetPropertyStr (ctx, val, key);
		double number = 0.0;
		if (JS_IsNumber (field)) {
		    JS_ToFloat64 (ctx, &number, field);
		}
		JS_FreeValue (ctx, field);
		return static_cast<float> (number);
	    };
	    glm::vec3 vector (component ("x"), component ("y"), component ("z"));
	    if (std::strcmp (name, "angles") == 0) {
		vector = glm::radians (vector);
	    }
	    property.update (vector, DynamicValue::UpdateSource::User);
	}
    } catch (const std::exception&) {
	// Keep rejected native-property values on their existing path.
    }

    return 1;
}

static constexpr const char* particleInstanceFields[] = {
    "enabled", "alpha", "brightness", "size", "lifetime", "rate", "speed", "count", "color", "colorn",
    "controlpoint0", "controlpoint1", "controlpoint2", "controlpoint3",
    "controlpoint4", "controlpoint5", "controlpoint6", "controlpoint7",
    "controlpointangle0", "controlpointangle1", "controlpointangle2", "controlpointangle3",
    "controlpointangle4", "controlpointangle5", "controlpointangle6", "controlpointangle7"
};

static JSValue particle_instance_field (
    JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValue* data
) {
    std::string property = particleInstanceFields[magic];
    if (property.starts_with ("controlpoint")) property.insert (0, "instance.");
    const JSAtom atom = JS_NewAtom (ctx, property.c_str ());
    ScopeGuard guard ([=] { JS_FreeAtom (ctx, atom); });
    if (argc == 0) return scriptableobject_property_get (ctx, data[0], atom, data[0]);
    return scriptableobject_property_set (ctx, data[0], atom, argv[0], data[0], JS_PROP_THROW) < 0
	? JS_EXCEPTION : JS_UNDEFINED;
}

static JSValue particle_instance_get_animation (
    JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValue* data
) {
    auto* container = scriptable_container (data[0]);
    if (container == nullptr || argc < 1 || !JS_IsString (argv[0])) return JS_UNDEFINED;
    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) return JS_EXCEPTION;
    const std::string qualified = "instance." + std::string (name);
    JS_FreeCString (ctx, name);
    if (!container->object.getAnimations ().contains (qualified)) return JS_UNDEFINED;
    JSValue key = JS_NewString (ctx, qualified.c_str ());
    ScopeGuard release ([&] { JS_FreeValue (ctx, key); });
    return property_animation_controller (ctx, data[0], key);
}

static JSValue create_particle_instance (JSContext* ctx, JSValueConst owner) {
    JSValue instance = JS_NewObject (ctx);
    JSValue data[] = { owner };
    JS_SetPropertyStr (ctx, instance, "getAnimation",
	JS_NewCFunctionData (ctx, particle_instance_get_animation, 1, 0, 1, data));
    for (int i = 0; i < static_cast<int> (std::size (particleInstanceFields)); ++i) {
	const JSAtom atom = JS_NewAtom (ctx, particleInstanceFields[i]);
	JS_DefinePropertyGetSet (ctx, instance, atom,
	    JS_NewCFunctionData (ctx, particle_instance_field, 0, i, 1, data),
	    JS_NewCFunctionData (ctx, particle_instance_field, 1, i, 1, data), JS_PROP_ENUMERABLE);
	JS_FreeAtom (ctx, atom);
    }
    return instance;
}

ScriptableObjectAdapter::ScriptableObjectAdapter (ScriptEngine& engine, std::string name) :
    ObjectAdapter (engine), m_exoticMethods (), m_name (std::move (name)) {
    JS_NewClassID (engine.getRuntime (), &m_effectClassId);
    const JSClassDef effectClass = { .class_name = "IEffect", .finalizer = effect_finalizer, .gc_mark = effect_mark };
    JS_NewClass (engine.getRuntime (), m_effectClassId, &effectClass);
    JSValue plainObject = JS_NewObject (engine.getContext ());
    JS_SetClassProto (engine.getContext (), m_effectClassId, JS_GetPrototype (engine.getContext (), plainObject));
    JS_FreeValue (engine.getContext (), plainObject);
    this->m_exoticMethods.get_property = scriptableobject_property_get;
    this->m_exoticMethods.set_property = scriptableobject_property_set;

    this->registerType (
	{
	    .class_name = m_name.c_str (),
	    .finalizer = scriptableobject_finalizer,
	    .exotic = &m_exoticMethods,
	}
    );
}

JSValue ScriptableObjectAdapter::instantiate (ScriptableObject& object) {
    if (const auto it = m_instances.find (&object); it != m_instances.end ()) {
	return JS_DupValue (m_engine.getContext (), it->second);
    }
    JSValue result = this->ObjectAdapter::instantiate (object);
    JS_SetOpaque (
	result,
	new OpaqueScriptableObjectAdapter { .magic = SCRIPTABLE_OPAQUE_MAGIC, .adapter = *this, .object = object }
    );

    m_instances.emplace (&object, JS_DupValue (m_engine.getContext (), result));
    return result;
}

ScriptableObjectAdapter::~ScriptableObjectAdapter () {
    for (const auto& [object, value] : m_instances) JS_FreeValue (m_engine.getContext (), value);
}

void ScriptableObjectAdapter::invalidate (const ScriptableObject* object) {
    const auto it = m_instances.find (object);
    if (it == m_instances.end ()) return;
    if (auto* container = scriptable_container (it->second)) container->magic = 0;
    JS_FreeValue (m_engine.getContext (), it->second);
    m_instances.erase (it);
}

JSValue ScriptableObjectAdapter::instantiate (DynamicValue& value) {
    throw std::runtime_error ("Cannot create a ScriptableObject instance from a DynamicValue");
}

WallpaperEngine::Scripting::ScriptableObject* ScriptableObjectAdapter::fromJS (JSValue value) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (value, &classId));

    if (container == nullptr || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return nullptr;
    }

    return &container->object;
}
