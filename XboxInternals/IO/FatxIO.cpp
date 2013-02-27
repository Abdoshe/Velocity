#include "FatxIO.h"

FatxIO::FatxIO(DeviceIO *device, FatxFileEntry *entry) : device(device), entry(entry)
{
    SetPosition(0);
}

void FatxIO::SetPosition(UINT64 position, std::ios_base::seek_dir dir = std::ios_base::beg)
{
    // we can't seek past the file
    if (position > entry->fileSize && !(entry->fileAttributes & FatxDirectory))
        throw std::string("FATX: Cannot seek past the file size.\n");

    if (dir == std::ios_base::cur)
        position += pos;
    else if (dir == std::ios_base::end)
        position = (device->DriveLength() - position);

    pos = position;

    if (pos == entry->fileSize && !(entry->fileAttributes & FatxDirectory))
        return;

    // calculate the actual offset on disk
    DWORD clusterIndex = position / entry->partition->clusterSize;

    if (clusterIndex >= entry->clusterChain.size())
        throw std::string("FATX: Cluster chain not sufficient enough for file size.\n");

    DWORD cluster = entry->clusterChain.at(clusterIndex);

    // calculate the read position
    DWORD startInCluster = (position % entry->partition->clusterSize);
    INT64 driveOff = (entry->partition->clusterStartingAddress + (entry->partition->clusterSize * (INT64)(cluster - 1))) + startInCluster;

    // this stores how many bytes we have until we reach another cluster
    maxReadConsecutive = entry->partition->clusterSize - startInCluster;

    // set the position
    device->SetPosition(driveOff);
}

void FatxIO::Flush()
{
    device->Flush();
}

void FatxIO::Close()
{
    // nothing to close since this doesn't actually have a file open
}

int FatxIO::AllocateMemory(DWORD byteAmount)
{
    // calcualte how many clusters to allocate
    DWORD clusterCount = (byteAmount + ((entry->partition->clusterSize - (entry->fileSize % entry->partition->clusterSize)) - 1)) / entry->partition->clusterSize;
    bool fileIsNull = (entry->fileSize == 0);

    if (fileIsNull && entry->fileAttributes & FatxDirectory)
    {
        entry->fileSize += byteAmount;
        return 0;
    }

    // get the free clusters
    std::vector<DWORD> freeClusters = getFreeClusters(entry->partition, clusterCount);
    if (freeClusters.size() != clusterCount)
        throw std::string("FATX: Cannot find requested amount of free clusters.\n");

    // add the free clusters to the cluster chain
    for (int i = 0; i < freeClusters.size(); i++)
        entry->clusterChain.push_back(freeClusters.at(i));

    if (fileIsNull)
        entry->startingCluster = entry->clusterChain.at(0);

    // write the cluster chain (only if it's changed)
    if (clusterCount != 0)
        writeClusterChain(entry->partition, entry->startingCluster, entry->clusterChain);

    // update the file size, only if it's not a directory
    if (!(entry->fileAttributes & FatxDirectory))
    {
        entry->fileSize += byteAmount;
        WriteEntryToDisk(entry);
    }
    return clusterCount;
}

UINT64 FatxIO::GetPosition()
{
    return pos;
}

UINT64 FatxIO::GetDrivePosition()
{
    return device->GetPosition();
}

FatxFileEntry* FatxIO::GetFatxFileEntry()
{
    return entry;
}

void FatxIO::ReadBytes(BYTE *outBuffer, DWORD len)
{
    // get the length
    DWORD origLen = len;

    DWORD bytesToRead = (len <= maxReadConsecutive) ? len : maxReadConsecutive;
    device->ReadBytes(outBuffer, bytesToRead);
    len -= bytesToRead;
    SetPosition(pos + bytesToRead);

    while (len >= entry->partition->clusterSize)
    {
        // calculate how many bytes to read
        device->ReadBytes(outBuffer + (origLen - len), maxReadConsecutive);

        // update the position
        SetPosition(pos + maxReadConsecutive);

        // update the length
        len -= maxReadConsecutive;
    }

    if (len > 0)
        device->ReadBytes(outBuffer + (origLen - len), len);
}

void FatxIO::WriteBytes(BYTE *buffer, DWORD len)
{
    // get the length
    DWORD origLen = len;

    DWORD bytesToWrite = (len <= maxReadConsecutive) ? len : maxReadConsecutive;
    device->WriteBytes(buffer, bytesToWrite);
    len -= bytesToWrite;
    SetPosition(pos + bytesToWrite);

    while (len >= entry->partition->clusterSize)
    {
        // calculate how many bytes to read
        device->WriteBytes(buffer + (origLen - len), maxReadConsecutive);

        // update the position
        SetPosition(pos + maxReadConsecutive);

        // update the length
        len -= maxReadConsecutive;
    }

    if (len > 0)
        device->WriteBytes(buffer + (origLen - len), len);
}

