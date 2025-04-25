#include "CImage.h"
#include <sstream>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CImage::CImage (Wallpapers::CScene* scene, const Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_texture (nullptr),
    m_sceneSpacePosition (GL_NONE),
    m_copySpacePosition (GL_NONE),
    m_passSpacePosition (GL_NONE),
    m_texcoordCopy (GL_NONE),
    m_texcoordPass (GL_NONE),
    m_modelViewProjectionScreen (),
    m_modelViewProjectionPass (glm::mat4 (1.0)),
    m_modelViewProjectionCopy (),
    m_modelViewProjectionScreenInverse (),
    m_modelViewProjectionPassInverse (glm::inverse (m_modelViewProjectionPass)),
    m_modelViewProjectionCopyInverse (),
    m_modelMatrix(),
    m_viewProjectionMatrix(),
    m_image (image),
    m_material (nullptr),
    m_colorBlendMaterial (nullptr),
    m_pos (),
    m_animationTime (0.0),
    m_initialized (false) {
    auto projection = this->getScene ()->getScene ()->getOrthogonalProjection ();

    // get scene width and height to calculate positions
    auto scene_width = static_cast<float> (projection->getWidth ());
    auto scene_height = static_cast<float> (projection->getHeight ());

    glm::vec3 origin = this->getImage ()->getOrigin ();
    glm::vec2 size = this->getSize ();
    glm::vec3 scale = this->getImage ()->getScale ();

    // fullscreen layers should use the whole projection's size
    // TODO: WHAT SHOULD AUTOSIZE DO?
    if (this->getImage ()->isFullscreen ()) {
        size = {scene_width, scene_height};
        origin = {scene_width / 2, scene_height / 2, 0};

        // TODO: CHANGE ALIGNMENT TOO?
    }

    glm::vec2 scaledSize = size * glm::vec2 (scale);

    // calculate the center and shift from there
    this->m_pos.x = origin.x - (scaledSize.x / 2);
    this->m_pos.w = origin.y + (scaledSize.y / 2);
    this->m_pos.z = origin.x + (scaledSize.x / 2);
    this->m_pos.y = origin.y - (scaledSize.y / 2);

    if (this->getImage ()->getAlignment ().find ("top") != std::string::npos) {
        this->m_pos.y -= scaledSize.y / 2;
        this->m_pos.w -= scaledSize.y / 2;
    } else if (this->getImage ()->getAlignment ().find ("bottom") != std::string::npos) {
        this->m_pos.y += scaledSize.y / 2;
        this->m_pos.w += scaledSize.y / 2;
    }

    if (this->getImage ()->getAlignment ().find ("left") != std::string::npos) {
        this->m_pos.x += scaledSize.x / 2;
        this->m_pos.z += scaledSize.x / 2;
    } else if (this->getImage ()->getAlignment ().find ("right") != std::string::npos) {
        this->m_pos.x -= scaledSize.x / 2;
        this->m_pos.z -= scaledSize.x / 2;
    }

    // wallpaper engine
    this->m_pos.x -= scene_width / 2;
    this->m_pos.y = scene_height / 2 - this->m_pos.y;
    this->m_pos.z -= scene_width / 2;
    this->m_pos.w = scene_height / 2 - this->m_pos.w;

    // detect texture (if any)
    auto textures = (*this->m_image->getMaterial ()->getPasses ().begin ())->getTextures ();

    if (!textures.empty ()) {
        std::string textureName = textures.begin ()->second;

        if (textureName.find ("_rt_") == 0) {
            this->m_texture = this->getScene ()->findFBO (textureName);
        } else {
            // get the first texture on the first pass (this one represents the image assigned to this object)
            this->m_texture = this->getContext ().resolveTexture (textureName);
        }
    } else {
        if (this->m_image->getMaterial ()->isSolidLayer()) {
            size.x = scene_width;
            size.y = scene_height;
        }
        // if (this->m_image->isSolid ()) // layer receives cursor events: https://docs.wallpaperengine.io/en/scene/scenescript/reference/event/cursor.html
        // same applies to effects
        // TODO: create a dummy texture of correct size, fbo constructors should be enough, but this should be properly
        // handled
        this->m_texture = std::make_shared<CFBO> (
            "", ITexture::TextureFormat::ARGB8888, ITexture::TextureFlags::NoFlags, 1, size.x,
                  size.y, size.x, size.y);
    }

    // register both FBOs into the scene
    std::ostringstream nameA, nameB;

    // TODO: determine when _rt_imageLayerComposite and _rt_imageLayerAlbedo is used
    nameA << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_a";
    nameB << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_b";

    this->m_currentMainFBO = this->m_mainFBO =
        scene->createFBO (nameA.str (), ITexture::TextureFormat::ARGB8888, this->m_texture->getFlags (), 1,
                          size.x, size.y, size.x, size.y);
    this->m_currentSubFBO = this->m_subFBO =
        scene->createFBO (nameB.str (), ITexture::TextureFormat::ARGB8888, this->m_texture->getFlags (), 1,
                          size.x, size.y, size.x, size.y);

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat sceneSpacePosition [] = {this->m_pos.x, this->m_pos.y, 0.0f, this->m_pos.x, this->m_pos.w, 0.0f,
                                     this->m_pos.z, this->m_pos.y, 0.0f, this->m_pos.z, this->m_pos.y, 0.0f,
                                     this->m_pos.x, this->m_pos.w, 0.0f, this->m_pos.z, this->m_pos.w, 0.0f};

    float width = 1.0f;
    float height = 1.0f;

    if (this->getTexture ()->isAnimated ()) {
        // animated images use different coordinates as they're essentially a texture atlas
        width = static_cast<float> (this->getTexture ()->getRealWidth ()) /
                static_cast<float> (this->getTexture ()->getTextureWidth (0));
        height = static_cast<float> (this->getTexture ()->getRealHeight ()) /
                 static_cast<float> (this->getTexture ()->getTextureHeight (0));
    }
    // calculate the correct texCoord limits for the texture based on the texture screen size and real size
    else if (this->getTexture () != nullptr &&
             (this->getTexture ()->getTextureWidth (0) != this->getTexture ()->getRealWidth () ||
              this->getTexture ()->getTextureHeight (0) != this->getTexture ()->getRealHeight ())) {
        uint32_t x = 1;
        uint32_t y = 1;

        while (x < size.x)
            x <<= 1;
        while (y < size.y)
            y <<= 1;

        width = scaledSize.x / x;
        height = scaledSize.y / y;
    }

    // TODO: RECALCULATE THESE POSITIONS FOR PASSTHROUGH SO THEY TAKE THE RIGHT PART OF THE TEXTURE
    float x = 0.0f;
    float y = 0.0f;

    if (this->getTexture ()->isAnimated ()) {
        // animations should be copied completely
        x = 0.0f;
        y = 0.0f;
        width = 1.0f;
        height = 1.0f;
    }

    GLfloat realWidth = size.x;
    GLfloat realHeight = size.y;
    GLfloat realX = 0.0;
    GLfloat realY = 0.0;

    if (this->getImage ()->isPassthrough ()) {
        x = -((this->m_pos.x + (scene_width / 2)) / size.x);
        y = -((this->m_pos.w + (scene_height / 2)) / size.y);
        height = (this->m_pos.y + (scene_height / 2)) / size.y;
        width = (this->m_pos.z + (scene_width / 2)) / size.x;

        if (this->getImage ()->isFullscreen ()) {
            realX = -1.0;
            realY = -1.0;
            realWidth = 1.0;
            realHeight = 1.0;
        }
    }

    GLfloat texcoordCopy [] = {x, height, x, y, width, height, width, height, x, y, width, y};

    GLfloat copySpacePosition [] = {realX,     realHeight, 0.0f, realX, realY, 0.0f, realWidth, realHeight, 0.0f,
                                    realWidth, realHeight, 0.0f, realX, realY, 0.0f, realWidth, realY,      0.0f};

    GLfloat texcoordPass [] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    GLfloat passSpacePosition [] = {-1.0, 1.0, 0.0f, -1.0, -1.0, 0.0f, 1.0, 1.0,  0.0f,
                                    1.0,  1.0, 0.0f, -1.0, -1.0, 0.0f, 1.0, -1.0, 0.0f};

    // bind vertex list to the openGL buffers
    glGenBuffers (1, &this->m_sceneSpacePosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_sceneSpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (sceneSpacePosition), sceneSpacePosition, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_copySpacePosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_copySpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (copySpacePosition), copySpacePosition, GL_STATIC_DRAW);

    // bind pass' vertex list to the openGL buffers
    glGenBuffers (1, &this->m_passSpacePosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_passSpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (passSpacePosition), passSpacePosition, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_texcoordCopy);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texcoordCopy);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texcoordCopy), texcoordCopy, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_texcoordPass);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texcoordPass);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texcoordPass), texcoordPass, GL_STATIC_DRAW);

    this->m_modelViewProjectionScreen =
        this->getScene ()->getCamera ()->getProjection () * this->getScene ()->getCamera ()->getLookAt ();

    this->m_modelViewProjectionScreenInverse = glm::inverse (this->m_modelViewProjectionScreen);

    this->m_modelViewProjectionCopy = glm::ortho<float> (0.0, size.x, 0.0, size.y);
    this->m_modelViewProjectionCopyInverse = glm::inverse (this->m_modelViewProjectionCopy);
    this->m_modelMatrix = glm::ortho<float> (0.0, size.x, 0.0, size.y);
    this->m_viewProjectionMatrix = glm::mat4 (1.0);
}

