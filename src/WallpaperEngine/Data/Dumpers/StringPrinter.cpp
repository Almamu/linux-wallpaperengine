#include <utility>

#include "StringPrinter.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

using namespace WallpaperEngine::Data::Dumpers;
using namespace WallpaperEngine::Data::Model;

StringPrinter::StringPrinter (std::string indentationCharacter) :
    m_out (&this->m_buffer),
    m_indentationCharacter (std::move(indentationCharacter)) { }

void StringPrinter::printWallpaper (const Wallpaper& wallpaper) {
    bool isScene = wallpaper.is <Scene> ();
    bool isVideo = wallpaper.is <Video> ();
    bool isWeb = wallpaper.is <Web> ();

    if (isVideo) {
        const auto video = wallpaper.as <Video> ();

        this->m_out << "Video wallpaper: " << video->filename;
        this->lineEnd ();
    } else if (isWeb) {
        const auto web = wallpaper.as <Web> ();

        this->m_out << "Web wallpaper: " << web->filename;
        this->lineEnd ();
    } else if (isScene) {
        const auto scene = wallpaper.as <Scene> ();

        this->m_out << "Scene wallpaper: ";
        this->increaseIndentation ();

        // TODO: IMPLEMENT FBO PRINTING, AS THIS WASN'T REALLY REFLECTION HOW IT ACTUALLY WORKS
        this->m_out << "Objects count: " << scene->objects.size ();
        this->increaseIndentation ();

        for (const auto& object : scene->objects) {
            if (object.second->is <Image> ()) {
                this->printImage (*object.second->as <Image> ());
            }
        }
    }
}

void StringPrinter::printImage (const Image& image) {
    this->m_out << "Image " << image.id << " " << image.name << ":";
    this->increaseIndentation ();

    this->printModel (*image.model);

    for (const auto& effect : image.effects) {
        this->printImageEffect (*effect);
    }

    this->decreaseIndentation ();
}

void StringPrinter::printModel (const ModelStruct& model) {
    this->m_out << "Model " << model.filename << ":";
    this->increaseIndentation ();

    this->m_out << "Autosize: " << model.autosize;
    this->lineEnd ();
    this->m_out << "Passthrough: " << model.passthrough;
    this->lineEnd ();
    this->m_out << "Fullscreen: " << model.fullscreen;
    this->lineEnd ();
    this->m_out << "No padding: " << model.nopadding;
    this->lineEnd ();
    this->m_out << "Solid layer: " << model.solidlayer;
    this->lineEnd ();
    if (model.width.has_value ()) {
        this->m_out << "Width: " << model.width.value ();
        this->lineEnd ();
    }
    if (model.height.has_value ()) {
        this->m_out << "Height: " << model.height.value ();
        this->lineEnd ();
    }

    this->printMaterial (*model.material);

    this->decreaseIndentation ();
}

void StringPrinter::printImageEffect (const ImageEffect& effect) {
    this->m_out << "Image effect " << effect.id << ":";

    this->increaseIndentation ();

    this->m_out << "Default visibility status: " << effect.visible->value->getBool ();
    this->lineEnd ();

    this->printEffect (*effect.effect);
    this->decreaseIndentation ();
}

