#include "CPrettyPrinter.h"

#include "WallpaperEngine/Render/Wallpapers/CVideo.h"
#include "WallpaperEngine/Render/Wallpapers/CWeb.h"
#include "WallpaperEngine/Render/Objects/CEffect.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::PrettyPrinter;
using namespace WallpaperEngine::Render::Objects;

CPrettyPrinter::CPrettyPrinter () :
    m_level (0),
    m_buffer (),
    m_out (&this->m_buffer) {}

void CPrettyPrinter::printWallpaper (const CWallpaper& wallpaper) {
    bool isScene = wallpaper.is <Wallpapers::CScene> ();
    bool isVideo = wallpaper.is <Wallpapers::CVideo> ();
    bool isWeb = wallpaper.is <Wallpapers::CWeb> ();

    if (isScene) {
        const auto scene = wallpaper.as <Wallpapers::CScene> ();

        this->m_out << "Scene wallpaper: ";
        this->increaseIndentation ();

        const auto fbos = scene->getFBOs ();

        this->m_out << "FBOs count: " << fbos.size ();

        if (fbos.size () > 0) {
            this->increaseIndentation ();

            for (const auto fbo : fbos) {
                this->printFBO (*fbo.second);
            }

            this->decreaseIndentation ();
        } else {
            this->lineEnd ();
        }

        const auto objects = scene->getObjectsByRenderOrder ();

        if (objects.size () > 0) {
            this->m_out << "Objects count: " << objects.size ();
            this->increaseIndentation ();

            for (const auto object : objects) {
                if (object->is <CImage> ()) {
                    this->printImage (*object->as <CImage> ());
                }
            }

            this->decreaseIndentation ();
        }

        this->decreaseIndentation ();
    } else if (isVideo) {
        const auto video = wallpaper.as <Wallpapers::CVideo> ();

        this->m_out << "Video wallpaper: " << video->getVideo ()->getFilename ();
        this->lineEnd ();
    } else if (isWeb) {
        const auto web = wallpaper.as <Wallpapers::CWeb> ();

        this->m_out << "Web wallpaper";
        this->lineEnd ();
    }
}

void CPrettyPrinter::printImage (const CImage& image) {
    auto base = image.getImage ();

    this->m_out << "Image " << image.getId () << " " << base->getName () << ":";
    this->increaseIndentation ();

    const auto material = image.getMaterial ();

    if (material != nullptr) {
        this->printMaterial (*image.getMaterial ());
    }

    int effects = 0;

    for (const auto effect : image.getEffects ()) {
        this->printEffect (*effect, ++effects);
    }

    this->decreaseIndentation ();
}

void CPrettyPrinter::printEffect (const CEffect& effect, int effectId) {
    if (effectId != 0) {
        this->m_out << "Effect " << effectId << ":";
    } else {
        this->m_out << "Effect:";
    }

    this->increaseIndentation ();
    this->m_out << "Visibility status: " << effect.isVisible ();
    this->lineEnd ();

    const auto fbos = effect.getFBOs();

    this->m_out << "FBOs count: " << fbos.size();

    if (fbos.size() > 0) {
        this->increaseIndentation ();

        for (const auto fbo : fbos) {
            this->printFBO (*fbo);
        }

        this->decreaseIndentation ();
    } else {
        this->lineEnd ();
    }

    const auto materials = effect.getMaterials ();

    for (const auto material : materials) {
        this->printMaterial (*material);
    }

    this->decreaseIndentation ();
}

void CPrettyPrinter::printFBO (const CFBO& fbo) {
    this->m_out << "FBO " << fbo.getName () << ":";
    this->increaseIndentation ();

    this->m_out << "Scale: " << fbo.getScale ();
    this->lineEnd ();
    this->printTextureInfo (fbo);

    this->decreaseIndentation ();
}

