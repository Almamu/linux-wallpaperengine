#include "GLSLContext.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cassert>
#include <memory>

#include "SPIRV/GlslangToSpv.h"
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "spirv_glsl.hpp"

using namespace WallpaperEngine::Render::Shaders;

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

std::pair<std::string, std::string> GLSLContext::toGlsl (const std::string& vertex, const std::string& fragment) {
    glslang::TShader vertexShader (EShLangVertex);

    const char* vertexSource = vertex.c_str ();
    vertexShader.setStrings (&vertexSource, 1);
    vertexShader.setEntryPoint ("main");
    vertexShader.setEnvInput (glslang::EShSourceGlsl, EShLangVertex, glslang::EShClientOpenGL, 330);
    vertexShader.setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    vertexShader.setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    vertexShader.setAutoMapLocations (true);
    vertexShader.setAutoMapBindings (true);

    if (!vertexShader.parse (&BuiltInResource, 100, false, EShMsgDefault)) {
	sLog.error ("GLSL vertex unit parsing Failed: ", vertexShader.getInfoLog ());
	return { "", "" };
    }
    glslang::TShader fragmentShader (EShLangFragment);

    const char* fragmentSource = fragment.c_str ();
    fragmentShader.setStrings (&fragmentSource, 1);
    fragmentShader.setEntryPoint ("main");
    fragmentShader.setEnvInput (glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientOpenGL, 330);
    fragmentShader.setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    fragmentShader.setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    fragmentShader.setAutoMapLocations (true);
    fragmentShader.setAutoMapBindings (true);

    if (!fragmentShader.parse (&BuiltInResource, 100, false, EShMsgDefault)) {
	sLog.error ("GLSL fragment unit parsing Failed: ", fragmentShader.getInfoLog ());
	return { "", "" };
    }
    glslang::TProgram program;
    program.addShader (&vertexShader);
    program.addShader (&fragmentShader);

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
    vertexCompiler.set_common_options (options);

    spirv.clear ();
    glslang::GlslangToSpv (*program.getIntermediate (EShLangFragment), spirv);

    spirv_cross::CompilerGLSL fragmentCompiler (spirv);
    options.version = 330;
    options.es = false;
    fragmentCompiler.set_common_options (options);

    return { vertexCompiler.compile () + "#if 0\n" + vertex + "\n#endif",
	     fragmentCompiler.compile () + "#if 0\n" + fragment + "\n#endif" };
}

std::unique_ptr<GLSLContext> GLSLContext::sInstance = nullptr;