#include "VolumetricLights.h"
#include "CScene.h"
#include "WallpaperEngine/Render/Depth.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Objects/CLight.h"
#include "WallpaperEngine/Render/Shaders/Shader.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;
using namespace WallpaperEngine::Data::Model;

namespace {
// Keep the stock shader's D3D screen-depth reconstruction intact. Only its
// raster position crosses into GL's bottom-left, [-1,1] clip convention.
GLuint compile (CScene& scene, const std::string& name, const ComboMap& combos, bool volume) {
    Shaders::Shader shader (scene.getAssetLocator (), name, combos, {}, {}, {}, {});
    std::string vertex = shader.vertex ();
    if (volume) {
        const auto main = vertex.find ("main", vertex.find ("void main"));
        vertex.replace (main, 4, "volumeMain");
        vertex += "\nvoid main() { volumeMain(); gl_Position.y = -gl_Position.y;";
        if (!scene.getCamera ().usesReverseDepth ())
            vertex += " gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;";
        vertex += " }\n";
    }
    std::string fragment = shader.fragment ();
    if (combos.contains ("POINTLIGHT") && combos.at ("SHADOW") != 0) {
        // Native volumetric sampling has no surface-light receiver bias.
        // Shader compatibility can wrap the initializer in an explicit float cast.
        const std::string sample = "float shadowSample =";
        const auto offset = fragment.find (sample);
        if (offset != std::string::npos) fragment.insert (offset,
            "#if !REVERSEDEPTH\nuvs.z = uvs.z * 0.5 + 0.5;\n#endif\n");
    }
    const auto [vert, frag] = Shaders::GLSLContext::get ().toGlsl (vertex, fragment);
    return scene.getContext ().getShaderProgramCache ().createProgram (vert, frag);
}

glm::mat4 nativeClip (bool reverse = false) {
    glm::mat4 matrix (1.0f);
    matrix[1][1] = -1.0f;
    matrix[2][2] = reverse ? 1.0f : 0.5f;
    matrix[3][2] = reverse ? 0.0f : 0.5f;
    return matrix;
}

void matrix (GLuint program, const char* name, const glm::mat4& value) {
    glUniformMatrix4fv (glGetUniformLocation (program, name), 1, GL_FALSE, glm::value_ptr (value));
}

void vector (GLuint program, const char* name, const glm::vec4& value) {
    glUniform4fv (glGetUniformLocation (program, name), 1, glm::value_ptr (value));
}
}

