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
    nameA << "_rt_imageLayerComposite_" << this->getImage ()->getId () << "_b";

    this->m_mainFBO = scene->createFBO (nameA.str (), ITexture::TextureFormat::ARGB8888, 1, this->m_texture->getRealWidth (), this->m_texture->getRealHeight (), this->m_texture->getTextureWidth (), this->m_texture->getTextureHeight ());
    this->m_subFBO = scene->createFBO (nameB.str (), ITexture::TextureFormat::ARGB8888, 1, this->m_texture->getRealWidth (), this->m_texture->getRealHeight (), this->m_texture->getTextureWidth (), this->m_texture->getTextureHeight ());

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
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
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

void CImage::pinpongFramebuffer (GLuint* drawTo, GLuint* inputTexture)
{
    // temporarily store FBOs used
    CFBO* currentMainFBO = this->m_mainFBO;
    CFBO* currentSubFBO = this->m_subFBO;

    if (drawTo != nullptr)
        *drawTo = currentSubFBO->getFramebuffer ();
    if (inputTexture != nullptr)
        *inputTexture = currentMainFBO->getTextureID ();

    // swap the FBOs
    this->m_mainFBO = currentSubFBO;
    this->m_subFBO = currentMainFBO;
}

void CImage::render ()
{
    // start drawing to the main framebuffer
    GLuint drawTo = this->m_mainFBO->getFramebuffer ();
    GLuint inputTexture = this->getTexture ()->getTextureID ();

    // render all the other materials
    auto cur = this->getEffects ().begin ();
    auto end = this->getEffects ().end ();

    inputTexture = this->getTexture ()->getTextureID ();

    // set the correct viewport
    glViewport (0, 0, this->getTexture ()->getRealWidth (), this->getTexture ()->getRealHeight ());

    // render all the passes first
    for (; cur != end; cur ++)
    {
        auto materialCur = (*cur)->getMaterials ().begin ();
        auto materialEnd = (*cur)->getMaterials ().end ();

        for (; materialCur != materialEnd; materialCur ++)
        {
            auto passCur = (*materialCur)->getPasses ().begin ();
            auto passEnd = (*materialCur)->getPasses ().end ();

            for (; passCur != passEnd; passCur ++)
            {
                (*passCur)->render (drawTo, inputTexture);

                this->pinpongFramebuffer (&drawTo, &inputTexture);
            }
        }
    }

    if (this->getImage ()->isVisible () == false)
        return;

    // this material only has one pass that we know of
    // so just take that and render it to the screen's framebuffer
    auto pass = this->m_material->getPasses ().begin ();
    auto projection = this->getScene ()->getScene ()->getOrthogonalProjection ();

    // set the viewport properly
    glViewport (0, 0, projection->getWidth (), projection->getHeight ());

    (*pass)->render (this->getScene ()->getWallpaperFramebuffer (), inputTexture);
}

const ITexture* CImage::getTexture () const
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