void CImage::setup () {
    // do not double-init stuff, that's bad!
    if (this->m_initialized)
        return;

    // TODO: SUPPORT PASSTHROUGH (IT'S A SHADER)

    // passthrough images without effects are bad, do not draw them
    if (this->getImage ()->isPassthrough () && this->getImage ()->getEffects ().empty ())
        return;

    // generate the main material used to render the image
    this->m_material = new Effects::CMaterial (
        new CEffect (
            this,
            new Core::Objects::CEffect (
                "", "", "", "", this->m_image->getScene ()->getProject (),
                Core::UserSettings::CUserSettingBoolean::fromScalar (true),
                {}, {}, {})),
        this->m_image->getMaterial ()
    );

    // add blendmode to the combos
    for (const auto& cur : this->m_material->getPasses ())
        this->m_passes.push_back (cur);

    // prepare the passes list
    if (!this->getImage ()->getEffects ().empty ()) {
        // generate the effects used by this material
        for (const auto& cur : this->getImage ()->getEffects ()) {
            auto effect = new CEffect (this, cur);

            for (const auto& material : effect->getMaterials ())
                for (const auto& pass : material->getPasses ())
                    this->m_passes.push_back (pass);

            this->m_effects.push_back (effect);
        }
    }

    if (this->m_image->getColorBlendMode () > 0) {
        Core::Objects::Images::CMaterial::OverrideInfo overrides;

        overrides.combos.insert (std::pair ("BLENDMODE", this->m_image->getColorBlendMode ()));
        const auto material =
            Core::Objects::Images::CMaterial::fromFile ("materials/util/effectpassthrough.json", this->getContainer (), false, {}, &overrides);

        // generate the main material used to render the image
        this->m_colorBlendMaterial = new Effects::CMaterial (
            new CEffect (
                this,
                new Core::Objects::CEffect (
                    "", "", "", "", this->m_image->getScene ()->getProject (),
                    Core::UserSettings::CUserSettingBoolean::fromScalar (true), {}, {}, {}
                )
            ),
            material
        );

        // add blendmode to the combos
        for (const auto& cur : this->m_colorBlendMaterial->getPasses ())
            this->m_passes.push_back (cur);
    }

    // if there's more than one pass the blendmode has to be moved from the beginning to the end
    if (this->m_passes.size () > 1) {
        const auto first = this->m_passes.begin ();
        const auto last = this->m_passes.rbegin ();

        (*last)->setBlendingMode ((*first)->getBlendingMode ());
        (*first)->setBlendingMode ("normal");
    }

    // calculate full animation time (if any)
    this->m_animationTime = 0.0f;

    for (const auto& cur : this->getTexture ()->getFrames ())
        this->m_animationTime += cur->frametime;

    this->setupPasses ();
    this->m_initialized = true;
}

