#include <sstream>
#include "CImage.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image),
    m_texture (nullptr)
{
    auto projection = this->getScene ()->getScene ()->getOrthogonalProjection ();

    // get scene width and height to calculate positions
    auto scene_width = static_cast <float> (projection->getWidth ());
    auto scene_height = static_cast <float> (projection->getHeight ());

    float xleft = 0.0f;
    float ytop = 0.0f;
    float xright = 0.0f;
    float ybottom = 0.0f;

    // TODO: TAKE INTO ACCOUNT SCALE
    // depending on the alignment these values might change, for now just support center
    if (this->getImage ()->getAlignment () == "center")
    {
        glm::vec2 size = this->getImage ()->getSize ();
        glm::vec3 scale = this->getImage ()->getScale ();

        // calculate the real position of the image
        xleft = this->getImage ()->getOrigin ().x - (size.x * scale.x / 2);
        xright = this->getImage ()->getOrigin ().x + (size.x * scale.x / 2);
        ytop = this->getImage ()->getOrigin ().y - (size.y * scale.y / 2);
        ybottom = this->getImage ()->getOrigin ().y + (size.y * scale.y / 2);
    }
    else
    {
        throw std::runtime_error ("Only centered images are supported for now!");
    }

    std::string textureName = (*(*this->m_image->getMaterial ()->getPasses ().begin ())->getTextures ().begin ());

    if (textureName.find ("_rt_") == 0)
    {
        this->m_texture = this->getScene ()->findFBO (textureName);
    }
    else
    {
        // get the first texture on the first pass (this one represents the image assigned to this object)
        this->m_texture = this->getScene ()->getContainer ()->readTexture (textureName);
    }

    // register both FBOs into the scene
    std::ostringstream nameA, nameB;

    nameA << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_a";
    nameB << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_b";

    this->m_currentMainFBO = this->m_mainFBO = scene->createFBO (nameA.str (), ITexture::TextureFormat::ARGB8888, 1, this->m_texture->getRealWidth (), this->m_texture->getRealHeight (), this->m_texture->getTextureWidth (), this->m_texture->getTextureHeight ());
    this->m_currentSubFBO = this->m_subFBO = scene->createFBO (nameB.str (), ITexture::TextureFormat::ARGB8888, 1, this->m_texture->getRealWidth (), this->m_texture->getRealHeight (), this->m_texture->getTextureWidth (), this->m_texture->getTextureHeight ());

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat data [] = {
        xleft, ytop, 0.0f,
        xright, ytop, 0.0f,
        xleft, ybottom, 0.0f,
        xleft, ybottom, 0.0f,
        xright, ytop, 0.0f,
        xright, ybottom, 0.0f
    };

    memcpy (this->m_vertexList, data, sizeof (data));

    GLfloat data1 [] = {
        0.0f, 0.0f, 0.0f,
        scene_width, 0.0f, 0.0f,
        0.0f, scene_height, 0.0f,
        0.0f, scene_height, 0.0f,
        scene_width, 0.0f, 0.0f,
        scene_width, scene_height, 0.0f
    };

    memcpy (this->m_passesVertexList, data1, sizeof (data1));

    float width = 1.0f;
    float height = 1.0f;

    // calculate the correct texCoord limits for the texture based on the texture screen size and real size
    if (this->getTexture () != nullptr &&
            (this->getTexture ()->getTextureWidth () != this->getTexture ()->getRealWidth () ||
             this->getTexture ()->getTextureHeight () != this->getTexture ()->getRealHeight ())
        )
    {
        uint32_t x = 1;
        uint32_t y = 1;
        glm::vec2 size = this->getImage ()->getSize ();
        glm::vec3 scale = this->getImage ()->getScale ();

        while (x < size.x) x <<= 1;
        while (y < size.y) y <<= 1;

        width = size.x * scale.x / x;
        height = size.y * scale.y / y;
    }

    GLfloat data2 [] = {
        0.0f, 0.0f,
        width, 0.0f,
        0.0f, height,
        0.0f, height,
        width, 0.0f,
        width, height
    };

    memcpy (this->m_texCoordList, data2, sizeof (data2));

    GLfloat data3 [] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
    };

    memcpy (this->m_passTexCoordList, data3, sizeof (data3));

    // bind vertex list to the openGL buffers
    glGenBuffers (1, &this->m_vertexBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_vertexBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_vertexList), this->m_vertexList, GL_STATIC_DRAW);

    // bind pass' vertex list to the openGL buffers
    glGenBuffers (1, &this->m_passesVertexBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_passesVertexBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_passesVertexList), this->m_passesVertexList, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_texCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_texCoordList), this->m_texCoordList, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_passTexCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_passTexCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (this->m_passTexCoordList), this->m_passTexCoordList, GL_STATIC_DRAW);
}

