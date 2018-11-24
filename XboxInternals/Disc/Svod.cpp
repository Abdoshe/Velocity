#include "Svod.h"

std::vector<std::string> SVOD::GetDataFilePaths(std::string rootDescriptorPath)
{
    SVOD svod(rootDescriptorPath);

    // check to see if the data files are there
    std::string dataFileDirectoryPath = rootDescriptorPath + ".data\\";
#ifdef __WIN32
    bool dataFileDirectoryExists = PathFileExistsA(dataFileDirectoryPath.c_str());
#else
    struct stat sb;
    bool dataFileDirectoryExists = stat(dataFileDirectoryPath.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode);
#endif

    std::vector<std::string> dataFiles;
    if (dataFileDirectoryExists)
    {
        // get a list of all the data files in the directory
        dataFiles = Utils::FilesInDirectory(dataFileDirectoryPath);

        // TODO: make sure that they're named DataXXXX
    }
    return dataFiles;
}

SVOD::SVOD(string rootPath, FatxDrive *drive, bool readFileListing) :
    drive(drive), didReadFileListing(false)
{
    // make sure all of the slashes are the same
    rootPath = Utils::NormalizeFilePath(rootPath, '\\', '/');

    // get the content folder name, and make sure it exists
    string fileName = rootPath.substr(rootPath.find_last_of("/") + 1);
    contentDirectory = rootPath.substr(0, rootPath.find_last_of("/")) + "/" + fileName + ".data/";

    // parse the XContentHeader
    if (drive == NULL)
    {
        rootFile = new FileIO(rootPath);
    }
    else
    {
        // for FATX paths the slashes have to be \ like on windows
        rootPath = Utils::NormalizeFilePath(rootPath) ;

        FatxFileEntry *rootFileEntry = drive->GetFileEntry(rootPath);
        rootFile = new FatxIO(drive->GetFatxIO(rootFileEntry));
    }
    metaData = new XContentHeader(rootFile);

    baseAddress = (metaData->svodVolumeDescriptor.flags & EnhancedGDFLayout) ? 0x2000 : 0x12000;
    offset = (metaData->svodVolumeDescriptor.flags & EnhancedGDFLayout) ? 0x2000 : 0x1000;

    if (metaData->fileSystem != FileSystemSVOD)
        throw string("SVOD: Invalid file system header.\n");

    switch (metaData->contentType)
    {
        case GameOnDemand:
        case InstalledGame:
        case XboxOriginalGame:
            break;
        default:
            throw string("SVOD: Unrecognized content type.\n");
    }

    // open an IO on the content files
    if (drive == NULL)
    {
        io = new LocalIndexableMultiFileIO(contentDirectory);
    }
    else
    {
        contentDirectory = Utils::NormalizeFilePath(contentDirectory);
        io = new FatxIndexableMultiFileIO(contentDirectory, drive);
    }

    // parse the header
    io->SetPosition(baseAddress, 0);
    GdfxReadHeader(io, &header);

    if (readFileListing)
        GetFileListing();
}

SVOD::~SVOD()
{
    io->Close();
    delete io;

    rootFile->Close();
    delete rootFile;
}

void SVOD::Resign(string kvPath)
{
    if (metaData->magic != CON)
        throw string("SVOD: Can only resign console systems.\n");
    metaData->ResignHeader(kvPath);
}

void SVOD::SectorToAddress(DWORD sector, DWORD *addressInDataFile, DWORD *dataFileIndex)
{
    DWORD trueSector = (sector - (metaData->svodVolumeDescriptor.dataBlockOffset * 2)) % 0x14388;
    *addressInDataFile = trueSector * 0x800;
    *dataFileIndex = (sector - (metaData->svodVolumeDescriptor.dataBlockOffset * 2)) / 0x14388;

    // for the silly stuff at the beginning
    *addressInDataFile += offset;

    // for the data hash table(s)
    *addressInDataFile += ((trueSector / 0x198) + ((trueSector % 0x198 == 0 && trueSector != 0) ? 0 : 1)) * 0x1000;
}

void SVOD::ReadFileListing(vector<GdfxFileEntry> *entryList, DWORD sector, int size, string path)
{
    DWORD eAddr, eIndex;
    SectorToAddress(sector, &eAddr, &eIndex);
    io->SetPosition(eAddr, eIndex);

    GdfxFileEntry current;

    while (true)
    {
        io->GetPosition(&current.address, &current.fileIndex);

        // make sure we're not at the end of the file listing
        if (!GdfxReadFileEntry(io, &current) && size != 0)
            break;

        // if it's a folder, then seek to it and read it's contents
        if (current.attributes & GdfxDirectory)
        {
            // seek to the folder's files
            DWORD seekAddr, seekIndex;

            // preserve the current positon
            io->GetPosition(&seekAddr, &seekIndex);

            ReadFileListing(&current.files, current.sector, current.size, path + current.name + "/");

            // reset position to current listing
            io->SetPosition(seekAddr, seekIndex);
        }

        current.filePath = path;
        entryList->push_back(current);

        // seek to the next entry
        eAddr += (current.nameLen + 0x11) & 0xFFFFFFFC;
        io->SetPosition(eAddr);

        // check for end
        if (io->ReadDword() == 0xFFFFFFFF)
        {
            if ((size - 0x800) <= 0)
            {
                // sort the file entries so that directories are first
                std::sort(entryList->begin(), entryList->end(), DirectoryFirstCompareGdfxEntries);
                return;
            }
            else
            {
                size -= 0x800;
                SectorToAddress(++sector, &eAddr, &eIndex);
                io->SetPosition(eAddr, eIndex);

                // get position of next entry
                io->GetPosition(&eAddr, &eIndex);
            }
        }

        // back up to the entry
        io->SetPosition(eAddr);

        // reset the directory
        current.files.clear();
    }

    std::sort(entryList->begin(), entryList->end(), DirectoryFirstCompareGdfxEntries);
}

