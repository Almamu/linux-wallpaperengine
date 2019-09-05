#pragma once

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Irrlicht
{
    //! An entry in a list of files, can be a folder or a file.
    struct SFileListEntry
    {
        //! The name of the file
        /** If this is a file or folder in the virtual filesystem and the archive
        was created with the ignoreCase flag then the file name will be lower case. */
        irr::io::path Name;

        //! The name of the file including the path
        /** If this is a file or folder in the virtual filesystem and the archive was
        created with the ignoreDirs flag then it will be the same as Name. */
        irr::io::path FullName;

        //! The size of the file in bytes
        irr::u32 Size;

        //! The ID of the file in an archive
        /** This is used to link the FileList entry to extra info held about this
        file in an archive, which can hold things like data offset and CRC. */
        irr::u32 ID;

        //! FileOffset inside an archive
        irr::u32 Offset;

        //! True if this is a folder, false if not.
        bool IsDirectory;

        //! The == operator is provided so that CFileList can slowly search the list!
        bool operator == (const struct SFileListEntry& other) const
        {
            if (IsDirectory != other.IsDirectory)
                return false;

            return FullName.equals_ignore_case (other.FullName);
        }

        //! The < operator is provided so that CFileList can sort and quickly search the list.
        bool operator < (const struct SFileListEntry& other) const
        {
            if (IsDirectory != other.IsDirectory)
                return IsDirectory;

            return FullName.lower_ignore_case (other.FullName);
        }
    };


    //! Implementation of a file list
    class CFileList : public irr::io::IFileList
    {
    public:

        // CFileList methods

        //! Constructor
        /** \param path The path of this file archive */
        CFileList (const irr::io::path& path, bool ignoreCase, bool ignorePaths);

        //! Destructor
        virtual ~CFileList ();

        //! Add as a file or folder to the list
        /** \param fullPath The file name including path, up to the root of the file list.
        \param isDirectory True if this is a directory rather than a file.
        \param offset The offset where the file is stored in an archive
        \param size The size of the file in bytes.
        \param id The ID of the file in the archive which owns it */
        virtual irr::u32 addItem (const irr::io::path& fullPath, irr::u32 offset, irr::u32 size, bool isDirectory, irr::u32 id=0);

        //! Sorts the file list. You should call this after adding any items to the file list
        virtual void sort ();

        //! Returns the amount of files in the filelist.
        virtual irr::u32 getFileCount () const;

        //! Gets the name of a file in the list, based on an index.
        virtual const irr::io::path& getFileName (irr::u32 index) const;

        //! Gets the full name of a file in the list, path included, based on an index.
        virtual const irr::io::path& getFullFileName (irr::u32 index) const;

        //! Returns the ID of a file in the file list, based on an index.
        virtual irr::u32 getID (irr::u32 index) const;

        //! Returns true if the file is a directory
        virtual bool isDirectory (irr::u32 index) const;

        //! Returns the size of a file
        virtual irr::u32 getFileSize (irr::u32 index) const;

        //! Returns the offest of a file
        virtual irr::u32 getFileOffset (irr::u32 index) const;

        //! Searches for a file or folder within the list, returns the index
        virtual irr::s32 findFile (const irr::io::path& filename, bool isFolder) const;

        //! Returns the base path of the file list
        virtual const irr::io::path& getPath () const;

    protected:

        //! Ignore paths when adding or searching for files
        bool m_ignorePaths;

        //! Ignore case when adding or searching for files
        bool m_ignoreCase;

        //! Path to the file list
        irr::io::path m_path;

        //! List of files
        irr::core::array<SFileListEntry> m_files;
    };
}