VolumetricLights::VolumetricLights (CScene& scene) : m_scene (scene) {
    WallpaperEngine::Data::Utils::ScopeGuard cleanup ([&] {
        glDeleteProgram (m_backProgram);
        glDeleteProgram (m_combineProgram);
        glDeleteTextures (1, &m_depthTexture);
        glDeleteSamplers (1, &m_sampler);
        glDeleteSamplers (1, &m_shadowSampler);
    });
    m_backProgram = compile (scene, "volumetricsback", {}, true);
    m_combineProgram = compile (scene, "passthrough", {}, false);
    glGenTextures (1, &m_depthTexture);
    glGenSamplers (1, &m_sampler);
    glSamplerParameteri (m_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri (m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri (m_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri (m_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri (m_sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glGenSamplers (1, &m_shadowSampler);
    glSamplerParameteri (m_shadowSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri (m_shadowSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri (m_shadowSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri (m_shadowSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameteri (m_shadowSampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri (m_shadowSampler, GL_TEXTURE_COMPARE_FUNC, m_scene.getCamera ().usesReverseDepth () ? GL_GREATER : GL_LEQUAL);
    const float farDepth = m_scene.getCamera ().usesReverseDepth () ? 0.0f : 1.0f;
    const float border[] = { farDepth, farDepth, farDepth, farDepth };
    glSamplerParameterfv (m_shadowSampler, GL_TEXTURE_BORDER_COLOR, border);
    resize ();
    cleanup.cancel ();
}

VolumetricLights::~VolumetricLights () {
    for (const auto& [key, program] : m_frontPrograms) glDeleteProgram (program);
    glDeleteProgram (m_backProgram);
    glDeleteProgram (m_combineProgram);
    glDeleteSamplers (1, &m_sampler);
    glDeleteSamplers (1, &m_shadowSampler);
    glDeleteTextures (1, &m_depthTexture);
    glDeleteFramebuffers (1, &m_depthFramebuffer);
    glDeleteVertexArrays (1, &m_vertexArray);
    glDeleteBuffers (1, &m_vertexBuffer);
}

void VolumetricLights::resize () {
    const auto size = glm::max (glm::uvec2 (m_scene.getFramebufferSize ()), glm::uvec2 (1));
    if (size == m_size) return;
    m_size = size;
    // Native QUALITY 3/4 uses quarter-size volume targets and no extra blur.
    const auto reduced = glm::max (size / 4u, glm::uvec2 (1));
    if (m_back) {
        m_back->resize (reduced.x, reduced.y);
        m_lightBuffer->resize (reduced.x, reduced.y);
    } else {
        m_back = std::make_shared<CFBO> ("_rt_volumetricsSingle", TextureFormat_ARGB8888,
            TextureFlags_ClampUVs, 1, reduced.x, reduced.y, reduced.x, reduced.y, false, true, 1, m_scene.getCamera ().usesReverseDepth ());
        m_lightBuffer = std::make_shared<CFBO> ("_rt_volumetricsLightBuffer", m_scene.getColorFormat (),
            TextureFlags_ClampUVs, 1, reduced.x, reduced.y, reduced.x, reduced.y);
    }
    // A depth blit requires exactly the source attachment's storage format.
    glBindTexture (GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D (GL_TEXTURE_2D, 0,
        m_scene.getFBO ()->getDepthInternalFormat (),
        size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}

GLuint VolumetricLights::frontProgram (bool shadow, bool fullscreen, bool cookie, bool point) {
    const auto& fog = m_scene.getFog ();
    const int key = shadow | (fullscreen << 1) | (fog.distanceEnabled << 2) | (fog.heightEnabled << 3)
        | (cookie << 4) | (point << 5);
    if (const auto found = m_frontPrograms.find (key); found != m_frontPrograms.end ()) return found->second;
    ComboMap combos {
        { "SHADOW", shadow }, { "FULLSCREEN", fullscreen }, { "QUALITY", 4 },
        { "FOG", 1 }, { "FOG_DIST", fog.distanceEnabled }, { "FOG_HEIGHT", fog.heightEnabled }
    };
    combos.emplace ("REVERSEDEPTH", m_scene.getCamera ().usesReverseDepth () ? 1 : 0);
    // The stock sampler declaration uses #ifdef COOKIE, so omit disabled keys.
    if (cookie) combos.emplace ("COOKIE", 1);
    if (point) {
        combos.emplace ("POINTLIGHT", 1);
        // Use the same atlas compensation as the existing point surface lights.
        combos.emplace ("LIGHTS_SHADOW_MAPPING_QUALITY", 2);
    }
    return m_frontPrograms[key] = compile (m_scene, "volumetricsfront", combos, true);
}

void VolumetricLights::draw (GLuint program, bool fullscreen, bool cookie, bool point) {
    const GLint position = glGetAttribLocation (program, "a_Position");
    const GLint uv = glGetAttribLocation (program, "a_TexCoord");
    glBindBuffer (GL_ARRAY_BUFFER, m_vertexBuffer);
    glEnableVertexAttribArray (position);
    glVertexAttribPointer (position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof (float), nullptr);
    if (uv >= 0) {
        glEnableVertexAttribArray (uv);
        glVertexAttribPointer (uv, 2, GL_FLOAT, GL_FALSE, 5 * sizeof (float), reinterpret_cast<void*> (3 * sizeof (float)));
    }
    glDrawArrays (GL_TRIANGLES, fullscreen ? 420 : point ? 423 : cookie ? 384 : 0,
        fullscreen ? 3 : point ? 3312 : cookie ? 36 : 384);
    glDisableVertexAttribArray (position);
    if (uv >= 0) glDisableVertexAttribArray (uv);
}

void VolumetricLights::render (
    const std::vector<Objects::CLight*>& lightObjects, const std::vector<Objects::CLight*>& batch
) {
    const ClipDepthScope clipDepth (m_scene.getCamera ().usesReverseDepth ());
    resize ();
    const auto target = m_scene.find ("_rt_FullFrameBuffer");
    if (m_vertexArray == GL_NONE) {
        // Native 140196ce0: 32 circular segments in light clip space, closed at
        // z=0/1 and transformed by the inverse spotlight projection.
        std::vector<float> vertices;
        const auto vertex = [&] (glm::vec3 p, glm::vec2 uv = {}) {
            vertices.insert (vertices.end (), { p.x, p.y, p.z, uv.x, uv.y });
        };
        for (int i = 0; i < 32; ++i) {
            const float a = float (i) * glm::two_pi<float> () / 32.0f;
            const float b = float (i + 1) * glm::two_pi<float> () / 32.0f;
            const glm::vec3 p (std::sin (a), -std::cos (a), 0), q (std::sin (b), -std::cos (b), 0);
            const auto r = p + glm::vec3 (0, 0, 1), s = q + glm::vec3 (0, 0, 1);
            vertex (r); vertex (p); vertex (q); vertex (r); vertex (q); vertex (s);
            vertex ({ .5f, .5f, 0 }); vertex (q); vertex (p);
            vertex ({ .5f, .5f, 1 }); vertex (r); vertex (s);
        }
        // Cookie lights fill a square frustum, including corners outside the
        // circular cone used by ordinary spotlights (native 140196ce0).
        const glm::vec3 corners[] = { {-1,-1,0}, {1,-1,0}, {1,1,0}, {-1,1,0} };
        for (int i = 0; i < 4; ++i) {
            const auto p = corners[i], q = corners[(i + 1) % 4];
            const auto r = p + glm::vec3 (0, 0, 1), s = q + glm::vec3 (0, 0, 1);
            vertex (r); vertex (p); vertex (q); vertex (r); vertex (q); vertex (s);
        }
        vertex (corners[0]); vertex (corners[2]); vertex (corners[1]);
        vertex (corners[0]); vertex (corners[3]); vertex (corners[2]);
        for (int index : {0,1,2,0,2,3}) vertex (corners[index] + glm::vec3 (0, 0, 1));
        vertex ({ -1,-1,0 }, { 0,0 }); vertex ({ 3,-1,0 }, { 2,0 }); vertex ({ -1,3,0 }, { 0,2 });
        // Native 14025c660: 24 longitude segments, 23 latitude rings and two
        // poles. Point volumes scale this unit sphere by their authored radius.
        const auto sphere = [] (int latitude, int longitude) {
            const float a = float (latitude) * (glm::pi<float> () / 24.0f);
            const float b = float (longitude) * (glm::two_pi<float> () / 24.0f);
            return glm::vec3 (std::cos (b) * std::sin (a), std::cos (a), std::sin (b) * std::sin (a));
        };
        for (int latitude = 1; latitude < 23; ++latitude) {
            for (int longitude = 0; longitude < 24; ++longitude) {
                const auto p = sphere (latitude, longitude), q = sphere (latitude, longitude + 1);
                const auto r = sphere (latitude + 1, longitude), s = sphere (latitude + 1, longitude + 1);
                vertex (p); vertex (q); vertex (r); vertex (q); vertex (s); vertex (r);
            }
        }
        for (int longitude = 0; longitude < 24; ++longitude) {
            vertex ({ 0,1,0 }); vertex (sphere (1, longitude + 1)); vertex (sphere (1, longitude));
            vertex ({ 0,-1,0 }); vertex (sphere (23, longitude)); vertex (sphere (23, longitude + 1));
        }
        glGenVertexArrays (1, &m_vertexArray);
        glGenBuffers (1, &m_vertexBuffer);
        glBindBuffer (GL_ARRAY_BUFFER, m_vertexBuffer);
        glBufferData (GL_ARRAY_BUFFER, vertices.size () * sizeof (float), vertices.data (), GL_STATIC_DRAW);
        glGenFramebuffers (1, &m_depthFramebuffer);
        glBindFramebuffer (GL_FRAMEBUFFER, m_depthFramebuffer);
        glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
        glDrawBuffer (GL_NONE);
        glReadBuffer (GL_NONE);
    }
    GLint samplers[4] {}, previousFront, previousArray;
    glGetIntegerv (GL_FRONT_FACE, &previousFront);
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &previousArray);
    for (GLuint unit = 0; unit < 4; ++unit) {
        glGetIntegeri_v (GL_SAMPLER_BINDING, unit, &samplers[unit]);
        glBindSampler (unit, unit == 0 ? m_shadowSampler : m_sampler);
    }
    WallpaperEngine::Data::Utils::ScopeGuard restore ([&] {
        for (GLuint unit = 0; unit < 4; ++unit) glBindSampler (unit, samplers[unit]);
        glFrontFace (previousFront);
        glBindVertexArray (previousArray);
        glActiveTexture (GL_TEXTURE0);
        glBindFramebuffer (GL_FRAMEBUFFER, target->getDrawFramebuffer ());
        glViewport (0, 0, m_size.x, m_size.y);
    });
    glDisable (GL_SCISSOR_TEST);
    glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask (GL_TRUE);
    glBindFramebuffer (GL_READ_FRAMEBUFFER, target->getDrawFramebuffer ());
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_depthFramebuffer);
    glBlitFramebuffer (0, 0, m_size.x, m_size.y, 0, 0, m_size.x, m_size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindVertexArray (m_vertexArray);
    glBindFramebuffer (GL_FRAMEBUFFER, m_lightBuffer->getFramebuffer ());
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    const float black[] = { 0, 0, 0, 0 };
    glClearBufferfv (GL_COLOR, 0, black);
    const auto& camera = m_scene.getCamera ();
    const auto viewProjection = nativeClip (camera.usesReverseDepth ()) * camera.getRasterProjection () * camera.getLookAt ();
    const auto& lights = m_scene.getLights ();
    const double time = m_scene.getElapsedTime ();
    for (const auto* light : lightObjects) {
        const auto& data = light->getLight ();
        const bool point = data.type == LightData::Type_Point;
        if (!point && data.type != LightData::Type_Spot) continue;
        if (std::ranges::find (batch, light) == batch.end ()) continue;
        if (!data.castVolumetrics->value->getBool () || data.density->evaluateFloat (time) <= 0
            || glm::length (light->getPremultipliedColor ()) <= 0) continue;
        const auto origin = light->getWorldPosition (), direction = light->getWorldDirection ();
        const float radius = data.radius->evaluateFloat (time);
        if (radius <= 0) continue;
        const bool cookie = !point && data.useCookie;
        glm::mat4 lightProjection (1.0f), volumeTransform (1.0f);
        if (point) {
            volumeTransform[0][0] = volumeTransform[1][1] = volumeTransform[2][2] = radius;
            volumeTransform[3] = glm::vec4 (origin, 1.0f);
        } else {
            lightProjection = nativeClip () * (cookie ? light->getCookieProjection ()
                : Objects::CLight::calculateSpotShadowViewProjection (
                    origin, direction, data.outerCone->evaluateFloat (time), radius));
            volumeTransform = glm::inverse (lightProjection);
        }
        glViewport (0, 0, m_back->getRealWidth (), m_back->getRealHeight ());
        glBindFramebuffer (GL_FRAMEBUFFER, m_back->getFramebuffer ());
        glDepthMask (GL_TRUE);
        glClearDepth (m_back->getFarDepth ());
        glClear (GL_DEPTH_BUFFER_BIT);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (m_back->usesReverseDepth () ? GL_GREATER : GL_LESS);
        glEnable (GL_CULL_FACE);
        glFrontFace (camera.isYFlipped () ? GL_CW : GL_CCW);
        glCullFace (GL_FRONT);
        glDisable (GL_BLEND);
        glUseProgram (m_backProgram);
        matrix (m_backProgram, "g_ViewProjectionMatrix", viewProjection);
        matrix (m_backProgram, "g_AltViewProjectionMatrix", volumeTransform);
        draw (m_backProgram, false, cookie, point);
        // Native tests containment at a fixed distance along the view ray.
        const auto cameraPoint = camera.getEye () + glm::normalize (camera.getCenter () - camera.getEye ())
            * (cookie ? 0.1f : 0.2f);
        const auto delta = cameraPoint - origin;
        const float axial = glm::dot (delta, direction);
        const glm::vec4 clipPoint = lightProjection * glm::vec4 (cameraPoint, 1.0f);
        const bool inside = point ? glm::dot (delta, delta) < radius * radius
            : cookie ? clipPoint.w > 0 && std::abs (clipPoint.x) <= clipPoint.w
            && std::abs (clipPoint.y) <= clipPoint.w && clipPoint.z >= 0 && clipPoint.z <= clipPoint.w
            : axial >= 0 && axial <= radius
            && glm::length (delta - direction * axial) <= axial * std::tan (glm::radians (data.outerCone->evaluateFloat (time)));
        // Atlas slots follow authored light identity through removals.
        const auto slot = m_scene.getAuthoredLightSlot (light->getId ());
        const int feature = !point && slot ? lights.spotShadowFeatures[*slot] : -1;
        const bool shadow = slot && (point ? lights.pointShadowEnabled[*slot] > .5f
            : feature >= 0 && lights.spotShadowEnabled[*slot] > .5f);
        const GLuint program = frontProgram (shadow, inside, cookie, point);
        glBindFramebuffer (GL_FRAMEBUFFER, m_lightBuffer->getFramebuffer ());
        glDisable (GL_DEPTH_TEST);
        glDepthMask (GL_FALSE);
        glCullFace (GL_BACK);
        if (inside) glDisable (GL_CULL_FACE);
        glEnable (GL_BLEND);
        glBlendEquation (GL_FUNC_ADD);
        glBlendFuncSeparate (GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
        glUseProgram (program);
        matrix (program, "g_ViewProjectionMatrix", viewProjection);
        matrix (program, "g_AltViewProjectionMatrix", volumeTransform);
        matrix (program, "g_EffectModelMatrix", glm::inverse (viewProjection));
        auto samplingProjection = shadow && !point ? nativeClip (camera.usesReverseDepth ()) * lights.shadowMatrices[feature] : lightProjection;
        // Shadow and cookie sampling retain native texture Y; raster reconstruction does not.
        if ((shadow && !point) || cookie) for (int column = 0; column < 4; ++column) samplingProjection[column][1] *= -1;
        matrix (program, "g_AltModelMatrix", samplingProjection);
        vector (program, "g_RenderVar0", shadow
            ? (point ? lights.pointShadowTransforms[*slot] : lights.shadowTransforms[feature]) : glm::vec4 (0));
        const auto cones = Objects::CLight::calculateSpotConeCosines (data.innerCone->evaluateFloat (time), data.outerCone->evaluateFloat (time));
        vector (program, "g_RenderVar1", { radius * .99f, cones.x, cones.y, data.intensity->evaluateFloat (time) });
        vector (program, "g_RenderVar2", glm::vec4 (origin, data.density->evaluateFloat (time)));
        vector (program, "g_RenderVar3", point
            ? (slot ? lights.pointShadowProjections[*slot] : glm::vec4 (0))
            : glm::vec4 (light->getWorldForwardAxis (), 0));
        vector (program, "g_RenderVar4", glm::vec4 (data.color->evaluateVec3 (time), data.volumetricsExponent->evaluateFloat (time)));
        vector (program, "g_Texture1Resolution", { m_size.x, m_size.y, m_size.x, m_size.y });
        vector (program, "g_Texture3Resolution", *m_back->getResolution ());
        vector (program, "g_FogDistanceParams", m_scene.getFog ().distanceParams);
        vector (program, "g_FogHeightParams", m_scene.getFog ().heightParams);
        glUniform3fv (glGetUniformLocation (program, "g_EyePosition"), 1, glm::value_ptr (camera.getEye ()));
        // Native clips against the copied scene depth before limiting the ray
        // to the cone's back face; swapping these makes foreground beams leak.
        const GLuint textures[] = { m_scene.find ("_rt_shadowAtlas")->getTextureID (0), m_depthTexture, 0, m_back->getTextureID (0) };
        for (GLuint unit : { 0u, 1u, 3u }) {
            glActiveTexture (GL_TEXTURE0 + unit);
            glBindTexture (GL_TEXTURE_2D, textures[unit]);
            glUniform1i (glGetUniformLocation (program, ("g_Texture" + std::to_string (unit)).c_str ()), unit);
        }
        if (cookie) {
            if (light->getCookie ()) m_scene.markTextureUsed (*light->getCookie ());
            glActiveTexture (GL_TEXTURE2);
            glBindTexture (GL_TEXTURE_2D, light->getCookie () ? light->getCookie ()->getTextureID (0) : 0);
            // Volumes use each light's own provider and its ordinary sampler,
            // not the shared surface alias's border override.
            glBindSampler (2, 0);
            glUniform1i (glGetUniformLocation (program, "g_Texture2"), 2);
        }
        draw (program, inside, cookie, point);
    }
    glBindFramebuffer (GL_FRAMEBUFFER, target->getDrawFramebuffer ());
    glViewport (0, 0, m_size.x, m_size.y);
    glDisable (GL_CULL_FACE);
    glDisable (GL_DEPTH_TEST);
    glEnable (GL_BLEND);
    glBlendEquation (GL_FUNC_ADD);
    glBlendFuncSeparate (GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
    glBindSampler (0, m_sampler);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_lightBuffer->getTextureID (0));
    const ClipDepthScope combineDepth (false);
    glUseProgram (m_combineProgram);
    glUniform1i (glGetUniformLocation (m_combineProgram, "g_Texture0"), 0);
    draw (m_combineProgram, true);
}
