#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Data/Assets/Texture.h"
#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/Depth.h"
#include "WallpaperEngine/Render/CTexture.h"
#include "WallpaperEngine/Render/FBOProvider.h"
#include "WallpaperEngine/Render/Shaders/ShaderProgramCache.h"
#include "WallpaperEngine/Render/TextureCache.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

#ifdef CHECK
#undef CHECK
#endif
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using WallpaperEngine::Data::Assets::TextureFlags_ClampUVs;
using WallpaperEngine::Data::Assets::TextureFormat_ARGB8888;
using WallpaperEngine::Data::Assets::TextureFormat_DXT1;
using WallpaperEngine::Data::Assets::TextureFormat_DXT5;
using WallpaperEngine::Data::Assets::TextureFormat_RG88;
using WallpaperEngine::Data::Assets::TextureFormat_R8;
using WallpaperEngine::Data::Assets::TextureFormat_RGBA16161616f;
using WallpaperEngine::Render::CFBO;
using WallpaperEngine::Render::CTexture;
using WallpaperEngine::Render::TextureCache;
using WallpaperEngine::Render::Shaders::ShaderProgramCache;

namespace {
struct TextureGLContext {
    GLFWwindow* window = nullptr;
    TextureGLContext () {
	REQUIRE (glfwInit ());
	glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow (32, 32, "Framebuffer texture test", nullptr, nullptr);
	REQUIRE (window != nullptr);
	glfwMakeContextCurrent (window);
	glewExperimental = GL_TRUE;
	const GLenum result = glewInit ();
	REQUIRE ((result == GLEW_OK || result == GLEW_ERROR_NO_GLX_DISPLAY));
	while (glGetError () != GL_NO_ERROR) { }
    }
    ~TextureGLContext () {
	glfwDestroyWindow (window);
	glfwTerminate ();
    }
};
} // namespace

TEST_CASE ("TEX payload layout is detected independently from its authored format", "[texture]") {
    SECTION ("DXT5 block payload") {
	CHECK (CTexture::isBlockCompressedPayload (TextureFormat_DXT5, 512, 512, 512 * 512));
    }

    SECTION ("DXT5 expanded RGBA payload") {
	CHECK_FALSE (CTexture::isBlockCompressedPayload (TextureFormat_DXT5, 512, 512, 512 * 512 * 4));
    }

    SECTION ("DXT1 block payload rounds dimensions to complete blocks") {
	CHECK (CTexture::isBlockCompressedPayload (TextureFormat_DXT1, 5, 7, 2 * 2 * 8));
    }

    SECTION ("uncompressed authored formats never use block upload") {
	CHECK_FALSE (CTexture::isBlockCompressedPayload (TextureFormat_ARGB8888, 512, 512, 512 * 512 * 4));
	CHECK_FALSE (CTexture::isBlockCompressedPayload (TextureFormat_RG88, 512, 512, 512 * 512 * 2));
    }
}

TEST_CASE ("only unscoped runtime texture aliases are pinned", "[texture][cache]") {
    CHECK (TextureCache::isPinnedRuntimeTextureKey ("$mediaThumbnail"));
    CHECK (TextureCache::isPinnedRuntimeTextureKey ("$mediaPreviousThumbnail"));

    const std::string authoredKey
	= "$mediaThumbnail=$mediaThumbnail;/=/workshop/431960/2297432332;" + std::string (1, '\x1f')
	+ "materials/background.tex";
    CHECK_FALSE (TextureCache::isPinnedRuntimeTextureKey (authoredKey));
    CHECK_FALSE (TextureCache::isPinnedRuntimeTextureKey ("materials/background.tex"));
}

TEST_CASE ("only the dedicated reflection framebuffer generates mipmaps", "[texture][fbo]") {
    CHECK (CFBO::targetUsesMipmaps ("_rt_MipMappedFrameBuffer"));
    CHECK_FALSE (CFBO::targetUsesMipmaps ("_rt_FullFrameBuffer"));
    CHECK_FALSE (CFBO::targetUsesMipmaps ("_rt_imageLayerComposite_206_a"));
    CHECK_FALSE (CFBO::targetUsesMipmaps ("_rt_imageLayerComposite_206_b"));
}

