#include <stdio.h>
#include <string.h>

#include "heap_file.h"
#include "bf.h"

HP_ErrorCode HP_Init() {
  // write your code here

  return HP_OK;
}

HP_ErrorCode HP_CreateIndex(const char *filename) {
  // 1 - Create the file
  BF_ErrorCode err = BF_CreateFile(filename);
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 2 - Open the file
  int fd;
  err = BF_OpenFile(filename, &fd);
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 3 - Write to first block and make it a Heap File
  BF_Block* block;
  BF_Block_Init(&block);
  err = BF_AllocateBlock(fd, block); // Pinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }
  char* data = BF_Block_GetData(block);
  memset(data, HP_FIRST_BLOCK_DATA, BF_BLOCK_SIZE);
  BF_Block_SetDirty(block);
  err = BF_UnpinBlock(block); // Unpinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 4 - Write Record counter (int countrec = 0) to second block
  err = BF_AllocateBlock(fd, block); // Pinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }
  data = BF_Block_GetData(block);
  int countrec = 0;
  memcpy(data, &countrec, sizeof(int));
  BF_Block_SetDirty(block);
  err = BF_UnpinBlock(block); // Unpinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 5 - Close the file
  BF_CloseFile(fd);
  BF_Block_Destroy(&block);

  return HP_OK;
}

HP_ErrorCode HP_OpenFile(const char *fileName, int *fileDesc){
  // 1 - Open the file
  BF_ErrorCode err = BF_OpenFile(fileName, fileDesc);
  if(err != BF_OK){
    BF_PrintError(err);
    return HP_ERROR;
  }

  return HP_OK;
}

HP_ErrorCode HP_CloseFile(int fileDesc) {
  // 1 - Close the file
  BF_ErrorCode err = BF_CloseFile(fileDesc);
  if(err != BF_OK){
    BF_PrintError(err);
    return HP_ERROR;
  }

  return HP_OK;
}

HP_ErrorCode HP_InsertEntry(int fileDesc, Record record) {
  // 1 - Get the last block of the file
  int countblocks;
  BF_ErrorCode err = BF_GetBlockCounter(fileDesc, &countblocks);
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }
  BF_Block* block;
  BF_Block_Init(&block);
  err = BF_GetBlock(fileDesc, countblocks-1, block); // Pinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 2 - Find how many Records are in the block
  int countrec;
  char* data = BF_Block_GetData(block);
  memcpy(&countrec, data, sizeof(int));
  BF_Block_SetDirty(block);

  // 3a - If there is enough space for a new Record, put it there
  int freebytes = BF_BLOCK_SIZE - sizeof(int) - countrec*(sizeof(Record));
  if (freebytes >= sizeof(Record)) {
    memcpy(data + BF_BLOCK_SIZE - freebytes, &record, sizeof(Record));
    countrec += 1;
    memcpy(data, &countrec, sizeof(int));
    err = BF_UnpinBlock(block); // Unpinned! (if)
    if (err != BF_OK) {
      BF_PrintError(err);
      return HP_ERROR;
    }
  }

  // 3b - If there isn't enough space in it, unpin it and allocate a new block
  else{
    err = BF_UnpinBlock(block); // Unpinned! (else)
    if (err != BF_OK) {
      BF_PrintError(err);
      return HP_ERROR;
    }
    err = BF_AllocateBlock(fileDesc, block); // Pinned!
    if (err != BF_OK) {
      BF_PrintError(err);
      return HP_ERROR;
    }
    data = BF_Block_GetData(block);
    countrec = 0;
    memcpy(data + sizeof(int), &record, sizeof(Record));
    BF_Block_SetDirty(block);
    countrec += 1;
    memcpy(data, &countrec, sizeof(int));
    err = BF_UnpinBlock(block); // Unpinned!
    if (err != BF_OK) {
      BF_PrintError(err);
      return HP_ERROR;
    }
  }
  BF_Block_Destroy(&block);

  return HP_OK;
}

HP_ErrorCode HP_PrintAllEntries(int fileDesc) {
  // 1 - Get the number of total blocks in the file
  int countblocks;
  BF_ErrorCode err = BF_GetBlockCounter(fileDesc, &countblocks);
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 2 - Print each Record in each block from i = 0 to countblocks-1
  BF_Block* block;
  BF_Block_Init(&block);
  char* data;
  Record temprecord;
  int countrec, i, j;
  for (i = 0; i < countblocks; i++) {
    err = BF_GetBlock(fileDesc, i, block); // Pinned! (for i)
    if (err != BF_OK) {
      BF_PrintError(err);
      return HP_ERROR;
    }
    data = BF_Block_GetData(block);
    memcpy(&countrec, data, sizeof(int));
    data += sizeof(int);
    for (j = 0; j < countrec; j++) {
      memcpy(&temprecord, data + j*sizeof(Record), sizeof(Record));
      // printf("%d,\"%s\",\"%s\",\"%s\"\n",
          // temprecord.id, temprecord.name, temprecord.surname, temprecord.city);
    }
    err = BF_UnpinBlock(block); // Unpinned! (for i)
    if (err != BF_OK) {
      BF_PrintError(err);
      return HP_ERROR;
    }
  }

  BF_Block_Destroy(&block);

  return HP_OK;
}

HP_ErrorCode HP_GetEntry(int fileDesc, int rowId, Record *record) {
  // 1 - Find in which block that row is (first row: rowId == 1)
  int records_per_block = BF_BLOCK_SIZE/sizeof(Record);
  int block_num = ((rowId - 1)/records_per_block) + 1;
  BF_Block* block;
  BF_Block_Init(&block);
  BF_ErrorCode err = BF_GetBlock(fileDesc, block_num, block); // Pinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  // 2 - Get the asked Record from that block and return it
  int rec_num = rowId % records_per_block - 1; // 0 is the first in the block
  char* data = BF_Block_GetData(block);
  memcpy(record, data + sizeof(int) + rec_num*sizeof(Record), sizeof(Record));

  err = BF_UnpinBlock(block); // Unpinned!
  if (err != BF_OK) {
    BF_PrintError(err);
    return HP_ERROR;
  }

  return HP_OK;
}