std::vector<DWORD> FatxIO::getFreeClusters(Partition *part, DWORD count)
{
    std::vector<DWORD> freeClusters(count);

    // copy over some free clusters
    std::copy(part->freeClusters.begin(), part->freeClusters.begin() + count, freeClusters.begin());

    // remove them from the list of free clusters
    part->freeClusters.erase(part->freeClusters.begin(), part->freeClusters.begin() + count);

    return freeClusters;
}

void FatxIO::SetAllClusters(DeviceIO *device, Partition *part, std::vector<DWORD> clusters, DWORD value)
{
    // sort the clusters numerically, order doesn't matter any more since we're just setting them all to the same value
    std::sort(clusters.begin(), clusters.end(), compareDWORDs);

    // we'll work with the clusters in 0x10000 chunks to minimize the amount of reads
    BYTE buffer[0x10000];

    for (DWORD i = 0; i < clusters.size(); i++)
    {
        // seek to the lowest cluster in the chainmap, but round to get a fast read
        UINT64 clusterEntryAddr = (part->address + 0x1000 + clusters.at(i) * part->clusterEntrySize);
        device->SetPosition(DOWN_TO_NEAREST_SECTOR(clusterEntryAddr));

        // read in a chunk of the chainmap
        device->ReadBytes(buffer, 0x10000);

        // open a MemoryIO on the memory for easier access
        MemoryIO bufferIO(buffer, 0x10000);

        // calculate the offset of the cluster in the buffer
        UINT64 clusterEntryOffset = clusterEntryAddr - DOWN_TO_NEAREST_SECTOR(clusterEntryAddr);

        // loop while still in the bounds of the buffer
        while (clusterEntryOffset < 0x10000)
        {
            // seek to the address of the cluster in the buffer
            bufferIO.SetPosition(clusterEntryOffset);

            // set the cluster chain map entry to the value requested
            if (part->clusterEntrySize == 2)
                bufferIO.Write((WORD)value);
            else
                bufferIO.Write((DWORD)value);

            if (i + 1 == clusters.size())
            {
                ++i;
                break;
            }

            // calculate the value of the next cluster entry
            clusterEntryOffset = (part->address + 0x1000 + clusters.at(++i) * part->clusterEntrySize) - DOWN_TO_NEAREST_SECTOR(clusterEntryAddr);
        }
        --i;

        // once we're done then write the block of memory back
        device->SetPosition(DOWN_TO_NEAREST_SECTOR(clusterEntryAddr));
        device->WriteBytes(buffer, 0x10000);
    }

    // flush the device just to be safe
    device->Flush();
}

void FatxIO::WriteEntryToDisk(FatxFileEntry *entry, std::vector<DWORD> *clusterChain)
{
    bool isDeleted = (entry->nameLen == FATX_ENTRY_DELETED);
    BYTE nameLen;
    if (isDeleted)
        nameLen = FATX_ENTRY_DELETED;
    else
    {
        nameLen = entry->name.length();
        if (nameLen > FATX_ENTRY_MAX_NAME_LENGTH)
            throw std::string("FATX: Entry name must be less than 42 characters.\n");
    }
    bool wantsToWriteClusterChain = (clusterChain != NULL);

    if (wantsToWriteClusterChain && entry->startingCluster != clusterChain->at(0))
        throw std::string("FATX: Entry starting cluster does not match with cluster chain.\n");

    device->SetPosition(entry->address);
    device->Write(nameLen);
    device->Write(entry->fileAttributes);
    device->Write(entry->name, FATX_ENTRY_MAX_NAME_LENGTH, false, 0xFF);
    device->Write(entry->startingCluster);
    device->Write(entry->fileSize);
    device->Write(entry->creationDate);
    device->Write(entry->lastWriteDate);
    device->Write(entry->lastAccessDate);

    if (wantsToWriteClusterChain)
    {
        SetAllClusters(device, entry->partition, entry->clusterChain, FAT_CLUSTER_AVAILABLE);
        writeClusterChain(entry->partition, entry->startingCluster, *clusterChain);
    }
}

