#ifndef __C_IMAGE_LOADER_TEX_H_INCLUDED__
#define __C_IMAGE_LOADER_TEX_H_INCLUDED__

#include <irrlicht/irrlicht.h>

namespace irr
{
namespace video
{

//!  Surface Loader for PNG files
class CImageLoaderTex : public IImageLoader
{
public:

	//! returns true if the file maybe is able to be loaded by this class
	//! based on the file extension (e.g. ".png")
	virtual bool isALoadableFileExtension(const io::path& filename) const;

	//! returns true if the file maybe is able to be loaded by this class
	virtual bool isALoadableFileFormat(io::IReadFile* file) const;

	//! creates a surface from the file
	virtual IImage* loadImage(io::IReadFile* input) const;

	virtual void loadImageFromARGB8Data (IImage* output, const char* input, u32 width, u32 height, u32 mipmap_width) const;

    virtual void loadImageFromDXT1 (IImage* output, const char* input, u32 destination_width, u32 destination_height, u32 origin_width, u32 origin_height) const;
    virtual void loadImageFromDXT3 (IImage* output, const char* input, u32 destination_width, u32 destination_height, u32 origin_width, u32 origin_height) const;
    virtual void loadImageFromDXT5 (IImage* output, const char* input, u32 destination_width, u32 destination_height, u32 origin_width, u32 origin_height) const;

private:
    void BlockDecompressImageDXT1(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const;
    void DecompressBlockDXT1(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const;
    unsigned long PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) const;
    void BlockDecompressImageDXT3(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const;
    void DecompressBlockDXT3(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const;
    void BlockDecompressImageDXT5(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const;
    void DecompressBlockDXT5(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const;

    enum TextureFormat
    {
        ARGB8888,
        RA88,
        A8,
        DXT1,
        DXT5, // most of the defaultprojects textures are DXT5 as indicated by their alpha data
        DXT3 = -1 // DXT3 support seems to be missing from wallpaper engine
    };

    // extracted from the free image library
    enum FREE_IMAGE_FORMAT
    {
        FIF_UNKNOWN = -1,
        FIF_BMP		= 0,
        FIF_ICO		= 1,
        FIF_JPEG	= 2,
        FIF_JNG		= 3,
        FIF_KOALA	= 4,
        FIF_LBM		= 5,
        FIF_IFF     = FIF_LBM,
        FIF_MNG		= 6,
        FIF_PBM		= 7,
        FIF_PBMRAW	= 8,
        FIF_PCD		= 9,
        FIF_PCX		= 10,
        FIF_PGM		= 11,
        FIF_PGMRAW	= 12,
        FIF_PNG		= 13,
        FIF_PPM		= 14,
        FIF_PPMRAW	= 15,
        FIF_RAS		= 16,
        FIF_TARGA	= 17,
        FIF_TIFF	= 18,
        FIF_WBMP	= 19,
        FIF_PSD		= 20,
        FIF_CUT		= 21,
        FIF_XBM		= 22,
        FIF_XPM		= 23,
        FIF_DDS		= 24,
        FIF_GIF     = 25,
        FIF_HDR		= 26,
        FIF_FAXG3	= 27,
        FIF_SGI		= 28,
        FIF_EXR		= 29,
        FIF_J2K		= 30,
        FIF_JP2		= 31,
        FIF_PFM		= 32,
        FIF_PICT	= 33,
        FIF_RAW		= 34,
        FIF_WEBP	= 35,
        FIF_JXR     = 36
    };
};


} // end namespace video
} // end namespace irr

#endif /* !__C_IMAGE_LOADER_TEX_H_INCLUDED__ */