TEST_CASE ("declared effect target samplers survive HDR allocation and resize", "[.][gl][texture][fbo][sampling]") {
    TextureGLContext context;
    WallpaperEngine::Render::FBOProvider provider (nullptr);
    provider.setBackbufferFormat (TextureFormat_RGBA16161616f);
    for (const bool repeat : { false, true }) {
	CAPTURE (repeat);
	const WallpaperEngine::Data::Model::FBO definition {
	    .name = "effect-target", .format = "rgba_backbuffer", .scale = 1.0f,
	    .unique = true, .repeat = repeat,
	};
	const auto target = provider.create (definition, glm::vec2 (9, 9));
	const auto texture = target->getTextureID (0);
	for (const uint32_t size : { 9, 13 }) {
	    if (size != 9) target->resize (size, size);
	    glBindTexture (GL_TEXTURE_2D, target->getTextureID (0));
	    CHECK (target->getTextureID (0) == texture);
	    for (const auto parameter : { GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER }) {
		GLint value = 0;
		glGetTexParameteriv (GL_TEXTURE_2D, parameter, &value);
		CHECK (value == GL_LINEAR);
	    }
	    for (const auto parameter : { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T }) {
		GLint value = 0;
		glGetTexParameteriv (GL_TEXTURE_2D, parameter, &value);
		CHECK (value == (repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
	    }
	    GLint format = 0;
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
	    CHECK (format == GL_RGBA16F);
	    CHECK (glGetError () == GL_NO_ERROR);
	}
    }
}

TEST_CASE ("RGBA16F framebuffers preserve HDR storage across resize and mipmap cleanup", "[.][gl][texture][fbo][hdr]") {
    TextureGLContext context;
    const size_t liveBefore = CFBO::getLiveGpuBytes ();
    {
	CFBO target (
	    "_rt_MipMappedFrameBuffer", TextureFormat_RGBA16161616f, TextureFlags_ClampUVs, 1.0f, 129, 65, 129,
	    65
	);
	const GLuint framebuffer = target.getFramebuffer ();
	const GLuint texture = target.getTextureID (0);

	auto checkStorage = [&] (const uint32_t width, const uint32_t height, const uint32_t levels,
			       const size_t expectedBytes) {
	    glBindTexture (GL_TEXTURE_2D, texture);
	    GLint internalFormat = 0;
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
	    CHECK (internalFormat == GL_RGBA16F);
	    CHECK (target.getMipMapCount (0) == levels);
	    CHECK (target.getApproximateGpuBytes () == expectedBytes);
	    CHECK (CFBO::getLiveGpuBytes () == liveBefore + expectedBytes);
	    GLint unusedWidth = -1;
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, levels, GL_TEXTURE_WIDTH, &unusedWidth);
	    CHECK (unusedWidth == 0);
	    CHECK (target.getTextureWidth (0) == width);
	    CHECK (target.getTextureHeight (0) == height);
	};

	auto uploadAndRead = [&] (const uint32_t width, const uint32_t height, const std::array<float, 4>& color) {
	    std::vector<float> upload (static_cast<size_t> (width) * height * 4);
	    for (size_t channel = 0; channel < upload.size (); channel += 4) {
		std::copy (color.begin (), color.end (), upload.begin () + channel);
	    }
	    glBindTexture (GL_TEXTURE_2D, texture);
	    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, upload.data ());
	    REQUIRE (glGetError () == GL_NO_ERROR);
	    target.generateMipmaps ();
	    REQUIRE (glGetError () == GL_NO_ERROR);
	    glBindTexture (GL_TEXTURE_2D, texture);
	    std::vector<float> readback (upload.size ());
	    glGetTexImage (GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, readback.data ());
	    REQUIRE (glGetError () == GL_NO_ERROR);
	    for (size_t channel = 0; channel < 4; ++channel) {
		CHECK (readback[channel] == Catch::Approx (color[channel]).margin (0.001f));
		CHECK (readback[readback.size () - 4 + channel] == Catch::Approx (color[channel]).margin (0.001f));
	    }
	    CHECK (readback[0] > 1.0f);
	};

	checkStorage (129, 65, 4, 8u * (129u * 65u + 64u * 32u + 32u * 16u + 16u * 8u));
	uploadAndRead (129, 65, { 2.5f, 1.5f, 0.25f, 1.0f });

	target.resize (17, 9);
	CHECK (target.getFramebuffer () == framebuffer);
	CHECK (target.getTextureID (0) == texture);
	checkStorage (17, 9, 1, 8u * 17u * 9u);
	uploadAndRead (17, 9, { 3.5f, 1.25f, 0.5f, 1.0f });

	target.resize (33, 17);
	checkStorage (33, 17, 2, 8u * (33u * 17u + 16u * 8u));
	uploadAndRead (33, 17, { 4.0f, 2.0f, 0.5f, 1.0f });
	glBindTexture (GL_TEXTURE_2D, texture);
	GLint mipFormat = 0;
	glGetTexLevelParameteriv (GL_TEXTURE_2D, 1, GL_TEXTURE_INTERNAL_FORMAT, &mipFormat);
	CHECK (mipFormat == GL_RGBA16F);
	std::vector<float> mipReadback (16u * 8u * 4u);
	glGetTexImage (GL_TEXTURE_2D, 1, GL_RGBA, GL_FLOAT, mipReadback.data ());
	CHECK (mipReadback[0] == Catch::Approx (4.0f).margin (0.001f));
	CHECK (mipReadback[0] > 1.0f);
	CHECK (glGetError () == GL_NO_ERROR);
    }
    CHECK (CFBO::getLiveGpuBytes () == liveBefore);
}

TEST_CASE ("RGBA16F multisample framebuffers preserve HDR values when resolved", "[.][gl][texture][fbo][hdr][msaa]") {
    TextureGLContext context;
    const size_t liveBefore = CFBO::getLiveGpuBytes ();

    CFBO unsupported (
	"hdr_msaa_unsupported", TextureFormat_RGBA16161616f, TextureFlags_ClampUVs, 1.0f, 2, 2, 2, 2, true,
	false, 3
    );
    REQUIRE (unsupported.getSamples () == 1);
    const GLuint fallbackFramebuffer = unsupported.getFramebuffer ();
    REQUIRE (unsupported.getDrawFramebuffer () == fallbackFramebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, fallbackFramebuffer);
    REQUIRE (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    CHECK (unsupported.getApproximateGpuBytes () == 2u * 2u * (8u + 4u));
    const GLfloat fallbackColor[] = { 2.0f, 0.5f, 0.25f, 1.0f };
    glClearBufferfv (GL_COLOR, 0, fallbackColor);
    std::array<float, 4> fallbackReadback {};
    glReadPixels (0, 0, 1, 1, GL_RGBA, GL_FLOAT, fallbackReadback.data ());
    CHECK (fallbackReadback[0] == Catch::Approx (2.0f).margin (0.001f));
    CHECK (fallbackReadback[0] > 1.0f);

    CFBO target (
	"hdr_msaa_behavior", TextureFormat_RGBA16161616f, TextureFlags_ClampUVs, 1.0f, 16, 16, 16, 16, true,
	false, 4
    );
    GLint maxSamples = 0;
    glGetIntegerv (GL_MAX_SAMPLES, &maxSamples);
    INFO ("GL_MAX_SAMPLES=" << maxSamples << " requested=4 actual=" << target.getSamples ());
    if (target.getSamples () == 1) SKIP ("RGBA16F plus DEPTH_COMPONENT32F does not support exact 4x MSAA");
    REQUIRE (target.getSamples () == 4);

    const GLuint drawFramebuffer = target.getDrawFramebuffer ();
    glBindFramebuffer (GL_FRAMEBUFFER, drawFramebuffer);
    REQUIRE (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    GLint colorRenderbuffer = GL_NONE;
    glGetFramebufferAttachmentParameteriv (
	GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colorRenderbuffer
    );
    REQUIRE (colorRenderbuffer != GL_NONE);
    glBindRenderbuffer (GL_RENDERBUFFER, colorRenderbuffer);
    GLint internalFormat = 0;
    glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internalFormat);
    CHECK (internalFormat == GL_RGBA16F);
    glBindRenderbuffer (GL_RENDERBUFFER, target.getDepthbuffer ());
    glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internalFormat);
    CHECK (internalFormat == GL_DEPTH_COMPONENT32F);
    CHECK (target.getApproximateGpuBytes () == 16u * 16u * (8u + target.getSamples () * (8u + 4u)));

    auto clearResolveAndRead = [&] (const std::array<float, 4>& color) {
	const GLuint draw = target.getDrawFramebuffer ();
	glBindFramebuffer (GL_FRAMEBUFFER, draw);
	glClearBufferfv (GL_COLOR, 0, color.data ());
	std::array<float, 4> readback {};
	glBindFramebuffer (GL_FRAMEBUFFER, target.getFramebuffer ());
	glReadPixels (0, 0, 1, 1, GL_RGBA, GL_FLOAT, readback.data ());
	for (size_t channel = 0; channel < readback.size (); ++channel) {
	    CHECK (readback[channel] == Catch::Approx (color[channel]).margin (0.001f));
	}
	CHECK (readback[0] > 1.0f);
    };

    clearResolveAndRead ({ 4.0f, 2.0f, 0.5f, 1.0f });
    const size_t liveBeforeResize = CFBO::getLiveGpuBytes ();
    const size_t bytesBeforeResize = target.getApproximateGpuBytes ();
    target.resize (9, 7);
    CHECK (target.getSamples () == 4);
    CHECK (target.getApproximateGpuBytes () == 9u * 7u * (8u + target.getSamples () * (8u + 4u)));
    CHECK (CFBO::getLiveGpuBytes () == liveBeforeResize - bytesBeforeResize + target.getApproximateGpuBytes ());
    clearResolveAndRead ({ 6.0f, 3.0f, 0.25f, 1.0f });
    CHECK (glGetError () == GL_NO_ERROR);
    CHECK (CFBO::getLiveGpuBytes () > liveBefore);
}

