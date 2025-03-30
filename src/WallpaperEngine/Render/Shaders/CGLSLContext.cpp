#include "CGLSLContext.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cassert>
#include <memory>

#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_glsl.hpp>

using namespace WallpaperEngine::Render::Shaders;

TBuiltInResource BuiltInResource = {
    .maxLights = 32,
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
        .nonInductiveForLoops = 1,
        .whileLoops = 1,
        .doWhileLoops = 1,
        .generalUniformIndexing = 1,
        .generalAttributeMatrixVectorIndexing = 1,
        .generalVaryingIndexing = 1,
        .generalSamplerIndexing = 1,
        .generalVariableIndexing = 1,
        .generalConstantMatrixVectorIndexing = 1,
    }
};

CGLSLContext::CGLSLContext () {
    assert (this->sInstance == nullptr);

    glslang::InitializeProcess();
}

CGLSLContext::~CGLSLContext () {
    glslang::FinalizeProcess();
}

CGLSLContext& CGLSLContext::get () {
    if (sInstance == nullptr)
        sInstance = std::make_shared<CGLSLContext> ();

    return *sInstance;
}

std::string CGLSLContext::toGlsl (const std::string& content, ShaderType type) {
    if (type == ShaderType_Include) {
        sLog.error ("Include shaders cannot be converted as they should be part of a bigger shader");
        return "";
    }

    EShLanguage shaderType = type == ShaderType_Vertex ? EShLangVertex : EShLangFragment;
    glslang::TShader shader (shaderType);

    const char* shaderSource = content.c_str();
    shader.setStrings(&shaderSource, 1);
    shader.setEntryPoint("main");
    // TODO: USE HLSL IF GLSL-TO-GLSL DOESN'T WORK
    shader.setEnvInput(glslang::EShSourceGlsl, shaderType, glslang::EShClientOpenGL, 330);
    shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    shader.setAutoMapLocations (true);
    shader.setAutoMapBindings (true);

    if (!shader.parse (&BuiltInResource, 100, false, EShMsgDefault)) {
        sLog.error ("GLSL Parsing Failed: %s", shader.getInfoLog());
        return "";
    }

    glslang::TProgram program;
    program.addShader (&shader);

    if (!program.link (EShMsgDefault)) {
        sLog.error ("Program Linking Failed: %s", program.getInfoLog());
        return "";
    }

    std::vector<uint32_t> spirv;
    glslang::GlslangToSpv (*program.getIntermediate (shaderType), spirv);

    spirv_cross::CompilerGLSL compiler(spirv);
    spirv_cross::CompilerGLSL::Options options;
    options.version = 450; // OpenGL 4.5 / Vulkan GLSL TODO: MAYBE THIS CAN BE CHANGED?
    options.es = false;
    compiler.set_common_options(options);

    return compiler.compile();
}

std::shared_ptr<CGLSLContext> CGLSLContext::sInstance = nullptr;