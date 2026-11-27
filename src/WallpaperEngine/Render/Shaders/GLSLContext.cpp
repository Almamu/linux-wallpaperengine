#include "GLSLContext.h"
#include "ShaderDiskCache.h"
#include "WallpaperEngine/Debug/RenderHealth.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cassert>
#include <memory>
#include <string_view>

#include "SPIRV/GlslangToSpv.h"
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "spirv_glsl.hpp"

using namespace WallpaperEngine::Render::Shaders;

namespace {
constexpr size_t GLSL_CACHE_BUDGET_BYTES = 64ULL * 1024 * 1024;
}

TBuiltInResource BuiltInResource = { .maxLights = 32,
				     .maxClipPlanes = 6,
				     .maxTextureUnits = 32,
				     .maxTextureCoords = 32,
				     .maxVertexAttribs = 64,
				     .maxVertexUniformComponents = 4096,
				     .maxVaryingFloats = 64,
				     .maxVertexTextureImageUnits = 32,
				     .maxCombinedTextureImageUnits = 80,
				     .maxTextureImageUnits = 32,
				     .maxFragmentUniformComponents = 4096,
				     .maxDrawBuffers = 32,
				     .maxVertexUniformVectors = 128,
				     .maxVaryingVectors = 8,
				     .maxFragmentUniformVectors = 16,
				     .maxVertexOutputVectors = 16,
				     .maxFragmentInputVectors = 15,
				     .minProgramTexelOffset = -8,
				     .maxProgramTexelOffset = 7,
				     .maxClipDistances = 8,
				     .maxComputeWorkGroupCountX = 65535,
				     .maxComputeWorkGroupCountY = 65535,
				     .maxComputeWorkGroupCountZ = 65535,
				     .maxComputeWorkGroupSizeX = 1024,
				     .maxComputeWorkGroupSizeY = 1024,
				     .maxComputeWorkGroupSizeZ = 64,
				     .maxComputeUniformComponents = 1024,
				     .maxComputeTextureImageUnits = 16,
				     .maxComputeImageUniforms = 8,
				     .maxComputeAtomicCounters = 8,
				     .maxComputeAtomicCounterBuffers = 1,
				     .maxVaryingComponents = 60,
				     .maxVertexOutputComponents = 64,
				     .maxGeometryInputComponents = 64,
				     .maxGeometryOutputComponents = 128,
				     .maxFragmentInputComponents = 128,
				     .maxImageUnits = 8,
				     .maxCombinedImageUnitsAndFragmentOutputs = 8,
				     .maxCombinedShaderOutputResources = 8,
				     .maxImageSamples = 0,
				     .maxVertexImageUniforms = 0,
				     .maxTessControlImageUniforms = 0,
				     .maxTessEvaluationImageUniforms = 0,
				     .maxGeometryImageUniforms = 0,
				     .maxFragmentImageUniforms = 8,
				     .maxCombinedImageUniforms = 8,
				     .maxGeometryTextureImageUnits = 16,
				     .maxGeometryOutputVertices = 256,
				     .maxGeometryTotalOutputComponents = 1024,
				     .maxGeometryUniformComponents = 1024,
				     .maxGeometryVaryingComponents = 64,
				     .maxTessControlInputComponents = 128,
				     .maxTessControlOutputComponents = 128,
				     .maxTessControlTextureImageUnits = 16,
				     .maxTessControlUniformComponents = 1024,
				     .maxTessControlTotalOutputComponents = 4096,
				     .maxTessEvaluationInputComponents = 128,
				     .maxTessEvaluationOutputComponents = 128,
				     .maxTessEvaluationTextureImageUnits = 16,
				     .maxTessEvaluationUniformComponents = 1024,
				     .maxTessPatchComponents = 120,
				     .maxPatchVertices = 32,
				     .maxTessGenLevel = 64,
				     .maxViewports = 16,
				     .maxVertexAtomicCounters = 0,
				     .maxTessControlAtomicCounters = 0,
				     .maxTessEvaluationAtomicCounters = 0,
				     .maxGeometryAtomicCounters = 0,
				     .maxFragmentAtomicCounters = 8,
				     .maxCombinedAtomicCounters = 8,
				     .maxAtomicCounterBindings = 1,
				     .maxVertexAtomicCounterBuffers = 0,
				     .maxTessControlAtomicCounterBuffers = 0,
				     .maxTessEvaluationAtomicCounterBuffers = 0,
				     .maxGeometryAtomicCounterBuffers = 0,
				     .maxFragmentAtomicCounterBuffers = 1,
				     .maxCombinedAtomicCounterBuffers = 1,
				     .maxAtomicCounterBufferSize = 16384,
				     .maxTransformFeedbackBuffers = 4,
				     .maxTransformFeedbackInterleavedComponents = 64,
				     .maxCullDistances = 8,
				     .maxCombinedClipAndCullDistances = 8,
				     .maxSamples = 4,
				     .maxMeshOutputVerticesNV = 256,
				     .maxMeshOutputPrimitivesNV = 512,
				     .maxMeshWorkGroupSizeX_NV = 32,
				     .maxMeshWorkGroupSizeY_NV = 1,
				     .maxMeshWorkGroupSizeZ_NV = 1,
				     .maxTaskWorkGroupSizeX_NV = 32,
				     .maxTaskWorkGroupSizeY_NV = 1,
				     .maxTaskWorkGroupSizeZ_NV = 1,
				     .maxMeshViewCountNV = 4,
				     .limits = {
					 .nonInductiveForLoops = true,
					 .whileLoops = true,
					 .doWhileLoops = true,
					 .generalUniformIndexing = true,
					 .generalAttributeMatrixVectorIndexing = true,
					 .generalVaryingIndexing = true,
					 .generalSamplerIndexing = true,
					 .generalVariableIndexing = true,
					 .generalConstantMatrixVectorIndexing = true,
				     } };