void CImage::setupPasses () {
    // do a pass on everything and setup proper inputs and values
    std::shared_ptr<const CFBO> drawTo = this->m_currentMainFBO;
    std::shared_ptr<const ITexture> asInput = this->getTexture ();
    GLuint texcoord = this->getTexCoordCopy ();

    auto cur = this->m_passes.begin ();
    auto end = this->m_passes.end ();
    bool first = true;

    for (; cur != end; ++cur) {
        // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
        Effects::CPass* pass = *cur;
        std::shared_ptr<const CFBO> prevDrawTo = drawTo;
        GLuint spacePosition = (first) ? this->getCopySpacePosition () : this->getPassSpacePosition ();
        const glm::mat4* projection = (first) ? &this->m_modelViewProjectionCopy : &this->m_modelViewProjectionPass;
        const glm::mat4* inverseProjection =
            (first) ? &this->m_modelViewProjectionCopyInverse : &this->m_modelViewProjectionPassInverse;
        first = false;

        pass->setModelMatrix (&this->m_modelMatrix);
        pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);

        // set viewport and target texture if needed
        if (pass->getMaterial ()->getMaterial ()->hasTarget ()) {
            // setup target texture
            std::string target = pass->getMaterial ()->getMaterial ()->getTarget ();
            drawTo = pass->getMaterial ()->getEffect ()->findFBO (target);
            // spacePosition = this->getPassSpacePosition ();

            // not a local fbo, try to find a scene fbo with the same name
            if (drawTo == nullptr)
                // this one throws if no fbo was found
                drawTo = this->getScene ()->findFBO (target);
        }
        // determine if it's the last element in the list as this is a screen-copy-like process
        else if (std::next (cur) == end && this->getImage ()->isVisible ()) {
            // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
            spacePosition = this->getSceneSpacePosition ();
            drawTo = this->getScene ()->getFBO ();
            projection = &this->m_modelViewProjectionScreen;
            inverseProjection = &this->m_modelViewProjectionScreenInverse;
        }

        pass->setDestination (drawTo);
        pass->setInput (asInput);
        pass->setPosition (spacePosition);
        pass->setTexCoord (texcoord);
        pass->setModelViewProjectionMatrix (projection);
        pass->setModelViewProjectionMatrixInverse (inverseProjection);

        texcoord = this->getTexCoordPass ();
        drawTo = prevDrawTo;

        if (!pass->getMaterial ()->getMaterial ()->hasTarget ())
            this->pinpongFramebuffer (&drawTo, &asInput);
    }
}