void FatxIO::writeClusterChain(Partition *part, DWORD startingCluster, std::vector<DWORD> clusterChain)
{
    if (startingCluster == 0 || clusterChain.size() == 0)
        return;
    else if (startingCluster > part->clusterCount)
        throw std::string("FATX: Cluster is greater than cluster count.\n");

    bool clusterSizeIs16 = part->clusterEntrySize == FAT16;

    clusterChain.push_back(FAT_CLUSTER_LAST);

    // we'll work with the clusters in 0x10000 chunks to minimize the amount of reads
    BYTE buffer[0x10000];

    for (DWORD i = 0; i < clusterChain.size(); i++)
    {
        // seek to the lowest cluster in the chainmap, but round to get a fast read
        UINT64 clusterEntryAddr = (part->address + 0x1000 + clusterChain.at(i) * part->clusterEntrySize);
        device->SetPosition(DOWN_TO_NEAREST_SECTOR(clusterEntryAddr));

        // read in a chunk of the chainmap
        device->ReadBytes(buffer, 0x10000);

        // open a MemoryIO on the memory for easier access
        MemoryIO bufferIO(buffer, 0x10000);

        // calculate the offset of the cluster in the buffer
        UINT64 clusterEntryOffset = clusterEntryAddr - DOWN_TO_NEAREST_SECTOR(clusterEntryAddr);

        // loop while still in the bounds of the buffer
        while (clusterEntryOffset < 0x10000)
        {
            // seek to the address of the cluster in the buffer
            bufferIO.SetPosition(clusterEntryOffset);

            // set the cluster chain map entry to the value requested
            if (clusterSizeIs16)
                bufferIO.Write((WORD)clusterChain.at(i));
            else
                bufferIO.Write((DWORD)clusterChain.at(i));

            if (i + 1 == clusterChain.size())
            {
                ++i;
                break;
            }

            // calculate the value of the next cluster entry
            clusterEntryOffset = (part->address + 0x1000 + clusterChain.at(++i) * part->clusterEntrySize) - DOWN_TO_NEAREST_SECTOR(clusterEntryAddr);
        }
        --i;

        // once we're done then write the block of memory back
        device->SetPosition(DOWN_TO_NEAREST_SECTOR(clusterEntryAddr));
        device->WriteBytes(buffer, 0x10000);
    }

    // flush the device just to be safe
    device->Flush();

    /*for (int i = 0; i < clusterChain.size(); i++)
    {
        device->SetPosition(part->address + 0x1000 + (previousCluster * part->clusterEntrySize));

        if (clusterSizeIs16)
            device->Write((WORD)clusterChain.at(i));
        else
            device->Write((DWORD)clusterChain.at(i));

        previousCluster = clusterChain.at(i);
    }*/
}

void FatxIO::SaveFile(std::string savePath, void(*progress)(void*, DWORD, DWORD) = NULL, void *arg = NULL)
{
    // get the current position
    INT64 pos;
    INT64 originalPos = device->GetPosition();

    // seek to the beggining of the file
    SetPosition(0);

    // calculate stuff
    DWORD bufferSize = (entry->fileSize / 0x10);
    if (bufferSize < 0x10000)
        bufferSize = 0x10000;
    else if (bufferSize > 0x100000)
        bufferSize = 0x100000;

    std::vector<Range> readRanges;
    BYTE *buffer = new BYTE[bufferSize];

    // generate the read ranges
    for (DWORD i = 0; i < entry->clusterChain.size() - 1; i++)
    {
        // calculate cluster's address
        pos = ClusterToOffset(entry->partition, entry->clusterChain.at(i));

        Range range = { pos, 0 };
        do
        {
            range.len += entry->partition->clusterSize;
        }
        while ((entry->clusterChain.at(i) + 1) == entry->clusterChain.at(++i) && i < (entry->clusterChain.size() - 2) && (range.len + entry->partition->clusterSize) <= bufferSize);
        i--;

        readRanges.push_back(range);
    }

    DWORD finalClusterSize = entry->fileSize % entry->partition->clusterSize;
    INT64 finalClusterOffset = ClusterToOffset(entry->partition, entry->clusterChain.at(entry->clusterChain.size() - 1));
    Range lastRange = { finalClusterOffset , (finalClusterSize == 0) ? entry->partition->clusterSize : finalClusterSize};
    readRanges.push_back(lastRange);

    DWORD modulus = readRanges.size() / 100;
    if (modulus == 0)
        modulus = 1;
    else if (modulus > 3)
        modulus = 3;

    // open the new file
    FileIO outFile(savePath, true);

    // read all the data in
    for (DWORD i = 0; i < readRanges.size(); i++)
    {
        // seek to the beginning of the range
        device->SetPosition(readRanges.at(i).start);

        // get the range from the device
        device->ReadBytes(buffer, readRanges.at(i).len);
        outFile.WriteBytes(buffer, readRanges.at(i).len);

        // update progress if needed
        if (progress && i % modulus == 0)
            progress(arg, i + 1, readRanges.size());
    }

    // make sure it hits the end
    progress(arg, readRanges.size(), readRanges.size());

    outFile.Flush();
    outFile.Close();

    delete[] buffer;

    device->SetPosition(originalPos);
}

INT64 FatxIO::ClusterToOffset(Partition *part, DWORD cluster)
{
    return part->clusterStartingAddress + (part->clusterSize * (INT64)(cluster - 1));
}

bool compareDWORDs(DWORD a, DWORD b)
{
    return a < b;
}
