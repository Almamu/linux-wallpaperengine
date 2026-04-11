#include "GLSLContext.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cassert>
#include <memory>
#include <regex>

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

static bool tryParse (
    const std::string& source, EShLanguage stage, const TBuiltInResource& resources, glslang::TShader& shader
) {
    const char* src = source.c_str ();
    shader.setStrings (&src, 1);
    shader.setEntryPoint ("main");
    shader.setEnvInput (glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, 330);
    shader.setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    shader.setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    shader.setAutoMapLocations (true);
    shader.setAutoMapBindings (true);
    return shader.parse (&resources, 100, false, EShMsgDefault);
}

std::pair<std::string, std::string> GLSLContext::toGlsl (const std::string& vertex, const std::string& fragment) {
    constexpr int maxRetries = 8;

    // compile vertex shader with type-mismatch retry
    std::string vertexSource = vertex;
    std::unique_ptr<glslang::TShader> vertexShader;

    for (int attempt = 0; attempt <= maxRetries; attempt++) {
	vertexShader = std::make_unique<glslang::TShader> (EShLangVertex);

	if (tryParse (vertexSource, EShLangVertex, BuiltInResource, *vertexShader)) {
	    break;
	}

	std::string errorLog = vertexShader->getInfoLog ();

	if (attempt < maxRetries && fixVectorTypeMismatch (vertexSource, errorLog)) {
	    sLog.out ("Applied HLSL vector type fix to vertex shader (attempt ", attempt + 1, ")");
	    continue;
	}

	sLog.error ("GLSL vertex unit parsing Failed: ", errorLog);
	return { "", "" };
    }

    // compile fragment shader with type-mismatch retry
    std::string fragmentSource = fragment;
    std::unique_ptr<glslang::TShader> fragmentShader;

    for (int attempt = 0; attempt <= maxRetries; attempt++) {
	fragmentShader = std::make_unique<glslang::TShader> (EShLangFragment);

	if (tryParse (fragmentSource, EShLangFragment, BuiltInResource, *fragmentShader)) {
	    break;
	}

	std::string errorLog = fragmentShader->getInfoLog ();

	if (attempt < maxRetries && fixVectorTypeMismatch (fragmentSource, errorLog)) {
	    sLog.out ("Applied HLSL vector type fix to fragment shader (attempt ", attempt + 1, ")");
	    continue;
	}

	sLog.error ("GLSL fragment unit parsing Failed: ", errorLog);
	return { "", "" };
    }

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

bool GLSLContext::fixVectorTypeMismatch (std::string& source, const std::string& errorLog) {
    // look for: "wrong operand types" with "N-component vector" mismatches
    const auto wrongPos = errorLog.find ("wrong operand types");

    if (wrongPos == std::string::npos) {
	return false;
    }

    // extract line number: find "0:NNN:" before the error
    auto colonPos = errorLog.rfind (':', wrongPos);
    if (colonPos == std::string::npos) return false;
    colonPos = errorLog.rfind (':', colonPos - 1);
    if (colonPos == std::string::npos) return false;

    // find the line number between "0:" and the next ":"
    const auto lineNumStart = errorLog.rfind ("0:", colonPos) + 2;
    const auto lineNumEnd = errorLog.find (':', lineNumStart);
    if (lineNumEnd == std::string::npos) return false;

    // extract component counts from "N-component vector" patterns
    const auto firstComp = errorLog.find ("-component vector", wrongPos);
    if (firstComp == std::string::npos) return false;

    const auto secondComp = errorLog.find ("-component vector", firstComp + 17);
    if (secondComp == std::string::npos) return false;

    int lineNum, leftComponents, rightComponents;

    try {
	lineNum = std::stoi (errorLog.substr (lineNumStart, lineNumEnd - lineNumStart));
	leftComponents = errorLog[firstComp - 1] - '0';
	rightComponents = errorLog[secondComp - 1] - '0';
    } catch (...) {
	return false;
    }

    sLog.out ("fixVectorTypeMismatch: line=", lineNum, " left=vec", leftComponents, " right=vec", rightComponents);

    if (leftComponents == rightComponents) {
	return false;
    }

    // determine which operand needs truncation and to what size
    const int targetComponents = std::min (leftComponents, rightComponents);

    std::string swizzle;
    switch (targetComponents) {
	case 1: swizzle = ".x"; break;
	case 2: swizzle = ".xy"; break;
	case 3: swizzle = ".xyz"; break;
	default: return false;
    }

    // find the source line
    size_t lineStart = 0;

    for (int i = 1; i < lineNum; i++) {
	lineStart = source.find ('\n', lineStart);

	if (lineStart == std::string::npos) {
	    return false;
	}

	lineStart++;
    }

    size_t lineEnd = source.find ('\n', lineStart);

    if (lineEnd == std::string::npos) {
	lineEnd = source.length ();
    }

    std::string line = source.substr (lineStart, lineEnd - lineStart);

    // collect all variable names with their component counts from declarations before this line
    std::map<std::string, int> varComponents;
    static const std::regex declPattern (
	R"(\b(?:out|in|uniform|varying|attribute)\s+(vec(\d)|ivec(\d)|float|int|mat\d)\s+(\w+))"
    );

    for (auto it = std::sregex_iterator (source.begin (), source.begin () + lineStart, declPattern);
	 it != std::sregex_iterator (); ++it) {
	const std::string type = (*it)[1].str ();
	const std::string name = (*it)[4].str ();

	if (type.substr (0, 3) == "vec" || type.substr (0, 4) == "ivec") {
	    const int components = type.back () - '0';
	    varComponents[name] = components;
	} else if (type == "float" || type == "int") {
	    varComponents[name] = 1;
	}
    }

    // find the larger-component variable on the error line and add swizzle
    bool fixed = false;
    const int largerComponents = std::max (leftComponents, rightComponents);

    for (const auto& [name, components] : varComponents) {
	if (components != largerComponents) {
	    continue;
	}

	// find this variable name on the error line followed by a space or operator (not .xyz already)
	size_t varPos = 0;

	while ((varPos = line.find (name, varPos)) != std::string::npos) {
	    const size_t afterVar = varPos + name.length ();

	    // make sure it's a whole word match
	    if (varPos > 0 && (std::isalnum (line[varPos - 1]) || line[varPos - 1] == '_')) {
		varPos = afterVar;
		continue;
	    }

	    // skip if already has a swizzle
	    if (afterVar < line.length () && line[afterVar] == '.') {
		varPos = afterVar;
		continue;
	    }

	    // skip if followed by alphanumeric (part of another identifier)
	    if (afterVar < line.length () && (std::isalnum (line[afterVar]) || line[afterVar] == '_')) {
		varPos = afterVar;
		continue;
	    }

	    // insert swizzle
	    line.insert (afterVar, swizzle);
	    fixed = true;
	    break;
	}

	if (fixed) {
	    break;
	}
    }

    if (fixed) {
	source.replace (lineStart, lineEnd - lineStart, line);
	sLog.out ("Fixed vector type mismatch on line ", lineNum, ": added ", swizzle, " swizzle");
    }

    return fixed;
}

std::unique_ptr<GLSLContext> GLSLContext::sInstance = nullptr;