GdfxFileEntry SVOD::GetFileEntry(string path, vector<GdfxFileEntry> *listing)
{
    string entryName = path.substr(1, path.substr(1).find_first_of('/'));

    // search for the entry
    for (DWORD i = 0; i < listing->size(); i++)
    {
        if (listing->at(i).name == entryName)
        {
            // check to see if it's at the end
            if (path.substr(1).find_first_of('/') == -1)
                return listing->at(i);
            else
                return GetFileEntry(path.substr(entryName.length() + 1), &listing->at(i).files);
        }
    }
}

SvodIO SVOD::GetSvodIO(string path)
{
    return GetSvodIO(GetFileEntry(path, &root));
}

SvodIO SVOD::GetSvodIO(GdfxFileEntry entry)
{
    return SvodIO(metaData, entry, io);
}

void SVOD::Rehash(void (*progress)(DWORD, DWORD, void*), void *arg)
{
    DWORD fileCount = io->FileCount();
    BYTE master[0x1000] = {0};
    BYTE level0[0x1000] = {0};
    BYTE currentBlock[0x1000];
    BYTE prevHash[0x14] = {0};

    // iterate through all of the files
    for (int i = fileCount - 1; i >= 0; i--)
    {
        io->SetPosition((DWORD)0x2000, i);
        DWORD hashTableCount = ((io->CurrentFileLength() - 0x2000) + 0xCCFFF) / 0xCD000;
        DWORD totalBlockCount = (io->CurrentFileLength() - 0x1000 - (hashTableCount * 0x1000)) >> 0xC;

        // iterate through all of the level0 hash tables
        for (DWORD x = 0; x < hashTableCount; x++)
        {
            // seek to the next set of blocks
            io->SetPosition(0x2000 + x * 0xCD000, i);

            DWORD blockCount = (totalBlockCount >= 0xCC) ? 0xCC : totalBlockCount % 0xCC;
            totalBlockCount -= 0xCC;

            // iterate through all of the blocks
            for (DWORD y = 0; y < blockCount; y++)
            {
                io->ReadBytes(currentBlock, 0x1000);
                HashBlock(currentBlock, level0 + y * 0x14);
            }

            // Write the table
            io->SetPosition(0x1000 + x * 0xCD000, i);
            io->WriteBytes(level0, 0x1000);

            // hash the level0 table for the master hash table
            HashBlock(level0, master + x * 0x14);

            // clear out the table
            memset(level0, 0, 0x1000);
        }

        // append the previous master hash to the table
        memcpy(master + hashTableCount * 0x14, prevHash, 0x14);

        // Write the master hash table
        io->SetPosition((DWORD)0, i);
        io->WriteBytes(master, 0x1000);

        // hash the master table
        HashBlock(master, prevHash);

        // clear out the table
        memset(master, 0, 0x1000);

        // update progress if needed
        if (progress)
            progress(fileCount - i, fileCount, arg);
    }

    // update the root hash
    memcpy(metaData->svodVolumeDescriptor.rootHash, prevHash, 0x14);
    metaData->WriteVolumeDescriptor();

    DWORD dataLen = ((metaData->headerSize + 0xFFF) & 0xFFFFF000) - 0x344;
    BYTE *buff = new BYTE[dataLen];

    rootFile->SetPosition(0x344);
    rootFile->ReadBytes(buff, dataLen);

    Botan::SHA_160 sha1;
    sha1.clear();
    sha1.update(buff, dataLen);
    sha1.final(metaData->headerHash);

    metaData->WriteMetaData();
}

void SVOD::HashBlock(BYTE *block, BYTE *outHash)
{
    Botan::SHA_160 sha1;
    sha1.clear();
    sha1.update(block, 0x1000);
    sha1.final(outHash);
}

void SVOD::WriteFileEntry(GdfxFileEntry *entry)
{
    io->SetPosition(entry->address, entry->fileIndex);
    GdfxWriteFileEntry(io, entry);
}

DWORD SVOD::GetSectorCount()
{
    io->SetPosition((DWORD)0, io->FileCount() - 1);
    DWORD fileLen = io->CurrentFileLength() - 0x2000;

    return (io->FileCount() * 0x14388) + ((fileLen - (0x1000 * (fileLen / 0xCD000))) / 0x800);
}

std::string SVOD::GetContentName()
{
    std::string headerHashBeginning = Utils::ConvertToHexString(metaData->headerHash, 16);
    std::string firstByteOfTitleID = Utils::ConvertToHexString((metaData->titleID >> 24) & 0xFF);

    return headerHashBeginning + firstByteOfTitleID;
}

void SVOD::GetFileListing()
{
    if (!didReadFileListing)
    {
        didReadFileListing = true;
        ReadFileListing(&root, header.rootSector, header.rootSize, "/");
    }
}


