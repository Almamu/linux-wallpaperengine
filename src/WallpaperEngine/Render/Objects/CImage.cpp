#include <sstream>
#include "CImage.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image),
    m_texture (nullptr),
    m_initialized (false)
{
    auto projection = this->getScene ()->getScene ()->getOrthogonalProjection ();

    // get scene width and height to calculate positions
    auto scene_width = static_cast <float> (projection->getWidth ());
    auto scene_height = static_cast <float> (projection->getHeight ());

    glm::vec3 origin = this->getImage ()->getOrigin ();
    glm::vec2 size = this->getSize ();
    glm::vec3 scale = this->getImage ()->getScale ();
    glm::vec2 scaledSize = size * glm::vec2 (scale);

    // calculate the center and shift from there
    this->m_pos.x = origin.x - (scaledSize.x / 2);
    this->m_pos.w = origin.y + (scaledSize.y / 2);
    this->m_pos.z = origin.x + (scaledSize.x / 2);
    this->m_pos.y = origin.y - (scaledSize.y / 2);

    if (this->getImage ()->getAlignment ().find ("top") != std::string::npos)
    {
        this->m_pos.y -= scaledSize.y / 2;
        this->m_pos.w -= scaledSize.y / 2;
    }
    else if (this->getImage ()->getAlignment ().find ("bottom") != std::string::npos)
    {
        this->m_pos.y += scaledSize.y / 2;
        this->m_pos.w += scaledSize.y / 2;
    }

    if (this->getImage ()->getAlignment ().find ("left") != std::string::npos)
    {
        this->m_pos.x += scaledSize.x / 2;
        this->m_pos.z += scaledSize.x / 2;
    }
    else if (this->getImage ()->getAlignment ().find ("right") != std::string::npos)
    {
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

    if (textures.empty() == false)
    {
        std::string textureName = *textures.begin ();

        if (textureName.find ("_rt_") == 0)
        {
            this->m_texture = this->getScene ()->findFBO (textureName);
        }
        else
        {
            // get the first texture on the first pass (this one represents the image assigned to this object)
            this->m_texture = this->getScene ()->getContext ()->resolveTexture (textureName);
        }
    }
    else
    {
        glm::vec2 realSize = size * glm::vec2 (scale);

        // TODO: create a dummy texture of correct size, fbo constructors should be enough, but this should be properly handled
        this->m_texture = new CFBO (
            "",
            ITexture::TextureFormat::ARGB8888,
            ITexture::TextureFlags::NoFlags,
            1,
            realSize.x, realSize.y,
            realSize.x, realSize.y
        );
    }

    // register both FBOs into the scene
    std::ostringstream nameA, nameB;

    nameA << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_a";
    nameB << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_b";

    this->m_currentMainFBO = this->m_mainFBO = scene->createFBO (
        nameA.str (),
        ITexture::TextureFormat::ARGB8888,
        this->m_texture->getFlags (),
        1,
        this->m_texture->getRealWidth (), this->m_texture->getRealHeight (),
        this->m_texture->getRealWidth (), this->m_texture->getRealHeight ()
    );
    this->m_currentSubFBO = this->m_subFBO = scene->createFBO (
        nameB.str (),
        ITexture::TextureFormat::ARGB8888,
        this->m_texture->getFlags (),
        1,
        this->m_texture->getRealWidth (), this->m_texture->getRealHeight (),
        this->m_texture->getRealWidth (), this->m_texture->getRealHeight ()
    );

    GLfloat realWidth = this->m_texture->getRealWidth ();
    GLfloat realHeight = this->m_texture->getRealHeight ();

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat sceneSpacePosition [] = {
        this->m_pos.x, this->m_pos.y, 0.0f,
        this->m_pos.x, this->m_pos.w, 0.0f,
        this->m_pos.z, this->m_pos.y, 0.0f,
        this->m_pos.z, this->m_pos.y, 0.0f,
        this->m_pos.x, this->m_pos.w, 0.0f,
        this->m_pos.z, this->m_pos.w, 0.0f
    };

    GLfloat copySpacePosition [] = {
        0.0, realHeight, 0.0f,
        0.0, 0.0, 0.0f,
        realWidth, realHeight, 0.0f,
        realWidth, realHeight, 0.0f,
        0.0, 0.0, 0.0f,
        realWidth, 0.0, 0.0f
    };

    GLfloat passSpacePosition [] = {
        -1.0, 1.0, 0.0f,
        -1.0, -1.0, 0.0f,
        1.0, 1.0, 0.0f,
        1.0, 1.0, 0.0f,
        -1.0, -1.0, 0.0f,
        1.0, -1.0, 0.0f
    };

    float width = 1.0f;
    float height = 1.0f;

    if (this->getTexture ()->isAnimated () == true)
    {
        // animated images use different coordinates as they're essentially a texture atlas
        width = static_cast<float> (this->getTexture ()->getRealWidth ()) / static_cast<float> (this->getTexture ()->getTextureWidth ());
        height = static_cast<float> (this->getTexture ()->getRealHeight ()) / static_cast<float> (this->getTexture ()->getTextureHeight ());
    }
    // calculate the correct texCoord limits for the texture based on the texture screen size and real size
    else if (this->getTexture () != nullptr &&
            (this->getTexture ()->getTextureWidth () != this->getTexture ()->getRealWidth () ||
             this->getTexture ()->getTextureHeight () != this->getTexture ()->getRealHeight ())
        )
    {
        uint32_t x = 1;
        uint32_t y = 1;

        while (x < size.x) x <<= 1;
        while (y < size.y) y <<= 1;

        width = scaledSize.x / x;
        height = scaledSize.y / y;
    }

    float x = 0.0f;
    float y = 0.0f;

    if (this->getTexture ()->isAnimated () == true)
    {
        // animations should be copied completely
        x = 0.0f;
        y = 0.0f;
        width = 1.0f;
        height = 1.0f;
    }

    GLfloat texcoordCopy [] = {
        x, height,
        x, y,
        width, height,
        width, height,
        x, y,
        width, y
    };

    GLfloat texcoordPass [] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

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
            this->getScene ()->getCamera ()->getProjection () *
            this->getScene ()->getCamera ()->getLookAt ();

    this->m_modelViewProjectionPass = glm::mat4(1.0f);
    this->m_modelViewProjectionCopy = glm::ortho <float> (0.0, size.x, 0.0, size.y);
}

void CImage::setup ()
{
    // do not double-init stuff, that's bad!
    if (this->m_initialized)
        return;

    // TODO: SUPPORT PASSTHROUGH (IT'S A SHADER)

    {
        // generate the main material used to render the image
        this->m_material = new Effects::CMaterial (
            new CEffect (this, new Core::Objects::CEffect ("", "", "", "", this->m_image, Core::UserSettings::CUserSettingBoolean::fromScalar (true))),
            this->m_image->getMaterial ()
        );

        // add blendmode to the combos
        for (const auto& cur : this->m_material->getPasses ())
            this->m_passes.push_back (cur);
    }

    // prepare the passes list
    if (this->getImage ()->getEffects ().empty () == false)
    {
        // generate the effects used by this material
        for (const auto& cur : this->getImage ()->getEffects ())
        {
            auto effect = new CEffect (this, cur);

            for (const auto& material : effect->getMaterials ())
                for (const auto& pass : material->getPasses ())
                    this->m_passes.push_back (pass);

            this->m_effects.push_back (effect);
        }
    }

    if (this->m_image->getColorBlendMode () > 0)
    {
        auto material = Core::Objects::Images::CMaterial::fromFile ("materials/util/effectpassthrough.json", this->getContainer ());

        // effectpasshthrough only has one pass
        (*material->getPasses ().begin ())->insertCombo ("BLENDMODE", this->m_image->getColorBlendMode ());

        // generate the main material used to render the image
        this->m_colorBlendMaterial = new Effects::CMaterial(
            new CEffect (this, new Core::Objects::CEffect ("", "", "", "", this->m_image, Core::UserSettings::CUserSettingBoolean::fromScalar (true))),
            material
        );

        // add blendmode to the combos
        for (const auto& cur : this->m_colorBlendMaterial->getPasses ())
            this->m_passes.push_back (cur);
    }

    // if there's more than one pass the blendmode has to be moved from the beginning to the end
    if (this->m_passes.size () > 1)
    {
        auto first = this->m_passes.begin ();
        auto last = this->m_passes.rbegin ();

        (*last)->getPass ()->setBlendingMode ((*first)->getPass ()->getBlendingMode ());
        (*first)->getPass ()->setBlendingMode ("normal");
    }

    // calculate full animation time (if any)
    this->m_animationTime = 0.0f;

    for (const auto& cur : this->getTexture ()->getFrames ())
        this->m_animationTime += cur->frametime;

    this->setupPasses ();
    this->m_initialized = true;
}

void CImage::setupPasses ()
{
    // do a pass on everything and setup proper inputs and values
    const CFBO* drawTo = this->m_currentMainFBO;
    const ITexture* asInput = this->getTexture ();
    GLuint texcoord = this->getTexCoordCopy ();

    auto cur = this->m_passes.begin ();
    auto end = this->m_passes.end ();
    bool first = true;

    for (; cur != end; cur ++)
    {
        // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
        Effects::CPass* pass = *cur;
        const CFBO* prevDrawTo = drawTo;
        GLuint spacePosition = (first) ? this->getCopySpacePosition () : this->getPassSpacePosition ();
        glm::mat4* projection = (first) ? &this->m_modelViewProjectionCopy : &this->m_modelViewProjectionPass;
        first = false;

        // set viewport and target texture if needed
        if (pass->getMaterial ()->getMaterial ()->hasTarget () == true)
        {
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
        else if (std::next (cur) == end && this->getImage ()->isVisible () == true)
        {
            // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
            spacePosition = this->getSceneSpacePosition ();
            drawTo = this->getScene ()->getFBO ();
            projection = &this->m_modelViewProjectionScreen;
        }

        pass->setDestination (drawTo);
        pass->setInput (asInput);
        pass->setPosition (spacePosition);
        pass->setTexCoord (texcoord);
        pass->setModelViewProjectionMatrix (projection);

        texcoord = this->getTexCoordPass ();
        drawTo = prevDrawTo;

        if (pass->getMaterial ()->getMaterial ()->hasTarget () == false)
            this->pinpongFramebuffer (&drawTo, &asInput);
    }
}

void CImage::pinpongFramebuffer (const CFBO** drawTo, const ITexture** asInput)
{
    // temporarily store FBOs used
    CFBO* currentMainFBO = this->m_currentMainFBO;
    CFBO* currentSubFBO = this->m_currentSubFBO;

    if (drawTo != nullptr)
        *drawTo = currentSubFBO;
    if (asInput != nullptr)
        *asInput = currentMainFBO;

    // swap the FBOs
    this->m_currentMainFBO = currentSubFBO;
    this->m_currentSubFBO = currentMainFBO;
}

void CImage::render ()
{
    // do not try to render something that did not initialize successfully
    if (this->m_initialized == false)
        return;

    glColorMask (true, true, true, true);

    // update the position if required
    if (this->getScene ()->getScene ()->isCameraParallax () == true)
        this->updateScreenSpacePosition ();

    if (DEBUG)
    {
        std::string str = "Rendering ";

        if (this->getScene ()->getScene ()->isBloom () && this->getId () == 0xFFFFFFFF)
            str += "bloom";
        else
        {
            str += this->getImage ()->getName () +
                   " (" + std::to_string (this->getId ()) + ", " + this->getImage ()->getMaterial ()->getName () + ")";
        }

        glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
    }

    auto cur = this->m_passes.begin ();
    auto end = this->m_passes.end ();

    for (; cur != end; cur ++)
    {
        // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
        if (std::next (cur) == end)
            glColorMask (true, true, true, false);

        (*cur)->render ();
    }

    if (DEBUG)
        glPopDebugGroup ();
}

void CImage::updateScreenSpacePosition ()
{
    double parallaxAmount = this->getScene ()->getScene ()->getCameraParallaxAmount ();
    glm::vec2 depth = this->getImage ()->getParallaxDepth ();
    glm::vec2* displacement = this->getScene ()->getParallaxDisplacement ();

    // no need to update if the depth is 0
    if (depth.x == 0.0 && depth.y == 0.0)
        return;

    float x = (depth.x + parallaxAmount) * displacement->x * this->getSize ().x;
    float y = (depth.y + parallaxAmount) * displacement->y * this->getSize ().x;
    this->m_modelViewProjectionScreen =
        glm::translate (
            this->getScene ()->getCamera ()->getProjection () *
            this->getScene ()->getCamera ()->getLookAt (),
            {x, y, 0.0f}
        );
}

const ITexture* CImage::getTexture () const
{
    return this->m_texture;
}

const double CImage::getAnimationTime () const
{
    return this->m_animationTime;
}

const Core::Objects::CImage* CImage::getImage () const
{
    return this->m_image;
}

const std::vector<CEffect*>& CImage::getEffects () const
{
    return this->m_effects;
}

const glm::vec2 CImage::getSize() const
{
    if (this->m_texture == nullptr)
        return this->getImage ()->getSize ();

    return {this->m_texture->getRealWidth (), this->m_texture->getRealHeight ()};
}

const GLuint CImage::getSceneSpacePosition () const
{
    return this->m_sceneSpacePosition;
}

const GLuint CImage::getCopySpacePosition () const
{
    return this->m_copySpacePosition;
}

const GLuint CImage::getPassSpacePosition () const
{
    return this->m_passSpacePosition;
}

const GLuint CImage::getTexCoordCopy () const
{
    return this->m_texcoordCopy;
}

const GLuint CImage::getTexCoordPass () const
{
    return this->m_texcoordPass;
}

const std::string CImage::Type = "image";