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

    float xleft = 0.0f;
    float ytop = 0.0f;
    float xright = 0.0f;
    float ybottom = 0.0f;

    // depending on the alignment these values might change, for now just support center
    if (this->getImage ()->getAlignment () == "center")
    {
        // calculate the real position of the image
        xleft = (-scene_width / 2) + (origin.x - (size.x * scale.x / 2));
        xright = (-scene_width / 2) + (origin.x + (size.x * scale.x / 2));
        ytop = (-scene_height / 2) + origin.y + (size.y * scale.y / 2);
        ybottom = (-scene_height / 2) + (origin.y - (size.y * scale.y / 2));
    }
    else
    {
        throw std::runtime_error ("Only centered images are supported for now!");
    }

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
            this->m_texture = this->getScene ()->getContainer ()->readTexture (textureName);
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

    GLfloat realWidth = this->m_texture->getRealWidth () / 2.0f;
    GLfloat realHeight = this->m_texture->getRealHeight () / 2.0f;

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat sceneSpacePosition [] = {
        xleft, ytop, 0.0f,
        xright, ytop, 0.0f,
        xleft, ybottom, 0.0f,
        xleft, ybottom, 0.0f,
        xright, ytop, 0.0f,
        xright, ybottom, 0.0f
    };

    GLfloat copySpacePosition [] = {
        -realWidth, -realHeight, 0.0f,
        realWidth, -realHeight, 0.0f,
        -realWidth, realHeight, 0.0f,
        -realWidth, realHeight, 0.0f,
        realWidth, -realHeight, 0.0f,
        realWidth, realHeight, 0.0f
    };

    GLfloat passSpacePosition [] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f
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

        width = size.x * scale.x / x;
        height = size.y * scale.y / y;
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
        x, y,
        width, y,
        x, height,
        x, height,
        width, y,
        width, height
    };

    GLfloat texcoordPass [] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
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

    this->m_modelViewProjectionPass =
            glm::ortho<float> (-size.x / 2, size.x / 2, -size.y / 2, size.y / 2, 0, 10000);
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
            new CEffect (this, new Core::Objects::CEffect ("", "", "", "", this->m_image)),
            this->m_image->getMaterial ()
        );

        // add blendmode to the combos
        auto cur = this->m_material->getPasses ().begin ();
        auto end = this->m_material->getPasses ().end ();

        for (; cur != end; cur ++)
            this->m_passes.push_back (*cur);
    }

    // prepare the passes list
    if (this->getImage ()->getEffects ().empty () == false)
    {
        // generate the effects used by this material
        auto cur = this->getImage ()->getEffects ().begin ();
        auto end = this->getImage ()->getEffects ().end ();

        for (; cur != end; cur ++)
        {
            auto effect = new CEffect (this, *cur);
            auto materialCur = effect->getMaterials ().begin ();
            auto materialEnd = effect->getMaterials ().end ();

            for (; materialCur != materialEnd; materialCur ++)
            {
                auto passCur = (*materialCur)->getPasses ().begin ();
                auto passEnd = (*materialCur)->getPasses ().end ();

                for (; passCur != passEnd; passCur ++)
                    this->m_passes.push_back (*passCur);
            }

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
            new CEffect (this, new Core::Objects::CEffect ("", "", "", "", this->m_image)),
            material
        );

        // add blendmode to the combos
        auto cur = this->m_colorBlendMaterial->getPasses ().begin ();
        auto end = this->m_colorBlendMaterial->getPasses ().end ();

        for (; cur != end; cur ++)
            this->m_passes.push_back (*cur);
    }

    // if there's more than one pass the blendmode has to be moved from the beginning to the end
    if (this->m_passes.size () > 1)
    {
        auto first = this->m_passes.begin ();
        auto last = this->m_passes.rbegin ();

        (*last)->getPass ()->setBlendingMode ((*first)->getPass ()->getBlendingMode ());
        (*first)->getPass ()->setBlendingMode ("normal");
    }

    {
        // calculate full animation time (if any)
        this->m_animationTime = 0.0f;

        auto cur = this->getTexture ()->getFrames ().begin ();
        auto end = this->getTexture ()->getFrames ().end ();

        for (; cur != end; cur++)
            this->m_animationTime += (*cur)->frametime;
    }

    this->m_initialized = true;
}

void CImage::pinpongFramebuffer (CFBO** drawTo, ITexture** asInput)
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

    // reset the framebuffers so the drawing always happens on the same order
    this->m_currentMainFBO = this->m_mainFBO;
    this->m_currentSubFBO = this->m_subFBO;

    glColorMask (true, true, true, true);

    // start drawing to the main framebuffer
    CFBO* drawTo = this->m_currentMainFBO;
    ITexture* asInput = this->getTexture ();
    GLuint texcoord = *this->getTexCoordCopy ();

    auto cur = this->m_passes.begin ();
    auto end = this->m_passes.end ();

    for (; cur != end; cur ++)
    {
        Effects::CPass* pass = *cur;
        CFBO* prevDrawTo = drawTo;
        GLuint spacePosition = *this->getCopySpacePosition ();
        glm::mat4 projection = this->m_modelViewProjectionPass;

        // set viewport and target texture if needed
        if (pass->getMaterial ()->getMaterial ()->hasTarget () == true)
        {
            // setup target texture
            std::string target = pass->getMaterial ()->getMaterial ()->getTarget ();
            drawTo = pass->getMaterial ()->getEffect ()->findFBO (target);
            spacePosition = *this->getPassSpacePosition ();

            // not a local fbo, try to find a scene fbo with the same name
            if (drawTo == nullptr)
                // this one throws if no fbo was found
                drawTo = this->getScene ()->findFBO (target);
        }

        // determine if it's the last element in the list as this is a screen-copy-like process
        else if (std::next (cur) == end && this->getImage ()->isVisible () == true)
        {
            spacePosition = *this->getSceneSpacePosition ();
            drawTo = this->getScene ()->getFBO ();
            projection = this->m_modelViewProjectionScreen;

            glColorMask (true, true, true, false);
        }

        pass->render (drawTo, asInput, spacePosition, texcoord, projection);

        texcoord = *this->getTexCoordPass ();
        drawTo = prevDrawTo;

        if (pass->getMaterial ()->getMaterial ()->hasTarget () == false)
            this->pinpongFramebuffer (&drawTo, &asInput);
    }
}

ITexture* CImage::getTexture () const
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

const GLuint* CImage::getSceneSpacePosition () const
{
    return &this->m_sceneSpacePosition;
}

const GLuint* CImage::getCopySpacePosition () const
{
    return &this->m_copySpacePosition;
}

const GLuint* CImage::getPassSpacePosition () const
{
    return &this->m_passSpacePosition;
}

const GLuint* CImage::getTexCoordCopy () const
{
    return &this->m_texcoordCopy;
}

const GLuint* CImage::getTexCoordPass () const
{
    return &this->m_texcoordPass;
}

const std::string CImage::Type = "image";