void StringPrinter::printImageEffectPass (const ImageEffectPass& imageEffectPass) {
    this->m_out << "Pass " << imageEffectPass.id << ":";
    this->increaseIndentation ();

    if (!imageEffectPass.textures.empty ()) {
        this->m_out << "Textures count: " << imageEffectPass.textures.size ();
        this->increaseIndentation ();

        for (const auto& texture : imageEffectPass.textures) {
            this->m_out << "Texture " << texture.first << ": " << texture.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }


}

void StringPrinter::printEffect (const Effect& effect) {
    this->m_out << "Effect " << effect.name << ":";
    this->increaseIndentation ();

    if (!effect.description.empty ()) {
        this->m_out << "Description: " << effect.description;
        this->lineEnd ();
    }

    if (!effect.dependencies.empty ()) {
        this->m_out << "Dependencies count: " << effect.dependencies.size ();
        this->increaseIndentation ();

        for (const auto& dependency : effect.dependencies) {
            this->m_out << "Dependency: " << dependency;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    if (!effect.group.empty ()) {
        this->m_out << "Group: " << effect.group;
        this->lineEnd ();
    }

    if (!effect.preview.empty ()) {
        this->m_out << "Preview: " << effect.preview;
        this->lineEnd ();
    }

    this->m_out << "Passes count: " << effect.passes.size ();
    this->increaseIndentation ();

    int passId = 0;

    for (const auto& pass : effect.passes) {
        this->m_out << "Pass " << passId << ":";
        this->increaseIndentation ();
        this->printEffectPass (*pass);
        this->decreaseIndentation ();
        passId ++;
    }

    this->decreaseIndentation ();

    this->m_out << "FBOs count: " << effect.fbos.size ();
    this->increaseIndentation ();

    for (const auto& fbo : effect.fbos) {
        this->printFBO (*fbo);
    }

    this->decreaseIndentation ();
    this->decreaseIndentation ();
}

void StringPrinter::printEffectPass (const EffectPass& effectPass) {
    if (effectPass.command.has_value ()) {
        const auto& command = effectPass.command.value ();

        this->m_out << "Command: " << (command.command == Command_Copy ? "copy" : "swap");
        this->lineEnd ();
        this->m_out << "Source: " << command.source;
        this->lineEnd ();
        this->m_out << "Target: " << command.target;
        this->lineEnd ();
    } else {
        if (effectPass.target.has_value ()) {
            this->m_out << "Target: " << effectPass.target.value ();
            this->lineEnd ();
        }

        if (!effectPass.binds.empty ()) {
            this->m_out << "Binds count: " << effectPass.binds.size ();
            this->increaseIndentation ();

            for (const auto& bind : effectPass.binds) {
                this->m_out << "Bind " << bind.first << ": " << bind.second;
                this->lineEnd ();
            }

            this->decreaseIndentation ();
        }

        this->printMaterial (*effectPass.material);
    }
}

void StringPrinter::printFBO (const FBO& fbo) {
    this->m_out << "FBO " << fbo.name << ":";
    this->increaseIndentation ();

    this->m_out << "Format: " << fbo.format;
    this->lineEnd ();

    this->m_out << "Scale: " << fbo.scale;
    this->decreaseIndentation ();
}

void StringPrinter::printMaterial (const Material& material) {
    this->m_out << "Material " << material.filename << ":";
    this->increaseIndentation ();
    this->m_out << "Passes count: " << material.passes.size ();
    this->increaseIndentation ();

    for (const auto& pass : material.passes) {
        this->printMaterialPass (*pass);
    }

    this->decreaseIndentation ();
    this->decreaseIndentation ();
}

void StringPrinter::printMaterialPass (const MaterialPass& materialPass) {
    this->m_out << "Pass " << materialPass.shader << ":";
    this->increaseIndentation ();

    this->m_out << "Depth test: " << materialPass.depthtest;
    this->lineEnd ();
    this->m_out << "Depth write: " << materialPass.depthwrite;
    this->lineEnd ();
    this->m_out << "Blend mode: " << materialPass.blending;
    this->lineEnd ();
    this->m_out << "Culling mode: " << materialPass.cullmode;
    this->lineEnd ();

    if (!materialPass.textures.empty ()) {
        this->m_out << "Textures count: " << materialPass.textures.size ();
        this->increaseIndentation ();

        for (const auto& texture : materialPass.textures) {
            this->m_out << "Texture " << texture.first << ": " << texture.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    if (!materialPass.combos.empty ()) {
        this->m_out << "Combos count: " << materialPass.combos.size ();
        this->increaseIndentation ();

        for (const auto& combo : materialPass.combos) {
            this->m_out << "Combo " << combo.first << ": " << combo.second;
            this->lineEnd ();
        }

        this->decreaseIndentation ();
    }

    this->decreaseIndentation ();
}

void StringPrinter::indentation () {
    for (int i = 0; i < this->m_level; i++)
        this->m_out << this->m_indentationCharacter;
}

void StringPrinter::lineEnd () {
    this->m_out << "\n";
    this->indentation ();
}

void StringPrinter::increaseIndentation () {
    this->m_level ++;
    this->lineEnd ();
}

void StringPrinter::decreaseIndentation () {
    this->m_level --;
    this->lineEnd ();
}

std::string StringPrinter::str () {
    return this->m_buffer.str ();
}