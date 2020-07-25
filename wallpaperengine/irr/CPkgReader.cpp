//
// Created by almamu on 4/04/19.
//

#include <wallpaperengine/irrlicht.h>
#include "CPkgReader.h"

CArchiveLoaderPkg::CArchiveLoaderPkg(irr::io::IFileSystem* fs)
        : FileSystem(fs)
{
#ifdef _DEBUG
    setDebugName("CArchiveLoaderWAD");
#endif
}


//! returns true if the file maybe is able to be loaded by this class
bool CArchiveLoaderPkg::isALoadableFileFormat(const irr::io::path& filename) const
{
    return irr::core::hasFileExtension (filename, "pkg");
}


//! Creates an archive from the filename
/** \param file File handle to check.
\return Pointer to newly created archive, or 0 upon error. */
irr::io::IFileArchive* CArchiveLoaderPkg::createArchive(const irr::io::path& filename, bool ignoreCase, bool ignorePaths) const
{
    irr::io::IFileArchive *archive = nullptr;
    irr::io::IReadFile* file = FileSystem->createAndOpenFile(filename);

    if (file)
    {
        archive = createArchive (file, ignoreCase, ignorePaths);
        file->drop ();
    }

    return archive;
}

//! creates/loads an archive from the file.
//! \return Pointer to the created archive. Returns 0 if loading failed.
irr::io::IFileArchive* CArchiveLoaderPkg::createArchive(irr::io::IReadFile* file, bool ignoreCase, bool ignorePaths) const
{
    irr::io::IFileArchive *archive = nullptr;

    if (file)
    {
        file->seek (0);
        archive = new CPkgReader (file, ignoreCase, ignorePaths);
    }

    return archive;
}


//! Check if the file might be loaded by this class
/** Check might look into the file.
\param file File handle to check.
\return True if file seems to be loadable. */
bool CArchiveLoaderPkg::isALoadableFileFormat(irr::io::IReadFile* file) const
{
    unsigned int size;
    char* pointer;

    file->read (&size, 4);

    // the string doesnt include the null terminator
    size ++;

    pointer = new char [size];
    memset (pointer, 0, size);

    file->read (pointer, size - 1);

    if (strcmp (pointer, "PKGV0001") != 0 &&
        strcmp (pointer, "PKGV0002") != 0 &&
        strcmp (pointer, "PKGV0007") != 0 &&
        strcmp (pointer, "PKGV0008") != 0)
    {
        delete [] pointer;
        return false;
    }

    delete [] pointer;
    return true;
}

//! Check to see if the loader can create archives of this type.
bool CArchiveLoaderPkg::isALoadableFileFormat(irr::io::E_FILE_ARCHIVE_TYPE fileType) const
{
    return false;
}

CPkgReader::CPkgReader (irr::io::IReadFile* file, bool ignoreCase, bool ignorePaths)
 : CFileList((file ? file->getFileName() : irr::io::path("")), ignoreCase, ignorePaths), mFile(file)
{
    if (this->mFile)
    {
        this->mFile->grab ();
        this->scanPkgHeader ();
    }
}

CPkgReader::~CPkgReader()
{
    if (this->mFile)
        this->mFile->drop();
}


//! get the archive type
irr::io::E_FILE_ARCHIVE_TYPE CPkgReader::getType() const
{
	return irr::io::E_FILE_ARCHIVE_TYPE::EFAT_ZIP;
}

const irr::io::IFileList* CPkgReader::getFileList() const
{
	return this;
}

void CPkgReader::scanPkgHeader ()
{
    char* headerVersion = this->readSizedString ();
    
    if (strcmp ("PKGV0001", headerVersion) != 0 &&
        strcmp ("PKGV0002", headerVersion) != 0 &&
        strcmp ("PKGV0007", headerVersion) != 0 &&
        strcmp ("PKGV0008", headerVersion) != 0)
    {
        wp::irrlicht::device->getLogger ()->log ("Unexpected package header... Aborting load", this->mFile->getFileName ().c_str (), irr::ELL_ERROR);

        delete [] headerVersion;
        return;
    }

    delete [] headerVersion;

    unsigned int entriesCount;

    this->mFile->read (&entriesCount, 4);


    for (int i = 0; i < entriesCount; i ++)
    {
        char* filename = this->readSizedString ();
        unsigned int offset, length;

        this->mFile->read (&offset, 4);
        this->mFile->read (&length, 4);

        this->addItem (filename, offset, length, false);

        delete [] filename;
    }

    // after the header is read we have to update the actual offsets
    for (int i = 0; i < this->Files.size (); i ++)
    {
        this->Files [i].Offset += this->mFile->getPos ();
    }
}

char* CPkgReader::readSizedString ()
{
    unsigned int size;
    char* pointer;

    this->mFile->read (&size, 4);

    // the string doesnt include the null terminator
    size ++;

    pointer = new char [size];
    memset (pointer, 0, size);

    this->mFile->read (pointer, size - 1);

    return pointer;
}

irr::io::IReadFile* CPkgReader::createAndOpenFile (const irr::io::path& filename)
{
    irr::s32 index = this->findFile (filename, false);

    if (index != -1)
        return createAndOpenFile (index);

    return nullptr;
}

irr::io::IReadFile* CPkgReader::createAndOpenFile (irr::u32 index)
{
    if (index > this->Files.size ())
        return nullptr;

    const irr::io::SFileListEntry entry = Files [index];
    return irr::io::createLimitReadFile (entry.FullName, mFile, entry.Offset, entry.Size);
}