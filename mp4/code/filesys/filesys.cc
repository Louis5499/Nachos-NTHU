// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG(dbgFile, "Initializing the file system.");
    if (format) {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
		FileHeader *mapHdr = new FileHeader;
		FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

		// First, allocate space for FileHeaders for the directory and bitmap
		// (make sure no one else grabs these!)
		freeMap->Mark(FreeMapSector);	    
		freeMap->Mark(DirectorySector);

		// Second, allocate space for the data blocks containing the contents
		// of the directory and bitmap files.  There better be enough space!

		ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
		ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

		// Flush the bitmap and directory FileHeaders back to disk
		// We need to do this before we can "Open" the file, since open
		// reads the file header off of disk (and currently the disk has garbage
		// on it!).

        DEBUG(dbgFile, "Writing headers back to disk.");
		mapHdr->WriteBack(FreeMapSector);    
		dirHdr->WriteBack(DirectorySector);

		// OK to open the bitmap and directory files now
		// The file system operations assume these two files are left open
		// while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
		// Once we have the files "open", we can write the initial version
		// of each file back to disk.  The directory at this point is completely
		// empty; but the bitmap has been changed to reflect the fact that
		// sectors on the disk have been allocated for the file headers and
		// to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
		freeMap->WriteBack(freeMapFile);	 // flush changes to disk
		directory->WriteBack(directoryFile);

		if (debug->IsEnabled('f')) {
			freeMap->Print();
			directory->Print();
        }
        currentOpenFile = NULL;
        delete freeMap; 
		delete directory; 
		delete mapHdr; 
		delete dirHdr;
    } else {
		// if we are not formatting the disk, just open the files representing
		// the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
	delete freeMapFile;
	delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

TraverseFile* FileSystem::GetTraverseFileByName(char *name) {
    TraverseFile *traverseFile = new TraverseFile();
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *tempOpenFile = directoryFile; // Start from root
    int foundSector = DirectorySector; // Start from root
    int belongSector = DirectorySector;
    int memoryForLastBelongSector; // When we traverse the directory, not file, the var "belongSector" will get wrong, the true belongSector is the above layer of belongSector
    char *finalName = "";

    directory->FetchFrom(tempOpenFile);

    char *pch = strtok(name, "/"); // Because path is seperated by '/'
    while(pch != NULL) {
        // Try to find the directory is existed or not
        finalName = pch;
        foundSector = directory->Find(pch);
        if (foundSector < 0 || !directory->checkIfDir(pch)) {
            // Means not exists, we need to break while loop, and construct the subDirectory / file
            break;
        }
        // Link to next directory
        tempOpenFile = new OpenFile(foundSector);
        directory->FetchFrom(tempOpenFile);
        memoryForLastBelongSector = belongSector;
        belongSector = foundSector;
        pch = strtok(NULL, "/");
    }
    traverseFile->directory = directory;
    strcpy(traverseFile->finalName, finalName);
    traverseFile->finalSector = foundSector;
    traverseFile->isDir = (belongSector == foundSector);
    // When traversed inode is dir, we need to find above them.
    traverseFile->belongSector = traverseFile->isDir ? memoryForLastBelongSector : belongSector;

    DEBUG('f', "traverse name: " << traverseFile->finalName);
    DEBUG('f', "traverse sector: " << traverseFile->finalSector);
    DEBUG('f', "traverse original sector: " << traverseFile->belongSector);

    return traverseFile;
}

bool
FileSystem::Create(char *name, int initialSize)
{
    TraverseFile *traverseFile;
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    char *finalName;
    bool success;

    DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);
    traverseFile = GetTraverseFileByName(name);
    directory = traverseFile->directory;
    finalName = traverseFile->finalName;
    OpenFile *belongDirOpenFile = new OpenFile(traverseFile->belongSector);

    if (directory->Find(finalName) != -1) {
        success = FALSE;			// file is already in directory
    } else {	
        freeMap = new PersistentBitmap(freeMapFile,NumSectors);
        sector = freeMap->FindAndSet();	// find a sector to hold the file header
    	if (sector == -1) {	
            success = FALSE;		// no free block for file header 
        }
        else if (!directory->Add(finalName, sector, false)) {
            success = FALSE;	// no space in directory
        }
	    else {
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize)) {
                success = FALSE;	// no space on disk for data
            } else {	
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector); 		
                directory->WriteBack(belongDirOpenFile);
                freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
	    }
        delete freeMap;
    }

    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{ 
    TraverseFile *traverseFile;
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    traverseFile = GetTraverseFileByName(name);
    openFile = new OpenFile(traverseFile->finalSector);

    return openFile;
}

