#include <utility>

#include "StringPrinter.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

using namespace WallpaperEngine::Data::Dumpers;
using namespace WallpaperEngine::Data::Model;

StringPrinter::StringPrinter (std::string indentationCharacter) :
    m_out (&this->m_buffer), m_indentationCharacter (std::move (indentationCharacter)) { }

void StringPrinter::printWallpaper (const Wallpaper& wallpaper) {
    const bool isScene = wallpaper.is<Scene> ();
    const bool isVideo = wallpaper.is<Video> ();
    const bool isWeb = wallpaper.is<Web> ();

    if (isVideo) {
	const auto video = wallpaper.as<Video> ();

	this->m_out << "Video wallpaper: " << video->filename;
	this->lineEnd ();
    } else if (isWeb) {
	const auto web = wallpaper.as<Web> ();

	this->m_out << "Web wallpaper: " << web->filename;
	this->lineEnd ();
    } else if (isScene) {
	const auto scene = wallpaper.as<Scene> ();

	this->m_out << "Scene wallpaper: ";
	this->increaseIndentation ();
	this->lineEnd ();

	// TODO: IMPLEMENT FBO PRINTING, AS THIS WASN'T REALLY REFLECTION HOW IT ACTUALLY WORKS
	this->m_out << "Objects count: " << scene->objects.size ();
	this->increaseIndentation ();

	for (const auto& object : scene->objects) {
	    this->lineEnd ();
	    this->printObject (*object);
	    this->lineEnd ();
	}

	this->decreaseIndentation ();
    }
}

void StringPrinter::printObject (const Object& object) {
    this->m_out << "Object " << object.id << " " << object.name << ":";
    this->increaseIndentation ();

    this->lineEnd ();
    this->m_out << "Dependencies (" << object.dependencies.size () << "): ";

    for (const auto& dependency : object.dependencies) {
	this->m_out << dependency << ", ";
    }

    if (object.is<Image> ()) {
	this->lineEnd ();
	this->printImage (*object.as<Image> ());
    } else if (object.is<Sound> ()) {
	this->lineEnd ();
	this->printSound (*object.as<Sound> ());
    }

    this->decreaseIndentation ();
}

void StringPrinter::printImage (const Image& image) {
    this->printModel (*image.model);

    this->lineEnd ();
    this->m_out << "Image effects count: " << image.effects.size ();
    this->increaseIndentation ();

    for (const auto& effect : image.effects) {
	this->lineEnd ();
	this->printImageEffect (*effect);
    }

    this->decreaseIndentation ();
}

void StringPrinter::printSound (const Sound& sound) {
    this->m_out << "Sounds: " << sound.sounds.size ();
    this->increaseIndentation ();

    for (const auto& filename : sound.sounds) {
	this->lineEnd ();
	this->m_out << "Filename " << filename;
    }

    this->decreaseIndentation ();
}