void CImage::pinpongFramebuffer (std::shared_ptr<const CFBO>* drawTo, std::shared_ptr<const ITexture>* asInput) {
    // temporarily store FBOs used
    std::shared_ptr<const CFBO> currentMainFBO = this->m_currentMainFBO;
    std::shared_ptr<const CFBO> currentSubFBO = this->m_currentSubFBO;

    if (drawTo != nullptr)
        *drawTo = currentSubFBO;
    if (asInput != nullptr)
        *asInput = currentMainFBO;

    // swap the FBOs
    this->m_currentMainFBO = currentSubFBO;
    this->m_currentSubFBO = currentMainFBO;
}

void CImage::render () {
    // do not try to render something that did not initialize successfully
    // non-visible materials do need to be rendered
    if (!this->m_initialized)
        return;

    if (!this->getImage()->isVisible())
        return;

    glColorMask (true, true, true, true);

    // update the position if required
    // TODO: There's more images that are not affected by parallax, autosize or fullscreen are not affected
    if (this->getScene ()->getScene ()->isCameraParallax () && !this->getImage ()->isFullscreen ())
        this->updateScreenSpacePosition ();

#if !NDEBUG
    std::string str = "Rendering ";

    if (this->getScene ()->getScene ()->isBloom () && this->getId () == -1)
        str += "bloom";
    else {
        str += this->getImage ()->getName () + " (" + std::to_string (this->getId ()) + ", " +
               this->getImage ()->getMaterial ()->getName () + ")";
    }

    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

    auto cur = this->m_passes.begin ();
    const auto end = this->m_passes.end ();

    for (; cur != end; ++cur) {
        // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
        if (std::next (cur) == end)
            glColorMask (true, true, true, false);

        (*cur)->render ();
    }

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */
}

void CImage::updateScreenSpacePosition () {
    const double parallaxAmount = this->getScene ()->getScene ()->getCameraParallaxAmount ();
    const glm::vec2 depth = this->getImage ()->getParallaxDepth ();
    const glm::vec2* displacement = this->getScene ()->getParallaxDisplacement ();

    float x = (depth.x + parallaxAmount) * displacement->x * this->getSize ().x;
    float y = (depth.y + parallaxAmount) * displacement->y * this->getSize ().x;

    this->m_modelViewProjectionScreen = glm::translate (this->getScene ()->getCamera ()->getProjection () *
                                                            this->getScene ()->getCamera ()->getLookAt (),
                                                        {x, y, 0.0f});
}

std::shared_ptr<const ITexture> CImage::getTexture () const {
    return this->m_texture;
}

double CImage::getAnimationTime () const {
    return this->m_animationTime;
}

const Core::Objects::CImage* CImage::getImage () const {
    return this->m_image;
}

const std::vector<CEffect*>& CImage::getEffects () const {
    return this->m_effects;
}

const Effects::CMaterial* CImage::getMaterial () const {
    return this->m_material;
}

glm::vec2 CImage::getSize () const {
    if (this->m_texture == nullptr)
        return this->getImage ()->getSize ();

    return {this->m_texture->getRealWidth (), this->m_texture->getRealHeight ()};
}

GLuint CImage::getSceneSpacePosition () const {
    return this->m_sceneSpacePosition;
}

GLuint CImage::getCopySpacePosition () const {
    return this->m_copySpacePosition;
}

GLuint CImage::getPassSpacePosition () const {
    return this->m_passSpacePosition;
}

GLuint CImage::getTexCoordCopy () const {
    return this->m_texcoordCopy;
}

GLuint CImage::getTexCoordPass () const {
    return this->m_texcoordPass;
}

const std::string CImage::Type = "image";