OpenFileId FileSystem::OpenAFile(char *name) {
    currentOpenFile = Open(name);
    return 1; // Becasue nachos only allow to accept one file open.
}

int FileSystem::CloseAFile() {
    currentOpenFile = NULL;
    return 1;
}

bool FileSystem::CreateDirectory(char *name) {
    TraverseFile *traverseFile;
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *dirHdr = new FileHeader;
    int newSector;
    bool success = true;
    char *pch;

    // Get the root directory first && the filename in path (e.g. /a/b.png  => b.png)
    traverseFile = GetTraverseFileByName(name);
    directory = traverseFile->directory;
    pch = traverseFile->finalName;
    OpenFile *belongDirOpenFile = new OpenFile(traverseFile->belongSector);

    // Out from while loop, which means we're going to construct subDirectory
    // 1. Find free sector
    freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    newSector = freeMap->FindAndSet();	// find a sector to hold the file header
    if (newSector == -1) success = FALSE;

    // 2. link subDirectory to original directory
    if (!directory->Add(pch, newSector, true)) success = FALSE;

    // 3. Build up subDirectory <File Header>
    dirHdr->Allocate(freeMap, DirectoryFileSize);
    dirHdr->WriteBack(newSector);

    // 4. Build up subDirectory <Directory>
    Directory *subDirectory = new Directory(NumDirEntries);
    OpenFile* newDirectoryFile = new OpenFile(newSector);
    subDirectory->WriteBack(newDirectoryFile);

    // 5. Update directory / freeMap on disk
    directory->WriteBack(belongDirOpenFile);
    freeMap->WriteBack(freeMapFile);

    // 6. Free local storage
    delete directory;
    delete subDirectory;
    delete freeMap;
    delete dirHdr;
    delete newDirectoryFile;

    return success;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name, bool shouldRecursive)
{ 
    TraverseFile *traverseFile;
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;
    char *finalName;
    char pwd[260],buffer[260];

    traverseFile = GetTraverseFileByName(name);
    directory = traverseFile->directory;

    // Check whether should recursive search
    if (shouldRecursive && traverseFile->isDir) {
        DirectoryEntry *table = directory->GetTable();
        strcpy(pwd, name);
        for (int i=0; i<directory->GetTableSize(); i++) {
            if (table[i].inUse) {
                sprintf(buffer, "%s/%s", pwd, table[i].name);
                Remove(buffer, true);
            }
        }
        // Redirect directory to directory above current.
        // Why?
        // e.g. We're deleting "/abc"
        // current directory is point to "/abc"
        // But what we need is "/" 
        OpenFile *tempOpenFile = new OpenFile(traverseFile->belongSector);
        directory->FetchFrom(tempOpenFile);
    }
    
    sector = traverseFile->finalSector;
    finalName = traverseFile->finalName;
    OpenFile *belongDirOpenFile = new OpenFile(traverseFile->belongSector);

    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new PersistentBitmap(freeMapFile,NumSectors);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(finalName);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(belongDirOpenFile);        // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List(char *name, bool shouldRecursive) {
    TraverseFile *traverseFile;
    Directory *directory = new Directory(NumDirEntries);
    traverseFile = GetTraverseFileByName(name);
    directory = traverseFile->directory;

    if (shouldRecursive) directory->RecursiveList();
    else directory->List();

    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile,NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}


#endif // FILESYS_STUB