void StringPrinter::printModel (const ModelStruct& model) {
    this->m_out << "Model " << model.filename << ":";
    this->increaseIndentation ();
    this->lineEnd ();

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

void StringPrinter::printImageEffect (const ImageEffect& imageEffect) {
    this->m_out << "Image effect " << imageEffect.id << ":";

    this->increaseIndentation ();

    this->lineEnd ();
    this->m_out << "Default visibility status: " << imageEffect.visible->value->getBool ();

    this->printEffect (*imageEffect.effect);

    this->lineEnd ();
    this->m_out << "Effect overrides count: " << imageEffect.passOverrides.size ();

    this->increaseIndentation ();

    for (const auto& override : imageEffect.passOverrides) {
	this->printImageEffectPassOverride (*override);
    }

    this->decreaseIndentation ();
    this->decreaseIndentation ();
}

void StringPrinter::printImageEffectPassOverride (const ImageEffectPassOverride& imageEffectPass) {
    this->lineEnd ();
    this->m_out << "Pass override " << imageEffectPass.id << ":";
    this->increaseIndentation ();

    this->lineEnd ();
    this->m_out << "Textures count: " << imageEffectPass.textures.size ();

    if (!imageEffectPass.textures.empty ()) {
	this->increaseIndentation ();

	for (const auto& [index, texture] : imageEffectPass.textures) {
	    this->lineEnd ();
	    this->m_out << "Texture " << index << ": " << texture;
	}

	this->decreaseIndentation ();
    }

    this->lineEnd ();
    this->m_out << "Combos count: " << imageEffectPass.combos.size ();

    if (!imageEffectPass.combos.empty ()) {
	this->increaseIndentation ();

	for (const auto& [name, value] : imageEffectPass.combos) {
	    this->lineEnd ();
	    this->m_out << "Combo " << name << "=" << value;
	}

	this->decreaseIndentation ();
    }

    this->lineEnd ();
    this->m_out << "Constants count: " << imageEffectPass.constants.size ();

    if (!imageEffectPass.constants.empty ()) {
	this->increaseIndentation ();

	for (const auto& [name, value] : imageEffectPass.constants) {
	    this->lineEnd ();
	    this->m_out << "Constant " << name << "=" << value->value->toString ();
	}

	this->decreaseIndentation ();
    }

    this->decreaseIndentation ();
}

void StringPrinter::printEffect (const Effect& effect) {
    this->lineEnd ();
    this->m_out << "Effect " << effect.name << ":";
    this->increaseIndentation ();

    if (!effect.description.empty ()) {
	this->lineEnd ();
	this->m_out << "Description: " << effect.description;
    }

    if (!effect.dependencies.empty ()) {
	this->lineEnd ();
	this->m_out << "Dependencies count: " << effect.dependencies.size ();
	this->increaseIndentation ();

	for (const auto& dependency : effect.dependencies) {
	    this->lineEnd ();
	    this->m_out << "Dependency: " << dependency;
	}

	this->decreaseIndentation ();
    }

    if (!effect.group.empty ()) {
	this->lineEnd ();
	this->m_out << "Group: " << effect.group;
    }

    if (!effect.preview.empty ()) {
	this->lineEnd ();
	this->m_out << "Preview: " << effect.preview;
    }

    this->lineEnd ();
    this->m_out << "Passes count: " << effect.passes.size ();
    this->increaseIndentation ();

    int passId = 0;

    for (const auto& pass : effect.passes) {
	this->lineEnd ();
	this->m_out << "Pass " << ++passId << ":";
	this->increaseIndentation ();
	this->lineEnd ();
	this->printEffectPass (*pass);
	this->decreaseIndentation ();
    }

    this->decreaseIndentation ();

    this->lineEnd ();
    this->m_out << "FBOs count: " << effect.fbos.size ();
    this->increaseIndentation ();

    for (const auto& fbo : effect.fbos) {
	this->lineEnd ();
	this->printFBO (*fbo);
    }

    this->decreaseIndentation ();
    this->decreaseIndentation ();
}

void StringPrinter::printEffectPass (const EffectPass& effectPass) {
    if (effectPass.command.has_value ()) {
	this->m_out << "Command: " << (*effectPass.command == Command_Copy ? "copy" : "swap");

	if (effectPass.target.has_value () || effectPass.source.has_value ()) {
	    this->lineEnd ();
	}
    }

    if (effectPass.target.has_value ()) {
	this->m_out << "Target: " << effectPass.target.value ();

	if (effectPass.source.has_value ()) {
	    this->lineEnd ();
	}
    }

    if (effectPass.source.has_value ()) {
	this->m_out << "Source: " << effectPass.source.value ();
    }

    if (!effectPass.binds.empty ()) {
	if (effectPass.target.has_value ()) {
	    this->lineEnd ();
	}

	this->m_out << "Binds count: " << effectPass.binds.size ();
	this->increaseIndentation ();

	for (const auto& [index, target] : effectPass.binds) {
	    this->lineEnd ();
	    this->m_out << "Bind " << index << ": " << target;
	}

	this->decreaseIndentation ();
    }

    if (effectPass.target.has_value () || !effectPass.binds.empty ()) {
	this->lineEnd ();
    }

    if (effectPass.material.has_value ()) {
	this->printMaterial (**effectPass.material);
    }
}

void StringPrinter::printFBO (const FBO& fbo) {
    this->m_out << "FBO " << fbo.name << ":";
    this->increaseIndentation ();

    this->lineEnd ();
    this->m_out << "Format: " << fbo.format;

    this->lineEnd ();
    this->m_out << "Scale: " << fbo.scale;
    this->decreaseIndentation ();
}

void StringPrinter::printMaterial (const Material& material) {
    this->m_out << "Material " << material.filename << ":";
    this->increaseIndentation ();
    this->lineEnd ();
    this->m_out << "Passes count: " << material.passes.size ();
    this->increaseIndentation ();

    for (const auto& pass : material.passes) {
	this->lineEnd ();
	this->printMaterialPass (*pass);
    }

    this->decreaseIndentation ();
    this->decreaseIndentation ();
}

void StringPrinter::printMaterialPass (const MaterialPass& materialPass) {
    this->m_out << "Pass " << materialPass.shader << ":";
    this->increaseIndentation ();

    this->lineEnd ();
    this->m_out << "Depth test: " << materialPass.depthtest;
    this->lineEnd ();
    this->m_out << "Depth write: " << materialPass.depthwrite;
    this->lineEnd ();
    this->m_out << "Blend mode: " << materialPass.blending;
    this->lineEnd ();
    this->m_out << "Culling mode: " << materialPass.cullmode;

    if (!materialPass.textures.empty ()) {
	this->lineEnd ();
	this->m_out << "Textures count: " << materialPass.textures.size ();
	this->increaseIndentation ();

	for (const auto& [index, texture] : materialPass.textures) {
	    this->lineEnd ();
	    this->m_out << "Texture " << index << ": " << texture;
	}

	this->decreaseIndentation ();
    }

    if (!materialPass.combos.empty ()) {
	this->lineEnd ();
	this->m_out << "Combos count: " << materialPass.combos.size ();
	this->increaseIndentation ();

	for (const auto& [name, value] : materialPass.combos) {
	    this->lineEnd ();
	    this->m_out << "Combo " << name << ": " << value;
	}

	this->decreaseIndentation ();
    }

    this->decreaseIndentation ();
}

void StringPrinter::indentation () {
    for (int i = 0; i < this->m_level; i++) {
	this->m_out << this->m_indentationCharacter;
    }
}

void StringPrinter::lineEnd () {
    this->m_out << "\n";
    this->indentation ();
}

void StringPrinter::increaseIndentation () { this->m_level++; }

void StringPrinter::decreaseIndentation () { this->m_level--; }

std::string StringPrinter::str () const { return this->m_buffer.str (); }