// Explicit opt-in, like the shader cache GPU tests: requires a graphics session.
TEST_CASE ("zero-sized framebuffer textures remain complete and sampleable", "[.][gl][texture][fbo]") {
    TextureGLContext context;
    ShaderProgramCache cache;
    const std::string vertex = "#version 330 core\nvoid main() {\n"
			       "vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
			       "gl_Position = vec4(p * 2.0 - 1.0, 0, 1); }\n";
    const ShaderProgramCache::SharedProgram colorProgram (cache.createProgram (
	vertex,
	"#version 330 core\nuniform sampler2D image;\nout vec4 color;\n"
	"void main() { color = texture(image, vec2((gl_FragCoord.x - 0.5) * 0.5)); }\n"
    ));
    const ShaderProgramCache::SharedProgram depthProgram (cache.createProgram (
	vertex,
	"#version 330 core\nuniform sampler2DShadow image;\nout vec4 color;\n"
	"void main() { color = vec4(texture(image, vec3(vec2((gl_FragCoord.x - 0.5) * 0.5), 0.5))); }\n"
    ));
    CFBO output ("fbo_size_test_output", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, 3, 1, 3, 1);
    GLuint vao = 0;
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glViewport (0, 0, 3, 1);

    struct Dimensions {
	uint32_t realWidth;
	uint32_t realHeight;
	uint32_t width;
	uint32_t height;
	uint32_t allocatedWidth;
	uint32_t allocatedHeight;
    };
    const Dimensions dimensions[] = {
	{ 64, 0, 64, 0, 64, 1 },
	{ 0, 64, 0, 64, 1, 64 },
	{ 0, 0, 0, 0, 1, 1 },
	// Ordinary padded textures must retain both their storage and logical sizes.
	{ 32, 16, 64, 32, 64, 32 },
    };
    enum class Attachment { Color, ColorAndDepth, DepthTexture };
    for (const Dimensions size : dimensions) {
	for (const Attachment attachment : { Attachment::Color, Attachment::ColorAndDepth, Attachment::DepthTexture }) {
	    const bool depthTexture = attachment == Attachment::DepthTexture;
	    const bool withDepth = attachment == Attachment::ColorAndDepth;
	    CAPTURE (size.width, size.height, withDepth, depthTexture);
	    CFBO source (
		"fbo_size_test_input", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, size.realWidth,
		size.realHeight, size.width, size.height, withDepth, depthTexture
	    );
	    CHECK (source.getTextureWidth (0) == size.allocatedWidth);
	    CHECK (source.getTextureHeight (0) == size.allocatedHeight);
	    CHECK (source.getRealWidth () == size.realWidth);
	    CHECK (source.getRealHeight () == size.realHeight);
	    REQUIRE (source.getFrames ().size () == 1);
	    CHECK (source.getFrames ()[0]->width1 == size.allocatedWidth);
	    CHECK (source.getFrames ()[0]->height1 == size.allocatedHeight);
	    CHECK (source.getFrames ()[0]->width2 == size.realWidth);
	    CHECK (source.getFrames ()[0]->height2 == size.realHeight);

	    // getTextureID must lazily create and initialize a usable attachment even
	    // when the authored dimensions describe an empty layer.
	    const GLuint texture = source.getTextureID (0);
	    glBindTexture (GL_TEXTURE_2D, texture);
	    GLint width = 0, height = 0;
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	    CHECK (width == static_cast<GLint> (size.allocatedWidth));
	    CHECK (height == static_cast<GLint> (size.allocatedHeight));
	    glBindFramebuffer (GL_FRAMEBUFFER, source.getFramebuffer ());
	    REQUIRE (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	    if (withDepth) {
		REQUIRE (source.getDepthbuffer () != GL_NONE);
		glBindRenderbuffer (GL_RENDERBUFFER, source.getDepthbuffer ());
		glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
		glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
		CHECK (width == static_cast<GLint> (size.allocatedWidth));
		CHECK (height == static_cast<GLint> (size.allocatedHeight));
	    }

	    glBindFramebuffer (GL_FRAMEBUFFER, output.getFramebuffer ());
	    glClearColor (1, 0, 1, 1);
	    glClear (GL_COLOR_BUFFER_BIT);
	    const GLuint program = depthTexture ? depthProgram.id : colorProgram.id;
	    glUseProgram (program);
	    const GLint sampler = glGetUniformLocation (program, "image");
	    REQUIRE (sampler >= 0);
	    glUniform1i (sampler, 0);
	    glActiveTexture (GL_TEXTURE0);
	    glBindTexture (GL_TEXTURE_2D, texture);
	    glDrawArrays (GL_TRIANGLES, 0, 3);
	    std::array<unsigned char, 12> pixels {};
	    glReadPixels (0, 0, 3, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
	    // Sample both edges and the center. A missing draw leaves magenta; an
	    // incomplete color texture returns opaque black, which also fails.
	    for (const unsigned char channel : pixels) {
		CHECK (channel == (depthTexture ? 255 : 0));
	    }
	    CHECK (glGetError () == GL_NO_ERROR);
	}
    }
    glUseProgram (0);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindVertexArray (0);
    glDeleteVertexArrays (1, &vao);
}

TEST_CASE ("reflection framebuffers allocate the native shortened mip chain", "[.][gl][texture][fbo][reflection]") {
    TextureGLContext context;
    struct Dimensions {
	uint32_t width;
	uint32_t height;
	uint32_t levels;
    };
    const Dimensions dimensions[] = { { 8, 8, 1 }, { 17, 17, 2 }, { 64, 32, 2 }, { 960, 540, 7 },
	{ 1920, 1080, 8 } };
    for (const auto [width, height, levels] : dimensions) {
	CAPTURE (width, height, levels);
	const size_t previousBytes = CFBO::getLiveGpuBytes ();
	{
	    CFBO target ("_rt_MipMappedFrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
		1.0f, width, height, width, height);
	    glBindTexture (GL_TEXTURE_2D, target.getTextureID (0));
	    CHECK (target.getMipMapCount (0) == levels);
	    GLint maxLevel = -1;
	    GLfloat maxLod = -1, anisotropy = -1;
	    glGetTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &maxLevel);
	    glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &maxLod);
	    glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, &anisotropy);
	    CHECK (maxLevel == static_cast<GLint> (levels - 1));
	    CHECK (maxLod == static_cast<GLfloat> (levels - 1));
	    CHECK (anisotropy == 1.0f);
	    size_t expectedBytes = 0;
	    for (uint32_t level = 0; level <= levels; ++level) {
		GLint allocatedWidth = -1, allocatedHeight = -1;
		glGetTexLevelParameteriv (GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &allocatedWidth);
		glGetTexLevelParameteriv (GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &allocatedHeight);
		if (level == levels) {
		    CHECK (allocatedWidth == 0);
		    CHECK (allocatedHeight == 0);
		    continue;
		}
		CHECK (allocatedWidth == static_cast<GLint> (width >> level));
		CHECK (allocatedHeight == static_cast<GLint> (height >> level));
		const size_t levelBytes = static_cast<size_t> (width >> level) * (height >> level) * 4;
		expectedBytes += levelBytes;
		std::vector<unsigned char> pixels (levelBytes, 255);
		glGetTexImage (GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
		CHECK (std::ranges::all_of (pixels, [] (unsigned char channel) { return channel == 0; }));
	    }
	    CHECK (target.getApproximateGpuBytes () == expectedBytes);
	    CHECK (CFBO::getLiveGpuBytes () == previousBytes + expectedBytes);
	    CHECK (glGetError () == GL_NO_ERROR);
	}
	CHECK (CFBO::getLiveGpuBytes () == previousBytes);
    }

    for (const auto* name : { "_rt_FullFrameBuffer", "_rt_imageLayerComposite_206_a" }) {
	CFBO target (name, TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, 64, 32, 64, 32);
	target.generateMipmaps ();
	glBindTexture (GL_TEXTURE_2D, target.getTextureID (0));
	GLint width = -1;
	glGetTexLevelParameteriv (GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &width);
	CHECK (width == 0);
	CHECK (target.getMipMapCount (0) == 1);
	CHECK (target.getApproximateGpuBytes () == 64 * 32 * 4);
    }
}