void CPrettyPrinter::printMaterial (const CMaterial& material) {
    const auto base = material.getMaterial ();

    this->m_out << "Material " << base->getName () << ":";
    this->increaseIndentation ();

    if (base->hasTarget ()) {
        this->m_out << "Target: " << base->getTarget ();
        this->lineEnd ();
    }

    const auto passes = material.getPasses ();

    this->m_out << "Passes count: " << passes.size ();

    if (passes.size () > 0) {
        this->increaseIndentation ();

        int passId = 0;
        for (const auto pass : passes) {
            this->printPass (*pass, ++passId);
        }
        this->decreaseIndentation ();
    } else {
        this->lineEnd ();
    }

    const auto textureBinds = base->getTextureBinds ();

    if (textureBinds.size () > 0) {
        this->m_out << "Texture binds count: " << textureBinds.size ();
        this->increaseIndentation ();

        for (const auto bind : textureBinds) {
            this->m_out << "Bind " << bind.first << " = " << bind.second->getIndex () << "," << bind.second->getName ();
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    this->decreaseIndentation ();
}

void CPrettyPrinter::printPass (const CPass& pass, int passId) {
    const auto base = pass.getPass ();

    this->m_out << "Pass " << passId << ":";
    this->increaseIndentation ();

    if (pass.getBlendingMode ().compare (base->getBlendingMode ()) != 0) {
        this->m_out << "Render blending mode: " << pass.getBlendingMode ();
        this->lineEnd ();
        this->m_out << "Blending mode: " << base->getBlendingMode ();
        this->lineEnd ();
    } else {
        this->m_out << "Blending mode: " << base->getBlendingMode ();
        this->lineEnd ();
    }

    this->m_out << "Culling mode: " << base->getCullingMode();
    this->lineEnd ();
    this->m_out << "Depth test: " << base->getDepthTest();
    this->lineEnd ();
    this->m_out << "Depth write: " << base->getDepthWrite();
    this->lineEnd ();
    this->m_out << "Shader: " << base->getShader();
    this->lineEnd ();
    const auto textures = base->getTextures ();

    if (textures.size () > 0) {
        this->m_out << "Textures " << textures.size () << ":";
        this->increaseIndentation ();

        for (const auto texture : textures) {
            this->m_out << texture.first << ": " << texture.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    const auto fragmentTextures = pass.getShader ()->getFragmentTextures();

    if (fragmentTextures.size () > 0) {
        this->m_out << "Fragment textures " << fragmentTextures.size () << ":";
        this->increaseIndentation ();

        for (const auto& texture : fragmentTextures) {
            this->m_out << texture.first << ": " << texture.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    const auto vertexTextures = pass.getShader ()->getVertexTextures ();

    if (vertexTextures.size () > 0) {
        this->m_out << "Vertex textures " << textures.size () << ":";
        this->increaseIndentation ();

        for (const auto& texture : vertexTextures) {
            this->m_out << texture.first << ": " << texture.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    const auto combos = base->getCombos();

    if (combos.size () > 0) {
        this->m_out << "Combos " << combos.size () << ":";
        this->increaseIndentation ();

        for (const auto combo : combos) {
            this->m_out << combo.first << " = " << combo.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    const auto constants = base->getConstants ();

    if (constants.size () > 0) {
        this->m_out << "Constants " << constants.size () << ":";
        this->increaseIndentation ();

        for (const auto constant : constants) {
            this->m_out << constant.first << " = " << constant.second->toString ();
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    this->decreaseIndentation ();
}

void CPrettyPrinter::printTextureInfo (const ITexture& texture) {
    this->m_out << "Texture Dimensions: " << texture.getTextureWidth(0) << "x" << texture.getTextureHeight (0);
    this->lineEnd ();
    this->m_out << "Part Dimensions: " << texture.getRealWidth  () << "x" << texture.getRealHeight ();
    this->lineEnd ();
    this->m_out << "Texture format: ";
    this->printTextureFormat (texture.getFormat ());
    this->lineEnd ();
    this->m_out << "Texture flags: ";
    this->printTextureFlags (texture.getFlags ());
    this->lineEnd ();
    this->m_out << "Is animated: " << texture.isAnimated ();
    this->lineEnd ();

    const auto frames = texture.getFrames ();

    if (frames.size () > 1) {
        this->m_out << frames.size () << " frames: ";
        this->increaseIndentation ();

        for (const auto frame : frames) {
            this->m_out << "Frame " << frame->frameNumber << ":";
            this->increaseIndentation ();
            this->m_out << "Frametime " << frame->frametime;
            this->lineEnd ();
            this->m_out << "Position in atlas: " << frame->x << "x" << frame->y;
            this->lineEnd ();
            this->m_out << "Size1: " << frame->width1 << "x" << frame->height1;
            this->lineEnd ();
            this->m_out << "Size2: " << frame->width2 << "x" << frame->height2;
            this->decreaseIndentation ();
        }

        this->decreaseIndentation ();
    }
}

void CPrettyPrinter::printTextureFormat (ITexture::TextureFormat format) {
    switch (format) {
        case ITexture::UNKNOWN: this->m_out << "UNKNOWN"; break;
        case ITexture::ARGB8888: this->m_out << "ARGB8888"; break;
        case ITexture::RGB888: this->m_out << "RGB888"; break;
        case ITexture::RGB565: this->m_out << "RGB565"; break;
        case ITexture::DXT5: this->m_out << "DXT5"; break;
        case ITexture::DXT3: this->m_out << "DXT3"; break;
        case ITexture::DXT1: this->m_out << "DXT1"; break;
        case ITexture::RG88: this->m_out << "RG88"; break;
        case ITexture::R8: this->m_out << "R8"; break;
        case ITexture::RG1616f: this->m_out << "RG1616f"; break;
        case ITexture::R16f: this->m_out << "R16f"; break;
        case ITexture::BC7: this->m_out << "BC7"; break;
        case ITexture::RGBa1010102: this->m_out << "RGBa1010102"; break;
        case ITexture::RGBA16161616f: this->m_out << "RGBA16161616f"; break;
        case ITexture::RGB161616f: this->m_out << "RGB161616f"; break;
    }
}

void CPrettyPrinter::printTextureFlags (ITexture::TextureFlags flags) {
    if (flags & ITexture::NoInterpolation) {
        this->m_out << "No interpolation";
    }
    if (flags & ITexture::ClampUVs) {
        this->m_out << "Clamp UVs ";
    }
    if (flags & ITexture::IsGif) {
        this->m_out << "Is gif ";
    }
    if (flags & ITexture::ClampUVsBorder) {
        this->m_out << "ClampUVs border ";
    }
    if (flags == ITexture::NoFlags) {
        this->m_out << "No flags";
    }
}

void CPrettyPrinter::indentation () {
    this->m_out << std::string(this->m_level, '\t');
}

void CPrettyPrinter::lineEnd () {
    this->m_out << '\n';
    this->indentation ();
}

void CPrettyPrinter::increaseIndentation () {
    this->m_level ++;
    this->lineEnd ();
}

void CPrettyPrinter::decreaseIndentation () {
    this->m_level --;
    this->lineEnd ();
}

std::string CPrettyPrinter::str () {
    return this->m_buffer.str ();
}