void CImage::setup ()
{
    // generate the main material used to render the image
    this->m_material = new Effects::CMaterial (
        new CEffect (this, new Core::Objects::CEffect ("", "", "", "", this->m_image)),
        this->m_image->getMaterial ()
    );

    // generate the effects used by this material
    auto cur = this->getImage ()->getEffects ().begin ();
    auto end = this->getImage ()->getEffects ().end ();

    for (; cur != end; cur ++)
        this->m_effects.emplace_back (new CEffect (this, *cur));
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

void CImage::simpleRender ()
{
    // a simple material renders directly to the screen
    auto cur = this->m_material->getPasses ().begin ();
    auto end = this->m_material->getPasses ().end ();

    for (; cur != end; cur ++)
        (*cur)->render (this->getScene ()->getFBO (), this->getTexture (), false);
}

void CImage::complexRender ()
{
    // start drawing to the main framebuffer
    CFBO* drawTo = this->m_mainFBO;
    ITexture* asInput = this->getTexture ();

    // do the first pass render into the main framebuffer
    auto cur = this->m_material->getPasses ().begin ();
    auto end = this->m_material->getPasses ().end ();

    for (; cur != end; cur ++)
        (*cur)->render (drawTo, asInput, false);

    // render all the other materials
    auto effectCur = this->getEffects ().begin ();
    auto effectEnd = this->getEffects ().end ();

    for (; effectCur != effectEnd; effectCur ++)
    {
        auto materialCur = (*effectCur)->getMaterials ().begin ();
        auto materialEnd = (*effectCur)->getMaterials ().end ();

        for (; materialCur != materialEnd; materialCur ++)
        {
            // set viewport and target texture if needed
            if ((*materialCur)->getMaterial ()->hasTarget () == true)
            {
                // setup target texture
                std::string target = (*materialCur)->getMaterial ()->getTarget ();
                drawTo = (*effectCur)->findFBO (target);

                // not a local FBO, so try that one now
                if (drawTo == nullptr)
                    // this one throws if no fbo was found
                    drawTo = this->getScene ()->findFBO (target);
            }

            auto passCur = (*materialCur)->getPasses ().begin ();
            auto passEnd = (*materialCur)->getPasses ().end ();

            for (; passCur != passEnd; passCur ++)
            {
                // ping-pong only if there's a target
                if ((*materialCur)->getMaterial ()->hasTarget () == false)
                    this->pinpongFramebuffer (&drawTo, &asInput);

                (*passCur)->render (drawTo, asInput, (*materialCur)->getMaterial ()->hasTarget ());
            }
        }
    }

    if (this->getImage ()->isVisible () == false)
        return;

    // pinpong the framebuffer so we know exactly what we're drawing to the scene
    this->pinpongFramebuffer (&drawTo, &asInput);

    // final step, this one might need more changes, should passes render directly to the output instead of an intermediate framebuffer?
    // do the first pass render into the main framebuffer
    cur = this->m_material->getPasses ().begin ();
    end = this->m_material->getPasses ().end ();

    glColorMask (true, true, true, false);

    for (; cur != end; cur ++)
        (*cur)->render (this->getScene ()->getFBO (), asInput, false);
}

void CImage::render ()
{
    // first and foremost reset the framebuffer switching
    this->m_currentMainFBO = this->m_mainFBO;
    this->m_currentSubFBO = this->m_subFBO;

    glColorMask (true, true, true, true);

    // check if there's more than one pass and do different things based on that
    if (this->m_effects.empty () == true)
        this->simpleRender ();
    else
        this->complexRender ();
}

ITexture* CImage::getTexture () const
{
    return this->m_texture;
}

const Core::Objects::CImage* CImage::getImage () const
{
    return this->m_image;
}

const std::vector<CEffect*>& CImage::getEffects () const
{
    return this->m_effects;
}

const GLfloat* CImage::getVertex () const
{
    return this->m_vertexList;
}

const GLuint* CImage::getVertexBuffer () const
{
    return &this->m_vertexBuffer;
}

const GLuint* CImage::getPassVertexBuffer () const
{
    return &this->m_passesVertexBuffer;
}

const GLuint* CImage::getTexCoordBuffer () const
{
    return &this->m_texCoordBuffer;
}

const GLuint* CImage::getPassTexCoordBuffer () const
{
    return &this->m_passTexCoordBuffer;
}

const std::string CImage::Type = "image";