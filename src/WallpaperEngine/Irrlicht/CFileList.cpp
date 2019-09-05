#include <irrlicht/irrlicht.h>

#include "CFileList.h"

namespace WallpaperEngine::Irrlicht
{
    static const irr::io::path emptyFileListEntry;

    CFileList::CFileList (const irr::io::path& path, bool ignoreCase, bool ignorePaths) :
            m_ignorePaths (ignorePaths),
            m_ignoreCase (ignoreCase),
            m_path(path)
    {
        this->m_path.replace ('\\', '/');
    }

    CFileList::~CFileList ()
    {
        this->m_files.clear ();
    }

    irr::u32 CFileList::getFileCount () const
    {
        return this->m_files.size ();
    }

    void CFileList::sort ()
    {
        this->m_files.sort ();
    }

    const irr::io::path& CFileList::getFileName (irr::u32 index) const
    {
        return (index < this->m_files.size ()) ? this->m_files [index].Name : emptyFileListEntry;
    }


//! Gets the full name of a file in the list, path included, based on an index.
    const irr::io::path& CFileList::getFullFileName (irr::u32 index) const
    {
        return (index < this->m_files.size ()) ? this->m_files [index].FullName : emptyFileListEntry;
    }

//! adds a file or folder
    irr::u32 CFileList::addItem (const irr::io::path& fullPath, irr::u32 offset, irr::u32 size, bool isDirectory, irr::u32 id)
    {
        SFileListEntry entry;
        entry.ID   = id ? id : this->m_files.size ();
        entry.Offset = offset;
        entry.Size = size;
        entry.Name = fullPath;
        entry.Name.replace ('\\', '/');
        entry.IsDirectory = isDirectory;

        // remove trailing slash
        if (entry.Name.lastChar () == '/')
        {
            entry.IsDirectory = true;
            entry.Name [entry.Name.size ()-1] = 0;
            entry.Name.validate ();
        }

        if (this->m_ignoreCase)
            entry.Name.make_lower ();

        entry.FullName = entry.Name;

        irr::core::deletePathFromFilename (entry.Name);

        if (this->m_ignorePaths)
            entry.FullName = entry.Name;

        this->m_files.push_back (entry);

        return this->m_files.size () - 1;
    }

//! Returns the ID of a file in the file list, based on an index.
    irr::u32 CFileList::getID (irr::u32 index) const
    {
        return (index < this->m_files.size ()) ? this->m_files [index].ID : 0;
    }

    bool CFileList::isDirectory (irr::u32 index) const
    {
        return (index < this->m_files.size ()) ? this->m_files [index].IsDirectory : false;
    }

//! Returns the size of a file
    irr::u32 CFileList::getFileSize (irr::u32 index) const
    {
        return (index < this->m_files.size ()) ? this->m_files [index].Size : 0;
    }

//! Returns the size of a file
    irr::u32 CFileList::getFileOffset (irr::u32 index) const
    {
        return (index < this->m_files.size ()) ? this->m_files [index].Offset : 0;
    }


//! Searches for a file or folder within the list, returns the index
    irr::s32 CFileList::findFile (const irr::io::path& filename, bool isDirectory = false) const
    {
        SFileListEntry entry;
        // we only need FullName to be set for the search
        entry.FullName = filename;
        entry.IsDirectory = isDirectory;

        // exchange
        entry.FullName.replace('\\', '/');

        // remove trailing slash
        if (entry.FullName.lastChar () == '/')
        {
            entry.IsDirectory = true;
            entry.FullName [entry.FullName.size ()-1] = 0;
            entry.FullName.validate ();
        }

        if (this->m_ignoreCase)
            entry.FullName.make_lower ();

        if (this->m_ignorePaths)
            irr::core::deletePathFromFilename (entry.FullName);

        return this->m_files.binary_search (entry);
    }


//! Returns the base path of the file list
    const irr::io::path& CFileList::getPath () const
    {
        return m_path;
    }
};
