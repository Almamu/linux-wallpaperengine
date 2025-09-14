#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"

#include "WallpaperEngine/Render/WallpaperState.h"

#include "CScene.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"

extern float g_Time;
extern float g_TimeLast;

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Render::Wallpapers;
using JSON = WallpaperEngine::Data::JSON::JSON;

CScene::CScene (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode,
    const uint32_t& clampMode
) :
    CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode) {
    // caller should check this, if not a std::bad_cast is good to throw
    auto scene = wallpaper.as <Scene> ();

    // setup the scene camera
    this->m_camera = std::make_unique<Camera> (*this, scene->camera);

    float width = scene->camera.projection.width;
    float height = scene->camera.projection.height;

    // detect size if the orthogonal project is auto
    if (scene->camera.projection.isAuto) {
        // TODO: CALCULATE ORTHOGONAL PROJECTION BASED ON CONTENT'S SIZE HERE
    }

    this->m_parallaxDisplacement = {0, 0};

    // TODO: CONVERSION
    this->m_camera->setOrthogonalProjection (width, height);

    // setup framebuffers here as they're required for the scene setup
    this->setupFramebuffers ();

    const uint32_t sceneWidth = this->m_camera->getWidth ();
    const uint32_t sceneHeight = this->m_camera->getHeight ();

    this->_rt_shadowAtlas =
        this->create ("_rt_shadowAtlas", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0,
                      {sceneWidth, sceneHeight}, {sceneWidth, sceneHeight});
    this->alias ("_alias_lightCookie", "_rt_shadowAtlas");

    // set clear color
    const glm::vec3 clearColor = scene->colors.clear->value->getVec3 ();

    glClearColor (clearColor.r, clearColor.g, clearColor.b, 1.0f);

    // create all objects based off their dependencies
    for (const auto& object : scene->objects)
        this->createObject (*object);

    // copy over objects by render order
    for (const auto& object : scene->objects) {
        this->addObjectToRenderOrder (*object);
    }

    // create extra framebuffers for the bloom effect
    this->_rt_4FrameBuffer =
        this->create ("_rt_4FrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0,
                      {sceneWidth / 4, sceneHeight / 4}, {sceneWidth / 4, sceneHeight / 4});
    this->_rt_8FrameBuffer =
        this->create ("_rt_8FrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0,
                      {sceneWidth / 8, sceneHeight / 8}, {sceneWidth / 8, sceneHeight / 8});
    this->_rt_Bloom = this->create ("_rt_Bloom", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
                                       1.0, {sceneWidth / 8, sceneHeight / 8}, {sceneWidth / 8, sceneHeight / 8});

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // this custom image loads some effect files from the virtual container to achieve the same bloom effect
    // this approach requires of two extra draw calls due to the way the effect works in official WPE
    // (it renders directly to the screen, whereas here we never do that from a scene)
    //

    const auto bloomOrigin = glm::vec3 { sceneWidth / 2, sceneHeight / 2, 0.0f };
    const auto bloomSize = glm::vec2 { sceneWidth, sceneHeight };

    const JSON bloom = {
        {"image", "models/wpenginelinux.json"},
        {"name", "bloomimagewpenginelinux"},
        {"visible", true},
        {"scale", "1.0 1.0 1.0"},
        {"angles", "0.0 0.0 0.0"},
        {"origin", std::to_string (bloomOrigin.x) + " " + std::to_string (bloomOrigin.y) + " " + std::to_string(bloomOrigin.z)},
        {"size", std::to_string (bloomSize.x) + " " + std::to_string (bloomSize.y)},
        {"id", -1},
        {"effects",
            JSON::array (
                {
                    {
                        {"file", "effects/wpenginelinux/bloomeffect.json"},
                        {"id", 15242000},
                        {"name", ""},
                        {"passes",
                             JSON::array (
                                 {
                                     {
                                         {"constantshadervalues",
                                             {
                                                 {"bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat ()},
                                                 {"bloomthreshold", this->getScene ().camera.bloom.threshold->value->getFloat ()}
                                             }
                                         }
                                     },
                                     {
                                         {"constantshadervalues",
                                            {
                                                {"bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat ()},
                                                {"bloomthreshold", this->getScene ().camera.bloom.threshold->value->getFloat ()}
                                            }
                                        }
                                     },
                                     {
                                         {"constantshadervalues",
                                             {
                                                 {"bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat ()},
                                                 {"bloomthreshold", this->getScene ().camera.bloom.threshold->value->getFloat ()}
                                             }
                                         }
                                     }
                                 }
                             )
                        }
                    }
                }
            )
        }
    };

    // create image for bloom passes
    if (scene->camera.bloom.enabled->value->getBool ()) {
        this->m_bloomObjectData = ObjectParser::parse (bloom, scene->project);
        this->m_bloomObject = this->createObject (*this->m_bloomObjectData);

        this->m_objectsByRenderOrder.push_back (this->m_bloomObject);
    }
}

Render::CObject* CScene::createObject (const Object& object) {
    Render::CObject* renderObject = nullptr;

    // ensure the item is not loaded already
    if (const auto current = this->m_objects.find (object.id); current != this->m_objects.end ())
        return current->second;

    // check dependencies too!
    for (const auto& cur : object.dependencies) {
        // self-dependency is a possibility...
        if (cur == object.id)
            continue;

        const auto dep = std::ranges::find_if (this->getScene ().objects,
            [&cur] (const auto& o) {
                return o->id == cur;
            }
        );

        if (dep != this->getScene ().objects.end ())
            this->createObject (**dep);
    }

    if (object.is<Image> ()) {
        auto* image = new Objects::CImage (*this, *object.as<Image> ());

        try {
            image->setup ();
        } catch (std::runtime_error&) {
            // this error message is already printed, so just show extra info about it
            sLog.error ("Cannot setup image ", image->getImage ().name);
        }

        renderObject = image;
    } else if (object.is<Sound> ()) {
        renderObject = new Objects::CSound (*this, *object.as<Sound> ());
    }

    if (renderObject != nullptr)
        this->m_objects.emplace (renderObject->getId (), renderObject);

    return renderObject;
}

void CScene::addObjectToRenderOrder (const Object& object) {
    const auto obj = this->m_objects.find (object.id);

    // ignores not created objects like particle systems
    if (obj == this->m_objects.end ())
        return;

    // take into account any dependency first
    for (const auto& dep : object.dependencies) {
        // self-dependency is possible
        if (dep == object.id) {
            continue;
        }

        // add the dependency to the list if it's created
        auto depIt = std::ranges::find_if (this->getScene ().objects,
            [&dep] (const auto& o) {
                return o->id == dep;
            }
        );

        if (depIt != this->getScene ().objects.end ()) {
            this->addObjectToRenderOrder (**depIt);
        } else {
            sLog.error ("Cannot find dependency ", dep, " for object ", object.id);
        }
    }

    // ensure we're added only once to the render list
    const auto renderIt = std::ranges::find_if (this->m_objectsByRenderOrder,
        [&object] (const auto& o) {
            return o->getId () == object.id;
        }
    );

    if (renderIt == this->m_objectsByRenderOrder.end ()) {
        this->m_objectsByRenderOrder.emplace_back (obj->second);
    }
}

Camera& CScene::getCamera () const {
    return *this->m_camera;
}

void CScene::renderFrame (const glm::ivec4& viewport) {
    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // update the parallax position if required
    if (this->getScene ().camera.parallax.enabled && !this->getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
        const float influence = this->getScene ().camera.parallax.mouseInfluence;
        const float amount = this->getScene ().camera.parallax.amount;
        const float delay =
            glm::min (static_cast<float> (this->getScene ().camera.parallax.delay), g_Time - g_TimeLast);

        this->m_parallaxDisplacement =
            glm::mix (this->m_parallaxDisplacement, (this->m_mousePosition * amount) * influence, delay);
    }

    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->m_sceneFBO->getRealWidth (), this->m_sceneFBO->getRealHeight ());

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& cur : this->m_objectsByRenderOrder)
        cur->render ();
}

void CScene::updateMouse (const glm::ivec4& viewport) {
    // update virtual mouse position first
    const glm::dvec2 position = this->getContext ().getInputContext ().getMouseInput ().position ();
    // TODO: PROPERLY TRANSLATE THESE TO WHAT'S VISIBLE ON SCREEN (FOR BACKGROUNDS THAT DO NOT EXACTLY FIT ON SCREEN)

    // rollover the position to the last
    this->m_mousePositionLast = this->m_mousePosition;

    // calculate the current position of the mouse
    this->m_mousePosition.x = glm::clamp ((position.x - viewport.x) / viewport.z, 0.0, 1.0);
    this->m_mousePosition.y = glm::clamp ((position.y - viewport.y) / viewport.w, 0.0, 1.0);

    // screen-space positions have to be transposed to what the screen will actually show
}

const Scene& CScene::getScene () const {
    return *this->getWallpaperData ().as<Scene> ();
}

int CScene::getWidth () const {
    return this->m_camera->getWidth ();
}

int CScene::getHeight () const {
    return this->m_camera->getHeight ();
}

const glm::vec2* CScene::getMousePosition () const {
    return &this->m_mousePosition;
}

const glm::vec2* CScene::getMousePositionLast () const {
    return &this->m_mousePositionLast;
}

const glm::vec2* CScene::getParallaxDisplacement () const {
    return &this->m_parallaxDisplacement;
}

const std::vector<CObject*>& CScene::getObjectsByRenderOrder () const {
    return this->m_objectsByRenderOrder;
}