TEST_CASE ("reflection mipmaps average copied color and resize without stale levels", "[.][gl][texture][fbo][reflection]") {
    TextureGLContext context;
    CFBO target ("_rt_MipMappedFrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, 64, 64, 64, 64);
    // Populate the shared texture before its context-local framebuffer is first
    // requested. Lazy attachment must not erase an already populated snapshot.
    GLint texture = 0;
    glGetIntegerv (GL_TEXTURE_BINDING_2D, &texture);
    REQUIRE (texture != 0);
    std::vector<unsigned char> checker (64 * 64 * 4);
    for (int y = 0; y < 64; ++y) {
	for (int x = 0; x < 64; ++x) {
	    const size_t pixel = (y * 64 + x) * 4;
	    checker[pixel] = (x + y) % 2 == 0 ? 255 : 0;
	    checker[pixel + 2] = (x + y) % 2 == 0 ? 0 : 255;
	    checker[pixel + 3] = 255;
	}
    }
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, checker.data ());
    target.generateMipmaps ();
    CHECK (target.getTextureID (0) == static_cast<GLuint> (texture));

    ShaderProgramCache cache;
    const ShaderProgramCache::SharedProgram program (cache.createProgram (
	"#version 330 core\nvoid main() { vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);"
	"gl_Position = vec4(p * 2.0 - 1.0, 0, 1); }\n",
	"#version 330 core\nuniform sampler2D image; out vec4 color;"
	"void main() { color = textureLod(image, vec2(0.5), 2.0); }\n"
    ));
    CFBO output ("reflection_test_output", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, 1, 1, 1, 1);
    GLuint vao = 0;
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glBindFramebuffer (GL_FRAMEBUFFER, output.getFramebuffer ());
    glViewport (0, 0, 1, 1);
    glUseProgram (program.id);
    glUniform1i (glGetUniformLocation (program.id, "image"), 0);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texture);
    glDrawArrays (GL_TRIANGLES, 0, 3);
    std::array<unsigned char, 4> averaged {};
    glReadPixels (0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, averaged.data ());
    CHECK (averaged[0] >= 127);
    CHECK (averaged[0] <= 128);
    CHECK (averaged[1] == 0);
    CHECK (averaged[2] >= 127);
    CHECK (averaged[2] <= 128);
    CHECK (averaged[3] == 255);

    const GLuint framebuffer = target.getFramebuffer ();
    for (const auto size : { glm::uvec2 (17, 9), glm::uvec2 (129, 65) }) {
	CAPTURE (size.x, size.y);
	// Reallocation must clear independently of the previous render pass's masks.
	glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable (GL_SCISSOR_TEST);
	glScissor (0, 0, 1, 1);
	target.resize (size.x, size.y);
	CHECK (target.getFramebuffer () == framebuffer);
	CHECK (target.getTextureID (0) == static_cast<GLuint> (texture));
	GLboolean colorMask[4];
	glGetBooleanv (GL_COLOR_WRITEMASK, colorMask);
	CHECK (std::ranges::all_of (colorMask, [] (GLboolean channel) { return channel == GL_FALSE; }));
	CHECK (glIsEnabled (GL_SCISSOR_TEST));
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable (GL_SCISSOR_TEST);
	glBindTexture (GL_TEXTURE_2D, texture);
	const uint32_t levels = size.x == 17 ? 1 : 4;
	CHECK (target.getMipMapCount (0) == levels);
	GLint maxLevel = -1, unusedWidth = -1;
	GLfloat maxLod = -1;
	glGetTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &maxLevel);
	glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &maxLod);
	glGetTexLevelParameteriv (GL_TEXTURE_2D, levels, GL_TEXTURE_WIDTH, &unusedWidth);
	CHECK (maxLevel == static_cast<GLint> (levels - 1));
	CHECK (maxLod == static_cast<GLfloat> (levels - 1));
	CHECK (unusedWidth == 0);
	size_t expectedBytes = 0;
	for (uint32_t level = 0; level < levels; ++level) {
	    const size_t bytes = static_cast<size_t> (size.x >> level) * (size.y >> level) * 4;
	    expectedBytes += bytes;
	    std::vector<unsigned char> pixels (bytes, 255);
	    glGetTexImage (GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
	    CHECK (std::ranges::all_of (pixels, [] (unsigned char channel) { return channel == 0; }));
	}
	CHECK (target.getApproximateGpuBytes () == expectedBytes);
	CHECK (glGetError () == GL_NO_ERROR);
    }
    glUseProgram (0);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindVertexArray (0);
    glDeleteVertexArrays (1, &vao);
}

