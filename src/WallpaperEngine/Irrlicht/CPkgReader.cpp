#include "CPkgReader.h"

namespace WallpaperEngine::Irrlicht
{

    CArchiveLoaderPkg::CArchiveLoaderPkg (CContext* context) :
            m_context (context)
    {
#ifdef _DEBUG
        setDebugName("CArchiveLoaderPKG");
#endif
    }

    bool CArchiveLoaderPkg::isALoadableFileFormat(const irr::io::path& filename) const
    {
        return irr::core::hasFileExtension (filename, "pkg");
    }

    irr::io::IFileArchive* CArchiveLoaderPkg::createArchive(const irr::io::path& filename, bool ignoreCase, bool ignorePaths) const
    {
        irr::io::IFileArchive *archive = nullptr;
        irr::io::IReadFile* file = this->m_context->getDevice ()->getFileSystem ()->createAndOpenFile(filename);

        if (file)
        {
            archive = this->createArchive (file, ignoreCase, ignorePaths);
            file->drop ();
        }

        return archive;
    }

    irr::io::IFileArchive* CArchiveLoaderPkg::createArchive(irr::io::IReadFile* file, bool ignoreCase, bool ignorePaths) const
    {
        irr::io::IFileArchive *archive = nullptr;

        if (file)
        {
            file->seek (0);
            archive = new CPkgReader (this->m_context, file, ignoreCase, ignorePaths);
        }

        return archive;
    }

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


        if (
                strcmp ("PKGV0007", pointer) != 0 &&
                strcmp ("PKGV0002", pointer) != 0 &&
                strcmp ("PKGV0001", pointer) != 0 &&
                strcmp ("PKGV0008", pointer) != 0)
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

    CPkgReader::CPkgReader (CContext* context, irr::io::IReadFile* file, bool ignoreCase, bool ignorePaths)
            : CFileList((file ? file->getFileName() : irr::io::path("")), ignoreCase, ignorePaths),
            m_file(file),
            m_context (context)
    {
        if (this->m_file)
        {
            this->m_file->grab ();
            this->scanPkgHeader ();
        }
    }

    CPkgReader::~CPkgReader()
    {
        if (this->m_file)
            this->m_file->drop();
    }

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

        if (
                strcmp ("PKGV0007", headerVersion) != 0 &&
                strcmp ("PKGV0002", headerVersion) != 0 &&
                strcmp ("PKGV0001", headerVersion) != 0 &&
                strcmp ("PKGV0008", headerVersion) != 0)
        {
            delete [] headerVersion;

            this->m_context->getDevice ()->getLogger ()->log (
                "unexpected package header", this->m_file->getFileName ().c_str (), irr::ELL_ERROR
            );

            return;
        }

        delete [] headerVersion;
        irr::u32 entriesCount;

        this->m_file->read (&entriesCount, 4);


        for (irr::u32 i = 0; i < entriesCount; i ++)
        {
            char* filename = this->readSizedString ();
            irr::u32 offset, length;

            this->m_file->read (&offset, 4);
            this->m_file->read (&length, 4);

            this->addItem (filename, offset, length, false);

            delete [] filename;
        }

        // after the header is read we have to update the actual offsets
        for (irr::u32 i = 0; i < this->m_files.size (); i ++)
        {
            this->m_files [i].Offset += this->m_file->getPos ();
        }
    }

    char* CPkgReader::readSizedString ()
    {
        unsigned int size;
        char* pointer;

        this->m_file->read (&size, 4);

        // the string doesnt include the null terminator
        size ++;

        pointer = new char [size];
        memset (pointer, 0, size);

        this->m_file->read (pointer, size - 1);

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
        if (index > this->m_files.size ())
            return nullptr;

        const SFileListEntry entry = m_files [index];
        return irr::io::createLimitReadFile (entry.FullName, m_file, entry.Offset, entry.Size);
    }
}