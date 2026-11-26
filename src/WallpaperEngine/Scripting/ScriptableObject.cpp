#include "ScriptableObject.h"

#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <ranges>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Scripting;

ScriptableObject::~ScriptableObject () {
    // registerProperty queued a script per property, each holding this object by reference and by
    // raw pointer. CScene::dispatchObjectType deletes objects whose setup() throws, and
    // CScene::destroyObjects deletes every object while the engine is still alive, so both paths
    // would otherwise leave the engine calling hooks on freed memory.
    this->getScene ().getScriptEngine ().unregisterScriptable (this);
}

ScriptableObject::ScriptableObject (Wallpapers::CScene& scene, const Object& object) : CObject (scene, object) {
    // A queued module keeps a reference to its initial DynamicValue. Select the
    // concrete renderer's fields before queueing: Image/Particle/Text parse their
    // own transform settings separately from the generic group fallbacks.
    const auto* scale = object.groupScale.get ();
    const auto* angles = object.groupAngles.get ();
    const auto* visible = object.groupVisible.get ();
    if (object.is<Image> ()) {
	const auto* image = object.as<Image> ();
	scale = image->scale.get ();
	angles = image->angles.get ();
	visible = image->visible.get ();
    } else if (object.is<Particle> ()) {
	const auto* particle = object.as<Particle> ();
	scale = particle->scale.get ();
	angles = particle->angles.get ();
	visible = particle->visible.get ();
    } else if (object.is<Text> ()) {
	const auto* text = object.as<Text> ();
	scale = text->scale.get ();
	visible = text->visible.get ();
    }
    this->registerProperty ("origin", *object.origin);
    this->registerProperty ("scale", *scale);
    this->registerProperty ("angles", *angles);
    this->registerProperty ("visible", *visible);

    for (const auto& projection : scene.getScene ().camera.objectProjections) {
	if (projection.id != object.id) {
	    continue;
	}
	if (projection.fov != nullptr) {
	    this->registerProperty ("fov", *projection.fov);
	}
	if (projection.zoom != nullptr) {
	    this->registerProperty ("zoom", *projection.zoom);
	}
	break;
    }
}

DynamicValue& ScriptableObject::getProperty (const std::string& name) {
    const auto it = this->m_properties.find (name);

    if (it == this->m_properties.end ()) {
	sLog.exception ("Property '" + name + "' not found on object '" + this->getObject ().name + "'");
    }

    return it->second.value;
}

const std::map<std::string, ScriptableObject::PropertyEntry>& ScriptableObject::getProperties () const {
    return this->m_properties;
}

const UserSetting* ScriptableObject::getPropertySetting (const std::string& name) const {
    const auto it = m_properties.find (name);
    return it != m_properties.end () ? it->second.setting : nullptr;
}

void ScriptableObject::registerProperty (
    const std::string& name, const UserSetting& setting, const std::string& animationScope, bool deferInitialization
) {
    if (setting.animation != nullptr) {
	m_animations.insert_or_assign (name, setting.animation.get ());
	if (!setting.animation->name.empty ())
	    m_animations.insert_or_assign (animationScope + setting.animation->name, setting.animation.get ());
    }
    registerProperty (name, *setting.value, animationScope, &setting, deferInitialization);
    // Resolve after each registration: a child can precede its clock property
    // in both serialized data and a derived renderer's registration order.
    for (const auto& [property, entry] : m_properties) {
	if (entry.setting == nullptr || entry.setting->animation == nullptr) continue;
	auto& animation = *entry.setting->animation;
	if (animation.parentKey.empty ()) continue;
	const auto* parent = getPropertySetting (entry.animationScope + animation.parentKey);
	if (parent == nullptr || parent->animation == nullptr) continue;
	auto* clock = parent->animation.get ();
	bool cycle = false;
	for (auto* ancestor = clock; ancestor != nullptr; ancestor = ancestor->timelineParent) {
	    if (ancestor == &animation) { cycle = true; break; }
	}
	if (!cycle) animation.timelineParent = clock;
    }
}

void ScriptableObject::registerProperty (
    const std::string& name, DynamicValue& value, const std::string& animationScope, const UserSetting* setting,
    bool deferInitialization
) {
    // Derived renderers repeat some common registrations and add their own
    // properties. Both this map and the queued script must refer to the final
    // values consumed by the renderer.
    const std::string key = name + "_" + std::to_string (this->getId ());
    // PropertyEntry holds a reference member (not assignable), so drop any prior registration and
    // re-emplace to let the derived value win.
    this->m_properties.erase (name);
    const auto [it, inserted] = this->m_properties.emplace (
	name, PropertyEntry { .key = key, .value = value, .setting = setting, .animationScope = animationScope }
    );

    // Re-registering the same value is safe: the keyed module is initialized only
    // once. The constructor must select the final value before its first queue.
    this->getScene ().getScriptEngine ().queueScript (it->second.key, it->second.value, *this, deferInitialization);
}

void ScriptableObject::unregisterProperty (const std::string& name) {
    const auto found = m_properties.find (name);
    if (found == m_properties.end ()) return;
    getScene ().getScriptEngine ().unregisterPropertyScript (found->second.key, this);
    if (found->second.setting != nullptr && found->second.setting->animation != nullptr) {
        const auto* animation = found->second.setting->animation.get ();
        std::erase_if (m_animations, [animation] (const auto& entry) { return entry.second == animation; });
        for (const auto& [property, entry] : m_properties) {
            if (entry.setting != nullptr && entry.setting->animation != nullptr
                && entry.setting->animation->timelineParent == animation) {
                entry.setting->animation->timelineParent = nullptr;
            }
        }
    }
    m_properties.erase (found);
}
