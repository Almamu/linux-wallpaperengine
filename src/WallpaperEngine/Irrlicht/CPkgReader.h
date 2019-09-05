#pragma once
#include <irrlicht/irrlicht.h>
#include <string>

#include "CContext.h"
#include "CFileList.h"

namespace WallpaperEngine::Irrlicht
{
//! Archiveloader capable of loading WAD Archives
    class CArchiveLoaderPkg : public irr::io::IArchiveLoader
    {
    public:

        //! Constructor
        CArchiveLoaderPkg (CContext* context);

        //! returns true if the file maybe is able to be loaded by this class
        //! based on the file extension (e.g. ".zip")
        virtual bool isALoadableFileFormat (const irr::io::path &filename) const;

        //! Check if the file might be loaded by this class
        /** Check might look into the file.
        \param file File handle to check.
        \return True if file seems to be loadable. */
        virtual bool isALoadableFileFormat (irr::io::IReadFile *file) const;

        //! Check to see if the loader can create archives of this type.
        /** Check based on the archive type.
        \param fileType The archive type to check.
        \return True if the archile loader supports this type, false if not */
        virtual bool isALoadableFileFormat (irr::io::E_FILE_ARCHIVE_TYPE fileType) const;

        //! Creates an archive from the filename
        /** \param file File handle to check.
        \return Pointer to newly created archive, or 0 upon error. */
        virtual irr::io::IFileArchive *
        createArchive (const irr::io::path &filename, bool ignoreCase, bool ignorePaths) const;

        //! creates/loads an archive from the file.
        //! \return Pointer to the created archive. Returns 0 if loading failed.
        virtual irr::io::IFileArchive *createArchive (irr::io::IReadFile *file, bool ignoreCase, bool ignorePaths) const;

    private:
        CContext* m_context;
    };

    class CPkgReader : public virtual irr::io::IFileArchive, virtual CFileList
    {
    public:

        //! constructor
        CPkgReader (irr::io::IReadFile *file, bool ignoreCase, bool ignorePaths);

        //! destructor
        virtual ~CPkgReader ();

        //! opens a file by file name
        virtual irr::io::IReadFile *createAndOpenFile (const irr::io::path &filename);

        //! opens a file by index
        virtual irr::io::IReadFile *createAndOpenFile (unsigned int index);

        //! returns the list of files
        virtual const IFileList *getFileList () const;

        //! get the archive type
        virtual irr::io::E_FILE_ARCHIVE_TYPE getType () const;

    protected:
        void scanPkgHeader ();

        char *readSizedString ();

        CContext* m_context;
        irr::io::IReadFile *m_file;
    };
}