namespace {
std::unique_ptr<glslang::TShader> parseTranslationStage (
    const EShLanguage stage, const char* const* source, bool& loweredScalarSwizzle
) {
    const auto makeShader = [&] {
        auto shader = std::make_unique<glslang::TShader> (stage);
        shader->setStrings (source, 1);
        shader->setEntryPoint ("main");
        shader->setEnvInput (glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, 330);
        shader->setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        shader->setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
        shader->setAutoMapLocations (true);
        shader->setAutoMapBindings (true);
        return shader;
    };
    auto shader = makeShader ();
    if (shader->parse (&BuiltInResource, 100, false, EShMsgDefault)) return shader;

    const std::string_view diagnostics = shader->getInfoLog ();
    constexpr std::string_view scalarSwizzleUnsupported =
        "'scalar swizzle' : not supported for this version or the enabled extensions";
    constexpr std::string_view wrappedSwizzle = "vector swizzle selection out of range";
    if (diagnostics.find (scalarSwizzleUnsupported) != std::string_view::npos
        && diagnostics.find (wrappedSwizzle) == std::string_view::npos) {
        auto retry = makeShader ();
        retry->setPreamble ("#extension GL_ARB_shading_language_420pack : enable\n");
        // The bundled compiler warns and wraps out-of-range selectors, including
        // scalar .yyyy. A successful retry must not rely on that recovery.
        if (retry->parse (&BuiltInResource, 100, false, EShMsgDefault)
            && std::string_view (retry->getInfoLog ()).find (wrappedSwizzle) == std::string_view::npos) {
            loweredScalarSwizzle = true;
            return retry;
        }
    }

    sLog.error (stage == EShLangVertex ? "GLSL vertex unit parsing Failed: " : "GLSL fragment unit parsing Failed: ",
                diagnostics);
    return {};
}
} // namespace

GLSLContext::GLSLContext () {
    assert (this->sInstance == nullptr);

    glslang::InitializeProcess ();
}

GLSLContext::~GLSLContext () { glslang::FinalizeProcess (); }

GLSLContext& GLSLContext::get () {
    if (sInstance == nullptr) {
	sInstance = std::make_unique<GLSLContext> ();
    }

    return *sInstance;
}

