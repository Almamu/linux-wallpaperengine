#include "CPackage.h"
#include "CAssetLoadException.h"
#include "CPackageLoadException.h"

#include <utility>
#include <sstream>

using namespace WallpaperEngine::Assets;

class CPackageEntry
{
public:
    CPackageEntry (std::string filename, uint32_t offset, uint32_t length) :
        filename (std::move(filename)),
        offset (offset),
        length (length) { }

    std::string filename;
    uint32_t offset;
    uint32_t length;
};

CPackage::CPackage (const std::string& path) :
    m_path (path),
    m_contents ()
{
    this->init ();
}

CPackage::~CPackage()
{

}


void* CPackage::readFile (std::string filename, uint32_t* length)
{
    auto it = this->m_contents.find (filename);

    if (it == this->m_contents.end ())
        throw CAssetLoadException(filename, "Cannot find the file in the package");

    // set file length if required
    if (length != nullptr)
        *length = (*it).second.length;

    return (*it).second.address;
}

void CPackage::init ()
{
    FILE* fp = fopen (this->m_path.c_str (), "rb+");

    if (fp == nullptr)
        throw CPackageLoadException (this->m_path, std::to_string (errno));

    // first validate header
    this->validateHeader (fp);
    // header is okay, load everything into memory
    this->loadFiles (fp);

    fclose (fp);
}

char* CPackage::readSizedString (FILE* fp)
{
    unsigned int length = 0;

    if (fread (&length, sizeof (unsigned int), 1, fp) != 1)
        throw std::runtime_error ("Cannot read enough bytes from file");

    // account for 0 termination of the string
    length ++;

    char* pointer = new char [length];
    memset (pointer, 0, length);

    // read only the string bytes so the last one in the memory is 0
    length --;

    // read data from file
    if (fread (pointer, sizeof (char), length, fp) != length)
        throw std::runtime_error ("Cannot read package version from file");

    return pointer;
}

uint32_t CPackage::readInteger (FILE* fp)
{
    uint32_t output;

    if (fread (&output, sizeof (uint32_t), 1, fp) != 1)
        throw std::runtime_error ("Cannot read integer value from file");

    return output;
}

void CPackage::validateHeader (FILE* fp)
{
    char* pointer = this->readSizedString (fp);

    // finally validate the header version
    if (strcmp ("PKGV0001", pointer) != 0 &&
        strcmp ("PKGV0002", pointer) != 0 &&
        strcmp ("PKGV0003", pointer) != 0 &&
        strcmp ("PKGV0004", pointer) != 0 &&
        strcmp ("PKGV0005", pointer) != 0 &&
        strcmp ("PKGV0006", pointer) != 0 &&
        strcmp ("PKGV0007", pointer) != 0 &&
        strcmp ("PKGV0008", pointer) != 0 &&
        strcmp ("PKGV0009", pointer) != 0 &&
        strcmp ("PKGV0010", pointer) != 0 &&
        strcmp ("PKGV0012", pointer) != 0 &&
        strcmp ("PKGV0013", pointer) != 0 &&
        strcmp ("PKGV0014", pointer) != 0 &&
        strcmp ("PKGV0015", pointer) != 0 &&
        strcmp ("PKGV0016", pointer) != 0)
    {
        std::stringstream msg;
        msg << "Unsupported package version: " << pointer;
        delete[] pointer;
        throw std::runtime_error (msg.str ());
    }

    // free memory
    delete[] pointer;
}

void CPackage::loadFiles (FILE* fp)
{
    uint32_t count = this->readInteger (fp);
    std::vector<CPackageEntry> list;

    for (uint32_t index = 0; index < count; index ++)
    {
        // first read the filename
        char* filename = this->readSizedString (fp);
        uint32_t offset = this->readInteger (fp);
        uint32_t length = this->readInteger (fp);

        // add the file to the list
        list.emplace_back(filename, offset, length);
        // only free filename, the file's contents are stored in a map for a later use
        delete[] filename;
    }

    // get current baseOffset, this is where the files start
    long baseOffset = ftell (fp);

    // read file contents now
    auto cur = list.begin ();
    auto end = list.end ();

    for (; cur != end; cur ++)
    {
        long offset = (*cur).offset + baseOffset;

        // with all the data we can jump to the offset and read the content
        if (fseek (fp, offset, SEEK_SET) != 0)
            throw std::runtime_error ("Cannot find file in package");

        // allocate memory for the file's contents and read it from the file
        char* fileContents = new char [(*cur).length];

        if (fread (fileContents, (*cur).length, 1, fp) != 1)
        {
            delete[] fileContents;

            throw std::runtime_error ("Cannot read file contents from package");
        }

        // add the file to the map
        this->m_contents.insert (std::make_pair <std::string, CFileEntry> (
            std::move((*cur).filename),
            CFileEntry (fileContents, (*cur).length))
        );
    }
}