TEST_CASE ("multisample framebuffers resolve coverage and preserve render mode state", "[.][gl][texture][fbo][msaa]") {
    TextureGLContext context;
    CFBO unsupported (
	"msaa_unsupported", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, 2, 2, 2, 2, true, false, 3
    );
    CHECK (unsupported.getSamples () == 1);
    const size_t liveBefore = CFBO::getLiveGpuBytes ();
    CFBO target (
	"msaa_behavior", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, 16, 16, 16, 16, true, false, 4
    );
    GLint maxSamples = 0;
    glGetIntegerv (GL_MAX_SAMPLES, &maxSamples);
    INFO ("GL_MAX_SAMPLES=" << maxSamples << " requested=4 actual=" << target.getSamples ());
    if (target.getSamples () == 1) SKIP ("RGBA8 plus DEPTH_COMPONENT32F does not support exact 4x MSAA");
    REQUIRE (target.getSamples () == 4);
    CHECK (CFBO::getLiveGpuBytes () == liveBefore + target.getApproximateGpuBytes ());
    REQUIRE (target.getDrawFramebuffer () != target.getFramebuffer ());

    glBindTexture (GL_TEXTURE_2D, target.getTextureID (0));
    GLint internalFormat = 0;
    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
    CHECK (internalFormat == GL_RGBA8);
    glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
    GLint colorRenderbuffer = GL_NONE;
    glGetFramebufferAttachmentParameteriv (
	GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colorRenderbuffer
    );
    REQUIRE (colorRenderbuffer != GL_NONE);
    glBindRenderbuffer (GL_RENDERBUFFER, colorRenderbuffer);
    glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internalFormat);
    CHECK (internalFormat == GL_RGBA8);

    ShaderProgramCache cache;
    const std::string fullscreenVertex =
	"#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
	"gl_Position=vec4(p*2.0-1.0,0,1);}\n";
    const ShaderProgramCache::SharedProgram solid (cache.createProgram (
	fullscreenVertex,
	"#version 330 core\nuniform vec4 tint; uniform float depth; out vec4 color;"
	"void main(){color=tint;gl_FragDepth=depth;}\n"
    ));
    const ShaderProgramCache::SharedProgram triangle (cache.createProgram (
	"#version 330 core\nvoid main(){const vec2 p[3]=vec2[3](vec2(-1,-1),vec2(1,-1),vec2(-1,1));"
	"gl_Position=vec4(p[gl_VertexID],0,1);}\n",
	"#version 330 core\nout vec4 color;void main(){color=vec4(1);}\n"
    ));
    GLuint vao = GL_NONE;
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glViewport (0, 0, 16, 16);

    auto drawSolid = [&] (const std::array<float, 4>& color, const float depth) {
	glUseProgram (solid.id);
	glUniform4fv (glGetUniformLocation (solid.id, "tint"), 1, color.data ());
	glUniform1f (glGetUniformLocation (solid.id, "depth"), depth);
	glDrawArrays (GL_TRIANGLES, 0, 3);
    };
    auto centerPixel = [&] {
	std::array<unsigned char, 4> pixel {};
	glBindFramebuffer (GL_FRAMEBUFFER, target.getFramebuffer ());
	glReadPixels (8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data ());
	return pixel;
    };

    SECTION ("triangle edges contain fractional coverage") {
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	glClearColor (0, 0, 0, 0);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram (triangle.id);
	glDrawArrays (GL_TRIANGLES, 0, 3);
	std::array<unsigned char, 16 * 16 * 4> pixels {};
	glBindFramebuffer (GL_FRAMEBUFFER, target.getFramebuffer ());
	glReadPixels (0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
	CHECK (std::ranges::any_of (pixels, [] (const unsigned char channel) { return channel > 0 && channel < 255; }));
    }

    SECTION ("depth rejects equal and farther fragments") {
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LESS);
	glClearDepth (1.0);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawSolid ({ 1, 0, 0, 1 }, 0.7f);
	drawSolid ({ 0, 0, 1, 1 }, 0.7f);
	drawSolid ({ 1, 0, 1, 1 }, 0.9f);
	auto pixel = centerPixel ();
	CHECK (pixel == std::array<unsigned char, 4> { 255, 0, 0, 255 });
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	drawSolid ({ 0, 1, 0, 1 }, 0.3f);
	pixel = centerPixel ();
	CHECK (pixel == std::array<unsigned char, 4> { 0, 255, 0, 255 });
	glDisable (GL_DEPTH_TEST);
    }

    SECTION ("texture reads resolve without losing multisample depth") {
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LESS);
	glClearDepth (1.0);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawSolid ({ 1, 0, 0, 1 }, 0.6f);
	GLint drawBefore = GL_NONE, readBefore = GL_NONE;
	glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &drawBefore);
	glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &readBefore);
	glEnable (GL_SCISSOR_TEST);
	CHECK (target.getTextureID (0) != GL_NONE);
	GLint drawAfter = GL_NONE, readAfter = GL_NONE;
	glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &drawAfter);
	glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &readAfter);
	CHECK (drawAfter == drawBefore);
	CHECK (readAfter == readBefore);
	CHECK (glIsEnabled (GL_SCISSOR_TEST));
	glDisable (GL_SCISSOR_TEST);
	drawSolid ({ 0, 0, 1, 1 }, 0.8f);
	auto pixel = centerPixel ();
	CHECK (pixel == std::array<unsigned char, 4> { 255, 0, 0, 255 });
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	drawSolid ({ 0, 1, 0, 1 }, 0.2f);
	target.setMultisampleRendering (false);
	pixel = centerPixel ();
	CHECK (pixel == std::array<unsigned char, 4> { 0, 255, 0, 255 });
	glDisable (GL_DEPTH_TEST);
    }

    SECTION ("ending multisample mode keeps later single-sample writes") {
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawSolid ({ 1, 0, 0, 1 }, 0.5f);
	target.setMultisampleRendering (false);
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	drawSolid ({ 0, 0, 1, 1 }, 0.5f);
	CHECK (target.getTextureID (0) != GL_NONE);
	const auto pixel = centerPixel ();
	CHECK (pixel == std::array<unsigned char, 4> { 0, 0, 255, 255 });
    }

    SECTION ("alpha-to-coverage affects resolved sample coverage") {
	REQUIRE (target.getDrawSamples () == 4);
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable (GL_SAMPLE_ALPHA_TO_COVERAGE);
	drawSolid ({ 1, 1, 1, 0.5f }, 0.5f);
	glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
	const auto pixel = centerPixel ();
	CHECK (pixel[0] > 0);
	CHECK (pixel[0] < 255);
    }

    SECTION ("alpha-to-coverage has no effect on the single-sample draw attachment") {
	target.setMultisampleRendering (false);
	REQUIRE (target.getSamples () == 4);
	REQUIRE (target.getDrawSamples () == 1);
	glBindFramebuffer (GL_FRAMEBUFFER, target.getDrawFramebuffer ());
	GLint sampleBuffers = 0;
	glGetIntegerv (GL_SAMPLE_BUFFERS, &sampleBuffers);
	REQUIRE (sampleBuffers == 0);
	glClearColor (0, 0, 0, 0);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable (GL_SAMPLE_ALPHA_TO_COVERAGE);
	drawSolid ({ 1, 1, 1, 0 }, 0.5f);
	glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
	CHECK (centerPixel () == std::array<unsigned char, 4> { 255, 255, 255, 0 });
    }

    SECTION ("resize reallocates every attachment and updates accounting") {
	const size_t before = target.getApproximateGpuBytes ();
	const size_t liveBeforeResize = CFBO::getLiveGpuBytes ();
	CHECK (before == 16u * 16u * 4u * (1u + target.getSamples () * 2u));
	target.resize (9, 7);
	CHECK (target.getApproximateGpuBytes () == 9u * 7u * 4u * (1u + target.getSamples () * 2u));
	CHECK (CFBO::getLiveGpuBytes () == liveBeforeResize - before + target.getApproximateGpuBytes ());
	glBindRenderbuffer (GL_RENDERBUFFER, target.getDepthbuffer ());
	GLint width = 0, height = 0, samples = 0;
	glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
	glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
	glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);
	CHECK (width == 9);
	CHECK (height == 7);
	CHECK (samples == static_cast<GLint> (target.getSamples ()));
    }

    CHECK (glGetError () == GL_NO_ERROR);
    glUseProgram (0);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindVertexArray (0);
    glDeleteVertexArrays (1, &vao);
}