std::string GLSLContext::preprocess (const UnitType type, const std::string& source) {
    const EShLanguage stage = type == UnitType_Vertex ? EShLangVertex : EShLangFragment;
    glslang::TShader shader (stage);
    const char* text = source.c_str ();
    shader.setStrings (&text, 1);
    shader.setEnvInput (glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, 330);
    shader.setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    shader.setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    glslang::TShader::ForbidIncluder includer;
    std::string result;
    if (!shader.preprocess (&BuiltInResource, 100, ENoProfile, false, false, EShMsgDefault, &result, includer)) {
	sLog.exception ("Cannot preprocess shader macros: ", shader.getInfoLog ());
    }
    return result;
}

std::pair<std::string, std::string> GLSLContext::toGlsl (const std::string& vertex, const std::string& fragment) {
    // the translation below is a pure function of (vertex, fragment), so the whole
    // glslang -> SPIR-V -> SPIRV-Cross round trip is memoized; failed translations are
    // not cached so repeated attempts keep logging their cause
    std::string cacheKey;
    cacheKey.reserve (vertex.size () + fragment.size () + 32);
    cacheKey.append (std::to_string (vertex.size ())).push_back (':');
    cacheKey.append (vertex);
    cacheKey.push_back ('\x1F');
    cacheKey.append (fragment);

    {
	const std::lock_guard<std::mutex> lock (m_cacheMutex);
	const auto it = m_cache.find (cacheKey);

	if (it != m_cache.end ()) {
	    return it->second;
	}
    }

    std::pair<std::string, std::string> result;
    if (auto cached = ShaderDiskCache::get ().load ("glsl", cacheKey)) {
        Debug::RenderHealth::count ("shader.disk_glsl_hit");
        result = std::move (*cached);
    } else {
        Debug::RenderHealth::count ("shader.disk_glsl_miss");
	bool loweredScalarSwizzle = false;
	const char* vertexSource = vertex.c_str ();
	auto vertexShader = parseTranslationStage (EShLangVertex, &vertexSource, loweredScalarSwizzle);
	if (!vertexShader) return { "", "" };

	const char* fragmentSource = fragment.c_str ();
	auto fragmentShader = parseTranslationStage (EShLangFragment, &fragmentSource, loweredScalarSwizzle);
	if (!fragmentShader) return { "", "" };

	glslang::TProgram program;
	program.addShader (vertexShader.get ());
	program.addShader (fragmentShader.get ());

	if (!program.link (EShMsgDefault)) {
	    sLog.error ("Program Linking Failed: ", program.getInfoLog ());
	    return { "", "" };
	}

	std::vector<uint32_t> spirv;
	glslang::GlslangToSpv (*program.getIntermediate (EShLangVertex), spirv);

	spirv_cross::CompilerGLSL vertexCompiler (spirv);
	spirv_cross::CompilerGLSL::Options options;
	options.version = 330;
	options.es = false;
	// Sampler units are set by the renderer. Lowered scalar swizzles need
	// no 420pack feature in the GLSL 330 sent to the driver.
	options.enable_420pack_extension = !loweredScalarSwizzle;
	vertexCompiler.set_common_options (options);

	spirv.clear ();
	glslang::GlslangToSpv (*program.getIntermediate (EShLangFragment), spirv);

	spirv_cross::CompilerGLSL fragmentCompiler (spirv);
	options.version = 330;
	options.es = false;
	fragmentCompiler.set_common_options (options);

        result = { vertexCompiler.compile () + "#if 0\n" + vertex + "\n#endif",
                   fragmentCompiler.compile () + "#if 0\n" + fragment + "\n#endif" };

        ShaderDiskCache::get ().store ("glsl", cacheKey, result);
    }

    {
	const std::lock_guard<std::mutex> lock (m_cacheMutex);
	const size_t entryBytes = cacheKey.size () + result.first.size () + result.second.size ();
	if (entryBytes > GLSL_CACHE_BUDGET_BYTES) {
	    return result;
	}
	if (m_cacheBytes + entryBytes > GLSL_CACHE_BUDGET_BYTES) {
	    m_cache.clear ();
	    m_cacheBytes = 0;
	}
	if (m_cache.emplace (std::move (cacheKey), result).second) {
	    m_cacheBytes += entryBytes;
	}
    }

    return result;
}

std::unique_ptr<GLSLContext> GLSLContext::sInstance = nullptr;