TEST_CASE ("R8 effect targets retain one-channel storage through resize", "[.][gl][texture][fbo][r8]") {
    TextureGLContext context;
    ShaderProgramCache cache;
    const ShaderProgramCache::SharedProgram program (cache.createProgram (
	"#version 330 core\nvoid main() {\n"
	"vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
	"gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n}\n",
	"#version 330 core\nout vec4 color;\n"
	"void main() { color = vec4(0.5, 0.0, 0.0, 1.0); }\n"
    ));
    GLuint vao = GL_NONE;
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glUseProgram (program.id);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glDisable (GL_SCISSOR_TEST);
    glDisable (GL_DITHER);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    for (const bool r8 : { true, false }) {
	CAPTURE (r8);
	const size_t liveBytes = CFBO::getLiveGpuBytes ();
	const size_t liveCount = CFBO::getLiveCount ();
	{
	    WallpaperEngine::Render::FBOProvider provider (nullptr);
	    const WallpaperEngine::Data::Model::FBO definition {
		.name = "_rt_R8Storage", .format = r8 ? "r8" : "rgba8888", .scale = 1.0f, .unique = true,
	    };
	    const auto target = provider.create (definition, glm::vec2 (9, 5));
	    const GLuint texture = target->getTextureID (0);
	    CHECK (target->getFormat () == (r8 ? TextureFormat_R8 : TextureFormat_ARGB8888));
	    for (const auto size : { glm::uvec2 (9, 5), glm::uvec2 (13, 7) }) {
		CAPTURE (size.x, size.y);
		if (size.x != 9) target->resize (size.x, size.y);
		CHECK (target->getTextureID (0) == texture);
		CHECK (target->getTextureWidth (0) == size.x);
		CHECK (target->getTextureHeight (0) == size.y);
		CHECK (target->getMipMapCount (0) == 1);
		CHECK (target->getSamples () == 1);
		const size_t expectedBytes = static_cast<size_t> (size.x) * size.y * (r8 ? 1 : 4);
		CHECK (target->getApproximateGpuBytes () == expectedBytes);
		CHECK (CFBO::getLiveGpuBytes () == liveBytes + expectedBytes);
		CHECK (CFBO::getLiveCount () == liveCount + 1);
		glBindTexture (GL_TEXTURE_2D, texture);
		GLint internalFormat = 0;
		glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
		CHECK (internalFormat == (r8 ? GL_R8 : GL_RGBA8));

		// Clear before first use too: lazy framebuffer creation must preserve storage.
		target->clear (glm::vec4 (0.25f, 0.5f, 0.75f, 0.125f));
		glBindFramebuffer (GL_FRAMEBUFFER, target->getFramebuffer ());
		REQUIRE (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		std::array<unsigned char, 4> pixel {};
		glReadPixels (0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data ());
		const std::array<int, 4> stored = r8
		    ? std::array<int, 4> { 64, 0, 0, 255 }
		    : std::array<int, 4> { 64, 128, 191, 32 };
		for (size_t channel = 0; channel < pixel.size (); ++channel) {
		    CAPTURE (channel);
		    CHECK (std::abs (static_cast<int> (pixel[channel]) - stored[channel]) <= 1);
		}

		// Missing destination alpha is one. This checks the blend unit directly,
		// beyond reading the texture's implicit alpha in a later shader.
		glViewport (0, 0, size.x, size.y);
		glEnable (GL_BLEND);
		glBlendEquation (GL_FUNC_ADD);
		glBlendFunc (GL_DST_ALPHA, GL_ZERO);
		glDrawArrays (GL_TRIANGLES, 0, 3);
		glReadPixels (0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data ());
		CHECK (std::abs (static_cast<int> (pixel[0]) - (r8 ? 128 : 16)) <= 1);
		CHECK (std::abs (static_cast<int> (pixel[3]) - stored[3]) <= 1);
		CHECK (glGetError () == GL_NO_ERROR);
	    }
	}
	CHECK (CFBO::getLiveGpuBytes () == liveBytes);
	CHECK (CFBO::getLiveCount () == liveCount);
    }
    glDeleteVertexArrays (1, &vao);
}

TEST_CASE ("Automatic texture quality uses native scene and output thresholds", "[texture][quality]") {
    using Context = WallpaperEngine::Application::ApplicationContext;
    using Quality = Context::TextureResolution;
    CHECK (Context::calculateTextureReduction (Quality::Auto, true, {0, 0}, {1920, 1026}) == 1);
    CHECK (Context::calculateTextureReduction (Quality::Auto, true, {0, 0}, {1920, 1025}) == 2);
    CHECK (Context::calculateTextureReduction (Quality::Auto, false, {390, 100}, {100, 100}) == 1);
    CHECK (Context::calculateTextureReduction (Quality::Auto, false, {391, 100}, {100, 100}) == 2);
    CHECK (Context::calculateTextureReduction (Quality::Auto, false, {0, 0}, {100, 100}) == 1);
    CHECK (Context::calculateTextureReduction (Quality::Auto, false, {15, 1}, {0, 0}) == 1);
    CHECK (Context::calculateTextureReduction (Quality::Auto, false, {16, 1}, {0, 0}) == 2);
    CHECK (Context::calculateTextureReduction (Quality::Full, true, {0, 0}, {1, 1}) == 1);
    CHECK (Context::calculateTextureReduction (Quality::Half, false, {0, 0}, {3840, 2160}) == 2);
}


TEST_CASE ("published layer targets survive their producer only while a consumer owns them", "[.][gl][fbo][layer-target-lifetime]") {
    TextureGLContext context;
    using WallpaperEngine::Render::FBOProvider;
    FBOProvider scene (nullptr);
    const auto before = CFBO::getLiveCount ();
    {
        FBOProvider producer (&scene);
        auto target = producer.create ("local", TextureFormat_ARGB8888, 0, 1, { 4, 4 }, { 4, 4 });
        scene.publish ("layer", target);
        scene.publish ("layer-alias", target);
        CHECK (scene.find ("layer-alias") == target);
        scene.alias ("consumer", "layer");
    }
    scene.pruneExpiredTargets ();
    REQUIRE (scene.find ("layer"));
    CHECK (CFBO::getLiveCount () == before + 1);
    scene.alias ("consumer", std::shared_ptr<CFBO> {});
    scene.pruneExpiredTargets ();
    CHECK_FALSE (scene.find ("layer"));
    CHECK_FALSE (scene.find ("layer-alias"));
    CHECK (CFBO::getLiveCount () == before);
}

TEST_CASE ("declared effect storage shares per scene and releases with its last effect", "[.][gl][texture][fbo][sharing]") {
    TextureGLContext context;
    using WallpaperEngine::Render::FBOProvider;
    using WallpaperEngine::Data::Model::FBO;
    const size_t bytes = CFBO::getLiveGpuBytes ();
    const size_t count = CFBO::getLiveCount ();
    FBOProvider scene (nullptr);
    {
        FBOProvider image1 (&scene), image2 (&scene), first (&image1), second (&image2);
        FBO definition { .name = "shared", .format = "rgba8888", .scale = 1.0f, .unique = false };
        const auto a = first.create (definition, { 9, 5 });
        const auto b = second.create (definition, { 9, 5 });
        REQUIRE (a == b);
        CHECK (CFBO::getLiveCount () == count + 1);
        CHECK (CFBO::getLiveGpuBytes () == bytes + 9 * 5 * 4);
        CHECK (a->getTextureID (0) == b->getTextureID (0));
        a->clear ({ 0.25f, 0.5f, 0.75f, 1.0f });
        const auto pixel = [] (const std::shared_ptr<CFBO>& target) {
            glBindFramebuffer (GL_FRAMEBUFFER, target->getFramebuffer ());
            std::array<unsigned char, 4> result {};
            glReadPixels (0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data ());
            return result;
        };
        CHECK (std::abs (static_cast<int> (pixel (b)[0]) - 64) <= 1);
        definition.clear = { 1, 0, 0, 1 };
        definition.clearOnCreate = true;
        REQUIRE (second.create (definition, { 9, 5 }) == a);
        CHECK (pixel (a) == std::array<unsigned char, 4> { 255, 0, 0, 255 });
        definition.clearOnCreate = false;
        definition.unique = true;
        const auto privateA = first.create (definition, { 9, 5 });
        const auto privateB = second.create (definition, { 9, 5 });
        CHECK (privateA != privateB);
        privateA->clear ({ 0, 1, 0, 1 });
        privateB->clear ({ 0, 0, 1, 1 });
        CHECK (pixel (privateA) == std::array<unsigned char, 4> { 0, 255, 0, 255 });
        CHECK (pixel (privateB) == std::array<unsigned char, 4> { 0, 0, 255, 255 });
        CHECK (pixel (a) == std::array<unsigned char, 4> { 255, 0, 0, 255 });
        definition.unique = false;
        const auto fullA = first.create (definition, { 9, 5 }, true);
        const auto fullB = second.create (definition, { 9, 5 }, true);
        REQUIRE (fullA == fullB);
        REQUIRE (fullA != a);
        const auto texture = fullA->getTextureID (0);
        fullA->resize (13, 7);
        CHECK (fullB->getTextureID (0) == texture);
        CHECK (fullB->getTextureWidth (0) == 13);
        CHECK (fullB->getTextureHeight (0) == 7);
        CHECK (first.create (definition, { 13, 7 }, true) == fullB);
        CHECK (glGetError () == GL_NO_ERROR);
    }
    // The root registry holds weak references, so even a live scene pins no storage.
    CHECK (CFBO::getLiveCount () == count);
    CHECK (CFBO::getLiveGpuBytes () == bytes);
}

TEST_CASE ("Reverse depth storage survives resize and MSAA fallback without leaking clip state", "[.][gl][reverse-depth-storage]") {
    TextureGLContext context;
    using WallpaperEngine::Render::ClipDepthScope;
    if (!ClipDepthScope::supported ()) SKIP ("Reverse depth needs OpenGL clip control");
    GLint originalOrigin = 0, originalDepth = 0;
    glGetIntegerv (GL_CLIP_ORIGIN, &originalOrigin);
    glGetIntegerv (GL_CLIP_DEPTH_MODE, &originalDepth);
    {
        const ClipDepthScope reversed (true);
        GLint depth = 0;
        glGetIntegerv (GL_CLIP_DEPTH_MODE, &depth);
        CHECK (depth == GL_ZERO_TO_ONE);
        {
            const ClipDepthScope ordinary (false);
            glGetIntegerv (GL_CLIP_DEPTH_MODE, &depth);
            CHECK (depth == GL_NEGATIVE_ONE_TO_ONE);
        }
        glGetIntegerv (GL_CLIP_DEPTH_MODE, &depth);
        CHECK (depth == GL_ZERO_TO_ONE);
    }
    GLint origin = 0, depth = 0;
    glGetIntegerv (GL_CLIP_ORIGIN, &origin);
    glGetIntegerv (GL_CLIP_DEPTH_MODE, &depth);
    CHECK (origin == originalOrigin);
    CHECK (depth == originalDepth);
    for (const uint32_t samples : { 1u, 3u, 4u }) {
        CAPTURE (samples);
        CFBO target ("reverse_depth", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
            1, 8, 8, 8, 8, true, false, samples, true);
        for (const uint32_t size : { 8u, 11u }) {
            if (size != 8) target.resize (size, size);
            const GLuint draw = target.getDrawFramebuffer ();
            glBindFramebuffer (GL_FRAMEBUFFER, draw);
            REQUIRE (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            glBindRenderbuffer (GL_RENDERBUFFER, target.getDepthbuffer ());
            GLint format = 0;
            glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
            CHECK (format == GL_DEPTH_COMPONENT32F);
            if (target.getSamples () == 1) target.clear (glm::vec4 (0));
            CFBO readback ("reverse_readback", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
                1, size, size, size, size, true, false, 1, true);
            const GLuint destination = readback.getDrawFramebuffer ();
            glBindFramebuffer (GL_READ_FRAMEBUFFER, draw);
            glBindFramebuffer (GL_DRAW_FRAMEBUFFER, destination);
            glBlitFramebuffer (0, 0, size, size, 0, 0, size, size, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer (GL_FRAMEBUFFER, destination);
            float value = -1;
            glReadPixels (0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &value);
            CHECK (value == 0.0f);
            CHECK (glGetError () == GL_NO_ERROR);
        }
    }
    CFBO volume ("reverse_volume", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
        1, 8, 8, 8, 8, false, true, 1, true);
    // Construction leaves its shared texture bound, before lazy framebuffer setup.
    GLint volumeTexture = 0;
    glGetIntegerv (GL_TEXTURE_BINDING_2D, &volumeTexture);
    REQUIRE (volumeTexture != 0);
    const std::array<float, 64> occupiedDepth = [] { std::array<float, 64> value; value.fill (1.0f); return value; } ();
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_DEPTH_COMPONENT, GL_FLOAT, occupiedDepth.data ());
    glDepthMask (GL_FALSE);
    glEnable (GL_SCISSOR_TEST);
    glScissor (0, 0, 0, 0);
    glBindFramebuffer (GL_FRAMEBUFFER, volume.getFramebuffer ());
    REQUIRE (volume.getTextureID (0) == static_cast<GLuint> (volumeTexture));
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv (GL_DEPTH_WRITEMASK, &depthMask);
    CHECK (depthMask == GL_FALSE);
    CHECK (glIsEnabled (GL_SCISSOR_TEST) == GL_TRUE);
    glDepthMask (GL_TRUE);
    glDisable (GL_SCISSOR_TEST);
    GLint format = 0;
    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    CHECK (format == GL_DEPTH_COMPONENT32F);
    glBindFramebuffer (GL_FRAMEBUFFER, volume.getFramebuffer ());
    float value = -1;
    glReadPixels (0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &value);
    CHECK (value == 0.0f);
    CHECK (glGetError () == GL_NO_ERROR);
}

TEST_CASE ("Reverse shadow atlas uses strict comparisons and a zero border", "[.][gl][reverse-shadow]") {
    TextureGLContext context;
    ShaderProgramCache cache;
    const ShaderProgramCache::SharedProgram program (cache.createProgram (
        "#version 330 core\nvoid main() { vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
        "gl_Position=vec4(p*2.0-1.0,0,1); }\n",
        "#version 330 core\nuniform sampler2DShadow atlas; out vec4 color;\n"
        "void main() { int cell=int(gl_FragCoord.x); float depth=0.000010742223;"
        "vec2 uv=cell<2?vec2(-1):vec2(.5);"
        "float reference=cell==0?0.0:cell==1?.5:cell==2?depth:cell==3?depth+1e-9:depth-1e-9;"
        "color=vec4(texture(atlas,vec3(uv,reference))); }\n"
    ));
    CFBO atlas ("reverse_shadow", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
        1, 4, 4, 4, 4, false, true, 1, true);
    glBindFramebuffer (GL_FRAMEBUFFER, atlas.getFramebuffer ());
    glDepthMask (GL_TRUE);
    glClearDepth (0.000010742223f);
    glClear (GL_DEPTH_BUFFER_BIT);
    CFBO output ("reverse_shadow_output", TextureFormat_ARGB8888, TextureFlags_ClampUVs,
        1, 5, 1, 5, 1);
    glBindFramebuffer (GL_FRAMEBUFFER, output.getFramebuffer ());
    glViewport (0, 0, 5, 1);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_BLEND);
    glUseProgram (program.id);
    GLuint vao = 0;
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, atlas.getTextureID (0));
    glUniform1i (glGetUniformLocation (program.id, "atlas"), 0);
    glDrawArrays (GL_TRIANGLES, 0, 3);
    std::array<unsigned char, 20> pixels {};
    glReadPixels (0, 0, 5, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
    // Native strict GREATER: zero border equality fails, positive border passes;
    // equality with an occupied far texel fails while its nearer neighbor passes.
    for (int index = 0; index < 5; ++index) {
        CAPTURE (index);
        CHECK (pixels[index * 4] == (index == 1 || index == 3 ? 255 : 0));
    }
    CHECK (glGetError () == GL_NO_ERROR);
    glDeleteVertexArrays (1, &vao);
}
