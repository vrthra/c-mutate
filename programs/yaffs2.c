/**************************yaffs_nand.c************************************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

const char *yaffs_nand_c_version =
    "$Id: yaffs_nand.c,v 1.8 2007-12-13 15:35:18 wookey Exp $";

#include "yaffs_nand.h"
#include "yaffs_tagscompat.h"
#include "yaffs_tagsvalidity.h"


int yaffs_ReadChunkWithTagsFromNAND(yaffs_Device * dev, int chunkInNAND,
					   __u8 * buffer,
					   yaffs_ExtendedTags * tags)
{
	int result;
	yaffs_ExtendedTags localTags;

	int realignedChunkInNAND = chunkInNAND - dev->chunkOffset;

	/* If there are no tags provided, use local tags to get prioritised gc working */
	if(!tags)
		tags = &localTags;

	if (dev->readChunkWithTagsFromNAND)
		result = dev->readChunkWithTagsFromNAND(dev, realignedChunkInNAND, buffer,
						      tags);
	else
		result = yaffs_TagsCompatabilityReadChunkWithTagsFromNAND(dev,
									realignedChunkInNAND,
									buffer,
									tags);
	if(tags &&
	   tags->eccResult > YAFFS_ECC_RESULT_NO_ERROR){

		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, chunkInNAND/dev->nChunksPerBlock);
                yaffs_HandleChunkError(dev,bi);
	}

	return result;
}

int yaffs_WriteChunkWithTagsToNAND(yaffs_Device * dev,
						   int chunkInNAND,
						   const __u8 * buffer,
						   yaffs_ExtendedTags * tags)
{
	chunkInNAND -= dev->chunkOffset;


	if (tags) {
		tags->sequenceNumber = dev->sequenceNumber;
		tags->chunkUsed = 1;
		if (!yaffs_ValidateTags(tags)) {
			T(YAFFS_TRACE_ERROR,
			  (TSTR("Writing uninitialised tags" TENDSTR)));
			YBUG();
		}
		T(YAFFS_TRACE_WRITE,
		  (TSTR("Writing chunk %d tags %d %d" TENDSTR), chunkInNAND,
		   tags->objectId, tags->chunkId));
	} else {
		T(YAFFS_TRACE_ERROR, (TSTR("Writing with no tags" TENDSTR)));
		YBUG();
	}

	if (dev->writeChunkWithTagsToNAND)
		return dev->writeChunkWithTagsToNAND(dev, chunkInNAND, buffer,
						     tags);
	else
		return yaffs_TagsCompatabilityWriteChunkWithTagsToNAND(dev,
								       chunkInNAND,
								       buffer,
								       tags);
}

int yaffs_MarkBlockBad(yaffs_Device * dev, int blockNo)
{
	blockNo -= dev->blockOffset;

;
	if (dev->markNANDBlockBad)
		return dev->markNANDBlockBad(dev, blockNo);
	else
		return yaffs_TagsCompatabilityMarkNANDBlockBad(dev, blockNo);
}

int yaffs_QueryInitialBlockState(yaffs_Device * dev,
						 int blockNo,
						 yaffs_BlockState * state,
						 unsigned *sequenceNumber)
{
	blockNo -= dev->blockOffset;

	if (dev->queryNANDBlock)
		return dev->queryNANDBlock(dev, blockNo, state, sequenceNumber);
	else
		return yaffs_TagsCompatabilityQueryNANDBlock(dev, blockNo,
							     state,
							     sequenceNumber);
}


int yaffs_EraseBlockInNAND(struct yaffs_DeviceStruct *dev,
				  int blockInNAND)
{
	int result;

	blockInNAND -= dev->blockOffset;


	dev->nBlockErasures++;
	result = dev->eraseBlockInNAND(dev, blockInNAND);

	return result;
}

int yaffs_InitialiseNAND(struct yaffs_DeviceStruct *dev)
{
	return dev->initialiseNAND(dev);
}

/**************************yaffs_guts.c************************************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

const char *yaffs_guts_c_version =
    "$Id: yaffs_guts.c,v 1.54 2007-12-13 15:35:17 wookey Exp $";

#include "yportenv.h"

#include "yaffsinterface.h"
#include "yaffs_guts.h"
#include "yaffs_tagsvalidity.h"

#include "yaffs_tagscompat.h"
#ifndef  CONFIG_YAFFS_USE_OWN_SORT
#include "yaffs_qsort.h"
#endif
#include "yaffs_nand.h"

#include "yaffs_checkptrw.h"

#include "yaffs_nand.h"
#include "yaffs_packedtags2.h"


#ifdef CONFIG_YAFFS_WINCE
void yfsd_LockYAFFS(BOOL fsLockOnly);
void yfsd_UnlockYAFFS(BOOL fsLockOnly);
#endif

#define YAFFS_PASSIVE_GC_CHUNKS 2

#include "yaffs_ecc.h"


/* Robustification (if it ever comes about...) */
static void yaffs_RetireBlock(yaffs_Device * dev, int blockInNAND);
static void yaffs_HandleWriteChunkError(yaffs_Device * dev, int chunkInNAND, int erasedOk);
static void yaffs_HandleWriteChunkOk(yaffs_Device * dev, int chunkInNAND,
				     const __u8 * data,
				     const yaffs_ExtendedTags * tags);
static void yaffs_HandleUpdateChunk(yaffs_Device * dev, int chunkInNAND,
				    const yaffs_ExtendedTags * tags);

/* Other local prototypes */
static int yaffs_UnlinkObject( yaffs_Object *obj);
static int yaffs_ObjectHasCachedWriteData(yaffs_Object *obj);

static void yaffs_HardlinkFixup(yaffs_Device *dev, yaffs_Object *hardList);

static int yaffs_WriteNewChunkWithTagsToNAND(yaffs_Device * dev,
					     const __u8 * buffer,
					     yaffs_ExtendedTags * tags,
					     int useReserve);
static int yaffs_PutChunkIntoFile(yaffs_Object * in, int chunkInInode,
				  int chunkInNAND, int inScan);

static yaffs_Object *yaffs_CreateNewObject(yaffs_Device * dev, int number,
					   yaffs_ObjectType type);
static void yaffs_AddObjectToDirectory(yaffs_Object * directory,
				       yaffs_Object * obj);
static int yaffs_UpdateObjectHeader(yaffs_Object * in, const YCHAR * name,
				    int force, int isShrink, int shadows);
static void yaffs_RemoveObjectFromDirectory(yaffs_Object * obj);
static int yaffs_CheckStructures(void);
static int yaffs_DeleteWorker(yaffs_Object * in, yaffs_Tnode * tn, __u32 level,
			      int chunkOffset, int *limit);
static int yaffs_DoGenericObjectDeletion(yaffs_Object * in);

static yaffs_BlockInfo *yaffs_GetBlockInfo(yaffs_Device * dev, int blockNo);

static __u8 *yaffs_GetTempBuffer(yaffs_Device * dev, int lineNo);
static void yaffs_ReleaseTempBuffer(yaffs_Device * dev, __u8 * buffer,
				    int lineNo);

static int yaffs_CheckChunkErased(struct yaffs_DeviceStruct *dev,
				  int chunkInNAND);

static int yaffs_UnlinkWorker(yaffs_Object * obj);
static void yaffs_DestroyObject(yaffs_Object * obj);

static int yaffs_TagsMatch(const yaffs_ExtendedTags * tags, int objectId,
			   int chunkInObject);

loff_t yaffs_GetFileSize(yaffs_Object * obj);

static int yaffs_AllocateChunk(yaffs_Device * dev, int useReserve, yaffs_BlockInfo **blockUsedPtr);

static void yaffs_VerifyFreeChunks(yaffs_Device * dev);

static void yaffs_CheckObjectDetailsLoaded(yaffs_Object *in);

#ifdef YAFFS_PARANOID
static int yaffs_CheckFileSanity(yaffs_Object * in);
#else
#define yaffs_CheckFileSanity(in)
#endif

static void yaffs_InvalidateWholeChunkCache(yaffs_Object * in);
static void yaffs_InvalidateChunkCache(yaffs_Object * object, int chunkId);

static void yaffs_InvalidateCheckpoint(yaffs_Device *dev);

static int yaffs_FindChunkInFile(yaffs_Object * in, int chunkInInode,
				 yaffs_ExtendedTags * tags);

static __u32 yaffs_GetChunkGroupBase(yaffs_Device *dev, yaffs_Tnode *tn, unsigned pos);
static yaffs_Tnode *yaffs_FindLevel0Tnode(yaffs_Device * dev,
					  yaffs_FileStructure * fStruct,
					  __u32 chunkId);


/* Function to calculate chunk and offset */

static void yaffs_AddrToChunk(yaffs_Device *dev, loff_t addr, __u32 *chunk, __u32 *offset)
{
	if(dev->chunkShift){
		/* Easy-peasy power of 2 case */
		*chunk  = (__u32)(addr >> dev->chunkShift);
		*offset = (__u32)(addr & dev->chunkMask);
	}
	else if(dev->crumbsPerChunk)
	{
		/* Case where we're using "crumbs" */
		*offset = (__u32)(addr & dev->crumbMask);
		addr >>= dev->crumbShift;
		*chunk = ((__u32)addr)/dev->crumbsPerChunk;
		*offset += ((addr - (*chunk * dev->crumbsPerChunk)) << dev->crumbShift);
	}
	else
		YBUG();
}

/* Function to return the number of shifts for a power of 2 greater than or equal
 * to the given number
 * Note we don't try to cater for all possible numbers and this does not have to
 * be hellishly efficient.
 */

static __u32 ShiftsGE(__u32 x)
{
	int extraBits;
	int nShifts;

	nShifts = extraBits = 0;

	while(x>1){
		if(x & 1) extraBits++;
		x>>=1;
		nShifts++;
	}

	if(extraBits)
		nShifts++;

	return nShifts;
}

/* Function to return the number of shifts to get a 1 in bit 0
 */

static __u32 ShiftDiv(__u32 x)
{
	int nShifts;

	nShifts =  0;

	if(!x) return 0;

	while( !(x&1)){
		x>>=1;
		nShifts++;
	}

	return nShifts;
}



/*
 * Temporary buffer manipulations.
 */

static int yaffs_InitialiseTempBuffers(yaffs_Device *dev)
{
	int i;
	__u8 *buf = (__u8 *)1;

	memset(dev->tempBuffer,0,sizeof(dev->tempBuffer));

	for (i = 0; buf && i < YAFFS_N_TEMP_BUFFERS; i++) {
		dev->tempBuffer[i].line = 0;	/* not in use */
		dev->tempBuffer[i].buffer = buf =
		    YMALLOC_DMA(dev->nDataBytesPerChunk);
	}

	return buf ? YAFFS_OK : YAFFS_FAIL;

}

static __u8 *yaffs_GetTempBuffer(yaffs_Device * dev, int lineNo)
{
	int i, j;
	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		if (dev->tempBuffer[i].line == 0) {
			dev->tempBuffer[i].line = lineNo;
			if ((i + 1) > dev->maxTemp) {
				dev->maxTemp = i + 1;
				for (j = 0; j <= i; j++)
					dev->tempBuffer[j].maxLine =
					    dev->tempBuffer[j].line;
			}

			return dev->tempBuffer[i].buffer;
		}
	}

	T(YAFFS_TRACE_BUFFERS,
	  (TSTR("Out of temp buffers at line %d, other held by lines:"),
	   lineNo));
	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		T(YAFFS_TRACE_BUFFERS, (TSTR(" %d "), dev->tempBuffer[i].line));
	}
	T(YAFFS_TRACE_BUFFERS, (TSTR(" " TENDSTR)));

	/*
	 * If we got here then we have to allocate an unmanaged one
	 * This is not good.
	 */

	dev->unmanagedTempAllocations++;
	return YMALLOC(dev->nDataBytesPerChunk);

}

static void yaffs_ReleaseTempBuffer(yaffs_Device * dev, __u8 * buffer,
				    int lineNo)
{
	int i;
	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		if (dev->tempBuffer[i].buffer == buffer) {
			dev->tempBuffer[i].line = 0;
			return;
		}
	}

	if (buffer) {
		/* assume it is an unmanaged one. */
		T(YAFFS_TRACE_BUFFERS,
		  (TSTR("Releasing unmanaged temp buffer in line %d" TENDSTR),
		   lineNo));
		YFREE(buffer);
		dev->unmanagedTempDeallocations++;
	}

}

/*
 * Determine if we have a managed buffer.
 */
int yaffs_IsManagedTempBuffer(yaffs_Device * dev, const __u8 * buffer)
{
	int i;
	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		if (dev->tempBuffer[i].buffer == buffer)
			return 1;

	}

    for (i = 0; i < dev->nShortOpCaches; i++) {
        if( dev->srCache[i].data == buffer )
            return 1;

    }

    if (buffer == dev->checkpointBuffer)
      return 1;

    T(YAFFS_TRACE_ALWAYS,
	  (TSTR("yaffs: unmaged buffer detected.\n" TENDSTR)));
    return 0;
}



/*
 * Chunk bitmap manipulations
 */

static Y_INLINE __u8 *yaffs_BlockBits(yaffs_Device * dev, int blk)
{
	if (blk < dev->internalStartBlock || blk > dev->internalEndBlock) {
		T(YAFFS_TRACE_ERROR,
		  (TSTR("**>> yaffs: BlockBits block %d is not valid" TENDSTR),
		   blk));
		YBUG();
	}
	return dev->chunkBits +
	    (dev->chunkBitmapStride * (blk - dev->internalStartBlock));
}

static Y_INLINE void yaffs_VerifyChunkBitId(yaffs_Device *dev, int blk, int chunk)
{
	if(blk < dev->internalStartBlock || blk > dev->internalEndBlock ||
	   chunk < 0 || chunk >= dev->nChunksPerBlock) {
	   T(YAFFS_TRACE_ERROR,
	    (TSTR("**>> yaffs: Chunk Id (%d:%d) invalid"TENDSTR),blk,chunk));
	    YBUG();
	}
}

static Y_INLINE void yaffs_ClearChunkBits(yaffs_Device * dev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);

	memset(blkBits, 0, dev->chunkBitmapStride);
}

static Y_INLINE void yaffs_ClearChunkBit(yaffs_Device * dev, int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);

	yaffs_VerifyChunkBitId(dev,blk,chunk);

	blkBits[chunk / 8] &= ~(1 << (chunk & 7));
}

static Y_INLINE void yaffs_SetChunkBit(yaffs_Device * dev, int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);

	yaffs_VerifyChunkBitId(dev,blk,chunk);

	blkBits[chunk / 8] |= (1 << (chunk & 7));
}

static Y_INLINE int yaffs_CheckChunkBit(yaffs_Device * dev, int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	yaffs_VerifyChunkBitId(dev,blk,chunk);

	return (blkBits[chunk / 8] & (1 << (chunk & 7))) ? 1 : 0;
}

static Y_INLINE int yaffs_StillSomeChunkBits(yaffs_Device * dev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	int i;
	for (i = 0; i < dev->chunkBitmapStride; i++) {
		if (*blkBits)
			return 1;
		blkBits++;
	}
	return 0;
}

static int yaffs_CountChunkBits(yaffs_Device * dev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	int i;
	int n = 0;
	for (i = 0; i < dev->chunkBitmapStride; i++) {
		__u8 x = *blkBits;
		while(x){
			if(x & 1)
				n++;
			x >>=1;
		}

		blkBits++;
	}
	return n;
}

/*
 * Verification code
 */

static int yaffs_SkipVerification(yaffs_Device *dev)
{
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY | YAFFS_TRACE_VERIFY_FULL));
}

static int yaffs_SkipFullVerification(yaffs_Device *dev)
{
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY_FULL));
}

static int yaffs_SkipNANDVerification(yaffs_Device *dev)
{
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY_NAND));
}

static const char * blockStateName[] = {
"Unknown",
"Needs scanning",
"Scanning",
"Empty",
"Allocating",
"Full",
"Dirty",
"Checkpoint",
"Collecting",
"Dead"
};

static void yaffs_VerifyBlock(yaffs_Device *dev,yaffs_BlockInfo *bi,int n)
{
	int actuallyUsed;
	int inUse;

	if(yaffs_SkipVerification(dev))
		return;

	/* Report illegal runtime states */
	if(bi->blockState <0 || bi->blockState >= YAFFS_NUMBER_OF_BLOCK_STATES)
		T(YAFFS_TRACE_VERIFY,(TSTR("Block %d has undefined state %d"TENDSTR),n,bi->blockState));

	switch(bi->blockState){
	 case YAFFS_BLOCK_STATE_UNKNOWN:
	 case YAFFS_BLOCK_STATE_SCANNING:
	 case YAFFS_BLOCK_STATE_NEEDS_SCANNING:
		T(YAFFS_TRACE_VERIFY,(TSTR("Block %d has bad run-state %s"TENDSTR),
		n,blockStateName[bi->blockState]));
	}

	/* Check pages in use and soft deletions are legal */

	actuallyUsed = bi->pagesInUse - bi->softDeletions;

	if(bi->pagesInUse < 0 || bi->pagesInUse > dev->nChunksPerBlock ||
	   bi->softDeletions < 0 || bi->softDeletions > dev->nChunksPerBlock ||
	   actuallyUsed < 0 || actuallyUsed > dev->nChunksPerBlock)
		T(YAFFS_TRACE_VERIFY,(TSTR("Block %d has illegal values pagesInUsed %d softDeletions %d"TENDSTR),
		n,bi->pagesInUse,bi->softDeletions));


	/* Check chunk bitmap legal */
	inUse = yaffs_CountChunkBits(dev,n);
	if(inUse != bi->pagesInUse)
		T(YAFFS_TRACE_VERIFY,(TSTR("Block %d has inconsistent values pagesInUse %d counted chunk bits %d"TENDSTR),
			n,bi->pagesInUse,inUse));

	/* Check that the sequence number is valid.
	 * Ten million is legal, but is very unlikely
	 */
	if(dev->isYaffs2 &&
	   (bi->blockState == YAFFS_BLOCK_STATE_ALLOCATING || bi->blockState == YAFFS_BLOCK_STATE_FULL) &&
	   (bi->sequenceNumber < YAFFS_LOWEST_SEQUENCE_NUMBER || bi->sequenceNumber > 10000000 ))
		T(YAFFS_TRACE_VERIFY,(TSTR("Block %d has suspect sequence number of %d"TENDSTR),
		n,bi->sequenceNumber));

}

static void yaffs_VerifyCollectedBlock(yaffs_Device *dev,yaffs_BlockInfo *bi,int n)
{
	yaffs_VerifyBlock(dev,bi,n);

	/* After collection the block should be in the erased state */
	/* TODO: This will need to change if we do partial gc */

	if(bi->blockState != YAFFS_BLOCK_STATE_EMPTY){
		T(YAFFS_TRACE_ERROR,(TSTR("Block %d is in state %d after gc, should be erased"TENDSTR),
			n,bi->blockState));
	}
}

static void yaffs_VerifyBlocks(yaffs_Device *dev)
{
	int i;
	int nBlocksPerState[YAFFS_NUMBER_OF_BLOCK_STATES];
	int nIllegalBlockStates = 0;


	if(yaffs_SkipVerification(dev))
		return;

	memset(nBlocksPerState,0,sizeof(nBlocksPerState));


	for(i = dev->internalStartBlock; i <= dev->internalEndBlock; i++){
		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev,i);
		yaffs_VerifyBlock(dev,bi,i);

		if(bi->blockState >=0 && bi->blockState < YAFFS_NUMBER_OF_BLOCK_STATES)
			nBlocksPerState[bi->blockState]++;
		else
			nIllegalBlockStates++;

	}

	T(YAFFS_TRACE_VERIFY,(TSTR(""TENDSTR)));
	T(YAFFS_TRACE_VERIFY,(TSTR("Block summary"TENDSTR)));

	T(YAFFS_TRACE_VERIFY,(TSTR("%d blocks have illegal states"TENDSTR),nIllegalBlockStates));
	if(nBlocksPerState[YAFFS_BLOCK_STATE_ALLOCATING] > 1)
		T(YAFFS_TRACE_VERIFY,(TSTR("Too many allocating blocks"TENDSTR)));

	for(i = 0; i < YAFFS_NUMBER_OF_BLOCK_STATES; i++)
		T(YAFFS_TRACE_VERIFY,
		  (TSTR("%s %d blocks"TENDSTR),
		  blockStateName[i],nBlocksPerState[i]));

	if(dev->blocksInCheckpoint != nBlocksPerState[YAFFS_BLOCK_STATE_CHECKPOINT])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Checkpoint block count wrong dev %d count %d"TENDSTR),
		 dev->blocksInCheckpoint, nBlocksPerState[YAFFS_BLOCK_STATE_CHECKPOINT]));

	if(dev->nErasedBlocks != nBlocksPerState[YAFFS_BLOCK_STATE_EMPTY])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Erased block count wrong dev %d count %d"TENDSTR),
		 dev->nErasedBlocks, nBlocksPerState[YAFFS_BLOCK_STATE_EMPTY]));

	if(nBlocksPerState[YAFFS_BLOCK_STATE_COLLECTING] > 1)
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Too many collecting blocks %d (max is 1)"TENDSTR),
		 nBlocksPerState[YAFFS_BLOCK_STATE_COLLECTING]));

	T(YAFFS_TRACE_VERIFY,(TSTR(""TENDSTR)));

}

/*
 * Verify the object header. oh must be valid, but obj and tags may be NULL in which
 * case those tests will not be performed.
 */
static void yaffs_VerifyObjectHeader(yaffs_Object *obj, yaffs_ObjectHeader *oh, yaffs_ExtendedTags *tags, int parentCheck)
{
	if(yaffs_SkipVerification(obj->myDev))
		return;

	if(!(tags && obj && oh)){
	 	T(YAFFS_TRACE_VERIFY,
		 		(TSTR("Verifying object header tags %x obj %x oh %x"TENDSTR),
		 		(__u32)tags,(__u32)obj,(__u32)oh));
		return;
	}

	if(oh->type <= YAFFS_OBJECT_TYPE_UNKNOWN ||
	   oh->type > YAFFS_OBJECT_TYPE_MAX)
	 	T(YAFFS_TRACE_VERIFY,
		 (TSTR("Obj %d header type is illegal value 0x%x"TENDSTR),
		 tags->objectId, oh->type));

	if(tags->objectId != obj->objectId)
	 	T(YAFFS_TRACE_VERIFY,
		 (TSTR("Obj %d header mismatch objectId %d"TENDSTR),
		 tags->objectId, obj->objectId));


	/*
	 * Check that the object's parent ids match if parentCheck requested.
	 *
	 * Tests do not apply to the root object.
	 */

	if(parentCheck && tags->objectId > 1 && !obj->parent)
	 	T(YAFFS_TRACE_VERIFY,
		 (TSTR("Obj %d header mismatch parentId %d obj->parent is NULL"TENDSTR),
	 	 tags->objectId, oh->parentObjectId));


	if(parentCheck && obj->parent &&
	   oh->parentObjectId != obj->parent->objectId &&
	   (oh->parentObjectId != YAFFS_OBJECTID_UNLINKED ||
	    obj->parent->objectId != YAFFS_OBJECTID_DELETED))
	 	T(YAFFS_TRACE_VERIFY,
		 (TSTR("Obj %d header mismatch parentId %d parentObjectId %d"TENDSTR),
	 	 tags->objectId, oh->parentObjectId, obj->parent->objectId));


	if(tags->objectId > 1 && oh->name[0] == 0) /* Null name */
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d header name is NULL"TENDSTR),
		 obj->objectId));

	if(tags->objectId > 1 && ((__u8)(oh->name[0])) == 0xff) /* Trashed name */
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d header name is 0xFF"TENDSTR),
		 obj->objectId));
}



static int yaffs_VerifyTnodeWorker(yaffs_Object * obj, yaffs_Tnode * tn,
				  	__u32 level, int chunkOffset)
{
	int i;
	yaffs_Device *dev = obj->myDev;
	int ok = 1;
	int nTnodeBytes = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if (tn) {
		if (level > 0) {

			for (i = 0; i < YAFFS_NTNODES_INTERNAL && ok; i++){
				if (tn->internal[i]) {
					ok = yaffs_VerifyTnodeWorker(obj,
							tn->internal[i],
							level - 1,
							(chunkOffset<<YAFFS_TNODES_INTERNAL_BITS) + i);
				}
			}
		} else if (level == 0) {
			int i;
			yaffs_ExtendedTags tags;
			__u32 objectId = obj->objectId;

			chunkOffset <<=  YAFFS_TNODES_LEVEL0_BITS;

			for(i = 0; i < YAFFS_NTNODES_LEVEL0; i++){
				__u32 theChunk = yaffs_GetChunkGroupBase(dev,tn,i);

				if(theChunk > 0){
					/* T(~0,(TSTR("verifying (%d:%d) %d"TENDSTR),tags.objectId,tags.chunkId,theChunk)); */
					yaffs_ReadChunkWithTagsFromNAND(dev,theChunk,NULL, &tags);
					if(tags.objectId != objectId || tags.chunkId != chunkOffset){
						T(~0,(TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
							objectId, chunkOffset, theChunk,
							tags.objectId, tags.chunkId));
					}
				}
				chunkOffset++;
			}
		}
	}

	return ok;

}


static void yaffs_VerifyFile(yaffs_Object *obj)
{
	int requiredTallness;
	int actualTallness;
	__u32 lastChunk;
	__u32 x;
	__u32 i;
	int ok;
	yaffs_Device *dev;
	yaffs_ExtendedTags tags;
	yaffs_Tnode *tn;
	__u32 objectId;

	if(obj && yaffs_SkipVerification(obj->myDev))
		return;

	dev = obj->myDev;
	objectId = obj->objectId;

	/* Check file size is consistent with tnode depth */
	lastChunk =  obj->variant.fileVariant.fileSize / dev->nDataBytesPerChunk + 1;
	x = lastChunk >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (x> 0) {
		x >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}

	actualTallness = obj->variant.fileVariant.topLevel;

	if(requiredTallness > actualTallness )
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d had tnode tallness %d, needs to be %d"TENDSTR),
		 obj->objectId,actualTallness, requiredTallness));


	/* Check that the chunks in the tnode tree are all correct.
	 * We do this by scanning through the tnode tree and
	 * checking the tags for every chunk match.
	 */

	if(yaffs_SkipNANDVerification(dev))
		return;

	for(i = 1; i <= lastChunk; i++){
		tn = yaffs_FindLevel0Tnode(dev, &obj->variant.fileVariant,i);

		if (tn) {
			__u32 theChunk = yaffs_GetChunkGroupBase(dev,tn,i);
			if(theChunk > 0){
				/* T(~0,(TSTR("verifying (%d:%d) %d"TENDSTR),objectId,i,theChunk)); */
				yaffs_ReadChunkWithTagsFromNAND(dev,theChunk,NULL, &tags);
				if(tags.objectId != objectId || tags.chunkId != i){
					T(~0,(TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
						objectId, i, theChunk,
						tags.objectId, tags.chunkId));
				}
			}
		}

	}

}

static void yaffs_VerifyDirectory(yaffs_Object *obj)
{
	if(obj && yaffs_SkipVerification(obj->myDev))
		return;

}

static void yaffs_VerifyHardLink(yaffs_Object *obj)
{
	if(obj && yaffs_SkipVerification(obj->myDev))
		return;

	/* Verify sane equivalent object */
}

static void yaffs_VerifySymlink(yaffs_Object *obj)
{
	if(obj && yaffs_SkipVerification(obj->myDev))
		return;

	/* Verify symlink string */
}

static void yaffs_VerifySpecial(yaffs_Object *obj)
{
	if(obj && yaffs_SkipVerification(obj->myDev))
		return;
}

static void yaffs_VerifyObject(yaffs_Object *obj)
{
	yaffs_Device *dev;

	__u32 chunkMin;
	__u32 chunkMax;

	__u32 chunkIdOk;
	__u32 chunkIsLive;

	if(!obj)
		return;

	dev = obj->myDev;

	if(yaffs_SkipVerification(dev))
		return;

	/* Check sane object header chunk */

	chunkMin = dev->internalStartBlock * dev->nChunksPerBlock;
	chunkMax = (dev->internalEndBlock+1) * dev->nChunksPerBlock - 1;

	chunkIdOk = (obj->chunkId >= chunkMin && obj->chunkId <= chunkMax);
	chunkIsLive = chunkIdOk &&
			yaffs_CheckChunkBit(dev,
					    obj->chunkId / dev->nChunksPerBlock,
					    obj->chunkId % dev->nChunksPerBlock);
	if(!obj->fake &&
	    (!chunkIdOk || !chunkIsLive)) {
	   T(YAFFS_TRACE_VERIFY,
	   (TSTR("Obj %d has chunkId %d %s %s"TENDSTR),
	   obj->objectId,obj->chunkId,
	   chunkIdOk ? "" : ",out of range",
	   chunkIsLive || !chunkIdOk ? "" : ",marked as deleted"));
	}

	if(chunkIdOk && chunkIsLive &&!yaffs_SkipNANDVerification(dev)) {
		yaffs_ExtendedTags tags;
		yaffs_ObjectHeader *oh;
		__u8 *buffer = yaffs_GetTempBuffer(dev,__LINE__);

		oh = (yaffs_ObjectHeader *)buffer;

		yaffs_ReadChunkWithTagsFromNAND(dev, obj->chunkId,buffer, &tags);

		yaffs_VerifyObjectHeader(obj,oh,&tags,1);

		yaffs_ReleaseTempBuffer(dev,buffer,__LINE__);
	}

	/* Verify it has a parent */
	if(obj && !obj->fake &&
	   (!obj->parent || obj->parent->myDev != dev)){
	   T(YAFFS_TRACE_VERIFY,
	   (TSTR("Obj %d has parent pointer %p which does not look like an object"TENDSTR),
	   obj->objectId,obj->parent));
	}

	/* Verify parent is a directory */
	if(obj->parent && obj->parent->variantType != YAFFS_OBJECT_TYPE_DIRECTORY){
	   T(YAFFS_TRACE_VERIFY,
	   (TSTR("Obj %d's parent is not a directory (type %d)"TENDSTR),
	   obj->objectId,obj->parent->variantType));
	}

	switch(obj->variantType){
	case YAFFS_OBJECT_TYPE_FILE:
		yaffs_VerifyFile(obj);
		break;
	case YAFFS_OBJECT_TYPE_SYMLINK:
		yaffs_VerifySymlink(obj);
		break;
	case YAFFS_OBJECT_TYPE_DIRECTORY:
		yaffs_VerifyDirectory(obj);
		break;
	case YAFFS_OBJECT_TYPE_HARDLINK:
		yaffs_VerifyHardLink(obj);
		break;
	case YAFFS_OBJECT_TYPE_SPECIAL:
		yaffs_VerifySpecial(obj);
		break;
	case YAFFS_OBJECT_TYPE_UNKNOWN:
	default:
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d has illegaltype %d"TENDSTR),
		obj->objectId,obj->variantType));
		break;
	}


}

static void yaffs_VerifyObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	int i;
	struct list_head *lh;

	if(yaffs_SkipVerification(dev))
		return;

	/* Iterate through the objects in each hash entry */

	 for(i = 0; i <  YAFFS_NOBJECT_BUCKETS; i++){
	 	list_for_each(lh, &dev->objectBucket[i].list) {
			if (lh) {
				obj = list_entry(lh, yaffs_Object, hashLink);
				yaffs_VerifyObject(obj);
			}
		}
	 }

}


/*
 *  Simple hash function. Needs to have a reasonable spread
 */

static Y_INLINE int yaffs_HashFunction(int n)
{
	n = abs(n);
	return (n % YAFFS_NOBJECT_BUCKETS);
}

/*
 * Access functions to useful fake objects
 */

yaffs_Object *yaffs_Root(yaffs_Device * dev)
{
	return dev->rootDir;
}

yaffs_Object *yaffs_LostNFound(yaffs_Device * dev)
{
	return dev->lostNFoundDir;
}


/*
 *  Erased NAND checking functions
 */

int yaffs_CheckFF(__u8 * buffer, int nBytes)
{
	/* Horrible, slow implementation */
	while (nBytes--) {
		if (*buffer != 0xFF)
			return 0;
		buffer++;
	}
	return 1;
}

static int yaffs_CheckChunkErased(struct yaffs_DeviceStruct *dev,
				  int chunkInNAND)
{

	int retval = YAFFS_OK;
	__u8 *data = yaffs_GetTempBuffer(dev, __LINE__);
	yaffs_ExtendedTags tags;
	int result;

	result = yaffs_ReadChunkWithTagsFromNAND(dev, chunkInNAND, data, &tags);

	if(tags.eccResult > YAFFS_ECC_RESULT_NO_ERROR)
		retval = YAFFS_FAIL;


	if (!yaffs_CheckFF(data, dev->nDataBytesPerChunk) || tags.chunkUsed) {
		T(YAFFS_TRACE_NANDACCESS,
		  (TSTR("Chunk %d not erased" TENDSTR), chunkInNAND));
		retval = YAFFS_FAIL;
	}

	yaffs_ReleaseTempBuffer(dev, data, __LINE__);

	return retval;

}

static int yaffs_WriteNewChunkWithTagsToNAND(struct yaffs_DeviceStruct *dev,
					     const __u8 * data,
					     yaffs_ExtendedTags * tags,
					     int useReserve)
{
	int attempts = 0;
	int writeOk = 0;
	int chunk;

	yaffs_InvalidateCheckpoint(dev);

	do {
		yaffs_BlockInfo *bi = 0;
		int erasedOk = 0;

		chunk = yaffs_AllocateChunk(dev, useReserve, &bi);
		if (chunk < 0) {
			/* no space */
			break;
		}

		/* First check this chunk is erased, if it needs
		 * checking.  The checking policy (unless forced
		 * always on) is as follows:
		 *
		 * Check the first page we try to write in a block.
		 * If the check passes then we don't need to check any
		 * more.	If the check fails, we check again...
		 * If the block has been erased, we don't need to check.
		 *
		 * However, if the block has been prioritised for gc,
		 * then we think there might be something odd about
		 * this block and stop using it.
		 *
		 * Rationale: We should only ever see chunks that have
		 * not been erased if there was a partially written
		 * chunk due to power loss.  This checking policy should
		 * catch that case with very few checks and thus save a
		 * lot of checks that are most likely not needed.
		 */
		if (bi->gcPrioritise) {
			yaffs_DeleteChunk(dev, chunk, 1, __LINE__);
			/* try another chunk */
			continue;
		}

		/* let's give it a try */
		attempts++;

#ifdef CONFIG_YAFFS_ALWAYS_CHECK_CHUNK_ERASED
		bi->skipErasedCheck = 0;
#endif
		if (!bi->skipErasedCheck) {
			erasedOk = yaffs_CheckChunkErased(dev, chunk);
			if (erasedOk != YAFFS_OK) {
				T(YAFFS_TRACE_ERROR,
				(TSTR ("**>> yaffs chunk %d was not erased"
				TENDSTR), chunk));

				/* try another chunk */
				continue;
			}
			bi->skipErasedCheck = 1;
		}

		writeOk = yaffs_WriteChunkWithTagsToNAND(dev, chunk,
				data, tags);
		if (writeOk != YAFFS_OK) {
			yaffs_HandleWriteChunkError(dev, chunk, erasedOk);
			/* try another chunk */
			continue;
		}

		/* Copy the data into the robustification buffer */
		yaffs_HandleWriteChunkOk(dev, chunk, data, tags);

	} while (writeOk != YAFFS_OK &&
	        (yaffs_wr_attempts <= 0 || attempts <= yaffs_wr_attempts));

	if(!writeOk)
		chunk = -1;

	if (attempts > 1) {
		T(YAFFS_TRACE_ERROR,
			(TSTR("**>> yaffs write required %d attempts" TENDSTR),
			attempts));

		dev->nRetriedWrites += (attempts - 1);
	}

	return chunk;
}

/*
 * Block retiring for handling a broken block.
 */

static void yaffs_RetireBlock(yaffs_Device * dev, int blockInNAND)
{
	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, blockInNAND);

	yaffs_InvalidateCheckpoint(dev);

	yaffs_MarkBlockBad(dev, blockInNAND);

	bi->blockState = YAFFS_BLOCK_STATE_DEAD;
	bi->gcPrioritise = 0;
	bi->needsRetiring = 0;

	dev->nRetiredBlocks++;
}

/*
 * Functions for robustisizing TODO
 *
 */

static void yaffs_HandleWriteChunkOk(yaffs_Device * dev, int chunkInNAND,
				     const __u8 * data,
				     const yaffs_ExtendedTags * tags)
{
}

static void yaffs_HandleUpdateChunk(yaffs_Device * dev, int chunkInNAND,
				    const yaffs_ExtendedTags * tags)
{
}

void yaffs_HandleChunkError(yaffs_Device *dev, yaffs_BlockInfo *bi)
{
	if(!bi->gcPrioritise){
		bi->gcPrioritise = 1;
		dev->hasPendingPrioritisedGCs = 1;
		bi->chunkErrorStrikes ++;

		if(bi->chunkErrorStrikes > 3){
			bi->needsRetiring = 1; /* Too many stikes, so retire this */
			T(YAFFS_TRACE_ALWAYS, (TSTR("yaffs: Block struck out" TENDSTR)));

		}

	}
}

static void yaffs_HandleWriteChunkError(yaffs_Device * dev, int chunkInNAND, int erasedOk)
{

	int blockInNAND = chunkInNAND / dev->nChunksPerBlock;
	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, blockInNAND);

	yaffs_HandleChunkError(dev,bi);


	if(erasedOk ) {
		/* Was an actual write failure, so mark the block for retirement  */
		bi->needsRetiring = 1;
		T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
		  (TSTR("**>> Block %d needs retiring" TENDSTR), blockInNAND));


	}

	/* Delete the chunk */
	yaffs_DeleteChunk(dev, chunkInNAND, 1, __LINE__);
}


/*---------------- Name handling functions ------------*/

static __u16 yaffs_CalcNameSum(const YCHAR * name)
{
	__u16 sum = 0;
	__u16 i = 1;

	YUCHAR *bname = (YUCHAR *) name;
	if (bname) {
		while ((*bname) && (i < (YAFFS_MAX_NAME_LENGTH/2))) {

#ifdef CONFIG_YAFFS_CASE_INSENSITIVE
			sum += yaffs_toupper(*bname) * i;
#else
			sum += (*bname) * i;
#endif
			i++;
			bname++;
		}
	}
	return sum;
}

static void yaffs_SetObjectName(yaffs_Object * obj, const YCHAR * name)
{
#ifdef CONFIG_YAFFS_SHORT_NAMES_IN_RAM
	if (name && yaffs_strlen(name) <= YAFFS_SHORT_NAME_LENGTH) {
		yaffs_strcpy(obj->shortName, name);
	} else {
		obj->shortName[0] = _Y('\0');
	}
#endif
	obj->sum = yaffs_CalcNameSum(name);
}

/*-------------------- TNODES -------------------

 * List of spare tnodes
 * The list is hooked together using the first pointer
 * in the tnode.
 */

/* yaffs_CreateTnodes creates a bunch more tnodes and
 * adds them to the tnode free list.
 * Don't use this function directly
 */

static int yaffs_CreateTnodes(yaffs_Device * dev, int nTnodes)
{
	int i;
	int tnodeSize;
	yaffs_Tnode *newTnodes;
	__u8 *mem;
	yaffs_Tnode *curr;
	yaffs_Tnode *next;
	yaffs_TnodeList *tnl;

	if (nTnodes < 1)
		return YAFFS_OK;

	/* Calculate the tnode size in bytes for variable width tnode support.
	 * Must be a multiple of 32-bits  */
	tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if(tnodeSize < sizeof(yaffs_Tnode))
		tnodeSize = sizeof(yaffs_Tnode);


	/* make these things */

	newTnodes = YMALLOC(nTnodes * tnodeSize);
	mem = (__u8 *)newTnodes;

	if (!newTnodes) {
		T(YAFFS_TRACE_ERROR,
		  (TSTR("yaffs: Could not allocate Tnodes" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Hook them into the free list */
#if 0
	for (i = 0; i < nTnodes - 1; i++) {
		newTnodes[i].internal[0] = &newTnodes[i + 1];
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
		newTnodes[i].internal[YAFFS_NTNODES_INTERNAL] = (void *)1;
#endif
	}

	newTnodes[nTnodes - 1].internal[0] = dev->freeTnodes;
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
	newTnodes[nTnodes - 1].internal[YAFFS_NTNODES_INTERNAL] = (void *)1;
#endif
	dev->freeTnodes = newTnodes;
#else
	/* New hookup for wide tnodes */
	for(i = 0; i < nTnodes -1; i++) {
		curr = (yaffs_Tnode *) &mem[i * tnodeSize];
		next = (yaffs_Tnode *) &mem[(i+1) * tnodeSize];
		curr->internal[0] = next;
	}

	curr = (yaffs_Tnode *) &mem[(nTnodes - 1) * tnodeSize];
	curr->internal[0] = dev->freeTnodes;
	dev->freeTnodes = (yaffs_Tnode *)mem;

#endif


	dev->nFreeTnodes += nTnodes;
	dev->nTnodesCreated += nTnodes;

	/* Now add this bunch of tnodes to a list for freeing up.
	 * NB If we can't add this to the management list it isn't fatal
	 * but it just means we can't free this bunch of tnodes later.
	 */

	tnl = YMALLOC(sizeof(yaffs_TnodeList));
	if (!tnl) {
		T(YAFFS_TRACE_ERROR,
		  (TSTR
		   ("yaffs: Could not add tnodes to management list" TENDSTR)));
		   return YAFFS_FAIL;

	} else {
		tnl->tnodes = newTnodes;
		tnl->next = dev->allocatedTnodeList;
		dev->allocatedTnodeList = tnl;
	}

	T(YAFFS_TRACE_ALLOCATE, (TSTR("yaffs: Tnodes added" TENDSTR)));

	return YAFFS_OK;
}

/* GetTnode gets us a clean tnode. Tries to make allocate more if we run out */

static yaffs_Tnode *yaffs_GetTnodeRaw(yaffs_Device * dev)
{
	yaffs_Tnode *tn = NULL;

	/* If there are none left make more */
	if (!dev->freeTnodes) {
		yaffs_CreateTnodes(dev, YAFFS_ALLOCATION_NTNODES);
	}

	if (dev->freeTnodes) {
		tn = dev->freeTnodes;
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
		if (tn->internal[YAFFS_NTNODES_INTERNAL] != (void *)1) {
			/* Hoosterman, this thing looks like it isn't in the list */
			T(YAFFS_TRACE_ALWAYS,
			  (TSTR("yaffs: Tnode list bug 1" TENDSTR)));
		}
#endif
		dev->freeTnodes = dev->freeTnodes->internal[0];
		dev->nFreeTnodes--;
	}

	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/

	return tn;
}

static yaffs_Tnode *yaffs_GetTnode(yaffs_Device * dev)
{
	yaffs_Tnode *tn = yaffs_GetTnodeRaw(dev);
	int tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if(tnodeSize < sizeof(yaffs_Tnode))
		tnodeSize = sizeof(yaffs_Tnode);

	if(tn)
		memset(tn, 0, tnodeSize);

	return tn;
}

/* FreeTnode frees up a tnode and puts it back on the free list */
static void yaffs_FreeTnode(yaffs_Device * dev, yaffs_Tnode * tn)
{
	if (tn) {
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
		if (tn->internal[YAFFS_NTNODES_INTERNAL] != 0) {
			/* Hoosterman, this thing looks like it is already in the list */
			T(YAFFS_TRACE_ALWAYS,
			  (TSTR("yaffs: Tnode list bug 2" TENDSTR)));
		}
		tn->internal[YAFFS_NTNODES_INTERNAL] = (void *)1;
#endif
		tn->internal[0] = dev->freeTnodes;
		dev->freeTnodes = tn;
		dev->nFreeTnodes++;
	}
	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/

}

static void yaffs_DeinitialiseTnodes(yaffs_Device * dev)
{
	/* Free the list of allocated tnodes */
	yaffs_TnodeList *tmp;

	while (dev->allocatedTnodeList) {
		tmp = dev->allocatedTnodeList->next;

		YFREE(dev->allocatedTnodeList->tnodes);
		YFREE(dev->allocatedTnodeList);
		dev->allocatedTnodeList = tmp;

	}

	dev->freeTnodes = NULL;
	dev->nFreeTnodes = 0;
}

static void yaffs_InitialiseTnodes(yaffs_Device * dev)
{
	dev->allocatedTnodeList = NULL;
	dev->freeTnodes = NULL;
	dev->nFreeTnodes = 0;
	dev->nTnodesCreated = 0;

}


void yaffs_PutLevel0Tnode(yaffs_Device *dev, yaffs_Tnode *tn, unsigned pos, unsigned val)
{
  __u32 *map = (__u32 *)tn;
  __u32 bitInMap;
  __u32 bitInWord;
  __u32 wordInMap;
  __u32 mask;

  pos &= YAFFS_TNODES_LEVEL0_MASK;
  val >>= dev->chunkGroupBits;

  bitInMap = pos * dev->tnodeWidth;
  wordInMap = bitInMap /32;
  bitInWord = bitInMap & (32 -1);

  mask = dev->tnodeMask << bitInWord;

  map[wordInMap] &= ~mask;
  map[wordInMap] |= (mask & (val << bitInWord));

  if(dev->tnodeWidth > (32-bitInWord)) {
    bitInWord = (32 - bitInWord);
    wordInMap++;;
    mask = dev->tnodeMask >> (/*dev->tnodeWidth -*/ bitInWord);
    map[wordInMap] &= ~mask;
    map[wordInMap] |= (mask & (val >> bitInWord));
  }
}

static __u32 yaffs_GetChunkGroupBase(yaffs_Device *dev, yaffs_Tnode *tn, unsigned pos)
{
  __u32 *map = (__u32 *)tn;
  __u32 bitInMap;
  __u32 bitInWord;
  __u32 wordInMap;
  __u32 val;

  pos &= YAFFS_TNODES_LEVEL0_MASK;

  bitInMap = pos * dev->tnodeWidth;
  wordInMap = bitInMap /32;
  bitInWord = bitInMap & (32 -1);

  val = map[wordInMap] >> bitInWord;

  if(dev->tnodeWidth > (32-bitInWord)) {
    bitInWord = (32 - bitInWord);
    wordInMap++;;
    val |= (map[wordInMap] << bitInWord);
  }

  val &= dev->tnodeMask;
  val <<= dev->chunkGroupBits;

  return val;
}

/* ------------------- End of individual tnode manipulation -----------------*/

/* ---------Functions to manipulate the look-up tree (made up of tnodes) ------
 * The look up tree is represented by the top tnode and the number of topLevel
 * in the tree. 0 means only the level 0 tnode is in the tree.
 */

/* FindLevel0Tnode finds the level 0 tnode, if one exists. */
static yaffs_Tnode *yaffs_FindLevel0Tnode(yaffs_Device * dev,
					  yaffs_FileStructure * fStruct,
					  __u32 chunkId)
{

	yaffs_Tnode *tn = fStruct->top;
	__u32 i;
	int requiredTallness;
	int level = fStruct->topLevel;

	/* Check sane level and chunk Id */
	if (level < 0 || level > YAFFS_TNODES_MAX_LEVEL) {
		return NULL;
	}

	if (chunkId > YAFFS_MAX_CHUNK_ID) {
		return NULL;
	}

	/* First check we're tall enough (ie enough topLevel) */

	i = chunkId >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (i) {
		i >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}

	if (requiredTallness > fStruct->topLevel) {
		/* Not tall enough, so we can't find it, return NULL. */
		return NULL;
	}

	/* Traverse down to level 0 */
	while (level > 0 && tn) {
		tn = tn->
		    internal[(chunkId >>
			       ( YAFFS_TNODES_LEVEL0_BITS +
			         (level - 1) *
			         YAFFS_TNODES_INTERNAL_BITS)
			      ) &
			     YAFFS_TNODES_INTERNAL_MASK];
		level--;

	}

	return tn;
}

/* AddOrFindLevel0Tnode finds the level 0 tnode if it exists, otherwise first expands the tree.
 * This happens in two steps:
 *  1. If the tree isn't tall enough, then make it taller.
 *  2. Scan down the tree towards the level 0 tnode adding tnodes if required.
 *
 * Used when modifying the tree.
 *
 *  If the tn argument is NULL, then a fresh tnode will be added otherwise the specified tn will
 *  be plugged into the ttree.
 */

static yaffs_Tnode *yaffs_AddOrFindLevel0Tnode(yaffs_Device * dev,
					       yaffs_FileStructure * fStruct,
					       __u32 chunkId,
					       yaffs_Tnode *passedTn)
{

	int requiredTallness;
	int i;
	int l;
	yaffs_Tnode *tn;

	__u32 x;


	/* Check sane level and page Id */
	if (fStruct->topLevel < 0 || fStruct->topLevel > YAFFS_TNODES_MAX_LEVEL) {
		return NULL;
	}

	if (chunkId > YAFFS_MAX_CHUNK_ID) {
		return NULL;
	}

	/* First check we're tall enough (ie enough topLevel) */

	x = chunkId >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (x) {
		x >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}


	if (requiredTallness > fStruct->topLevel) {
		/* Not tall enough,gotta make the tree taller */
		for (i = fStruct->topLevel; i < requiredTallness; i++) {

			tn = yaffs_GetTnode(dev);

			if (tn) {
				tn->internal[0] = fStruct->top;
				fStruct->top = tn;
			} else {
				T(YAFFS_TRACE_ERROR,
				  (TSTR("yaffs: no more tnodes" TENDSTR)));
			}
		}

		fStruct->topLevel = requiredTallness;
	}

	/* Traverse down to level 0, adding anything we need */

	l = fStruct->topLevel;
	tn = fStruct->top;

	if(l > 0) {
		while (l > 0 && tn) {
			x = (chunkId >>
			     ( YAFFS_TNODES_LEVEL0_BITS +
			      (l - 1) * YAFFS_TNODES_INTERNAL_BITS)) &
			    YAFFS_TNODES_INTERNAL_MASK;


			if((l>1) && !tn->internal[x]){
				/* Add missing non-level-zero tnode */
				tn->internal[x] = yaffs_GetTnode(dev);

			} else if(l == 1) {
				/* Looking from level 1 at level 0 */
			 	if (passedTn) {
					/* If we already have one, then release it.*/
					if(tn->internal[x])
						yaffs_FreeTnode(dev,tn->internal[x]);
					tn->internal[x] = passedTn;

				} else if(!tn->internal[x]) {
					/* Don't have one, none passed in */
					tn->internal[x] = yaffs_GetTnode(dev);
				}
			}

			tn = tn->internal[x];
			l--;
		}
	} else {
		/* top is level 0 */
		if(passedTn) {
			memcpy(tn,passedTn,(dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8);
			yaffs_FreeTnode(dev,passedTn);
		}
	}

	return tn;
}

static int yaffs_FindChunkInGroup(yaffs_Device * dev, int theChunk,
				  yaffs_ExtendedTags * tags, int objectId,
				  int chunkInInode)
{
	int j;

	for (j = 0; theChunk && j < dev->chunkGroupSize; j++) {
		if (yaffs_CheckChunkBit
		    (dev, theChunk / dev->nChunksPerBlock,
		     theChunk % dev->nChunksPerBlock)) {
			yaffs_ReadChunkWithTagsFromNAND(dev, theChunk, NULL,
							tags);
			if (yaffs_TagsMatch(tags, objectId, chunkInInode)) {
				/* found it; */
				return theChunk;

			}
		}
		theChunk++;
	}
	return -1;
}


/* DeleteWorker scans backwards through the tnode tree and deletes all the
 * chunks and tnodes in the file
 * Returns 1 if the tree was deleted.
 * Returns 0 if it stopped early due to hitting the limit and the delete is incomplete.
 */
#if 0
static int yaffs_DeleteWorker(yaffs_Object * in, yaffs_Tnode * tn, __u32 level,
			      int chunkOffset, int *limit)
{
	int i;
	int chunkInInode;
	int theChunk;
	yaffs_ExtendedTags tags;
	int foundChunk;
	yaffs_Device *dev = in->myDev;

	int allDone = 1;

	if (tn) {
		if (level > 0) {

			for (i = YAFFS_NTNODES_INTERNAL - 1; allDone && i >= 0;
			     i--) {
				if (tn->internal[i]) {
					if (limit && (*limit) < 0) {
						allDone = 0;
					} else {
						allDone =
						    yaffs_DeleteWorker(in,
								       tn->
								       internal
								       [i],
								       level -
								       1,
								       (chunkOffset
									<<
									YAFFS_TNODES_INTERNAL_BITS)
								       + i,
								       limit);
					}
					if (allDone) {
						yaffs_FreeTnode(dev,
								tn->
								internal[i]);
						tn->internal[i] = NULL;
					}
				}

			}
			return (allDone) ? 1 : 0;
		} else if (level == 0) {
			int hitLimit = 0;

			for (i = YAFFS_NTNODES_LEVEL0 - 1; i >= 0 && !hitLimit;
			     i--) {
			        theChunk = yaffs_GetChunkGroupBase(dev,tn,i);
				if (theChunk) {

					chunkInInode =
					    (chunkOffset <<
					     YAFFS_TNODES_LEVEL0_BITS) + i;

					foundChunk =
					    yaffs_FindChunkInGroup(dev,
								   theChunk,
								   &tags,
								   in->objectId,
								   chunkInInode);

					if (foundChunk > 0) {
						yaffs_DeleteChunk(dev,
								  foundChunk, 1,
								  __LINE__);
						in->nDataChunks--;
						if (limit) {
							*limit = *limit - 1;
							if (*limit <= 0) {
								hitLimit = 1;
							}
						}

					}

					yaffs_PutLevel0Tnode(dev,tn,i,0);
				}

			}
			return (i < 0) ? 1 : 0;

		}

	}

	return 1;

}
#endif
static void yaffs_SoftDeleteChunk(yaffs_Device * dev, int chunk)
{

	yaffs_BlockInfo *theBlock;

	T(YAFFS_TRACE_DELETION, (TSTR("soft delete chunk %d" TENDSTR), chunk));

	theBlock = yaffs_GetBlockInfo(dev, chunk / dev->nChunksPerBlock);
	if (theBlock) {
		theBlock->softDeletions++;
		dev->nFreeChunks++;
	}
}

/* SoftDeleteWorker scans backwards through the tnode tree and soft deletes all the chunks in the file.
 * All soft deleting does is increment the block's softdelete count and pulls the chunk out
 * of the tnode.
 * Thus, essentially this is the same as DeleteWorker except that the chunks are soft deleted.
 */

static int yaffs_SoftDeleteWorker(yaffs_Object * in, yaffs_Tnode * tn,
				  __u32 level, int chunkOffset)
{
	int i;
	int theChunk;
	int allDone = 1;
	yaffs_Device *dev = in->myDev;

	if (tn) {
		if (level > 0) {

			for (i = YAFFS_NTNODES_INTERNAL - 1; allDone && i >= 0;
			     i--) {
				if (tn->internal[i]) {
					allDone =
					    yaffs_SoftDeleteWorker(in,
								   tn->
								   internal[i],
								   level - 1,
								   (chunkOffset
								    <<
								    YAFFS_TNODES_INTERNAL_BITS)
								   + i);
					if (allDone) {
						yaffs_FreeTnode(dev,
								tn->
								internal[i]);
						tn->internal[i] = NULL;
					} else {
						/* Hoosterman... how could this happen? */
					}
				}
			}
			return (allDone) ? 1 : 0;
		} else if (level == 0) {

			for (i = YAFFS_NTNODES_LEVEL0 - 1; i >= 0; i--) {
				theChunk = yaffs_GetChunkGroupBase(dev,tn,i);
				if (theChunk) {
					/* Note this does not find the real chunk, only the chunk group.
					 * We make an assumption that a chunk group is not larger than
					 * a block.
					 */
					yaffs_SoftDeleteChunk(dev, theChunk);
					yaffs_PutLevel0Tnode(dev,tn,i,0);
				}

			}
			return 1;

		}

	}

	return 1;

}

static void yaffs_SoftDeleteFile(yaffs_Object * obj)
{
	if (obj->deleted &&
	    obj->variantType == YAFFS_OBJECT_TYPE_FILE && !obj->softDeleted) {
		if (obj->nDataChunks <= 0) {
			/* Empty file with no duplicate object headers, just delete it immediately */
			yaffs_FreeTnode(obj->myDev,
					obj->variant.fileVariant.top);
			obj->variant.fileVariant.top = NULL;
			T(YAFFS_TRACE_TRACING,
			  (TSTR("yaffs: Deleting empty file %d" TENDSTR),
			   obj->objectId));
			yaffs_DoGenericObjectDeletion(obj);
		} else {
			yaffs_SoftDeleteWorker(obj,
					       obj->variant.fileVariant.top,
					       obj->variant.fileVariant.
					       topLevel, 0);
			obj->softDeleted = 1;
		}
	}
}

/* Pruning removes any part of the file structure tree that is beyond the
 * bounds of the file (ie that does not point to chunks).
 *
 * A file should only get pruned when its size is reduced.
 *
 * Before pruning, the chunks must be pulled from the tree and the
 * level 0 tnode entries must be zeroed out.
 * Could also use this for file deletion, but that's probably better handled
 * by a special case.
 */

static yaffs_Tnode *yaffs_PruneWorker(yaffs_Device * dev, yaffs_Tnode * tn,
				      __u32 level, int del0)
{
	int i;
	int hasData;

	if (tn) {
		hasData = 0;

		for (i = 0; i < YAFFS_NTNODES_INTERNAL; i++) {
			if (tn->internal[i] && level > 0) {
				tn->internal[i] =
				    yaffs_PruneWorker(dev, tn->internal[i],
						      level - 1,
						      (i == 0) ? del0 : 1);
			}

			if (tn->internal[i]) {
				hasData++;
			}
		}

		if (hasData == 0 && del0) {
			/* Free and return NULL */

			yaffs_FreeTnode(dev, tn);
			tn = NULL;
		}

	}

	return tn;

}

static int yaffs_PruneFileStructure(yaffs_Device * dev,
				    yaffs_FileStructure * fStruct)
{
	int i;
	int hasData;
	int done = 0;
	yaffs_Tnode *tn;

	if (fStruct->topLevel > 0) {
		fStruct->top =
		    yaffs_PruneWorker(dev, fStruct->top, fStruct->topLevel, 0);

		/* Now we have a tree with all the non-zero branches NULL but the height
		 * is the same as it was.
		 * Let's see if we can trim internal tnodes to shorten the tree.
		 * We can do this if only the 0th element in the tnode is in use
		 * (ie all the non-zero are NULL)
		 */

		while (fStruct->topLevel && !done) {
			tn = fStruct->top;

			hasData = 0;
			for (i = 1; i < YAFFS_NTNODES_INTERNAL; i++) {
				if (tn->internal[i]) {
					hasData++;
				}
			}

			if (!hasData) {
				fStruct->top = tn->internal[0];
				fStruct->topLevel--;
				yaffs_FreeTnode(dev, tn);
			} else {
				done = 1;
			}
		}
	}

	return YAFFS_OK;
}

/*-------------------- End of File Structure functions.-------------------*/

/* yaffs_CreateFreeObjects creates a bunch more objects and
 * adds them to the object free list.
 */
static int yaffs_CreateFreeObjects(yaffs_Device * dev, int nObjects)
{
	int i;
	yaffs_Object *newObjects;
	yaffs_ObjectList *list;

	if (nObjects < 1)
		return YAFFS_OK;

	/* make these things */
	newObjects = YMALLOC(nObjects * sizeof(yaffs_Object));
	list = YMALLOC(sizeof(yaffs_ObjectList));

	if (!newObjects || !list) {
		if(newObjects)
			YFREE(newObjects);
		if(list)
			YFREE(list);
		T(YAFFS_TRACE_ALLOCATE,
		  (TSTR("yaffs: Could not allocate more objects" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Hook them into the free list */
	for (i = 0; i < nObjects - 1; i++) {
		newObjects[i].siblings.next =
		    (struct list_head *)(&newObjects[i + 1]);
	}

	newObjects[nObjects - 1].siblings.next = (void *)dev->freeObjects;
	dev->freeObjects = newObjects;
	dev->nFreeObjects += nObjects;
	dev->nObjectsCreated += nObjects;

	/* Now add this bunch of Objects to a list for freeing up. */

	list->objects = newObjects;
	list->next = dev->allocatedObjectList;
	dev->allocatedObjectList = list;

	return YAFFS_OK;
}


/* AllocateEmptyObject gets us a clean Object. Tries to make allocate more if we run out */
static yaffs_Object *yaffs_AllocateEmptyObject(yaffs_Device * dev)
{
	yaffs_Object *tn = NULL;

	/* If there are none left make more */
	if (!dev->freeObjects) {
		yaffs_CreateFreeObjects(dev, YAFFS_ALLOCATION_NOBJECTS);
	}

	if (dev->freeObjects) {
		tn = dev->freeObjects;
		dev->freeObjects =
		    (yaffs_Object *) (dev->freeObjects->siblings.next);
		dev->nFreeObjects--;

		/* Now sweeten it up... */

		memset(tn, 0, sizeof(yaffs_Object));
		tn->myDev = dev;
		tn->chunkId = -1;
		tn->variantType = YAFFS_OBJECT_TYPE_UNKNOWN;
		INIT_LIST_HEAD(&(tn->hardLinks));
		INIT_LIST_HEAD(&(tn->hashLink));
		INIT_LIST_HEAD(&tn->siblings);

		/* Add it to the lost and found directory.
		 * NB Can't put root or lostNFound in lostNFound so
		 * check if lostNFound exists first
		 */
		if (dev->lostNFoundDir) {
			yaffs_AddObjectToDirectory(dev->lostNFoundDir, tn);
		}
	}

	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/

	return tn;
}

static yaffs_Object *yaffs_CreateFakeDirectory(yaffs_Device * dev, int number,
					       __u32 mode)
{

	yaffs_Object *obj =
	    yaffs_CreateNewObject(dev, number, YAFFS_OBJECT_TYPE_DIRECTORY);
	if (obj) {
		obj->fake = 1;		/* it is fake so it has no NAND presence... */
		obj->renameAllowed = 0;	/* ... and we're not allowed to rename it... */
		obj->unlinkAllowed = 0;	/* ... or unlink it */
		obj->deleted = 0;
		obj->unlinked = 0;
		obj->yst_mode = mode;
		obj->myDev = dev;
		obj->chunkId = 0;	/* Not a valid chunk. */
	}

	return obj;

}

static void yaffs_UnhashObject(yaffs_Object * tn)
{
	int bucket;
	yaffs_Device *dev = tn->myDev;

	/* If it is still linked into the bucket list, free from the list */
	if (!list_empty(&tn->hashLink)) {
		list_del_init(&tn->hashLink);
		bucket = yaffs_HashFunction(tn->objectId);
		dev->objectBucket[bucket].count--;
	}

}

/*  FreeObject frees up a Object and puts it back on the free list */
static void yaffs_FreeObject(yaffs_Object * tn)
{

	yaffs_Device *dev = tn->myDev;

#ifdef  __KERNEL__
	if (tn->myInode) {
		/* We're still hooked up to a cached inode.
		 * Don't delete now, but mark for later deletion
		 */
		tn->deferedFree = 1;
		return;
	}
#endif

	yaffs_UnhashObject(tn);

	/* Link into the free list. */
	tn->siblings.next = (struct list_head *)(dev->freeObjects);
	dev->freeObjects = tn;
	dev->nFreeObjects++;

	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/

}

#ifdef __KERNEL__

void yaffs_HandleDeferedFree(yaffs_Object * obj)
{
	if (obj->deferedFree) {
		yaffs_FreeObject(obj);
	}
}

#endif

static void yaffs_DeinitialiseObjects(yaffs_Device * dev)
{
	/* Free the list of allocated Objects */

	yaffs_ObjectList *tmp;

	while (dev->allocatedObjectList) {
		tmp = dev->allocatedObjectList->next;
		YFREE(dev->allocatedObjectList->objects);
		YFREE(dev->allocatedObjectList);

		dev->allocatedObjectList = tmp;
	}

	dev->freeObjects = NULL;
	dev->nFreeObjects = 0;
}

static void yaffs_InitialiseObjects(yaffs_Device * dev)
{
	int i;

	dev->allocatedObjectList = NULL;
	dev->freeObjects = NULL;
	dev->nFreeObjects = 0;

	for (i = 0; i < YAFFS_NOBJECT_BUCKETS; i++) {
		INIT_LIST_HEAD(&dev->objectBucket[i].list);
		dev->objectBucket[i].count = 0;
	}

}

static int yaffs_FindNiceObjectBucket(yaffs_Device * dev)
{
	static int x = 0;
	int i;
	int l = 999;
	int lowest = 999999;

	/* First let's see if we can find one that's empty. */

	for (i = 0; i < 10 && lowest > 0; i++) {
		x++;
		x %= YAFFS_NOBJECT_BUCKETS;
		if (dev->objectBucket[x].count < lowest) {
			lowest = dev->objectBucket[x].count;
			l = x;
		}

	}

	/* If we didn't find an empty list, then try
	 * looking a bit further for a short one
	 */

	for (i = 0; i < 10 && lowest > 3; i++) {
		x++;
		x %= YAFFS_NOBJECT_BUCKETS;
		if (dev->objectBucket[x].count < lowest) {
			lowest = dev->objectBucket[x].count;
			l = x;
		}

	}

	return l;
}

static int yaffs_CreateNewObjectNumber(yaffs_Device * dev)
{
	int bucket = yaffs_FindNiceObjectBucket(dev);

	/* Now find an object value that has not already been taken
	 * by scanning the list.
	 */

	int found = 0;
	struct list_head *i;

	__u32 n = (__u32) bucket;

	/* yaffs_CheckObjectHashSanity();  */

	while (!found) {
		found = 1;
		n += YAFFS_NOBJECT_BUCKETS;
		if (1 || dev->objectBucket[bucket].count > 0) {
			list_for_each(i, &dev->objectBucket[bucket].list) {
				/* If there is already one in the list */
				if (i
				    && list_entry(i, yaffs_Object,
						  hashLink)->objectId == n) {
					found = 0;
				}
			}
		}
	}


	return n;
}

static void yaffs_HashObject(yaffs_Object * in)
{
	int bucket = yaffs_HashFunction(in->objectId);
	yaffs_Device *dev = in->myDev;

	list_add(&in->hashLink, &dev->objectBucket[bucket].list);
	dev->objectBucket[bucket].count++;

}

yaffs_Object *yaffs_FindObjectByNumber(yaffs_Device * dev, __u32 number)
{
	int bucket = yaffs_HashFunction(number);
	struct list_head *i;
	yaffs_Object *in;

	list_for_each(i, &dev->objectBucket[bucket].list) {
		/* Look if it is in the list */
		if (i) {
			in = list_entry(i, yaffs_Object, hashLink);
			if (in->objectId == number) {
#ifdef __KERNEL__
				/* Don't tell the VFS about this one if it is defered free */
				if (in->deferedFree)
					return NULL;
#endif

				return in;
			}
		}
	}

	return NULL;
}

yaffs_Object *yaffs_CreateNewObject(yaffs_Device * dev, int number,
				    yaffs_ObjectType type)
{

	yaffs_Object *theObject;
	yaffs_Tnode *tn;

	if (number < 0) {
		number = yaffs_CreateNewObjectNumber(dev);
	}

	theObject = yaffs_AllocateEmptyObject(dev);
	if(!theObject)
		return NULL;

	if(type == YAFFS_OBJECT_TYPE_FILE){
		tn = yaffs_GetTnode(dev);
		if(!tn){
			yaffs_FreeObject(theObject);
			return NULL;
		}
	}



	if (theObject) {
		theObject->fake = 0;
		theObject->renameAllowed = 1;
		theObject->unlinkAllowed = 1;
		theObject->objectId = number;
		yaffs_HashObject(theObject);
		theObject->variantType = type;
#ifdef CONFIG_YAFFS_WINCE
		yfsd_WinFileTimeNow(theObject->win_atime);
		theObject->win_ctime[0] = theObject->win_mtime[0] =
		    theObject->win_atime[0];
		theObject->win_ctime[1] = theObject->win_mtime[1] =
		    theObject->win_atime[1];

#else

		theObject->yst_atime = theObject->yst_mtime =
		    theObject->yst_ctime = Y_CURRENT_TIME;
#endif
		switch (type) {
		case YAFFS_OBJECT_TYPE_FILE:
			theObject->variant.fileVariant.fileSize = 0;
			theObject->variant.fileVariant.scannedFileSize = 0;
			theObject->variant.fileVariant.shrinkSize = 0xFFFFFFFF;	/* max __u32 */
			theObject->variant.fileVariant.topLevel = 0;
			theObject->variant.fileVariant.top = tn;
			break;
		case YAFFS_OBJECT_TYPE_DIRECTORY:
			INIT_LIST_HEAD(&theObject->variant.directoryVariant.
				       children);
			break;
		case YAFFS_OBJECT_TYPE_SYMLINK:
		case YAFFS_OBJECT_TYPE_HARDLINK:
		case YAFFS_OBJECT_TYPE_SPECIAL:
			/* No action required */
			break;
		case YAFFS_OBJECT_TYPE_UNKNOWN:
			/* todo this should not happen */
			break;
		}
	}

	return theObject;
}

static yaffs_Object *yaffs_FindOrCreateObjectByNumber(yaffs_Device * dev,
						      int number,
						      yaffs_ObjectType type)
{
	yaffs_Object *theObject = NULL;

	if (number > 0) {
		theObject = yaffs_FindObjectByNumber(dev, number);
	}

	if (!theObject) {
		theObject = yaffs_CreateNewObject(dev, number, type);
	}

	return theObject;

}


static YCHAR *yaffs_CloneString(const YCHAR * str)
{
	YCHAR *newStr = NULL;

	if (str && *str) {
		newStr = YMALLOC((yaffs_strlen(str) + 1) * sizeof(YCHAR));
		if(newStr)
			yaffs_strcpy(newStr, str);
	}

	return newStr;

}

/*
 * Mknod (create) a new object.
 * equivalentObject only has meaning for a hard link;
 * aliasString only has meaning for a sumlink.
 * rdev only has meaning for devices (a subset of special objects)
 */

static yaffs_Object *yaffs_MknodObject(yaffs_ObjectType type,
				       yaffs_Object * parent,
				       const YCHAR * name,
				       __u32 mode,
				       __u32 uid,
				       __u32 gid,
				       yaffs_Object * equivalentObject,
				       const YCHAR * aliasString, __u32 rdev)
{
	yaffs_Object *in;
	YCHAR *str;

        // added by Super to remove crash due to null pointer derefercne.
        if( !parent ) return 0;
        // end fix

	yaffs_Device *dev = parent->myDev;

	/* Check if the entry exists. If it does then fail the call since we don't want a dup.*/
	if (yaffs_FindObjectByName(parent, name)) {
		return NULL;
	}

	in = yaffs_CreateNewObject(dev, -1, type);

	if(type == YAFFS_OBJECT_TYPE_SYMLINK){
		str = yaffs_CloneString(aliasString);
		if(!str){
			yaffs_FreeObject(in);
			return NULL;
		}
	}



	if (in) {
		in->chunkId = -1;
		in->valid = 1;
		in->variantType = type;

		in->yst_mode = mode;

#ifdef CONFIG_YAFFS_WINCE
		yfsd_WinFileTimeNow(in->win_atime);
		in->win_ctime[0] = in->win_mtime[0] = in->win_atime[0];
		in->win_ctime[1] = in->win_mtime[1] = in->win_atime[1];

#else
		in->yst_atime = in->yst_mtime = in->yst_ctime = Y_CURRENT_TIME;

		in->yst_rdev = rdev;
		in->yst_uid = uid;
		in->yst_gid = gid;
#endif
		in->nDataChunks = 0;

		yaffs_SetObjectName(in, name);
		in->dirty = 1;

		yaffs_AddObjectToDirectory(parent, in);

		in->myDev = parent->myDev;

		switch (type) {
		case YAFFS_OBJECT_TYPE_SYMLINK:
			in->variant.symLinkVariant.alias = str;
			break;
		case YAFFS_OBJECT_TYPE_HARDLINK:
			in->variant.hardLinkVariant.equivalentObject =
			    equivalentObject;
			in->variant.hardLinkVariant.equivalentObjectId =
			    equivalentObject->objectId;
			list_add(&in->hardLinks, &equivalentObject->hardLinks);
			break;
		case YAFFS_OBJECT_TYPE_FILE:
		case YAFFS_OBJECT_TYPE_DIRECTORY:
		case YAFFS_OBJECT_TYPE_SPECIAL:
		case YAFFS_OBJECT_TYPE_UNKNOWN:
			/* do nothing */
			break;
		}

		if (yaffs_UpdateObjectHeader(in, name, 0, 0, 0) < 0) {
			/* Could not create the object header, fail the creation */
			yaffs_DestroyObject(in);
			in = NULL;
		}

	}

	return in;
}

yaffs_Object *yaffs_MknodFile(yaffs_Object * parent, const YCHAR * name,
			      __u32 mode, __u32 uid, __u32 gid)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_FILE, parent, name, mode,
				 uid, gid, NULL, NULL, 0);
}

yaffs_Object *yaffs_MknodDirectory(yaffs_Object * parent, const YCHAR * name,
				   __u32 mode, __u32 uid, __u32 gid)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_DIRECTORY, parent, name,
				 mode, uid, gid, NULL, NULL, 0);
}

yaffs_Object *yaffs_MknodSpecial(yaffs_Object * parent, const YCHAR * name,
				 __u32 mode, __u32 uid, __u32 gid, __u32 rdev)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_SPECIAL, parent, name, mode,
				 uid, gid, NULL, NULL, rdev);
}

yaffs_Object *yaffs_MknodSymLink(yaffs_Object * parent, const YCHAR * name,
				 __u32 mode, __u32 uid, __u32 gid,
				 const YCHAR * alias)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_SYMLINK, parent, name, mode,
				 uid, gid, NULL, alias, 0);
}

/* yaffs_Link returns the object id of the equivalent object.*/
yaffs_Object *yaffs_Link(yaffs_Object * parent, const YCHAR * name,
			 yaffs_Object * equivalentObject)
{
	/* Get the real object in case we were fed a hard link as an equivalent object */
	equivalentObject = yaffs_GetEquivalentObject(equivalentObject);

	if (yaffs_MknodObject
	    (YAFFS_OBJECT_TYPE_HARDLINK, parent, name, 0, 0, 0,
	     equivalentObject, NULL, 0)) {
		return equivalentObject;
	} else {
		return NULL;
	}

}

static int yaffs_ChangeObjectName(yaffs_Object * obj, yaffs_Object * newDir,
				  const YCHAR * newName, int force, int shadows)
{
	int unlinkOp;
	int deleteOp;

	yaffs_Object *existingTarget;

	if (newDir == NULL) {
		newDir = obj->parent;	/* use the old directory */
	}

	if (newDir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragendy: yaffs_ChangeObjectName: newDir is not a directory"
		    TENDSTR)));
		YBUG();
	}

	/* TODO: Do we need this different handling for YAFFS2 and YAFFS1?? */
	if (obj->myDev->isYaffs2) {
		unlinkOp = (newDir == obj->myDev->unlinkedDir);
	} else {
		unlinkOp = (newDir == obj->myDev->unlinkedDir
			    && obj->variantType == YAFFS_OBJECT_TYPE_FILE);
	}

	deleteOp = (newDir == obj->myDev->deletedDir);

	existingTarget = yaffs_FindObjectByName(newDir, newName);

	/* If the object is a file going into the unlinked directory,
	 *   then it is OK to just stuff it in since duplicate names are allowed.
	 *   else only proceed if the new name does not exist and if we're putting
	 *   it into a directory.
	 */
	if ((unlinkOp ||
	     deleteOp ||
	     force ||
	     (shadows > 0) ||
	     !existingTarget) &&
	    newDir->variantType == YAFFS_OBJECT_TYPE_DIRECTORY) {
		yaffs_SetObjectName(obj, newName);
		obj->dirty = 1;

		yaffs_AddObjectToDirectory(newDir, obj);

		if (unlinkOp)
			obj->unlinked = 1;

		/* If it is a deletion then we mark it as a shrink for gc purposes. */
		if (yaffs_UpdateObjectHeader(obj, newName, 0, deleteOp, shadows)>= 0)
			return YAFFS_OK;
	}

	return YAFFS_FAIL;
}

int yaffs_RenameObject(yaffs_Object * oldDir, const YCHAR * oldName,
		       yaffs_Object * newDir, const YCHAR * newName)
{
	yaffs_Object *obj;
	yaffs_Object *existingTarget;
	int force = 0;

#ifdef CONFIG_YAFFS_CASE_INSENSITIVE
	/* Special case for case insemsitive systems (eg. WinCE).
	 * While look-up is case insensitive, the name isn't.
	 * Therefore we might want to change x.txt to X.txt
	*/
	if (oldDir == newDir && yaffs_strcmp(oldName, newName) == 0) {
		force = 1;
	}
#endif

	obj = yaffs_FindObjectByName(oldDir, oldName);
	/* Check new name to long. */
        // added by super to fix a null pointer derefernce problem
        if ( !obj ) return YAFFS_FAIL;	
        if (obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK &&
	    yaffs_strlen(newName) > YAFFS_MAX_ALIAS_LENGTH)
	  /* ENAMETOOLONG */
	  return YAFFS_FAIL;
	else if (obj->variantType != YAFFS_OBJECT_TYPE_SYMLINK &&
		 yaffs_strlen(newName) > YAFFS_MAX_NAME_LENGTH)
	  /* ENAMETOOLONG */
	  return YAFFS_FAIL;

	if (obj && obj->renameAllowed) {

		/* Now do the handling for an existing target, if there is one */

		existingTarget = yaffs_FindObjectByName(newDir, newName);
		if (existingTarget &&
		    existingTarget->variantType == YAFFS_OBJECT_TYPE_DIRECTORY &&
		    !list_empty(&existingTarget->variant.directoryVariant.children)) {
			/* There is a target that is a non-empty directory, so we fail */
			return YAFFS_FAIL;	/* EEXIST or ENOTEMPTY */
		} else if (existingTarget && existingTarget != obj) {
			/* Nuke the target first, using shadowing,
			 * but only if it isn't the same object
			 */
			yaffs_ChangeObjectName(obj, newDir, newName, force,
					       existingTarget->objectId);
			yaffs_UnlinkObject(existingTarget);
		}

		return yaffs_ChangeObjectName(obj, newDir, newName, 1, 0);
	}
	return YAFFS_FAIL;
}

/*------------------------- Block Management and Page Allocation ----------------*/

static int yaffs_InitialiseBlocks(yaffs_Device * dev)
{
	int nBlocks = dev->internalEndBlock - dev->internalStartBlock + 1;

	dev->blockInfo = NULL;
	dev->chunkBits = NULL;

	dev->allocationBlock = -1;	/* force it to get a new one */

	/* If the first allocation strategy fails, thry the alternate one */
	dev->blockInfo = YMALLOC(nBlocks * sizeof(yaffs_BlockInfo));
	if(!dev->blockInfo){
		dev->blockInfo = YMALLOC_ALT(nBlocks * sizeof(yaffs_BlockInfo));
		dev->blockInfoAlt = 1;
	}
	else
		dev->blockInfoAlt = 0;

	if(dev->blockInfo){

		/* Set up dynamic blockinfo stuff. */
		dev->chunkBitmapStride = (dev->nChunksPerBlock + 7) / 8; /* round up bytes */
		dev->chunkBits = YMALLOC(dev->chunkBitmapStride * nBlocks);
		if(!dev->chunkBits){
			dev->chunkBits = YMALLOC_ALT(dev->chunkBitmapStride * nBlocks);
			dev->chunkBitsAlt = 1;
		}
		else
			dev->chunkBitsAlt = 0;
	}

	if (dev->blockInfo && dev->chunkBits) {
		memset(dev->blockInfo, 0, nBlocks * sizeof(yaffs_BlockInfo));
		memset(dev->chunkBits, 0, dev->chunkBitmapStride * nBlocks);
		return YAFFS_OK;
	}

	return YAFFS_FAIL;

}

static void yaffs_DeinitialiseBlocks(yaffs_Device * dev)
{
	if(dev->blockInfoAlt && dev->blockInfo)
		YFREE_ALT(dev->blockInfo);
	else if(dev->blockInfo)
		YFREE(dev->blockInfo);

	dev->blockInfoAlt = 0;

	dev->blockInfo = NULL;

	if(dev->chunkBitsAlt && dev->chunkBits)
		YFREE_ALT(dev->chunkBits);
	else if(dev->chunkBits)
		YFREE(dev->chunkBits);
	dev->chunkBitsAlt = 0;
	dev->chunkBits = NULL;
}

static int yaffs_BlockNotDisqualifiedFromGC(yaffs_Device * dev,
					    yaffs_BlockInfo * bi)
{
	int i;
	__u32 seq;
	yaffs_BlockInfo *b;

	if (!dev->isYaffs2)
		return 1;	/* disqualification only applies to yaffs2. */

	if (!bi->hasShrinkHeader)
		return 1;	/* can gc */

	/* Find the oldest dirty sequence number if we don't know it and save it
	 * so we don't have to keep recomputing it.
	 */
	if (!dev->oldestDirtySequence) {
		seq = dev->sequenceNumber;

		for (i = dev->internalStartBlock; i <= dev->internalEndBlock;
		     i++) {
			b = yaffs_GetBlockInfo(dev, i);
			if (b->blockState == YAFFS_BLOCK_STATE_FULL &&
			    (b->pagesInUse - b->softDeletions) <
			    dev->nChunksPerBlock && b->sequenceNumber < seq) {
				seq = b->sequenceNumber;
			}
		}
		dev->oldestDirtySequence = seq;
	}

	/* Can't do gc of this block if there are any blocks older than this one that have
	 * discarded pages.
	 */
	return (bi->sequenceNumber <= dev->oldestDirtySequence);

}

/* FindDiretiestBlock is used to select the dirtiest block (or close enough)
 * for garbage collection.
 */

static int yaffs_FindBlockForGarbageCollection(yaffs_Device * dev,
					       int aggressive)
{

	int b = dev->currentDirtyChecker;

	int i;
	int iterations;
	int dirtiest = -1;
	int pagesInUse = 0;
	int prioritised=0;
	yaffs_BlockInfo *bi;
	int pendingPrioritisedExist = 0;

	/* First let's see if we need to grab a prioritised block */
	if(dev->hasPendingPrioritisedGCs){
		for(i = dev->internalStartBlock; i < dev->internalEndBlock && !prioritised; i++){

			bi = yaffs_GetBlockInfo(dev, i);
			//yaffs_VerifyBlock(dev,bi,i);

			if(bi->gcPrioritise) {
				pendingPrioritisedExist = 1;
				if(bi->blockState == YAFFS_BLOCK_STATE_FULL &&
				   yaffs_BlockNotDisqualifiedFromGC(dev, bi)){
					pagesInUse = (bi->pagesInUse - bi->softDeletions);
					dirtiest = i;
					prioritised = 1;
					aggressive = 1; /* Fool the non-aggressive skip logiv below */
				}
			}
		}

		if(!pendingPrioritisedExist) /* None found, so we can clear this */
			dev->hasPendingPrioritisedGCs = 0;
	}

	/* If we're doing aggressive GC then we are happy to take a less-dirty block, and
	 * search harder.
	 * else (we're doing a leasurely gc), then we only bother to do this if the
	 * block has only a few pages in use.
	 */

	dev->nonAggressiveSkip--;

	if (!aggressive && (dev->nonAggressiveSkip > 0)) {
		return -1;
	}

	if(!prioritised)
		pagesInUse =
	    		(aggressive) ? dev->nChunksPerBlock : YAFFS_PASSIVE_GC_CHUNKS + 1;

	if (aggressive) {
		iterations =
		    dev->internalEndBlock - dev->internalStartBlock + 1;
	} else {
		iterations =
		    dev->internalEndBlock - dev->internalStartBlock + 1;
		iterations = iterations / 16;
		if (iterations > 200) {
			iterations = 200;
		}
	}

	for (i = 0; i <= iterations && pagesInUse > 0 && !prioritised; i++) {
		b++;
		if (b < dev->internalStartBlock || b > dev->internalEndBlock) {
			b = dev->internalStartBlock;
		}

		if (b < dev->internalStartBlock || b > dev->internalEndBlock) {
			T(YAFFS_TRACE_ERROR,
			  (TSTR("**>> Block %d is not valid" TENDSTR), b));
			YBUG();
		}

		bi = yaffs_GetBlockInfo(dev, b);

#if 0
		if (bi->blockState == YAFFS_BLOCK_STATE_CHECKPOINT) {
			dirtiest = b;
			pagesInUse = 0;
		}
		else
#endif

		if (bi->blockState == YAFFS_BLOCK_STATE_FULL &&
		       (bi->pagesInUse - bi->softDeletions) < pagesInUse &&
		        yaffs_BlockNotDisqualifiedFromGC(dev, bi)) {
			dirtiest = b;
			pagesInUse = (bi->pagesInUse - bi->softDeletions);
		}
	}

	dev->currentDirtyChecker = b;

	if (dirtiest > 0) {
		T(YAFFS_TRACE_GC,
		  (TSTR("GC Selected block %d with %d free, prioritised:%d" TENDSTR), dirtiest,
		   dev->nChunksPerBlock - pagesInUse,prioritised));
	}

	dev->oldestDirtySequence = 0;

	if (dirtiest > 0) {
		dev->nonAggressiveSkip = 4;
	}

	return dirtiest;
}

static void yaffs_BlockBecameDirty(yaffs_Device * dev, int blockNo)
{
	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, blockNo);

	int erasedOk = 0;

	/* If the block is still healthy erase it and mark as clean.
	 * If the block has had a data failure, then retire it.
	 */

	T(YAFFS_TRACE_GC | YAFFS_TRACE_ERASE,
		(TSTR("yaffs_BlockBecameDirty block %d state %d %s"TENDSTR),
		blockNo, bi->blockState, (bi->needsRetiring) ? "needs retiring" : ""));

	bi->blockState = YAFFS_BLOCK_STATE_DIRTY;

	if (!bi->needsRetiring) {
		yaffs_InvalidateCheckpoint(dev);
		erasedOk = yaffs_EraseBlockInNAND(dev, blockNo);
		if (!erasedOk) {
			dev->nErasureFailures++;
			T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
			  (TSTR("**>> Erasure failed %d" TENDSTR), blockNo));
		}
	}

	if (erasedOk &&
	    ((yaffs_traceMask & YAFFS_TRACE_ERASE) || !yaffs_SkipVerification(dev))) {
		int i;
		for (i = 0; i < dev->nChunksPerBlock; i++) {
			if (!yaffs_CheckChunkErased
			    (dev, blockNo * dev->nChunksPerBlock + i)) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   (">>Block %d erasure supposedly OK, but chunk %d not erased"
				    TENDSTR), blockNo, i));
			}
		}
	}

	if (erasedOk) {
		/* Clean it up... */
		bi->blockState = YAFFS_BLOCK_STATE_EMPTY;
		dev->nErasedBlocks++;
		bi->pagesInUse = 0;
		bi->softDeletions = 0;
		bi->hasShrinkHeader = 0;
		bi->skipErasedCheck = 1;  /* This is clean, so no need to check */
		bi->gcPrioritise = 0;
		yaffs_ClearChunkBits(dev, blockNo);

		T(YAFFS_TRACE_ERASE,
		  (TSTR("Erased block %d" TENDSTR), blockNo));
	} else {
		dev->nFreeChunks -= dev->nChunksPerBlock;	/* We lost a block of free space */

		yaffs_RetireBlock(dev, blockNo);
		T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
		  (TSTR("**>> Block %d retired" TENDSTR), blockNo));
	}
}

static int yaffs_FindBlockForAllocation(yaffs_Device * dev)
{
	int i;

	yaffs_BlockInfo *bi;

	if (dev->nErasedBlocks < 1) {
		/* Hoosterman we've got a problem.
		 * Can't get space to gc
		 */
		T(YAFFS_TRACE_ERROR,
		  (TSTR("yaffs tragedy: no more eraased blocks" TENDSTR)));

		return -1;
	}

	/* Find an empty block. */

	for (i = dev->internalStartBlock; i <= dev->internalEndBlock; i++) {
		dev->allocationBlockFinder++;
		if (dev->allocationBlockFinder < dev->internalStartBlock
		    || dev->allocationBlockFinder > dev->internalEndBlock) {
			dev->allocationBlockFinder = dev->internalStartBlock;
		}

		bi = yaffs_GetBlockInfo(dev, dev->allocationBlockFinder);

		if (bi->blockState == YAFFS_BLOCK_STATE_EMPTY) {
			bi->blockState = YAFFS_BLOCK_STATE_ALLOCATING;
			dev->sequenceNumber++;
			bi->sequenceNumber = dev->sequenceNumber;
			dev->nErasedBlocks--;
			T(YAFFS_TRACE_ALLOCATE,
			  (TSTR("Allocated block %d, seq  %d, %d left" TENDSTR),
			   dev->allocationBlockFinder, dev->sequenceNumber,
			   dev->nErasedBlocks));
			return dev->allocationBlockFinder;
		}
	}

	T(YAFFS_TRACE_ALWAYS,
	  (TSTR
	   ("yaffs tragedy: no more eraased blocks, but there should have been %d"
	    TENDSTR), dev->nErasedBlocks));

	return -1;
}



static int yaffs_CalcCheckpointBlocksRequired(yaffs_Device *dev)
{
	if(!dev->nCheckpointBlocksRequired){
		/* Not a valid value so recalculate */
		int nBytes = 0;
		int nBlocks;
		int devBlocks = (dev->endBlock - dev->startBlock + 1);
		int tnodeSize;

		tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

		if(tnodeSize < sizeof(yaffs_Tnode))
			tnodeSize = sizeof(yaffs_Tnode);

		nBytes += sizeof(yaffs_CheckpointValidity);
		nBytes += sizeof(yaffs_CheckpointDevice);
		nBytes += devBlocks * sizeof(yaffs_BlockInfo);
		nBytes += devBlocks * dev->chunkBitmapStride;
		nBytes += (sizeof(yaffs_CheckpointObject) + sizeof(__u32)) * (dev->nObjectsCreated - dev->nFreeObjects);
		nBytes += (tnodeSize + sizeof(__u32)) * (dev->nTnodesCreated - dev->nFreeTnodes);
		nBytes += sizeof(yaffs_CheckpointValidity);
		nBytes += sizeof(__u32); /* checksum*/

		/* Round up and add 2 blocks to allow for some bad blocks, so add 3 */

		nBlocks = (nBytes/(dev->nDataBytesPerChunk * dev->nChunksPerBlock)) + 3;

		dev->nCheckpointBlocksRequired = nBlocks;
	}

	return dev->nCheckpointBlocksRequired;
}

// Check if there's space to allocate...
// Thinks.... do we need top make this ths same as yaffs_GetFreeChunks()?
static int yaffs_CheckSpaceForAllocation(yaffs_Device * dev)
{
	int reservedChunks;
	int reservedBlocks = dev->nReservedBlocks;
	int checkpointBlocks;

	checkpointBlocks =  yaffs_CalcCheckpointBlocksRequired(dev) - dev->blocksInCheckpoint;
	if(checkpointBlocks < 0)
		checkpointBlocks = 0;

	reservedChunks = ((reservedBlocks + checkpointBlocks) * dev->nChunksPerBlock);

	return (dev->nFreeChunks > reservedChunks);
}

static int yaffs_AllocateChunk(yaffs_Device * dev, int useReserve, yaffs_BlockInfo **blockUsedPtr)
{
	int retVal;
	yaffs_BlockInfo *bi;

	if (dev->allocationBlock < 0) {
		/* Get next block to allocate off */
		dev->allocationBlock = yaffs_FindBlockForAllocation(dev);
		dev->allocationPage = 0;
	}

	if (!useReserve && !yaffs_CheckSpaceForAllocation(dev)) {
		/* Not enough space to allocate unless we're allowed to use the reserve. */
		return -1;
	}

	if (dev->nErasedBlocks < dev->nReservedBlocks
	    && dev->allocationPage == 0) {
		T(YAFFS_TRACE_ALLOCATE, (TSTR("Allocating reserve" TENDSTR)));
	}

	/* Next page please.... */
	if (dev->allocationBlock >= 0) {
		bi = yaffs_GetBlockInfo(dev, dev->allocationBlock);

		retVal = (dev->allocationBlock * dev->nChunksPerBlock) +
		    dev->allocationPage;
		bi->pagesInUse++;
		yaffs_SetChunkBit(dev, dev->allocationBlock,
				  dev->allocationPage);

		dev->allocationPage++;

		dev->nFreeChunks--;

		/* If the block is full set the state to full */
		if (dev->allocationPage >= dev->nChunksPerBlock) {
			bi->blockState = YAFFS_BLOCK_STATE_FULL;
			dev->allocationBlock = -1;
		}

		if(blockUsedPtr)
			*blockUsedPtr = bi;

		return retVal;
	}

	T(YAFFS_TRACE_ERROR,
	  (TSTR("!!!!!!!!! Allocator out !!!!!!!!!!!!!!!!!" TENDSTR)));

	return -1;
}

static int yaffs_GetErasedChunks(yaffs_Device * dev)
{
	int n;

	n = dev->nErasedBlocks * dev->nChunksPerBlock;

	if (dev->allocationBlock > 0) {
		n += (dev->nChunksPerBlock - dev->allocationPage);
	}

	return n;

}

static int yaffs_GarbageCollectBlock(yaffs_Device * dev, int block)
{
	int oldChunk;
	int newChunk;
	int chunkInBlock;
	int markNAND;
	int retVal = YAFFS_OK;
	int cleanups = 0;
	int i;
	int isCheckpointBlock;
	int matchingChunk;

	int chunksBefore = yaffs_GetErasedChunks(dev);
	int chunksAfter;

	yaffs_ExtendedTags tags;

	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, block);

	yaffs_Object *object;

	isCheckpointBlock = (bi->blockState == YAFFS_BLOCK_STATE_CHECKPOINT);

	bi->blockState = YAFFS_BLOCK_STATE_COLLECTING;

	T(YAFFS_TRACE_TRACING,
	  (TSTR("Collecting block %d, in use %d, shrink %d, " TENDSTR), block,
	   bi->pagesInUse, bi->hasShrinkHeader));

	/*yaffs_VerifyFreeChunks(dev); */

	bi->hasShrinkHeader = 0;	/* clear the flag so that the block can erase */

	/* Take off the number of soft deleted entries because
	 * they're going to get really deleted during GC.
	 */
	dev->nFreeChunks -= bi->softDeletions;

	dev->isDoingGC = 1;

	if (isCheckpointBlock ||
	    !yaffs_StillSomeChunkBits(dev, block)) {
		T(YAFFS_TRACE_TRACING,
		  (TSTR
		   ("Collecting block %d that has no chunks in use" TENDSTR),
		   block));
		yaffs_BlockBecameDirty(dev, block);
	} else {

		__u8 *buffer = yaffs_GetTempBuffer(dev, __LINE__);

		yaffs_VerifyBlock(dev,bi,block);

		for (chunkInBlock = 0, oldChunk = block * dev->nChunksPerBlock;
		     chunkInBlock < dev->nChunksPerBlock
		     && yaffs_StillSomeChunkBits(dev, block);
		     chunkInBlock++, oldChunk++) {
			if (yaffs_CheckChunkBit(dev, block, chunkInBlock)) {

				/* This page is in use and might need to be copied off */

				markNAND = 1;

				yaffs_InitialiseTags(&tags);

				yaffs_ReadChunkWithTagsFromNAND(dev, oldChunk,
								buffer, &tags);

				object =
				    yaffs_FindObjectByNumber(dev,
							     tags.objectId);

				T(YAFFS_TRACE_GC_DETAIL,
				  (TSTR
				   ("Collecting page %d, %d %d %d " TENDSTR),
				   chunkInBlock, tags.objectId, tags.chunkId,
				   tags.byteCount));

				if(object && !yaffs_SkipVerification(dev)){
					if(tags.chunkId == 0)
						matchingChunk = object->chunkId;
					else if(object->softDeleted)
						matchingChunk = oldChunk; /* Defeat the test */
					else
						matchingChunk = yaffs_FindChunkInFile(object,tags.chunkId,NULL);

					if(oldChunk != matchingChunk)
						T(YAFFS_TRACE_ERROR,
						  (TSTR("gc: page in gc mismatch: %d %d %d %d"TENDSTR),
						  oldChunk,matchingChunk,tags.objectId, tags.chunkId));

				}

				if (!object) {
					T(YAFFS_TRACE_ERROR,
					  (TSTR
					   ("page %d in gc has no object: %d %d %d "
					    TENDSTR), oldChunk,
					    tags.objectId, tags.chunkId, tags.byteCount));
				}

				if (object && object->deleted
				    && tags.chunkId != 0) {
					/* Data chunk in a deleted file, throw it away
					 * It's a soft deleted data chunk,
					 * No need to copy this, just forget about it and
					 * fix up the object.
					 */

					object->nDataChunks--;

					if (object->nDataChunks <= 0) {
						/* remeber to clean up the object */
						dev->gcCleanupList[cleanups] =
						    tags.objectId;
						cleanups++;
					}
					markNAND = 0;
				} else if (0
					   /* Todo object && object->deleted && object->nDataChunks == 0 */
					   ) {
					/* Deleted object header with no data chunks.
					 * Can be discarded and the file deleted.
					 */
					object->chunkId = 0;
					yaffs_FreeTnode(object->myDev,
							object->variant.
							fileVariant.top);
					object->variant.fileVariant.top = NULL;
					yaffs_DoGenericObjectDeletion(object);

				} else if (object) {
					/* It's either a data chunk in a live file or
					 * an ObjectHeader, so we're interested in it.
					 * NB Need to keep the ObjectHeaders of deleted files
					 * until the whole file has been deleted off
					 */
					tags.serialNumber++;

					dev->nGCCopies++;

					if (tags.chunkId == 0) {
						/* It is an object Id,
						 * We need to nuke the shrinkheader flags first
						 * We no longer want the shrinkHeader flag since its work is done
						 * and if it is left in place it will mess up scanning.
						 * Also, clear out any shadowing stuff
						 */

						yaffs_ObjectHeader *oh;
						oh = (yaffs_ObjectHeader *)buffer;
						oh->isShrink = 0;
						oh->shadowsObject = -1;
						tags.extraShadows = 0;
						tags.extraIsShrinkHeader = 0;

						yaffs_VerifyObjectHeader(object,oh,&tags,1);
					}

					newChunk =
					    yaffs_WriteNewChunkWithTagsToNAND(dev, buffer, &tags, 1);

					if (newChunk < 0) {
						retVal = YAFFS_FAIL;
					} else {

						/* Ok, now fix up the Tnodes etc. */

						if (tags.chunkId == 0) {
							/* It's a header */
							object->chunkId =  newChunk;
							object->serial =   tags.serialNumber;
						} else {
							/* It's a data chunk */
							yaffs_PutChunkIntoFile
							    (object,
							     tags.chunkId,
							     newChunk, 0);
						}
					}
				}

				yaffs_DeleteChunk(dev, oldChunk, markNAND, __LINE__);

			}
		}

		yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);


		/* Do any required cleanups */
		for (i = 0; i < cleanups; i++) {
			/* Time to delete the file too */
			object =
			    yaffs_FindObjectByNumber(dev,
						     dev->gcCleanupList[i]);
			if (object) {
				yaffs_FreeTnode(dev,
						object->variant.fileVariant.
						top);
				object->variant.fileVariant.top = NULL;
				T(YAFFS_TRACE_GC,
				  (TSTR
				   ("yaffs: About to finally delete object %d"
				    TENDSTR), object->objectId));
				yaffs_DoGenericObjectDeletion(object);
				object->myDev->nDeletedFiles--;
			}

		}

	}

	yaffs_VerifyCollectedBlock(dev,bi,block);

	if (chunksBefore >= (chunksAfter = yaffs_GetErasedChunks(dev))) {
		T(YAFFS_TRACE_GC,
		  (TSTR
		   ("gc did not increase free chunks before %d after %d"
		    TENDSTR), chunksBefore, chunksAfter));
	}

	dev->isDoingGC = 0;

	return YAFFS_OK;
}

/* New garbage collector
 * If we're very low on erased blocks then we do aggressive garbage collection
 * otherwise we do "leasurely" garbage collection.
 * Aggressive gc looks further (whole array) and will accept less dirty blocks.
 * Passive gc only inspects smaller areas and will only accept more dirty blocks.
 *
 * The idea is to help clear out space in a more spread-out manner.
 * Dunno if it really does anything useful.
 */
static int yaffs_CheckGarbageCollection(yaffs_Device * dev)
{
	int block;
	int aggressive;
	int gcOk = YAFFS_OK;
	int maxTries = 0;

	int checkpointBlockAdjust;

	if (dev->isDoingGC) {
		/* Bail out so we don't get recursive gc */
		return YAFFS_OK;
	}

	/* This loop should pass the first time.
	 * We'll only see looping here if the erase of the collected block fails.
	 */

	do {
		maxTries++;

		checkpointBlockAdjust = yaffs_CalcCheckpointBlocksRequired(dev) - dev->blocksInCheckpoint;
		if(checkpointBlockAdjust < 0)
			checkpointBlockAdjust = 0;

		if (dev->nErasedBlocks < (dev->nReservedBlocks + checkpointBlockAdjust + 2)) {
			/* We need a block soon...*/
			aggressive = 1;
		} else {
			/* We're in no hurry */
			aggressive = 0;
		}

		block = yaffs_FindBlockForGarbageCollection(dev, aggressive);

		if (block > 0) {
			dev->garbageCollections++;
			if (!aggressive) {
				dev->passiveGarbageCollections++;
			}

			T(YAFFS_TRACE_GC,
			  (TSTR
			   ("yaffs: GC erasedBlocks %d aggressive %d" TENDSTR),
			   dev->nErasedBlocks, aggressive));

			gcOk = yaffs_GarbageCollectBlock(dev, block);
		}

		if (dev->nErasedBlocks < (dev->nReservedBlocks) && block > 0) {
			T(YAFFS_TRACE_GC,
			  (TSTR
			   ("yaffs: GC !!!no reclaim!!! erasedBlocks %d after try %d block %d"
			    TENDSTR), dev->nErasedBlocks, maxTries, block));
		}
	} while ((dev->nErasedBlocks < dev->nReservedBlocks) && (block > 0)
		 && (maxTries < 2));

	return aggressive ? gcOk : YAFFS_OK;
}

/*-------------------------  TAGS --------------------------------*/

static int yaffs_TagsMatch(const yaffs_ExtendedTags * tags, int objectId,
			   int chunkInObject)
{
	return (tags->chunkId == chunkInObject &&
		tags->objectId == objectId && !tags->chunkDeleted) ? 1 : 0;

}


/*-------------------- Data file manipulation -----------------*/

static int yaffs_FindChunkInFile(yaffs_Object * in, int chunkInInode,
				 yaffs_ExtendedTags * tags)
{
	/*Get the Tnode, then get the level 0 offset chunk offset */
	yaffs_Tnode *tn;
	int theChunk = -1;
	yaffs_ExtendedTags localTags;
	int retVal = -1;

	yaffs_Device *dev = in->myDev;

	if (!tags) {
		/* Passed a NULL, so use our own tags space */
		tags = &localTags;
	}

	tn = yaffs_FindLevel0Tnode(dev, &in->variant.fileVariant, chunkInInode);

	if (tn) {
		theChunk = yaffs_GetChunkGroupBase(dev,tn,chunkInInode);

		retVal =
		    yaffs_FindChunkInGroup(dev, theChunk, tags, in->objectId,
					   chunkInInode);
	}
	return retVal;
}

static int yaffs_FindAndDeleteChunkInFile(yaffs_Object * in, int chunkInInode,
					  yaffs_ExtendedTags * tags)
{
	/* Get the Tnode, then get the level 0 offset chunk offset */
	yaffs_Tnode *tn;
	int theChunk = -1;
	yaffs_ExtendedTags localTags;

	yaffs_Device *dev = in->myDev;
	int retVal = -1;

	if (!tags) {
		/* Passed a NULL, so use our own tags space */
		tags = &localTags;
	}

	tn = yaffs_FindLevel0Tnode(dev, &in->variant.fileVariant, chunkInInode);

	if (tn) {

		theChunk = yaffs_GetChunkGroupBase(dev,tn,chunkInInode);

		retVal =
		    yaffs_FindChunkInGroup(dev, theChunk, tags, in->objectId,
					   chunkInInode);

		/* Delete the entry in the filestructure (if found) */
		if (retVal != -1) {
			yaffs_PutLevel0Tnode(dev,tn,chunkInInode,0);
		}
	} else {
		/*T(("No level 0 found for %d\n", chunkInInode)); */
	}

	if (retVal == -1) {
		/* T(("Could not find %d to delete\n",chunkInInode)); */
	}
	return retVal;
}

#ifdef YAFFS_PARANOID

static int yaffs_CheckFileSanity(yaffs_Object * in)
{
	int chunk;
	int nChunks;
	int fSize;
	int failed = 0;
	int objId;
	yaffs_Tnode *tn;
	yaffs_Tags localTags;
	yaffs_Tags *tags = &localTags;
	int theChunk;
	int chunkDeleted;

	if (in->variantType != YAFFS_OBJECT_TYPE_FILE) {
		/* T(("Object not a file\n")); */
		return YAFFS_FAIL;
	}

	objId = in->objectId;
	fSize = in->variant.fileVariant.fileSize;
	nChunks =
	    (fSize + in->myDev->nDataBytesPerChunk - 1) / in->myDev->nDataBytesPerChunk;

	for (chunk = 1; chunk <= nChunks; chunk++) {
		tn = yaffs_FindLevel0Tnode(in->myDev, &in->variant.fileVariant,
					   chunk);

		if (tn) {

			theChunk = yaffs_GetChunkGroupBase(dev,tn,chunk);

			if (yaffs_CheckChunkBits
			    (dev, theChunk / dev->nChunksPerBlock,
			     theChunk % dev->nChunksPerBlock)) {

				yaffs_ReadChunkTagsFromNAND(in->myDev, theChunk,
							    tags,
							    &chunkDeleted);
				if (yaffs_TagsMatch
				    (tags, in->objectId, chunk, chunkDeleted)) {
					/* found it; */

				}
			} else {

				failed = 1;
			}

		} else {
			/* T(("No level 0 found for %d\n", chunk)); */
		}
	}

	return failed ? YAFFS_FAIL : YAFFS_OK;
}

#endif

static int yaffs_PutChunkIntoFile(yaffs_Object * in, int chunkInInode,
				  int chunkInNAND, int inScan)
{
	/* NB inScan is zero unless scanning.
	 * For forward scanning, inScan is > 0;
	 * for backward scanning inScan is < 0
	 */

	yaffs_Tnode *tn;
	yaffs_Device *dev = in->myDev;
	int existingChunk;
	yaffs_ExtendedTags existingTags;
	yaffs_ExtendedTags newTags;
	unsigned existingSerial, newSerial;

	if (in->variantType != YAFFS_OBJECT_TYPE_FILE) {
		/* Just ignore an attempt at putting a chunk into a non-file during scanning
		 * If it is not during Scanning then something went wrong!
		 */
		if (!inScan) {
			T(YAFFS_TRACE_ERROR,
			  (TSTR
			   ("yaffs tragedy:attempt to put data chunk into a non-file"
			    TENDSTR)));
			YBUG();
		}

		yaffs_DeleteChunk(dev, chunkInNAND, 1, __LINE__);
		return YAFFS_OK;
	}

	tn = yaffs_AddOrFindLevel0Tnode(dev,
					&in->variant.fileVariant,
					chunkInInode,
					NULL);
	if (!tn) {
		return YAFFS_FAIL;
	}

	existingChunk = yaffs_GetChunkGroupBase(dev,tn,chunkInInode);

	if (inScan != 0) {
		/* If we're scanning then we need to test for duplicates
		 * NB This does not need to be efficient since it should only ever
		 * happen when the power fails during a write, then only one
		 * chunk should ever be affected.
		 *
		 * Correction for YAFFS2: This could happen quite a lot and we need to think about efficiency! TODO
		 * Update: For backward scanning we don't need to re-read tags so this is quite cheap.
		 */

		if (existingChunk != 0) {
			/* NB Right now existing chunk will not be real chunkId if the device >= 32MB
			 *    thus we have to do a FindChunkInFile to get the real chunk id.
			 *
			 * We have a duplicate now we need to decide which one to use:
			 *
			 * Backwards scanning YAFFS2: The old one is what we use, dump the new one.
			 * Forward scanning YAFFS2: The new one is what we use, dump the old one.
			 * YAFFS1: Get both sets of tags and compare serial numbers.
			 */

			if (inScan > 0) {
				/* Only do this for forward scanning */
				yaffs_ReadChunkWithTagsFromNAND(dev,
								chunkInNAND,
								NULL, &newTags);

				/* Do a proper find */
				existingChunk =
				    yaffs_FindChunkInFile(in, chunkInInode,
							  &existingTags);
			}

			if (existingChunk <= 0) {
				/*Hoosterman - how did this happen? */

				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("yaffs tragedy: existing chunk < 0 in scan"
				    TENDSTR)));

			}

			/* NB The deleted flags should be false, otherwise the chunks will
			 * not be loaded during a scan
			 */

			newSerial = newTags.serialNumber;
			existingSerial = existingTags.serialNumber;

			if ((inScan > 0) &&
			    (in->myDev->isYaffs2 ||
			     existingChunk <= 0 ||
			     ((existingSerial + 1) & 3) == newSerial)) {
				/* Forward scanning.
				 * Use new
				 * Delete the old one and drop through to update the tnode
				 */
				yaffs_DeleteChunk(dev, existingChunk, 1,
						  __LINE__);
			} else {
				/* Backward scanning or we want to use the existing one
				 * Use existing.
				 * Delete the new one and return early so that the tnode isn't changed
				 */
				yaffs_DeleteChunk(dev, chunkInNAND, 1,
						  __LINE__);
				return YAFFS_OK;
			}
		}

	}

	if (existingChunk == 0) {
		in->nDataChunks++;
	}

	yaffs_PutLevel0Tnode(dev,tn,chunkInInode,chunkInNAND);

	return YAFFS_OK;
}

static int yaffs_ReadChunkDataFromObject(yaffs_Object * in, int chunkInInode,
					 __u8 * buffer)
{
	int chunkInNAND = yaffs_FindChunkInFile(in, chunkInInode, NULL);

	if (chunkInNAND >= 0) {
		return yaffs_ReadChunkWithTagsFromNAND(in->myDev, chunkInNAND,
						       buffer,NULL);
	} else {
		T(YAFFS_TRACE_NANDACCESS,
		  (TSTR("Chunk %d not found zero instead" TENDSTR),
		   chunkInNAND));
		/* get sane (zero) data if you read a hole */
		memset(buffer, 0, in->myDev->nDataBytesPerChunk);
		return 0;
	}

}

void yaffs_DeleteChunk(yaffs_Device * dev, int chunkId, int markNAND, int lyn)
{
	int block;
	int page;
	yaffs_ExtendedTags tags;
	yaffs_BlockInfo *bi;

	if (chunkId <= 0)
		return;


	dev->nDeletions++;
	block = chunkId / dev->nChunksPerBlock;
	page = chunkId % dev->nChunksPerBlock;


	if(!yaffs_CheckChunkBit(dev,block,page))
		T(YAFFS_TRACE_VERIFY,
		 	(TSTR("Deleting invalid chunk %d"TENDSTR),
		 	 chunkId));

	bi = yaffs_GetBlockInfo(dev, block);

	T(YAFFS_TRACE_DELETION,
	  (TSTR("line %d delete of chunk %d" TENDSTR), lyn, chunkId));

	if (markNAND &&
	    bi->blockState != YAFFS_BLOCK_STATE_COLLECTING && !dev->isYaffs2) {

		yaffs_InitialiseTags(&tags);

		tags.chunkDeleted = 1;

		yaffs_WriteChunkWithTagsToNAND(dev, chunkId, NULL, &tags);
		yaffs_HandleUpdateChunk(dev, chunkId, &tags);
	} else {
		dev->nUnmarkedDeletions++;
	}

	/* Pull out of the management area.
	 * If the whole block became dirty, this will kick off an erasure.
	 */
	if (bi->blockState == YAFFS_BLOCK_STATE_ALLOCATING ||
	    bi->blockState == YAFFS_BLOCK_STATE_FULL ||
	    bi->blockState == YAFFS_BLOCK_STATE_NEEDS_SCANNING ||
	    bi->blockState == YAFFS_BLOCK_STATE_COLLECTING) {
		dev->nFreeChunks++;

		yaffs_ClearChunkBit(dev, block, page);

		bi->pagesInUse--;

		if (bi->pagesInUse == 0 &&
		    !bi->hasShrinkHeader &&
		    bi->blockState != YAFFS_BLOCK_STATE_ALLOCATING &&
		    bi->blockState != YAFFS_BLOCK_STATE_NEEDS_SCANNING) {
			yaffs_BlockBecameDirty(dev, block);
		}

	} else {
		/* T(("Bad news deleting chunk %d\n",chunkId)); */
	}

}

static int yaffs_WriteChunkDataToObject(yaffs_Object * in, int chunkInInode,
					const __u8 * buffer, int nBytes,
					int useReserve)
{
	/* Find old chunk Need to do this to get serial number
	 * Write new one and patch into tree.
	 * Invalidate old tags.
	 */

	int prevChunkId;
	yaffs_ExtendedTags prevTags;

	int newChunkId;
	yaffs_ExtendedTags newTags;

	yaffs_Device *dev = in->myDev;

	yaffs_CheckGarbageCollection(dev);

	/* Get the previous chunk at this location in the file if it exists */
	prevChunkId = yaffs_FindChunkInFile(in, chunkInInode, &prevTags);

	/* Set up new tags */
	yaffs_InitialiseTags(&newTags);

	newTags.chunkId = chunkInInode;
	newTags.objectId = in->objectId;
	newTags.serialNumber =
	    (prevChunkId >= 0) ? prevTags.serialNumber + 1 : 1;
	newTags.byteCount = nBytes;

	newChunkId =
	    yaffs_WriteNewChunkWithTagsToNAND(dev, buffer, &newTags,
					      useReserve);

	if (newChunkId >= 0) {
		yaffs_PutChunkIntoFile(in, chunkInInode, newChunkId, 0);

		if (prevChunkId >= 0) {
			yaffs_DeleteChunk(dev, prevChunkId, 1, __LINE__);

		}

		yaffs_CheckFileSanity(in);
	}
	return newChunkId;

}

/* UpdateObjectHeader updates the header on NAND for an object.
 * If name is not NULL, then that new name is used.
 */
int yaffs_UpdateObjectHeader(yaffs_Object * in, const YCHAR * name, int force,
			     int isShrink, int shadows)
{

	yaffs_BlockInfo *bi;

	yaffs_Device *dev = in->myDev;

	int prevChunkId;
	int retVal = 0;
	int result = 0;

	int newChunkId;
	yaffs_ExtendedTags newTags;
	yaffs_ExtendedTags oldTags;

	__u8 *buffer = NULL;
	YCHAR oldName[YAFFS_MAX_NAME_LENGTH + 1];

	yaffs_ObjectHeader *oh = NULL;

	yaffs_strcpy(oldName,"silly old name");

	if (!in->fake || force) {

		yaffs_CheckGarbageCollection(dev);
		yaffs_CheckObjectDetailsLoaded(in);

		buffer = yaffs_GetTempBuffer(in->myDev, __LINE__);
		oh = (yaffs_ObjectHeader *) buffer;

		prevChunkId = in->chunkId;

		if (prevChunkId >= 0) {
			result = yaffs_ReadChunkWithTagsFromNAND(dev, prevChunkId,
							buffer, &oldTags);

			yaffs_VerifyObjectHeader(in,oh,&oldTags,0);

			memcpy(oldName, oh->name, sizeof(oh->name));
		}

		memset(buffer, 0xFF, dev->nDataBytesPerChunk);

		oh->type = in->variantType;
		oh->yst_mode = in->yst_mode;
		oh->shadowsObject = shadows;

#ifdef CONFIG_YAFFS_WINCE
		oh->win_atime[0] = in->win_atime[0];
		oh->win_ctime[0] = in->win_ctime[0];
		oh->win_mtime[0] = in->win_mtime[0];
		oh->win_atime[1] = in->win_atime[1];
		oh->win_ctime[1] = in->win_ctime[1];
		oh->win_mtime[1] = in->win_mtime[1];
#else
		oh->yst_uid = in->yst_uid;
		oh->yst_gid = in->yst_gid;
		oh->yst_atime = in->yst_atime;
		oh->yst_mtime = in->yst_mtime;
		oh->yst_ctime = in->yst_ctime;
		oh->yst_rdev = in->yst_rdev;
#endif
		if (in->parent) {
			oh->parentObjectId = in->parent->objectId;
		} else {
			oh->parentObjectId = 0;
		}

		if (name && *name) {
			memset(oh->name, 0, sizeof(oh->name));
			yaffs_strncpy(oh->name, name, YAFFS_MAX_NAME_LENGTH);
		} else if (prevChunkId>=0) {
			memcpy(oh->name, oldName, sizeof(oh->name));
		} else {
			memset(oh->name, 0, sizeof(oh->name));
		}

		oh->isShrink = isShrink;

		switch (in->variantType) {
		case YAFFS_OBJECT_TYPE_UNKNOWN:
			/* Should not happen */
			break;
		case YAFFS_OBJECT_TYPE_FILE:
			oh->fileSize =
			    (oh->parentObjectId == YAFFS_OBJECTID_DELETED
			     || oh->parentObjectId ==
			     YAFFS_OBJECTID_UNLINKED) ? 0 : in->variant.
			    fileVariant.fileSize;
			break;
		case YAFFS_OBJECT_TYPE_HARDLINK:
			oh->equivalentObjectId =
			    in->variant.hardLinkVariant.equivalentObjectId;
			break;
		case YAFFS_OBJECT_TYPE_SPECIAL:
			/* Do nothing */
			break;
		case YAFFS_OBJECT_TYPE_DIRECTORY:
			/* Do nothing */
			break;
		case YAFFS_OBJECT_TYPE_SYMLINK:
			yaffs_strncpy(oh->alias,
				      in->variant.symLinkVariant.alias,
				      YAFFS_MAX_ALIAS_LENGTH);
			oh->alias[YAFFS_MAX_ALIAS_LENGTH] = 0;
			break;
		}

		/* Tags */
		yaffs_InitialiseTags(&newTags);
		in->serial++;
		newTags.chunkId = 0;
		newTags.objectId = in->objectId;
		newTags.serialNumber = in->serial;

		/* Add extra info for file header */

		newTags.extraHeaderInfoAvailable = 1;
		newTags.extraParentObjectId = oh->parentObjectId;
		newTags.extraFileLength = oh->fileSize;
		newTags.extraIsShrinkHeader = oh->isShrink;
		newTags.extraEquivalentObjectId = oh->equivalentObjectId;
		newTags.extraShadows = (oh->shadowsObject > 0) ? 1 : 0;
		newTags.extraObjectType = in->variantType;

		yaffs_VerifyObjectHeader(in,oh,&newTags,1);

		/* Create new chunk in NAND */
		newChunkId =
		    yaffs_WriteNewChunkWithTagsToNAND(dev, buffer, &newTags,
						      (prevChunkId >= 0) ? 1 : 0);

		if (newChunkId >= 0) {

			in->chunkId = newChunkId;

			if (prevChunkId >= 0) {
				yaffs_DeleteChunk(dev, prevChunkId, 1,
						  __LINE__);
			}

			if(!yaffs_ObjectHasCachedWriteData(in))
				in->dirty = 0;

			/* If this was a shrink, then mark the block that the chunk lives on */
			if (isShrink) {
				bi = yaffs_GetBlockInfo(in->myDev,
							newChunkId /in->myDev->	nChunksPerBlock);
				bi->hasShrinkHeader = 1;
			}

		}

		retVal = newChunkId;

	}

	if (buffer)
		yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);

	return retVal;
}

/*------------------------ Short Operations Cache ----------------------------------------
 *   In many situations where there is no high level buffering (eg WinCE) a lot of
 *   reads might be short sequential reads, and a lot of writes may be short
 *   sequential writes. eg. scanning/writing a jpeg file.
 *   In these cases, a short read/write cache can provide a huge perfomance benefit
 *   with dumb-as-a-rock code.
 *   In Linux, the page cache provides read buffering aand the short op cache provides write
 *   buffering.
 *
 *   There are a limited number (~10) of cache chunks per device so that we don't
 *   need a very intelligent search.
 */

static int yaffs_ObjectHasCachedWriteData(yaffs_Object *obj)
{
	yaffs_Device *dev = obj->myDev;
	int i;
	yaffs_ChunkCache *cache;
	int nCaches = obj->myDev->nShortOpCaches;

	for(i = 0; i < nCaches; i++){
		cache = &dev->srCache[i];
		if (cache->object == obj &&
		    cache->dirty)
			return 1;
	}

	return 0;
}


static void yaffs_FlushFilesChunkCache(yaffs_Object * obj)
{
	yaffs_Device *dev = obj->myDev;
	int lowest = -99;	/* Stop compiler whining. */
	int i;
	yaffs_ChunkCache *cache;
	int chunkWritten = 0;
	int nCaches = obj->myDev->nShortOpCaches;

	if (nCaches > 0) {
		do {
			cache = NULL;

			/* Find the dirty cache for this object with the lowest chunk id. */
			for (i = 0; i < nCaches; i++) {
				if (dev->srCache[i].object == obj &&
				    dev->srCache[i].dirty) {
					if (!cache
					    || dev->srCache[i].chunkId <
					    lowest) {
						cache = &dev->srCache[i];
						lowest = cache->chunkId;
					}
				}
			}

			if (cache && !cache->locked) {
				/* Write it out and free it up */

				chunkWritten =
				    yaffs_WriteChunkDataToObject(cache->object,
								 cache->chunkId,
								 cache->data,
								 cache->nBytes,
								 1);
				cache->dirty = 0;
				cache->object = NULL;
			}

		} while (cache && chunkWritten > 0);

		if (cache) {
			/* Hoosterman, disk full while writing cache out. */
			T(YAFFS_TRACE_ERROR,
			  (TSTR("yaffs tragedy: no space during cache write" TENDSTR)));

		}
	}

}

/*yaffs_FlushEntireDeviceCache(dev)
 *
 *
 */

void yaffs_FlushEntireDeviceCache(yaffs_Device *dev)
{
	yaffs_Object *obj;
	int nCaches = dev->nShortOpCaches;
	int i;

	/* Find a dirty object in the cache and flush it...
	 * until there are no further dirty objects.
	 */
	do {
		obj = NULL;
		for( i = 0; i < nCaches && !obj; i++) {
			if (dev->srCache[i].object &&
			    dev->srCache[i].dirty)
				obj = dev->srCache[i].object;

		}
		if(obj)
			yaffs_FlushFilesChunkCache(obj);

	} while(obj);

}


/* Grab us a cache chunk for use.
 * First look for an empty one.
 * Then look for the least recently used non-dirty one.
 * Then look for the least recently used dirty one...., flush and look again.
 */
static yaffs_ChunkCache *yaffs_GrabChunkCacheWorker(yaffs_Device * dev)
{
	int i;
	int usage;
	int theOne;

	if (dev->nShortOpCaches > 0) {
		for (i = 0; i < dev->nShortOpCaches; i++) {
			if (!dev->srCache[i].object)
				return &dev->srCache[i];
		}

		return NULL;

		theOne = -1;
		usage = 0;	/* just to stop the compiler grizzling */

		for (i = 0; i < dev->nShortOpCaches; i++) {
			if (!dev->srCache[i].dirty &&
			    ((dev->srCache[i].lastUse < usage && theOne >= 0) ||
			     theOne < 0)) {
				usage = dev->srCache[i].lastUse;
				theOne = i;
			}
		}


		return theOne >= 0 ? &dev->srCache[theOne] : NULL;
	} else {
		return NULL;
	}

}

static yaffs_ChunkCache *yaffs_GrabChunkCache(yaffs_Device * dev)
{
	yaffs_ChunkCache *cache;
	yaffs_Object *theObj;
	int usage;
	int i;
	int pushout;

	if (dev->nShortOpCaches > 0) {
		/* Try find a non-dirty one... */

		cache = yaffs_GrabChunkCacheWorker(dev);

		if (!cache) {
			/* They were all dirty, find the last recently used object and flush
			 * its cache, then  find again.
			 * NB what's here is not very accurate, we actually flush the object
			 * the last recently used page.
			 */

			/* With locking we can't assume we can use entry zero */

			theObj = NULL;
			usage = -1;
			cache = NULL;
			pushout = -1;

			for (i = 0; i < dev->nShortOpCaches; i++) {
				if (dev->srCache[i].object &&
				    !dev->srCache[i].locked &&
				    (dev->srCache[i].lastUse < usage || !cache))
				{
					usage = dev->srCache[i].lastUse;
					theObj = dev->srCache[i].object;
					cache = &dev->srCache[i];
					pushout = i;
				}
			}

			if (!cache || cache->dirty) {
				/* Flush and try again */
				yaffs_FlushFilesChunkCache(theObj);
				cache = yaffs_GrabChunkCacheWorker(dev);
			}

		}
		return cache;
	} else
		return NULL;

}

/* Find a cached chunk */
static yaffs_ChunkCache *yaffs_FindChunkCache(const yaffs_Object * obj,
					      int chunkId)
{
	yaffs_Device *dev = obj->myDev;
	int i;
	if (dev->nShortOpCaches > 0) {
		for (i = 0; i < dev->nShortOpCaches; i++) {
			if (dev->srCache[i].object == obj &&
			    dev->srCache[i].chunkId == chunkId) {
				dev->cacheHits++;

				return &dev->srCache[i];
			}
		}
	}
	return NULL;
}

/* Mark the chunk for the least recently used algorithym */
static void yaffs_UseChunkCache(yaffs_Device * dev, yaffs_ChunkCache * cache,
				int isAWrite)
{

	if (dev->nShortOpCaches > 0) {
		if (dev->srLastUse < 0 || dev->srLastUse > 100000000) {
			/* Reset the cache usages */
			int i;
			for (i = 1; i < dev->nShortOpCaches; i++) {
				dev->srCache[i].lastUse = 0;
			}
			dev->srLastUse = 0;
		}

		dev->srLastUse++;

		cache->lastUse = dev->srLastUse;

		if (isAWrite) {
			cache->dirty = 1;
		}
	}
}

/* Invalidate a single cache page.
 * Do this when a whole page gets written,
 * ie the short cache for this page is no longer valid.
 */
static void yaffs_InvalidateChunkCache(yaffs_Object * object, int chunkId)
{
	if (object->myDev->nShortOpCaches > 0) {
		yaffs_ChunkCache *cache = yaffs_FindChunkCache(object, chunkId);

		if (cache) {
			cache->object = NULL;
		}
	}
}

/* Invalidate all the cache pages associated with this object
 * Do this whenever ther file is deleted or resized.
 */
static void yaffs_InvalidateWholeChunkCache(yaffs_Object * in)
{
	int i;
	yaffs_Device *dev = in->myDev;

	if (dev->nShortOpCaches > 0) {
		/* Invalidate it. */
		for (i = 0; i < dev->nShortOpCaches; i++) {
			if (dev->srCache[i].object == in) {
				dev->srCache[i].object = NULL;
			}
		}
	}
}

/*--------------------- Checkpointing --------------------*/


static int yaffs_WriteCheckpointValidityMarker(yaffs_Device *dev,int head)
{
	yaffs_CheckpointValidity cp;

	memset(&cp,0,sizeof(cp));

	cp.structType = sizeof(cp);
	cp.magic = YAFFS_MAGIC;
	cp.version = YAFFS_CHECKPOINT_VERSION;
	cp.head = (head) ? 1 : 0;

	return (yaffs_CheckpointWrite(dev,&cp,sizeof(cp)) == sizeof(cp))?
		1 : 0;
}

static int yaffs_ReadCheckpointValidityMarker(yaffs_Device *dev, int head)
{
	yaffs_CheckpointValidity cp;
	int ok;

	ok = (yaffs_CheckpointRead(dev,&cp,sizeof(cp)) == sizeof(cp));

	if(ok)
		ok = (cp.structType == sizeof(cp)) &&
		     (cp.magic == YAFFS_MAGIC) &&
		     (cp.version == YAFFS_CHECKPOINT_VERSION) &&
		     (cp.head == ((head) ? 1 : 0));
	return ok ? 1 : 0;
}

static void yaffs_DeviceToCheckpointDevice(yaffs_CheckpointDevice *cp,
					   yaffs_Device *dev)
{
	cp->nErasedBlocks = dev->nErasedBlocks;
	cp->allocationBlock = dev->allocationBlock;
	cp->allocationPage = dev->allocationPage;
	cp->nFreeChunks = dev->nFreeChunks;

	cp->nDeletedFiles = dev->nDeletedFiles;
	cp->nUnlinkedFiles = dev->nUnlinkedFiles;
	cp->nBackgroundDeletions = dev->nBackgroundDeletions;
	cp->sequenceNumber = dev->sequenceNumber;
	cp->oldestDirtySequence = dev->oldestDirtySequence;

}

static void yaffs_CheckpointDeviceToDevice(yaffs_Device *dev,
					   yaffs_CheckpointDevice *cp)
{
	dev->nErasedBlocks = cp->nErasedBlocks;
	dev->allocationBlock = cp->allocationBlock;
	dev->allocationPage = cp->allocationPage;
	dev->nFreeChunks = cp->nFreeChunks;

	dev->nDeletedFiles = cp->nDeletedFiles;
	dev->nUnlinkedFiles = cp->nUnlinkedFiles;
	dev->nBackgroundDeletions = cp->nBackgroundDeletions;
	dev->sequenceNumber = cp->sequenceNumber;
	dev->oldestDirtySequence = cp->oldestDirtySequence;
}


static int yaffs_WriteCheckpointDevice(yaffs_Device *dev)
{
	yaffs_CheckpointDevice cp;
	__u32 nBytes;
	__u32 nBlocks = (dev->internalEndBlock - dev->internalStartBlock + 1);

	int ok;

	/* Write device runtime values*/
	yaffs_DeviceToCheckpointDevice(&cp,dev);
	cp.structType = sizeof(cp);

	ok = (yaffs_CheckpointWrite(dev,&cp,sizeof(cp)) == sizeof(cp));

	/* Write block info */
	if(ok) {
		nBytes = nBlocks * sizeof(yaffs_BlockInfo);
		ok = (yaffs_CheckpointWrite(dev,dev->blockInfo,nBytes) == nBytes);
	}

	/* Write chunk bits */
	if(ok) {
		nBytes = nBlocks * dev->chunkBitmapStride;
		ok = (yaffs_CheckpointWrite(dev,dev->chunkBits,nBytes) == nBytes);
	}
	return	 ok ? 1 : 0;

}

static int yaffs_ReadCheckpointDevice(yaffs_Device *dev)
{
	yaffs_CheckpointDevice cp;
	__u32 nBytes;
	__u32 nBlocks = (dev->internalEndBlock - dev->internalStartBlock + 1);

	int ok;

	ok = (yaffs_CheckpointRead(dev,&cp,sizeof(cp)) == sizeof(cp));
	if(!ok)
		return 0;

	if(cp.structType != sizeof(cp))
		return 0;


	yaffs_CheckpointDeviceToDevice(dev,&cp);

	nBytes = nBlocks * sizeof(yaffs_BlockInfo);

	ok = (yaffs_CheckpointRead(dev,dev->blockInfo,nBytes) == nBytes);

	if(!ok)
		return 0;
	nBytes = nBlocks * dev->chunkBitmapStride;

	ok = (yaffs_CheckpointRead(dev,dev->chunkBits,nBytes) == nBytes);

	return ok ? 1 : 0;
}

static void yaffs_ObjectToCheckpointObject(yaffs_CheckpointObject *cp,
					   yaffs_Object *obj)
{

	cp->objectId = obj->objectId;
	cp->parentId = (obj->parent) ? obj->parent->objectId : 0;
	cp->chunkId = obj->chunkId;
	cp->variantType = obj->variantType;
	cp->deleted = obj->deleted;
	cp->softDeleted = obj->softDeleted;
	cp->unlinked = obj->unlinked;
	cp->fake = obj->fake;
	cp->renameAllowed = obj->renameAllowed;
	cp->unlinkAllowed = obj->unlinkAllowed;
	cp->serial = obj->serial;
	cp->nDataChunks = obj->nDataChunks;

	if(obj->variantType == YAFFS_OBJECT_TYPE_FILE)
		cp->fileSizeOrEquivalentObjectId = obj->variant.fileVariant.fileSize;
	else if(obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK)
		cp->fileSizeOrEquivalentObjectId = obj->variant.hardLinkVariant.equivalentObjectId;
}

static void yaffs_CheckpointObjectToObject( yaffs_Object *obj,yaffs_CheckpointObject *cp)
{

	yaffs_Object *parent;

	obj->objectId = cp->objectId;

	if(cp->parentId)
		parent = yaffs_FindOrCreateObjectByNumber(
					obj->myDev,
					cp->parentId,
					YAFFS_OBJECT_TYPE_DIRECTORY);
	else
		parent = NULL;

	if(parent)
		yaffs_AddObjectToDirectory(parent, obj);

	obj->chunkId = cp->chunkId;
	obj->variantType = cp->variantType;
	obj->deleted = cp->deleted;
	obj->softDeleted = cp->softDeleted;
	obj->unlinked = cp->unlinked;
	obj->fake = cp->fake;
	obj->renameAllowed = cp->renameAllowed;
	obj->unlinkAllowed = cp->unlinkAllowed;
	obj->serial = cp->serial;
	obj->nDataChunks = cp->nDataChunks;

	if(obj->variantType == YAFFS_OBJECT_TYPE_FILE)
		obj->variant.fileVariant.fileSize = cp->fileSizeOrEquivalentObjectId;
	else if(obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK)
		obj->variant.hardLinkVariant.equivalentObjectId = cp->fileSizeOrEquivalentObjectId;

	if(obj->objectId >= YAFFS_NOBJECT_BUCKETS)
		obj->lazyLoaded = 1;
}



static int yaffs_CheckpointTnodeWorker(yaffs_Object * in, yaffs_Tnode * tn,
				  	__u32 level, int chunkOffset)
{
	int i;
	yaffs_Device *dev = in->myDev;
	int ok = 1;
	int tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if(tnodeSize < sizeof(yaffs_Tnode))
		tnodeSize = sizeof(yaffs_Tnode);


	if (tn) {
		if (level > 0) {

			for (i = 0; i < YAFFS_NTNODES_INTERNAL && ok; i++){
				if (tn->internal[i]) {
					ok = yaffs_CheckpointTnodeWorker(in,
							tn->internal[i],
							level - 1,
							(chunkOffset<<YAFFS_TNODES_INTERNAL_BITS) + i);
				}
			}
		} else if (level == 0) {
			__u32 baseOffset = chunkOffset <<  YAFFS_TNODES_LEVEL0_BITS;
			/* printf("write tnode at %d\n",baseOffset); */
			ok = (yaffs_CheckpointWrite(dev,&baseOffset,sizeof(baseOffset)) == sizeof(baseOffset));
			if(ok)
				ok = (yaffs_CheckpointWrite(dev,tn,tnodeSize) == tnodeSize);
		}
	}

	return ok;

}

static int yaffs_WriteCheckpointTnodes(yaffs_Object *obj)
{
	__u32 endMarker = ~0;
	int ok = 1;

	if(obj->variantType == YAFFS_OBJECT_TYPE_FILE){
		ok = yaffs_CheckpointTnodeWorker(obj,
					    obj->variant.fileVariant.top,
					    obj->variant.fileVariant.topLevel,
					    0);
		if(ok)
			ok = (yaffs_CheckpointWrite(obj->myDev,&endMarker,sizeof(endMarker)) ==
				sizeof(endMarker));
	}

	return ok ? 1 : 0;
}

static int yaffs_ReadCheckpointTnodes(yaffs_Object *obj)
{
	__u32 baseChunk;
	int ok = 1;
	yaffs_Device *dev = obj->myDev;
	yaffs_FileStructure *fileStructPtr = &obj->variant.fileVariant;
	yaffs_Tnode *tn;
	int nread = 0;
	int tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if(tnodeSize < sizeof(yaffs_Tnode))
		tnodeSize = sizeof(yaffs_Tnode);

	ok = (yaffs_CheckpointRead(dev,&baseChunk,sizeof(baseChunk)) == sizeof(baseChunk));

	while(ok && (~baseChunk)){
		nread++;
		/* Read level 0 tnode */


		/* printf("read  tnode at %d\n",baseChunk); */
		tn = yaffs_GetTnodeRaw(dev);
		if(tn)
			ok = (yaffs_CheckpointRead(dev,tn,tnodeSize) == tnodeSize);
		else
			ok = 0;

		if(tn && ok){
			ok = yaffs_AddOrFindLevel0Tnode(dev,
					       		fileStructPtr,
					       		baseChunk,
					       		tn) ? 1 : 0;

		}

		if(ok)
			ok = (yaffs_CheckpointRead(dev,&baseChunk,sizeof(baseChunk)) == sizeof(baseChunk));

	}

	T(YAFFS_TRACE_CHECKPOINT,(
		TSTR("Checkpoint read tnodes %d records, last %d. ok %d" TENDSTR),
		nread,baseChunk,ok));

	return ok ? 1 : 0;
}


static int yaffs_WriteCheckpointObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	yaffs_CheckpointObject cp;
	int i;
	int ok = 1;
	struct list_head *lh;


	/* Iterate through the objects in each hash entry,
	 * dumping them to the checkpointing stream.
	 */

	 for(i = 0; ok &&  i <  YAFFS_NOBJECT_BUCKETS; i++){
	 	list_for_each(lh, &dev->objectBucket[i].list) {
			if (lh) {
				obj = list_entry(lh, yaffs_Object, hashLink);
				if (!obj->deferedFree) {
					yaffs_ObjectToCheckpointObject(&cp,obj);
					cp.structType = sizeof(cp);

					T(YAFFS_TRACE_CHECKPOINT,(
						TSTR("Checkpoint write object %d parent %d type %d chunk %d obj addr %x" TENDSTR),
						cp.objectId,cp.parentId,cp.variantType,cp.chunkId,(unsigned) obj));

					ok = (yaffs_CheckpointWrite(dev,&cp,sizeof(cp)) == sizeof(cp));

					if(ok && obj->variantType == YAFFS_OBJECT_TYPE_FILE){
						ok = yaffs_WriteCheckpointTnodes(obj);
					}
				}
			}
		}
	 }

	 /* Dump end of list */
	memset(&cp,0xFF,sizeof(yaffs_CheckpointObject));
	cp.structType = sizeof(cp);

	if(ok)
		ok = (yaffs_CheckpointWrite(dev,&cp,sizeof(cp)) == sizeof(cp));

	return ok ? 1 : 0;
}

static int yaffs_ReadCheckpointObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	yaffs_CheckpointObject cp;
	int ok = 1;
	int done = 0;
	yaffs_Object *hardList = NULL;

	while(ok && !done) {
		ok = (yaffs_CheckpointRead(dev,&cp,sizeof(cp)) == sizeof(cp));
		if(cp.structType != sizeof(cp)) {
			T(YAFFS_TRACE_CHECKPOINT,(TSTR("struct size %d instead of %d ok %d"TENDSTR),
				cp.structType,sizeof(cp),ok));
			ok = 0;
		}

		T(YAFFS_TRACE_CHECKPOINT,(TSTR("Checkpoint read object %d parent %d type %d chunk %d " TENDSTR),
			cp.objectId,cp.parentId,cp.variantType,cp.chunkId));

		if(ok && cp.objectId == ~0)
			done = 1;
		else if(ok){
			obj = yaffs_FindOrCreateObjectByNumber(dev,cp.objectId, cp.variantType);
			if(obj) {
				yaffs_CheckpointObjectToObject(obj,&cp);
				if(obj->variantType == YAFFS_OBJECT_TYPE_FILE) {
					ok = yaffs_ReadCheckpointTnodes(obj);
				} else if(obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK) {
					obj->hardLinks.next =
						    (struct list_head *)
						    hardList;
					hardList = obj;
				}

			}
		}
	}

	if(ok)
		yaffs_HardlinkFixup(dev,hardList);

	return ok ? 1 : 0;
}

static int yaffs_WriteCheckpointSum(yaffs_Device *dev)
{
	__u32 checkpointSum;
	int ok;

	yaffs_GetCheckpointSum(dev,&checkpointSum);

	ok = (yaffs_CheckpointWrite(dev,&checkpointSum,sizeof(checkpointSum)) == sizeof(checkpointSum));

	if(!ok)
		return 0;

	return 1;
}

static int yaffs_ReadCheckpointSum(yaffs_Device *dev)
{
	__u32 checkpointSum0;
	__u32 checkpointSum1;
	int ok;

	yaffs_GetCheckpointSum(dev,&checkpointSum0);

	ok = (yaffs_CheckpointRead(dev,&checkpointSum1,sizeof(checkpointSum1)) == sizeof(checkpointSum1));

	if(!ok)
		return 0;

	if(checkpointSum0 != checkpointSum1)
		return 0;

	return 1;
}


static int yaffs_WriteCheckpointData(yaffs_Device *dev)
{

	int ok = 1;

	if(dev->skipCheckpointWrite || !dev->isYaffs2){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("skipping checkpoint write" TENDSTR)));
		ok = 0;
	}

	if(ok)
		ok = yaffs_CheckpointOpen(dev,1);

	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("write checkpoint validity" TENDSTR)));
		ok = yaffs_WriteCheckpointValidityMarker(dev,1);
	}
	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("write checkpoint device" TENDSTR)));
		ok = yaffs_WriteCheckpointDevice(dev);
	}
	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("write checkpoint objects" TENDSTR)));
		ok = yaffs_WriteCheckpointObjects(dev);
	}
	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("write checkpoint validity" TENDSTR)));
		ok = yaffs_WriteCheckpointValidityMarker(dev,0);
	}

	if(ok){
		ok = yaffs_WriteCheckpointSum(dev);
	}


	if(!yaffs_CheckpointClose(dev))
		 ok = 0;

	if(ok)
	    	dev->isCheckpointed = 1;
	 else
	 	dev->isCheckpointed = 0;

	return dev->isCheckpointed;
}

static int yaffs_ReadCheckpointData(yaffs_Device *dev)
{
	int ok = 1;

	if(dev->skipCheckpointRead || !dev->isYaffs2){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("skipping checkpoint read" TENDSTR)));
		ok = 0;
	}

	if(ok)
		ok = yaffs_CheckpointOpen(dev,0); /* open for read */

	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("read checkpoint validity" TENDSTR)));
		ok = yaffs_ReadCheckpointValidityMarker(dev,1);
	}
	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("read checkpoint device" TENDSTR)));
		ok = yaffs_ReadCheckpointDevice(dev);
	}
	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("read checkpoint objects" TENDSTR)));
		ok = yaffs_ReadCheckpointObjects(dev);
	}
	if(ok){
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("read checkpoint validity" TENDSTR)));
		ok = yaffs_ReadCheckpointValidityMarker(dev,0);
	}

	if(ok){
		ok = yaffs_ReadCheckpointSum(dev);
		T(YAFFS_TRACE_CHECKPOINT,(TSTR("read checkpoint checksum %d" TENDSTR),ok));
	}

	if(!yaffs_CheckpointClose(dev))
		ok = 0;

	if(ok)
	    	dev->isCheckpointed = 1;
	 else
	 	dev->isCheckpointed = 0;

	return ok ? 1 : 0;

}

static void yaffs_InvalidateCheckpoint(yaffs_Device *dev)
{
	if(dev->isCheckpointed ||
	   dev->blocksInCheckpoint > 0){
		dev->isCheckpointed = 0;
		yaffs_CheckpointInvalidateStream(dev);
		if(dev->superBlock && dev->markSuperBlockDirty)
			dev->markSuperBlockDirty(dev->superBlock);
	}
}


int yaffs_CheckpointSave(yaffs_Device *dev)
{

	T(YAFFS_TRACE_CHECKPOINT,(TSTR("save entry: isCheckpointed %d"TENDSTR),dev->isCheckpointed));

	yaffs_VerifyObjects(dev);
	yaffs_VerifyBlocks(dev);
	yaffs_VerifyFreeChunks(dev);

	if(!dev->isCheckpointed) {
		yaffs_InvalidateCheckpoint(dev);
		yaffs_WriteCheckpointData(dev);
	}

	T(YAFFS_TRACE_ALWAYS,(TSTR("save exit: isCheckpointed %d"TENDSTR),dev->isCheckpointed));

	return dev->isCheckpointed;
}

int yaffs_CheckpointRestore(yaffs_Device *dev)
{
	int retval;
	T(YAFFS_TRACE_CHECKPOINT,(TSTR("restore entry: isCheckpointed %d"TENDSTR),dev->isCheckpointed));

	retval = yaffs_ReadCheckpointData(dev);

	if(dev->isCheckpointed){
		yaffs_VerifyObjects(dev);
		yaffs_VerifyBlocks(dev);
		yaffs_VerifyFreeChunks(dev);
	}

	T(YAFFS_TRACE_CHECKPOINT,(TSTR("restore exit: isCheckpointed %d"TENDSTR),dev->isCheckpointed));

	return retval;
}

/*--------------------- File read/write ------------------------
 * Read and write have very similar structures.
 * In general the read/write has three parts to it
 * An incomplete chunk to start with (if the read/write is not chunk-aligned)
 * Some complete chunks
 * An incomplete chunk to end off with
 *
 * Curve-balls: the first chunk might also be the last chunk.
 */

int yaffs_ReadDataFromFile(yaffs_Object * in, __u8 * buffer, loff_t offset,
			   int nBytes)
{

	int chunk;
	int start;
	int nToCopy;
	int n = nBytes;
	int nDone = 0;
	yaffs_ChunkCache *cache;

	yaffs_Device *dev;

	dev = in->myDev;

	while (n > 0) {
		//chunk = offset / dev->nDataBytesPerChunk + 1;
		//start = offset % dev->nDataBytesPerChunk;
		yaffs_AddrToChunk(dev,offset,&chunk,&start);
		chunk++;

		/* OK now check for the curveball where the start and end are in
		 * the same chunk.
		 */
		if ((start + n) < dev->nDataBytesPerChunk) {
			nToCopy = n;
		} else {
			nToCopy = dev->nDataBytesPerChunk - start;
		}

		cache = yaffs_FindChunkCache(in, chunk);

		/* If the chunk is already in the cache or it is less than a whole chunk
		 * then use the cache (if there is caching)
		 * else bypass the cache.
		 */
		if (cache || nToCopy != dev->nDataBytesPerChunk) {
			if (dev->nShortOpCaches > 0) {

				/* If we can't find the data in the cache, then load it up. */

				if (!cache) {
					cache = yaffs_GrabChunkCache(in->myDev);
					cache->object = in;
					cache->chunkId = chunk;
					cache->dirty = 0;
					cache->locked = 0;
					yaffs_ReadChunkDataFromObject(in, chunk,
								      cache->
								      data);
					cache->nBytes = 0;
				}

				yaffs_UseChunkCache(dev, cache, 0);

				cache->locked = 1;

#ifdef CONFIG_YAFFS_WINCE
				yfsd_UnlockYAFFS(TRUE);
#endif
				memcpy(buffer, &cache->data[start], nToCopy);

#ifdef CONFIG_YAFFS_WINCE
				yfsd_LockYAFFS(TRUE);
#endif
				cache->locked = 0;
			} else {
				/* Read into the local buffer then copy..*/

				__u8 *localBuffer =
				    yaffs_GetTempBuffer(dev, __LINE__);
				yaffs_ReadChunkDataFromObject(in, chunk,
							      localBuffer);
#ifdef CONFIG_YAFFS_WINCE
				yfsd_UnlockYAFFS(TRUE);
#endif
				memcpy(buffer, &localBuffer[start], nToCopy);

#ifdef CONFIG_YAFFS_WINCE
				yfsd_LockYAFFS(TRUE);
#endif
				yaffs_ReleaseTempBuffer(dev, localBuffer,
							__LINE__);
			}

		} else {
#ifdef CONFIG_YAFFS_WINCE
			__u8 *localBuffer = yaffs_GetTempBuffer(dev, __LINE__);

			/* Under WinCE can't do direct transfer. Need to use a local buffer.
			 * This is because we otherwise screw up WinCE's memory mapper
			 */
			yaffs_ReadChunkDataFromObject(in, chunk, localBuffer);

#ifdef CONFIG_YAFFS_WINCE
			yfsd_UnlockYAFFS(TRUE);
#endif
			memcpy(buffer, localBuffer, dev->nDataBytesPerChunk);

#ifdef CONFIG_YAFFS_WINCE
			yfsd_LockYAFFS(TRUE);
			yaffs_ReleaseTempBuffer(dev, localBuffer, __LINE__);
#endif

#else
			/* A full chunk. Read directly into the supplied buffer. */
			yaffs_ReadChunkDataFromObject(in, chunk, buffer);
#endif
		}

		n -= nToCopy;
		offset += nToCopy;
		buffer += nToCopy;
		nDone += nToCopy;

	}

	return nDone;
}

int yaffs_WriteDataToFile(yaffs_Object * in, const __u8 * buffer, loff_t offset,
			  int nBytes, int writeThrough)
{

	int chunk;
	int start;
	int nToCopy;
	int n = nBytes;
	int nDone = 0;
	int nToWriteBack;
	int startOfWrite = offset;
	int chunkWritten = 0;
	int nBytesRead;

	yaffs_Device *dev;

	dev = in->myDev;

	while (n > 0 && chunkWritten >= 0) {
		//chunk = offset / dev->nDataBytesPerChunk + 1;
		//start = offset % dev->nDataBytesPerChunk;
		yaffs_AddrToChunk(dev,offset,&chunk,&start);
		chunk++;

		/* OK now check for the curveball where the start and end are in
		 * the same chunk.
		 */

		if ((start + n) < dev->nDataBytesPerChunk) {
			nToCopy = n;

			/* Now folks, to calculate how many bytes to write back....
			 * If we're overwriting and not writing to then end of file then
			 * we need to write back as much as was there before.
			 */

			nBytesRead =
			    in->variant.fileVariant.fileSize -
			    ((chunk - 1) * dev->nDataBytesPerChunk);

			if (nBytesRead > dev->nDataBytesPerChunk) {
				nBytesRead = dev->nDataBytesPerChunk;
			}

			nToWriteBack =
			    (nBytesRead >
			     (start + n)) ? nBytesRead : (start + n);

		} else {
			nToCopy = dev->nDataBytesPerChunk - start;
			nToWriteBack = dev->nDataBytesPerChunk;
		}

		if (nToCopy != dev->nDataBytesPerChunk) {
			/* An incomplete start or end chunk (or maybe both start and end chunk) */
			if (dev->nShortOpCaches > 0) {
				yaffs_ChunkCache *cache;
				/* If we can't find the data in the cache, then load the cache */
				cache = yaffs_FindChunkCache(in, chunk);

				if (!cache
				    && yaffs_CheckSpaceForAllocation(in->
								     myDev)) {
					cache = yaffs_GrabChunkCache(in->myDev);
					cache->object = in;
					cache->chunkId = chunk;
					cache->dirty = 0;
					cache->locked = 0;
					yaffs_ReadChunkDataFromObject(in, chunk,
								      cache->
								      data);
				}
				else if(cache &&
				        !cache->dirty &&
					!yaffs_CheckSpaceForAllocation(in->myDev)){
					/* Drop the cache if it was a read cache item and
					 * no space check has been made for it.
					 */
					 cache = NULL;
				}

				if (cache) {
					yaffs_UseChunkCache(dev, cache, 1);
					cache->locked = 1;
#ifdef CONFIG_YAFFS_WINCE
					yfsd_UnlockYAFFS(TRUE);
#endif

					memcpy(&cache->data[start], buffer,
					       nToCopy);

#ifdef CONFIG_YAFFS_WINCE
					yfsd_LockYAFFS(TRUE);
#endif
					cache->locked = 0;
					cache->nBytes = nToWriteBack;

					if (writeThrough) {
						chunkWritten =
						    yaffs_WriteChunkDataToObject
						    (cache->object,
						     cache->chunkId,
						     cache->data, cache->nBytes,
						     1);
						cache->dirty = 0;
					}

				} else {
					chunkWritten = -1;	/* fail the write */
				}
			} else {
				/* An incomplete start or end chunk (or maybe both start and end chunk)
				 * Read into the local buffer then copy, then copy over and write back.
				 */

				__u8 *localBuffer =
				    yaffs_GetTempBuffer(dev, __LINE__);

				yaffs_ReadChunkDataFromObject(in, chunk,
							      localBuffer);

#ifdef CONFIG_YAFFS_WINCE
				yfsd_UnlockYAFFS(TRUE);
#endif

				memcpy(&localBuffer[start], buffer, nToCopy);

#ifdef CONFIG_YAFFS_WINCE
				yfsd_LockYAFFS(TRUE);
#endif
				chunkWritten =
				    yaffs_WriteChunkDataToObject(in, chunk,
								 localBuffer,
								 nToWriteBack,
								 0);

				yaffs_ReleaseTempBuffer(dev, localBuffer,
							__LINE__);

			}

		} else {

#ifdef CONFIG_YAFFS_WINCE
			/* Under WinCE can't do direct transfer. Need to use a local buffer.
			 * This is because we otherwise screw up WinCE's memory mapper
			 */
			__u8 *localBuffer = yaffs_GetTempBuffer(dev, __LINE__);
#ifdef CONFIG_YAFFS_WINCE
			yfsd_UnlockYAFFS(TRUE);
#endif
			memcpy(localBuffer, buffer, dev->nDataBytesPerChunk);
#ifdef CONFIG_YAFFS_WINCE
			yfsd_LockYAFFS(TRUE);
#endif
			chunkWritten =
			    yaffs_WriteChunkDataToObject(in, chunk, localBuffer,
							 dev->nDataBytesPerChunk,
							 0);
			yaffs_ReleaseTempBuffer(dev, localBuffer, __LINE__);
#else
			/* A full chunk. Write directly from the supplied buffer. */
			chunkWritten =
			    yaffs_WriteChunkDataToObject(in, chunk, buffer,
							 dev->nDataBytesPerChunk,
							 0);
#endif
			/* Since we've overwritten the cached data, we better invalidate it. */
			yaffs_InvalidateChunkCache(in, chunk);
		}

		if (chunkWritten >= 0) {
			n -= nToCopy;
			offset += nToCopy;
			buffer += nToCopy;
			nDone += nToCopy;
		}

	}

	/* Update file object */

	if ((startOfWrite + nDone) > in->variant.fileVariant.fileSize) {
		in->variant.fileVariant.fileSize = (startOfWrite + nDone);
	}

	in->dirty = 1;

	return nDone;
}


/* ---------------------- File resizing stuff ------------------ */

static void yaffs_PruneResizedChunks(yaffs_Object * in, int newSize)
{

	yaffs_Device *dev = in->myDev;
	int oldFileSize = in->variant.fileVariant.fileSize;

	int lastDel = 1 + (oldFileSize - 1) / dev->nDataBytesPerChunk;

	int startDel = 1 + (newSize + dev->nDataBytesPerChunk - 1) /
	    dev->nDataBytesPerChunk;
	int i;
	int chunkId;

	/* Delete backwards so that we don't end up with holes if
	 * power is lost part-way through the operation.
	 */
	for (i = lastDel; i >= startDel; i--) {
		/* NB this could be optimised somewhat,
		 * eg. could retrieve the tags and write them without
		 * using yaffs_DeleteChunk
		 */

		chunkId = yaffs_FindAndDeleteChunkInFile(in, i, NULL);
		if (chunkId > 0) {
			if (chunkId <
			    (dev->internalStartBlock * dev->nChunksPerBlock)
			    || chunkId >=
			    ((dev->internalEndBlock +
			      1) * dev->nChunksPerBlock)) {
				T(YAFFS_TRACE_ALWAYS,
				  (TSTR("Found daft chunkId %d for %d" TENDSTR),
				   chunkId, i));
			} else {
				in->nDataChunks--;
				yaffs_DeleteChunk(dev, chunkId, 1, __LINE__);
			}
		}
	}

}

int yaffs_ResizeFile(yaffs_Object * in, loff_t newSize)
{

	int oldFileSize = in->variant.fileVariant.fileSize;
	int newSizeOfPartialChunk;
	int newFullChunks;

	yaffs_Device *dev = in->myDev;

	yaffs_AddrToChunk(dev, newSize, &newFullChunks, &newSizeOfPartialChunk);

	yaffs_FlushFilesChunkCache(in);
	yaffs_InvalidateWholeChunkCache(in);

	yaffs_CheckGarbageCollection(dev);

	if (in->variantType != YAFFS_OBJECT_TYPE_FILE) {
		return yaffs_GetFileSize(in);
	}

	if (newSize == oldFileSize) {
		return oldFileSize;
	}

	if (newSize < oldFileSize) {

		yaffs_PruneResizedChunks(in, newSize);

		if (newSizeOfPartialChunk != 0) {
			int lastChunk = 1 + newFullChunks;

			__u8 *localBuffer = yaffs_GetTempBuffer(dev, __LINE__);

			/* Got to read and rewrite the last chunk with its new size and zero pad */
			yaffs_ReadChunkDataFromObject(in, lastChunk,
						      localBuffer);

			memset(localBuffer + newSizeOfPartialChunk, 0,
			       dev->nDataBytesPerChunk - newSizeOfPartialChunk);

			yaffs_WriteChunkDataToObject(in, lastChunk, localBuffer,
						     newSizeOfPartialChunk, 1);

			yaffs_ReleaseTempBuffer(dev, localBuffer, __LINE__);
		}

		in->variant.fileVariant.fileSize = newSize;

		yaffs_PruneFileStructure(dev, &in->variant.fileVariant);
	} else {
		/* newsSize > oldFileSize */
		in->variant.fileVariant.fileSize = newSize;
	}



	/* Write a new object header.
	 * show we've shrunk the file, if need be
	 * Do this only if the file is not in the deleted directories.
	 */
	if (in->parent->objectId != YAFFS_OBJECTID_UNLINKED &&
	    in->parent->objectId != YAFFS_OBJECTID_DELETED) {
		yaffs_UpdateObjectHeader(in, NULL, 0,
					 (newSize < oldFileSize) ? 1 : 0, 0);
	}

	return YAFFS_OK;
}

loff_t yaffs_GetFileSize(yaffs_Object * obj)
{
	obj = yaffs_GetEquivalentObject(obj);

	switch (obj->variantType) {
	case YAFFS_OBJECT_TYPE_FILE:
		return obj->variant.fileVariant.fileSize;
	case YAFFS_OBJECT_TYPE_SYMLINK:
		return yaffs_strlen(obj->variant.symLinkVariant.alias);
	default:
		return 0;
	}
}



int yaffs_FlushFile(yaffs_Object * in, int updateTime)
{
	int retVal;
	if (in->dirty) {
		yaffs_FlushFilesChunkCache(in);
		if (updateTime) {
#ifdef CONFIG_YAFFS_WINCE
			yfsd_WinFileTimeNow(in->win_mtime);
#else

			in->yst_mtime = Y_CURRENT_TIME;

#endif
		}

		retVal =
		    (yaffs_UpdateObjectHeader(in, NULL, 0, 0, 0) >=
		     0) ? YAFFS_OK : YAFFS_FAIL;
	} else {
		retVal = YAFFS_OK;
	}

	return retVal;

}

static int yaffs_DoGenericObjectDeletion(yaffs_Object * in)
{

	/* First off, invalidate the file's data in the cache, without flushing. */
	yaffs_InvalidateWholeChunkCache(in);

	if (in->myDev->isYaffs2 && (in->parent != in->myDev->deletedDir)) {
		/* Move to the unlinked directory so we have a record that it was deleted. */
		yaffs_ChangeObjectName(in, in->myDev->deletedDir,"deleted", 0, 0);

	}

	yaffs_RemoveObjectFromDirectory(in);
	yaffs_DeleteChunk(in->myDev, in->chunkId, 1, __LINE__);
	in->chunkId = -1;

	yaffs_FreeObject(in);
	return YAFFS_OK;

}

/* yaffs_DeleteFile deletes the whole file data
 * and the inode associated with the file.
 * It does not delete the links associated with the file.
 */
static int yaffs_UnlinkFile(yaffs_Object * in)
{

	int retVal;
	int immediateDeletion = 0;

	if (1) {
#ifdef __KERNEL__
		if (!in->myInode) {
			immediateDeletion = 1;

		}
#else
		if (in->inUse <= 0) {
			immediateDeletion = 1;

		}
#endif
		if (immediateDeletion) {
			retVal =
			    yaffs_ChangeObjectName(in, in->myDev->deletedDir,
						   "deleted", 0, 0);
			T(YAFFS_TRACE_TRACING,
			  (TSTR("yaffs: immediate deletion of file %d" TENDSTR),
			   in->objectId));
			in->deleted = 1;
			in->myDev->nDeletedFiles++;
			if (0 && in->myDev->isYaffs2) {
				yaffs_ResizeFile(in, 0);
			}
			yaffs_SoftDeleteFile(in);
		} else {
			retVal =
			    yaffs_ChangeObjectName(in, in->myDev->unlinkedDir,
						   "unlinked", 0, 0);
		}

	}
	return retVal;
}

int yaffs_DeleteFile(yaffs_Object * in)
{
	int retVal = YAFFS_OK;

	if (in->nDataChunks > 0) {
		/* Use soft deletion if there is data in the file */
		if (!in->unlinked) {
			retVal = yaffs_UnlinkFile(in);
		}
		if (retVal == YAFFS_OK && in->unlinked && !in->deleted) {
			in->deleted = 1;
			in->myDev->nDeletedFiles++;
			yaffs_SoftDeleteFile(in);
		}
		return in->deleted ? YAFFS_OK : YAFFS_FAIL;
	} else {
		/* The file has no data chunks so we toss it immediately */
		yaffs_FreeTnode(in->myDev, in->variant.fileVariant.top);
		in->variant.fileVariant.top = NULL;
		yaffs_DoGenericObjectDeletion(in);

		return YAFFS_OK;
	}
}

static int yaffs_DeleteDirectory(yaffs_Object * in)
{
	/* First check that the directory is empty. */
	if (list_empty(&in->variant.directoryVariant.children)) {
		return yaffs_DoGenericObjectDeletion(in);
	}

	return YAFFS_FAIL;

}

static int yaffs_DeleteSymLink(yaffs_Object * in)
{
	YFREE(in->variant.symLinkVariant.alias);

	return yaffs_DoGenericObjectDeletion(in);
}

static int yaffs_DeleteHardLink(yaffs_Object * in)
{
	/* remove this hardlink from the list assocaited with the equivalent
	 * object
	 */
	list_del(&in->hardLinks);
	return yaffs_DoGenericObjectDeletion(in);
}

static void yaffs_DestroyObject(yaffs_Object * obj)
{
	switch (obj->variantType) {
	case YAFFS_OBJECT_TYPE_FILE:
		yaffs_DeleteFile(obj);
		break;
	case YAFFS_OBJECT_TYPE_DIRECTORY:
		yaffs_DeleteDirectory(obj);
		break;
	case YAFFS_OBJECT_TYPE_SYMLINK:
		yaffs_DeleteSymLink(obj);
		break;
	case YAFFS_OBJECT_TYPE_HARDLINK:
		yaffs_DeleteHardLink(obj);
		break;
	case YAFFS_OBJECT_TYPE_SPECIAL:
		yaffs_DoGenericObjectDeletion(obj);
		break;
	case YAFFS_OBJECT_TYPE_UNKNOWN:
		break;		/* should not happen. */
	}
}

static int yaffs_UnlinkWorker(yaffs_Object * obj)
{

	if (obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK) {
		return yaffs_DeleteHardLink(obj);
	} else if (!list_empty(&obj->hardLinks)) {
		/* Curve ball: We're unlinking an object that has a hardlink.
		 *
		 * This problem arises because we are not strictly following
		 * The Linux link/inode model.
		 *
		 * We can't really delete the object.
		 * Instead, we do the following:
		 * - Select a hardlink.
		 * - Unhook it from the hard links
		 * - Unhook it from its parent directory (so that the rename can work)
		 * - Rename the object to the hardlink's name.
		 * - Delete the hardlink
		 */

		yaffs_Object *hl;
		int retVal;
		YCHAR name[YAFFS_MAX_NAME_LENGTH + 1];

		hl = list_entry(obj->hardLinks.next, yaffs_Object, hardLinks);

		list_del_init(&hl->hardLinks);
		list_del_init(&hl->siblings);

		yaffs_GetObjectName(hl, name, YAFFS_MAX_NAME_LENGTH + 1);

		retVal = yaffs_ChangeObjectName(obj, hl->parent, name, 0, 0);

		if (retVal == YAFFS_OK) {
			retVal = yaffs_DoGenericObjectDeletion(hl);
		}
		return retVal;

	} else {
		switch (obj->variantType) {
		case YAFFS_OBJECT_TYPE_FILE:
			return yaffs_UnlinkFile(obj);
			break;
		case YAFFS_OBJECT_TYPE_DIRECTORY:
			return yaffs_DeleteDirectory(obj);
			break;
		case YAFFS_OBJECT_TYPE_SYMLINK:
			return yaffs_DeleteSymLink(obj);
			break;
		case YAFFS_OBJECT_TYPE_SPECIAL:
			return yaffs_DoGenericObjectDeletion(obj);
			break;
		case YAFFS_OBJECT_TYPE_HARDLINK:
		case YAFFS_OBJECT_TYPE_UNKNOWN:
		default:
			return YAFFS_FAIL;
		}
	}
}


static int yaffs_UnlinkObject( yaffs_Object *obj)
{

	if (obj && obj->unlinkAllowed) {
		return yaffs_UnlinkWorker(obj);
	}

	return YAFFS_FAIL;

}
int yaffs_Unlink(yaffs_Object * dir, const YCHAR * name)
{
	yaffs_Object *obj;

	obj = yaffs_FindObjectByName(dir, name);
	return yaffs_UnlinkObject(obj);
}

/*----------------------- Initialisation Scanning ---------------------- */

static void yaffs_HandleShadowedObject(yaffs_Device * dev, int objId,
				       int backwardScanning)
{
	yaffs_Object *obj;

	if (!backwardScanning) {
		/* Handle YAFFS1 forward scanning case
		 * For YAFFS1 we always do the deletion
		 */

	} else {
		/* Handle YAFFS2 case (backward scanning)
		 * If the shadowed object exists then ignore.
		 */
		if (yaffs_FindObjectByNumber(dev, objId)) {
			return;
		}
	}

	/* Let's create it (if it does not exist) assuming it is a file so that it can do shrinking etc.
	 * We put it in unlinked dir to be cleaned up after the scanning
	 */
	obj =
	    yaffs_FindOrCreateObjectByNumber(dev, objId,
					     YAFFS_OBJECT_TYPE_FILE);
	yaffs_AddObjectToDirectory(dev->unlinkedDir, obj);
	obj->variant.fileVariant.shrinkSize = 0;
	obj->valid = 1;		/* So that we don't read any other info for this file */

}

typedef struct {
	int seq;
	int block;
} yaffs_BlockIndex;


static void yaffs_HardlinkFixup(yaffs_Device *dev, yaffs_Object *hardList)
{
	yaffs_Object *hl;
	yaffs_Object *in;

	while (hardList) {
		hl = hardList;
		hardList = (yaffs_Object *) (hardList->hardLinks.next);

		in = yaffs_FindObjectByNumber(dev,
					      hl->variant.hardLinkVariant.
					      equivalentObjectId);

		if (in) {
			/* Add the hardlink pointers */
			hl->variant.hardLinkVariant.equivalentObject = in;
			list_add(&hl->hardLinks, &in->hardLinks);
		} else {
			/* Todo Need to report/handle this better.
			 * Got a problem... hardlink to a non-existant object
			 */
			hl->variant.hardLinkVariant.equivalentObject = NULL;
			INIT_LIST_HEAD(&hl->hardLinks);

		}

	}

}





static int ybicmp(const void *a, const void *b){
    register int aseq = ((yaffs_BlockIndex *)a)->seq;
    register int bseq = ((yaffs_BlockIndex *)b)->seq;
    register int ablock = ((yaffs_BlockIndex *)a)->block;
    register int bblock = ((yaffs_BlockIndex *)b)->block;
    if( aseq == bseq )
        return ablock - bblock;
    else
        return aseq - bseq;

}

static int yaffs_Scan(yaffs_Device * dev)
{
	yaffs_ExtendedTags tags;
	int blk;
	int blockIterator;
	int startIterator;
	int endIterator;
	int nBlocksToScan = 0;
	int result;

	int chunk;
	int c;
	int deleted;
	yaffs_BlockState state;
	yaffs_Object *hardList = NULL;
	yaffs_BlockInfo *bi;
	int sequenceNumber;
	yaffs_ObjectHeader *oh;
	yaffs_Object *in;
	yaffs_Object *parent;
	int nBlocks = dev->internalEndBlock - dev->internalStartBlock + 1;

	int alloc_failed = 0;


	__u8 *chunkData;

	yaffs_BlockIndex *blockIndex = NULL;

	if (dev->isYaffs2) {
		T(YAFFS_TRACE_SCAN,
		  (TSTR("yaffs_Scan is not for YAFFS2!" TENDSTR)));
		return YAFFS_FAIL;
	}

	//TODO  Throw all the yaffs2 stuuf out of yaffs_Scan since it is only for yaffs1 format.

	T(YAFFS_TRACE_SCAN,
	  (TSTR("yaffs_Scan starts  intstartblk %d intendblk %d..." TENDSTR),
	   dev->internalStartBlock, dev->internalEndBlock));

	chunkData = yaffs_GetTempBuffer(dev, __LINE__);

	dev->sequenceNumber = YAFFS_LOWEST_SEQUENCE_NUMBER;

	if (dev->isYaffs2) {
		blockIndex = YMALLOC(nBlocks * sizeof(yaffs_BlockIndex));
		if(!blockIndex)
			return YAFFS_FAIL;
	}

	/* Scan all the blocks to determine their state */
	for (blk = dev->internalStartBlock; blk <= dev->internalEndBlock; blk++) {
		bi = yaffs_GetBlockInfo(dev, blk);
		yaffs_ClearChunkBits(dev, blk);
		bi->pagesInUse = 0;
		bi->softDeletions = 0;

		yaffs_QueryInitialBlockState(dev, blk, &state, &sequenceNumber);

		bi->blockState = state;
		bi->sequenceNumber = sequenceNumber;

		T(YAFFS_TRACE_SCAN_DEBUG,
		  (TSTR("Block scanning block %d state %d seq %d" TENDSTR), blk,
		   state, sequenceNumber));

		if (state == YAFFS_BLOCK_STATE_DEAD) {
			T(YAFFS_TRACE_BAD_BLOCKS,
			  (TSTR("block %d is bad" TENDSTR), blk));
		} else if (state == YAFFS_BLOCK_STATE_EMPTY) {
			T(YAFFS_TRACE_SCAN_DEBUG,
			  (TSTR("Block empty " TENDSTR)));
			dev->nErasedBlocks++;
			dev->nFreeChunks += dev->nChunksPerBlock;
		} else if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {

			/* Determine the highest sequence number */
			if (dev->isYaffs2 &&
			    sequenceNumber >= YAFFS_LOWEST_SEQUENCE_NUMBER &&
			    sequenceNumber < YAFFS_HIGHEST_SEQUENCE_NUMBER) {

				blockIndex[nBlocksToScan].seq = sequenceNumber;
				blockIndex[nBlocksToScan].block = blk;

				nBlocksToScan++;

				if (sequenceNumber >= dev->sequenceNumber) {
					dev->sequenceNumber = sequenceNumber;
				}
			} else if (dev->isYaffs2) {
				/* TODO: Nasty sequence number! */
				T(YAFFS_TRACE_SCAN,
				  (TSTR
				   ("Block scanning block %d has bad sequence number %d"
				    TENDSTR), blk, sequenceNumber));

			}
		}
	}

	/* Sort the blocks
	 * Dungy old bubble sort for now...
	 */
	if (dev->isYaffs2) {
		yaffs_BlockIndex temp;
		int i;
		int j;

		for (i = 0; i < nBlocksToScan; i++)
			for (j = i + 1; j < nBlocksToScan; j++)
				if (blockIndex[i].seq > blockIndex[j].seq) {
					temp = blockIndex[j];
					blockIndex[j] = blockIndex[i];
					blockIndex[i] = temp;
				}
	}

	/* Now scan the blocks looking at the data. */
	if (dev->isYaffs2) {
		startIterator = 0;
		endIterator = nBlocksToScan - 1;
		T(YAFFS_TRACE_SCAN_DEBUG,
		  (TSTR("%d blocks to be scanned" TENDSTR), nBlocksToScan));
	} else {
		startIterator = dev->internalStartBlock;
		endIterator = dev->internalEndBlock;
	}

	/* For each block.... */
	for (blockIterator = startIterator; !alloc_failed && blockIterator <= endIterator;
	     blockIterator++) {

		if (dev->isYaffs2) {
			/* get the block to scan in the correct order */
			blk = blockIndex[blockIterator].block;
		} else {
			blk = blockIterator;
		}

		bi = yaffs_GetBlockInfo(dev, blk);
		state = bi->blockState;

		deleted = 0;

		/* For each chunk in each block that needs scanning....*/
		for (c = 0; !alloc_failed && c < dev->nChunksPerBlock &&
		     state == YAFFS_BLOCK_STATE_NEEDS_SCANNING; c++) {
			/* Read the tags and decide what to do */
			chunk = blk * dev->nChunksPerBlock + c;

			result = yaffs_ReadChunkWithTagsFromNAND(dev, chunk, NULL,
							&tags);

			/* Let's have a good look at this chunk... */

			if (!dev->isYaffs2 && tags.chunkDeleted) {
				/* YAFFS1 only...
				 * A deleted chunk
				 */
				deleted++;
				dev->nFreeChunks++;
				/*T((" %d %d deleted\n",blk,c)); */
			} else if (!tags.chunkUsed) {
				/* An unassigned chunk in the block
				 * This means that either the block is empty or
				 * this is the one being allocated from
				 */

				if (c == 0) {
					/* We're looking at the first chunk in the block so the block is unused */
					state = YAFFS_BLOCK_STATE_EMPTY;
					dev->nErasedBlocks++;
				} else {
					/* this is the block being allocated from */
					T(YAFFS_TRACE_SCAN,
					  (TSTR
					   (" Allocating from %d %d" TENDSTR),
					   blk, c));
					state = YAFFS_BLOCK_STATE_ALLOCATING;
					dev->allocationBlock = blk;
					dev->allocationPage = c;
					dev->allocationBlockFinder = blk;
					/* Set it to here to encourage the allocator to go forth from here. */

					/* Yaffs2 sanity check:
					 * This should be the one with the highest sequence number
					 */
					if (dev->isYaffs2
					    && (dev->sequenceNumber !=
						bi->sequenceNumber)) {
						T(YAFFS_TRACE_ALWAYS,
						  (TSTR
						   ("yaffs: Allocation block %d was not highest sequence id:"
						    " block seq = %d, dev seq = %d"
						    TENDSTR), blk,bi->sequenceNumber,dev->sequenceNumber));
					}
				}

				dev->nFreeChunks += (dev->nChunksPerBlock - c);
			} else if (tags.chunkId > 0) {
				/* chunkId > 0 so it is a data chunk... */
				unsigned int endpos;

				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				in = yaffs_FindOrCreateObjectByNumber(dev,
								      tags.
								      objectId,
								      YAFFS_OBJECT_TYPE_FILE);
				/* PutChunkIntoFile checks for a clash (two data chunks with
				 * the same chunkId).
				 */

				if(!in)
					alloc_failed = 1;

				if(in){
					if(!yaffs_PutChunkIntoFile(in, tags.chunkId, chunk,1))
						alloc_failed = 1;
				}

				endpos =
				    (tags.chunkId - 1) * dev->nDataBytesPerChunk +
				    tags.byteCount;
				if (in &&
				    in->variantType == YAFFS_OBJECT_TYPE_FILE
				    && in->variant.fileVariant.scannedFileSize <
				    endpos) {
					in->variant.fileVariant.
					    scannedFileSize = endpos;
					if (!dev->useHeaderFileSize) {
						in->variant.fileVariant.
						    fileSize =
						    in->variant.fileVariant.
						    scannedFileSize;
					}

				}
				/* T((" %d %d data %d %d\n",blk,c,tags.objectId,tags.chunkId));   */
			} else {
				/* chunkId == 0, so it is an ObjectHeader.
				 * Thus, we read in the object header and make the object
				 */
				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				result = yaffs_ReadChunkWithTagsFromNAND(dev, chunk,
								chunkData,
								NULL);

				oh = (yaffs_ObjectHeader *) chunkData;

				in = yaffs_FindObjectByNumber(dev,
							      tags.objectId);
				if (in && in->variantType != oh->type) {
					/* This should not happen, but somehow
					 * Wev'e ended up with an objectId that has been reused but not yet
					 * deleted, and worse still it has changed type. Delete the old object.
					 */

					yaffs_DestroyObject(in);

					in = 0;
				}

				in = yaffs_FindOrCreateObjectByNumber(dev,
								      tags.
								      objectId,
								      oh->type);

				if(!in)
					alloc_failed = 1;

				if (in && oh->shadowsObject > 0) {
					yaffs_HandleShadowedObject(dev,
								   oh->
								   shadowsObject,
								   0);
				}

				if (in && in->valid) {
					/* We have already filled this one. We have a duplicate and need to resolve it. */

					unsigned existingSerial = in->serial;
					unsigned newSerial = tags.serialNumber;

					if (dev->isYaffs2 ||
					    ((existingSerial + 1) & 3) ==
					    newSerial) {
						/* Use new one - destroy the exisiting one */
						yaffs_DeleteChunk(dev,
								  in->chunkId,
								  1, __LINE__);
						in->valid = 0;
					} else {
						/* Use existing - destroy this one. */
						yaffs_DeleteChunk(dev, chunk, 1,
								  __LINE__);
					}
				}

				if (in && !in->valid &&
				    (tags.objectId == YAFFS_OBJECTID_ROOT ||
				     tags.objectId == YAFFS_OBJECTID_LOSTNFOUND)) {
					/* We only load some info, don't fiddle with directory structure */
					in->valid = 1;
					in->variantType = oh->type;

					in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
					in->win_atime[0] = oh->win_atime[0];
					in->win_ctime[0] = oh->win_ctime[0];
					in->win_mtime[0] = oh->win_mtime[0];
					in->win_atime[1] = oh->win_atime[1];
					in->win_ctime[1] = oh->win_ctime[1];
					in->win_mtime[1] = oh->win_mtime[1];
#else
					in->yst_uid = oh->yst_uid;
					in->yst_gid = oh->yst_gid;
					in->yst_atime = oh->yst_atime;
					in->yst_mtime = oh->yst_mtime;
					in->yst_ctime = oh->yst_ctime;
					in->yst_rdev = oh->yst_rdev;
#endif
					in->chunkId = chunk;

				} else if (in && !in->valid) {
					/* we need to load this info */

					in->valid = 1;
					in->variantType = oh->type;

					in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
					in->win_atime[0] = oh->win_atime[0];
					in->win_ctime[0] = oh->win_ctime[0];
					in->win_mtime[0] = oh->win_mtime[0];
					in->win_atime[1] = oh->win_atime[1];
					in->win_ctime[1] = oh->win_ctime[1];
					in->win_mtime[1] = oh->win_mtime[1];
#else
					in->yst_uid = oh->yst_uid;
					in->yst_gid = oh->yst_gid;
					in->yst_atime = oh->yst_atime;
					in->yst_mtime = oh->yst_mtime;
					in->yst_ctime = oh->yst_ctime;
					in->yst_rdev = oh->yst_rdev;
#endif
					in->chunkId = chunk;

					yaffs_SetObjectName(in, oh->name);
					in->dirty = 0;

					/* directory stuff...
					 * hook up to parent
					 */

					parent =
					    yaffs_FindOrCreateObjectByNumber
					    (dev, oh->parentObjectId,
					     YAFFS_OBJECT_TYPE_DIRECTORY);
					if (parent->variantType ==
					    YAFFS_OBJECT_TYPE_UNKNOWN) {
						/* Set up as a directory */
						parent->variantType =
						    YAFFS_OBJECT_TYPE_DIRECTORY;
						INIT_LIST_HEAD(&parent->variant.
							       directoryVariant.
							       children);
					} else if (parent->variantType !=
						   YAFFS_OBJECT_TYPE_DIRECTORY)
					{
						/* Hoosterman, another problem....
						 * We're trying to use a non-directory as a directory
						 */

						T(YAFFS_TRACE_ERROR,
						  (TSTR
						   ("yaffs tragedy: attempting to use non-directory as"
						    " a directory in scan. Put in lost+found."
						    TENDSTR)));
						parent = dev->lostNFoundDir;
					}

					yaffs_AddObjectToDirectory(parent, in);

					if (0 && (parent == dev->deletedDir ||
						  parent == dev->unlinkedDir)) {
						in->deleted = 1;	/* If it is unlinked at start up then it wants deleting */
						dev->nDeletedFiles++;
					}
					/* Note re hardlinks.
					 * Since we might scan a hardlink before its equivalent object is scanned
					 * we put them all in a list.
					 * After scanning is complete, we should have all the objects, so we run through this
					 * list and fix up all the chains.
					 */

					switch (in->variantType) {
					case YAFFS_OBJECT_TYPE_UNKNOWN:
						/* Todo got a problem */
						break;
					case YAFFS_OBJECT_TYPE_FILE:
						if (dev->isYaffs2
						    && oh->isShrink) {
							/* Prune back the shrunken chunks */
							yaffs_PruneResizedChunks
							    (in, oh->fileSize);
							/* Mark the block as having a shrinkHeader */
							bi->hasShrinkHeader = 1;
						}

						if (dev->useHeaderFileSize)

							in->variant.fileVariant.
							    fileSize =
							    oh->fileSize;

						break;
					case YAFFS_OBJECT_TYPE_HARDLINK:
						in->variant.hardLinkVariant.
						    equivalentObjectId =
						    oh->equivalentObjectId;
						in->hardLinks.next =
						    (struct list_head *)
						    hardList;
						hardList = in;
						break;
					case YAFFS_OBJECT_TYPE_DIRECTORY:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SPECIAL:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SYMLINK:
						in->variant.symLinkVariant.alias =
						    yaffs_CloneString(oh->alias);
						if(!in->variant.symLinkVariant.alias)
							alloc_failed = 1;
						break;
					}

					if (parent == dev->deletedDir) {
						yaffs_DestroyObject(in);
						bi->hasShrinkHeader = 1;
					}
				}
			}
		}

		if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {
			/* If we got this far while scanning, then the block is fully allocated.*/
			state = YAFFS_BLOCK_STATE_FULL;
		}

		bi->blockState = state;

		/* Now let's see if it was dirty */
		if (bi->pagesInUse == 0 &&
		    !bi->hasShrinkHeader &&
		    bi->blockState == YAFFS_BLOCK_STATE_FULL) {
			yaffs_BlockBecameDirty(dev, blk);
		}

	}

	if (blockIndex) {
		YFREE(blockIndex);
	}


	/* Ok, we've done all the scanning.
	 * Fix up the hard link chains.
	 * We should now have scanned all the objects, now it's time to add these
	 * hardlinks.
	 */

	yaffs_HardlinkFixup(dev,hardList);

	/* Handle the unlinked files. Since they were left in an unlinked state we should
	 * just delete them.
	 */
	{
		struct list_head *i;
		struct list_head *n;

		yaffs_Object *l;
		/* Soft delete all the unlinked files */
		list_for_each_safe(i, n,
				   &dev->unlinkedDir->variant.directoryVariant.
				   children) {
			if (i) {
				l = list_entry(i, yaffs_Object, siblings);
				yaffs_DestroyObject(l);
			}
		}
	}

	yaffs_ReleaseTempBuffer(dev, chunkData, __LINE__);

	if(alloc_failed){
		return YAFFS_FAIL;
	}

	T(YAFFS_TRACE_SCAN, (TSTR("yaffs_Scan ends" TENDSTR)));


	return YAFFS_OK;
}

static void yaffs_CheckObjectDetailsLoaded(yaffs_Object *in)
{
	__u8 *chunkData;
	yaffs_ObjectHeader *oh;
	yaffs_Device *dev = in->myDev;
	yaffs_ExtendedTags tags;
	int result;
	int alloc_failed = 0;

	if(!in)
		return;

#if 0
	T(YAFFS_TRACE_SCAN,(TSTR("details for object %d %s loaded" TENDSTR),
		in->objectId,
		in->lazyLoaded ? "not yet" : "already"));
#endif

	if(in->lazyLoaded){
		in->lazyLoaded = 0;
		chunkData = yaffs_GetTempBuffer(dev, __LINE__);

		result = yaffs_ReadChunkWithTagsFromNAND(dev,in->chunkId,chunkData,&tags);
		oh = (yaffs_ObjectHeader *) chunkData;

		in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
		in->win_atime[0] = oh->win_atime[0];
		in->win_ctime[0] = oh->win_ctime[0];
		in->win_mtime[0] = oh->win_mtime[0];
		in->win_atime[1] = oh->win_atime[1];
		in->win_ctime[1] = oh->win_ctime[1];
		in->win_mtime[1] = oh->win_mtime[1];
#else
		in->yst_uid = oh->yst_uid;
		in->yst_gid = oh->yst_gid;
		in->yst_atime = oh->yst_atime;
		in->yst_mtime = oh->yst_mtime;
		in->yst_ctime = oh->yst_ctime;
		in->yst_rdev = oh->yst_rdev;

#endif
		yaffs_SetObjectName(in, oh->name);

		if(in->variantType == YAFFS_OBJECT_TYPE_SYMLINK){
			 in->variant.symLinkVariant.alias =
						    yaffs_CloneString(oh->alias);
			if(!in->variant.symLinkVariant.alias)
				alloc_failed = 1; /* Not returned to caller */
		}

		yaffs_ReleaseTempBuffer(dev,chunkData, __LINE__);
	}
}

static int yaffs_ScanBackwards(yaffs_Device * dev)
{
	yaffs_ExtendedTags tags;
	int blk;
	int blockIterator;
	int startIterator;
	int endIterator;
	int nBlocksToScan = 0;

	int chunk;
	int result;
	int c;
	int deleted;
	yaffs_BlockState state;
	yaffs_Object *hardList = NULL;
	yaffs_BlockInfo *bi;
	int sequenceNumber;
	yaffs_ObjectHeader *oh;
	yaffs_Object *in;
	yaffs_Object *parent;
	int nBlocks = dev->internalEndBlock - dev->internalStartBlock + 1;
	int itsUnlinked;
	__u8 *chunkData;

	int fileSize;
	int isShrink;
	int foundChunksInBlock;
	int equivalentObjectId;
	int alloc_failed = 0;


	yaffs_BlockIndex *blockIndex = NULL;
	int altBlockIndex = 0;

	if (!dev->isYaffs2) {
		T(YAFFS_TRACE_SCAN,
		  (TSTR("yaffs_ScanBackwards is only for YAFFS2!" TENDSTR)));
		return YAFFS_FAIL;
	}

	T(YAFFS_TRACE_SCAN,
	  (TSTR
	   ("yaffs_ScanBackwards starts  intstartblk %d intendblk %d..."
	    TENDSTR), dev->internalStartBlock, dev->internalEndBlock));


	dev->sequenceNumber = YAFFS_LOWEST_SEQUENCE_NUMBER;

	blockIndex = YMALLOC(nBlocks * sizeof(yaffs_BlockIndex));

	if(!blockIndex) {
		blockIndex = YMALLOC_ALT(nBlocks * sizeof(yaffs_BlockIndex));
		altBlockIndex = 1;
	}

	if(!blockIndex) {
		T(YAFFS_TRACE_SCAN,
		  (TSTR("yaffs_Scan() could not allocate block index!" TENDSTR)));
		return YAFFS_FAIL;
	}

	dev->blocksInCheckpoint = 0;

	chunkData = yaffs_GetTempBuffer(dev, __LINE__);

	/* Scan all the blocks to determine their state */
	for (blk = dev->internalStartBlock; blk <= dev->internalEndBlock; blk++) {
		bi = yaffs_GetBlockInfo(dev, blk);
		yaffs_ClearChunkBits(dev, blk);
		bi->pagesInUse = 0;
		bi->softDeletions = 0;

		yaffs_QueryInitialBlockState(dev, blk, &state, &sequenceNumber);

		bi->blockState = state;
		bi->sequenceNumber = sequenceNumber;

		if(bi->sequenceNumber == YAFFS_SEQUENCE_CHECKPOINT_DATA)
			bi->blockState = state = YAFFS_BLOCK_STATE_CHECKPOINT;

		T(YAFFS_TRACE_SCAN_DEBUG,
		  (TSTR("Block scanning block %d state %d seq %d" TENDSTR), blk,
		   state, sequenceNumber));


		if(state == YAFFS_BLOCK_STATE_CHECKPOINT){
			dev->blocksInCheckpoint++;

		} else if (state == YAFFS_BLOCK_STATE_DEAD) {
			T(YAFFS_TRACE_BAD_BLOCKS,
			  (TSTR("block %d is bad" TENDSTR), blk));
		} else if (state == YAFFS_BLOCK_STATE_EMPTY) {
			T(YAFFS_TRACE_SCAN_DEBUG,
			  (TSTR("Block empty " TENDSTR)));
			dev->nErasedBlocks++;
			dev->nFreeChunks += dev->nChunksPerBlock;
		} else if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {

			/* Determine the highest sequence number */
			if (dev->isYaffs2 &&
			    sequenceNumber >= YAFFS_LOWEST_SEQUENCE_NUMBER &&
			    sequenceNumber < YAFFS_HIGHEST_SEQUENCE_NUMBER) {

				blockIndex[nBlocksToScan].seq = sequenceNumber;
				blockIndex[nBlocksToScan].block = blk;

				nBlocksToScan++;

				if (sequenceNumber >= dev->sequenceNumber) {
					dev->sequenceNumber = sequenceNumber;
				}
			} else if (dev->isYaffs2) {
				/* TODO: Nasty sequence number! */
				T(YAFFS_TRACE_SCAN,
				  (TSTR
				   ("Block scanning block %d has bad sequence number %d"
				    TENDSTR), blk, sequenceNumber));

			}
		}
	}

	T(YAFFS_TRACE_SCAN,
	(TSTR("%d blocks to be sorted..." TENDSTR), nBlocksToScan));



	YYIELD();

	/* Sort the blocks */
#ifndef CONFIG_YAFFS_USE_OWN_SORT
	{
		/* Use qsort now. */
		yaffs_qsort(blockIndex, nBlocksToScan, sizeof(yaffs_BlockIndex), ybicmp);
	}
#else
	{
	 	/* Dungy old bubble sort... */

		yaffs_BlockIndex temp;
		int i;
		int j;

		for (i = 0; i < nBlocksToScan; i++)
			for (j = i + 1; j < nBlocksToScan; j++)
				if (blockIndex[i].seq > blockIndex[j].seq) {
					temp = blockIndex[j];
					blockIndex[j] = blockIndex[i];
					blockIndex[i] = temp;
				}
	}
#endif

	YYIELD();

    	T(YAFFS_TRACE_SCAN, (TSTR("...done" TENDSTR)));

	/* Now scan the blocks looking at the data. */
	startIterator = 0;
	endIterator = nBlocksToScan - 1;
	T(YAFFS_TRACE_SCAN_DEBUG,
	  (TSTR("%d blocks to be scanned" TENDSTR), nBlocksToScan));

	/* For each block.... backwards */
	for (blockIterator = endIterator; !alloc_failed && blockIterator >= startIterator;
	     blockIterator--) {
	        /* Cooperative multitasking! This loop can run for so
		   long that watchdog timers expire. */
	        YYIELD();

		/* get the block to scan in the correct order */
		blk = blockIndex[blockIterator].block;

		bi = yaffs_GetBlockInfo(dev, blk);


		state = bi->blockState;

		deleted = 0;

		/* For each chunk in each block that needs scanning.... */
		foundChunksInBlock = 0;
		for (c = dev->nChunksPerBlock - 1;
		     !alloc_failed && c >= 0 &&
		     (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING ||
		      state == YAFFS_BLOCK_STATE_ALLOCATING); c--) {
			/* Scan backwards...
			 * Read the tags and decide what to do
			 */

			chunk = blk * dev->nChunksPerBlock + c;

			result = yaffs_ReadChunkWithTagsFromNAND(dev, chunk, NULL,
							&tags);

			/* Let's have a good look at this chunk... */

			if (!tags.chunkUsed) {
				/* An unassigned chunk in the block.
				 * If there are used chunks after this one, then
				 * it is a chunk that was skipped due to failing the erased
				 * check. Just skip it so that it can be deleted.
				 * But, more typically, We get here when this is an unallocated
				 * chunk and his means that either the block is empty or
				 * this is the one being allocated from
				 */

				if(foundChunksInBlock)
				{
					/* This is a chunk that was skipped due to failing the erased check */

				} else if (c == 0) {
					/* We're looking at the first chunk in the block so the block is unused */
					state = YAFFS_BLOCK_STATE_EMPTY;
					dev->nErasedBlocks++;
				} else {
					if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING ||
					    state == YAFFS_BLOCK_STATE_ALLOCATING) {
					    	if(dev->sequenceNumber == bi->sequenceNumber) {
							/* this is the block being allocated from */

							T(YAFFS_TRACE_SCAN,
							  (TSTR
							   (" Allocating from %d %d"
							    TENDSTR), blk, c));

							state = YAFFS_BLOCK_STATE_ALLOCATING;
							dev->allocationBlock = blk;
							dev->allocationPage = c;
							dev->allocationBlockFinder = blk;
						}
						else {
							/* This is a partially written block that is not
							 * the current allocation block. This block must have
							 * had a write failure, so set up for retirement.
							 */

							 bi->needsRetiring = 1;
							 bi->gcPrioritise = 1;

							 T(YAFFS_TRACE_ALWAYS,
							 (TSTR("Partially written block %d being set for retirement" TENDSTR),
							 blk));
						}

					}

				}

				dev->nFreeChunks++;

			} else if (tags.chunkId > 0) {
				/* chunkId > 0 so it is a data chunk... */
				unsigned int endpos;
				__u32 chunkBase =
				    (tags.chunkId - 1) * dev->nDataBytesPerChunk;

				foundChunksInBlock = 1;


				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				in = yaffs_FindOrCreateObjectByNumber(dev,
								      tags.
								      objectId,
								      YAFFS_OBJECT_TYPE_FILE);
				if(!in){
					/* Out of memory */
					alloc_failed = 1;
				}

				if (in &&
				    in->variantType == YAFFS_OBJECT_TYPE_FILE
				    && chunkBase <
				    in->variant.fileVariant.shrinkSize) {
					/* This has not been invalidated by a resize */
					if(!yaffs_PutChunkIntoFile(in, tags.chunkId,
							       chunk, -1)){
						alloc_failed = 1;
					}

					/* File size is calculated by looking at the data chunks if we have not
					 * seen an object header yet. Stop this practice once we find an object header.
					 */
					endpos =
					    (tags.chunkId -
					     1) * dev->nDataBytesPerChunk +
					    tags.byteCount;

					if (!in->valid &&	/* have not got an object header yet */
					    in->variant.fileVariant.
					    scannedFileSize < endpos) {
						in->variant.fileVariant.
						    scannedFileSize = endpos;
						in->variant.fileVariant.
						    fileSize =
						    in->variant.fileVariant.
						    scannedFileSize;
					}

				} else if(in) {
					/* This chunk has been invalidated by a resize, so delete */
					yaffs_DeleteChunk(dev, chunk, 1, __LINE__);

				}
			} else {
				/* chunkId == 0, so it is an ObjectHeader.
				 * Thus, we read in the object header and make the object
				 */
				foundChunksInBlock = 1;

				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				oh = NULL;
				in = NULL;

				if (tags.extraHeaderInfoAvailable) {
					in = yaffs_FindOrCreateObjectByNumber
					    (dev, tags.objectId,
					     tags.extraObjectType);
				}

				if (!in ||
#ifdef CONFIG_YAFFS_DISABLE_LAZY_LOAD
				    !in->valid ||
#endif
				    tags.extraShadows ||
				    (!in->valid &&
				    (tags.objectId == YAFFS_OBJECTID_ROOT ||
				     tags.objectId == YAFFS_OBJECTID_LOSTNFOUND))
				    ) {

					/* If we don't have  valid info then we need to read the chunk
					 * TODO In future we can probably defer reading the chunk and
					 * living with invalid data until needed.
					 */

					result = yaffs_ReadChunkWithTagsFromNAND(dev,
									chunk,
									chunkData,
									NULL);

					oh = (yaffs_ObjectHeader *) chunkData;

					if (!in)
						in = yaffs_FindOrCreateObjectByNumber(dev, tags.objectId, oh->type);

				}

				if (!in) {
					/* TODO Hoosterman we have a problem! */
					T(YAFFS_TRACE_ERROR,
					  (TSTR
					   ("yaffs tragedy: Could not make object for object  %d  "
					    "at chunk %d during scan"
					    TENDSTR), tags.objectId, chunk));

				}

				if (in->valid) {
					/* We have already filled this one.
					 * We have a duplicate that will be discarded, but
					 * we first have to suck out resize info if it is a file.
					 */

					if ((in->variantType == YAFFS_OBJECT_TYPE_FILE) &&
					     ((oh &&
					       oh-> type == YAFFS_OBJECT_TYPE_FILE)||
					      (tags.extraHeaderInfoAvailable  &&
					       tags.extraObjectType == YAFFS_OBJECT_TYPE_FILE))
					    ) {
						__u32 thisSize =
						    (oh) ? oh->fileSize : tags.
						    extraFileLength;
						__u32 parentObjectId =
						    (oh) ? oh->
						    parentObjectId : tags.
						    extraParentObjectId;
						unsigned isShrink =
						    (oh) ? oh->isShrink : tags.
						    extraIsShrinkHeader;

						/* If it is deleted (unlinked at start also means deleted)
						 * we treat the file size as being zeroed at this point.
						 */
						if (parentObjectId ==
						    YAFFS_OBJECTID_DELETED
						    || parentObjectId ==
						    YAFFS_OBJECTID_UNLINKED) {
							thisSize = 0;
							isShrink = 1;
						}

						if (isShrink &&
						    in->variant.fileVariant.
						    shrinkSize > thisSize) {
							in->variant.fileVariant.
							    shrinkSize =
							    thisSize;
						}

						if (isShrink) {
							bi->hasShrinkHeader = 1;
						}

					}
					/* Use existing - destroy this one. */
					yaffs_DeleteChunk(dev, chunk, 1, __LINE__);

				}

				if (!in->valid &&
				    (tags.objectId == YAFFS_OBJECTID_ROOT ||
				     tags.objectId ==
				     YAFFS_OBJECTID_LOSTNFOUND)) {
					/* We only load some info, don't fiddle with directory structure */
					in->valid = 1;

					if(oh) {
						in->variantType = oh->type;

						in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
						in->win_atime[0] = oh->win_atime[0];
						in->win_ctime[0] = oh->win_ctime[0];
						in->win_mtime[0] = oh->win_mtime[0];
						in->win_atime[1] = oh->win_atime[1];
						in->win_ctime[1] = oh->win_ctime[1];
						in->win_mtime[1] = oh->win_mtime[1];
#else
						in->yst_uid = oh->yst_uid;
						in->yst_gid = oh->yst_gid;
						in->yst_atime = oh->yst_atime;
						in->yst_mtime = oh->yst_mtime;
						in->yst_ctime = oh->yst_ctime;
						in->yst_rdev = oh->yst_rdev;

#endif
					} else {
						in->variantType = tags.extraObjectType;
						in->lazyLoaded = 1;
					}

					in->chunkId = chunk;

				} else if (!in->valid) {
					/* we need to load this info */

					in->valid = 1;
					in->chunkId = chunk;

					if(oh) {
						in->variantType = oh->type;

						in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
						in->win_atime[0] = oh->win_atime[0];
						in->win_ctime[0] = oh->win_ctime[0];
						in->win_mtime[0] = oh->win_mtime[0];
						in->win_atime[1] = oh->win_atime[1];
						in->win_ctime[1] = oh->win_ctime[1];
						in->win_mtime[1] = oh->win_mtime[1];
#else
						in->yst_uid = oh->yst_uid;
						in->yst_gid = oh->yst_gid;
						in->yst_atime = oh->yst_atime;
						in->yst_mtime = oh->yst_mtime;
						in->yst_ctime = oh->yst_ctime;
						in->yst_rdev = oh->yst_rdev;
#endif

						if (oh->shadowsObject > 0)
							yaffs_HandleShadowedObject(dev,
									   oh->
									   shadowsObject,
									   1);


						yaffs_SetObjectName(in, oh->name);
						parent =
						    yaffs_FindOrCreateObjectByNumber
					    		(dev, oh->parentObjectId,
					     		 YAFFS_OBJECT_TYPE_DIRECTORY);

						 fileSize = oh->fileSize;
 						 isShrink = oh->isShrink;
						 equivalentObjectId = oh->equivalentObjectId;

					}
					else {
						in->variantType = tags.extraObjectType;
						parent =
						    yaffs_FindOrCreateObjectByNumber
					    		(dev, tags.extraParentObjectId,
					     		 YAFFS_OBJECT_TYPE_DIRECTORY);
						 fileSize = tags.extraFileLength;
						 isShrink = tags.extraIsShrinkHeader;
						 equivalentObjectId = tags.extraEquivalentObjectId;
						in->lazyLoaded = 1;

					}
					in->dirty = 0;

					/* directory stuff...
					 * hook up to parent
					 */

					if (parent->variantType ==
					    YAFFS_OBJECT_TYPE_UNKNOWN) {
						/* Set up as a directory */
						parent->variantType =
						    YAFFS_OBJECT_TYPE_DIRECTORY;
						INIT_LIST_HEAD(&parent->variant.
							       directoryVariant.
							       children);
					} else if (parent->variantType !=
						   YAFFS_OBJECT_TYPE_DIRECTORY)
					{
						/* Hoosterman, another problem....
						 * We're trying to use a non-directory as a directory
						 */

						T(YAFFS_TRACE_ERROR,
						  (TSTR
						   ("yaffs tragedy: attempting to use non-directory as"
						    " a directory in scan. Put in lost+found."
						    TENDSTR)));
						parent = dev->lostNFoundDir;
					}

					yaffs_AddObjectToDirectory(parent, in);

					itsUnlinked = (parent == dev->deletedDir) ||
						      (parent == dev->unlinkedDir);

					if (isShrink) {
						/* Mark the block as having a shrinkHeader */
						bi->hasShrinkHeader = 1;
					}

					/* Note re hardlinks.
					 * Since we might scan a hardlink before its equivalent object is scanned
					 * we put them all in a list.
					 * After scanning is complete, we should have all the objects, so we run
					 * through this list and fix up all the chains.
					 */

					switch (in->variantType) {
					case YAFFS_OBJECT_TYPE_UNKNOWN:
						/* Todo got a problem */
						break;
					case YAFFS_OBJECT_TYPE_FILE:

						if (in->variant.fileVariant.
						    scannedFileSize < fileSize) {
							/* This covers the case where the file size is greater
							 * than where the data is
							 * This will happen if the file is resized to be larger
							 * than its current data extents.
							 */
							in->variant.fileVariant.fileSize = fileSize;
							in->variant.fileVariant.scannedFileSize =
							    in->variant.fileVariant.fileSize;
						}

						if (isShrink &&
						    in->variant.fileVariant.shrinkSize > fileSize) {
							in->variant.fileVariant.shrinkSize = fileSize;
						}

						break;
					case YAFFS_OBJECT_TYPE_HARDLINK:
						if(!itsUnlinked) {
						  in->variant.hardLinkVariant.equivalentObjectId =
						    equivalentObjectId;
						  in->hardLinks.next =
						    (struct list_head *) hardList;
						  hardList = in;
						}
						break;
					case YAFFS_OBJECT_TYPE_DIRECTORY:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SPECIAL:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SYMLINK:
						if(oh){
						   in->variant.symLinkVariant.alias =
						    yaffs_CloneString(oh->
								      alias);
						   if(!in->variant.symLinkVariant.alias)
						   	alloc_failed = 1;
						}
						break;
					}

				}

			}

		} /* End of scanning for each chunk */

		if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {
			/* If we got this far while scanning, then the block is fully allocated. */
			state = YAFFS_BLOCK_STATE_FULL;
		}

		bi->blockState = state;

		/* Now let's see if it was dirty */
		if (bi->pagesInUse == 0 &&
		    !bi->hasShrinkHeader &&
		    bi->blockState == YAFFS_BLOCK_STATE_FULL) {
			yaffs_BlockBecameDirty(dev, blk);
		}

	}

	if (altBlockIndex)
		YFREE_ALT(blockIndex);
	else
		YFREE(blockIndex);

	/* Ok, we've done all the scanning.
	 * Fix up the hard link chains.
	 * We should now have scanned all the objects, now it's time to add these
	 * hardlinks.
	 */
	yaffs_HardlinkFixup(dev,hardList);


	/*
	*  Sort out state of unlinked and deleted objects.
	*/
	{
		struct list_head *i;
		struct list_head *n;

		yaffs_Object *l;

		/* Soft delete all the unlinked files */
		list_for_each_safe(i, n,
				   &dev->unlinkedDir->variant.directoryVariant.
				   children) {
			if (i) {
				l = list_entry(i, yaffs_Object, siblings);
				yaffs_DestroyObject(l);
			}
		}

		/* Soft delete all the deletedDir files */
		list_for_each_safe(i, n,
				   &dev->deletedDir->variant.directoryVariant.
				   children) {
			if (i) {
				l = list_entry(i, yaffs_Object, siblings);
				yaffs_DestroyObject(l);

			}
		}
	}

	yaffs_ReleaseTempBuffer(dev, chunkData, __LINE__);

	if(alloc_failed){
		return YAFFS_FAIL;
	}

	T(YAFFS_TRACE_SCAN, (TSTR("yaffs_ScanBackwards ends" TENDSTR)));

	return YAFFS_OK;
}

/*------------------------------  Directory Functions ----------------------------- */

static void yaffs_RemoveObjectFromDirectory(yaffs_Object * obj)
{
	yaffs_Device *dev = obj->myDev;

	if(dev && dev->removeObjectCallback)
		dev->removeObjectCallback(obj);

	list_del_init(&obj->siblings);
	obj->parent = NULL;
}


static void yaffs_AddObjectToDirectory(yaffs_Object * directory,
				       yaffs_Object * obj)
{

	if (!directory) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: Trying to add an object to a null pointer directory"
		    TENDSTR)));
		YBUG();
	}
	if (directory->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: Trying to add an object to a non-directory"
		    TENDSTR)));
		YBUG();
	}

	if (obj->siblings.prev == NULL) {
		/* Not initialised */
		INIT_LIST_HEAD(&obj->siblings);

	} else if (!list_empty(&obj->siblings)) {
		/* If it is holed up somewhere else, un hook it */
		yaffs_RemoveObjectFromDirectory(obj);
	}
	/* Now add it */
	list_add(&obj->siblings, &directory->variant.directoryVariant.children);
	obj->parent = directory;

	if (directory == obj->myDev->unlinkedDir
	    || directory == obj->myDev->deletedDir) {
		obj->unlinked = 1;
		obj->myDev->nUnlinkedFiles++;
		obj->renameAllowed = 0;
	}
}

yaffs_Object *yaffs_FindObjectByName(yaffs_Object * directory,
				     const YCHAR * name)
{
	int sum;

	struct list_head *i;
	YCHAR buffer[YAFFS_MAX_NAME_LENGTH + 1];

	yaffs_Object *l;

	if (!name) {
		return NULL;
	}

	if (!directory) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: yaffs_FindObjectByName: null pointer directory"
		    TENDSTR)));
		YBUG();
	}
	if (directory->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: yaffs_FindObjectByName: non-directory" TENDSTR)));
		YBUG();
	}

	sum = yaffs_CalcNameSum(name);

	list_for_each(i, &directory->variant.directoryVariant.children) {
		if (i) {
			l = list_entry(i, yaffs_Object, siblings);

			yaffs_CheckObjectDetailsLoaded(l);

			/* Special case for lost-n-found */
			if (l->objectId == YAFFS_OBJECTID_LOSTNFOUND) {
				if (yaffs_strcmp(name, YAFFS_LOSTNFOUND_NAME) == 0) {
					return l;
				}
			} else if (yaffs_SumCompare(l->sum, sum) || l->chunkId <= 0)
			{
				/* LostnFound cunk called Objxxx
				 * Do a real check
				 */
				yaffs_GetObjectName(l, buffer,
						    YAFFS_MAX_NAME_LENGTH);
				if (yaffs_strncmp(name, buffer,YAFFS_MAX_NAME_LENGTH) == 0) {
					return l;
				}

			}
		}
	}

	return NULL;
}


#if 0
int yaffs_ApplyToDirectoryChildren(yaffs_Object * theDir,
				   int (*fn) (yaffs_Object *))
{
	struct list_head *i;
	yaffs_Object *l;

	if (!theDir) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: yaffs_FindObjectByName: null pointer directory"
		    TENDSTR)));
		YBUG();
	}
	if (theDir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: yaffs_FindObjectByName: non-directory" TENDSTR)));
		YBUG();
	}

	list_for_each(i, &theDir->variant.directoryVariant.children) {
		if (i) {
			l = list_entry(i, yaffs_Object, siblings);
			if (l && !fn(l)) {
				return YAFFS_FAIL;
			}
		}
	}

	return YAFFS_OK;

}
#endif

/* GetEquivalentObject dereferences any hard links to get to the
 * actual object.
 */

yaffs_Object *yaffs_GetEquivalentObject(yaffs_Object * obj)
{
	if (obj && obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK) {
		/* We want the object id of the equivalent object, not this one */
		obj = obj->variant.hardLinkVariant.equivalentObject;
		yaffs_CheckObjectDetailsLoaded(obj);
	}
	return obj;

}

int yaffs_GetObjectName(yaffs_Object * obj, YCHAR * name, int buffSize)
{
	memset(name, 0, buffSize * sizeof(YCHAR));

	yaffs_CheckObjectDetailsLoaded(obj);

	if (obj->objectId == YAFFS_OBJECTID_LOSTNFOUND) {
		yaffs_strncpy(name, YAFFS_LOSTNFOUND_NAME, buffSize - 1);
	} else if (obj->chunkId <= 0) {
		YCHAR locName[20];
		/* make up a name */
		yaffs_sprintf(locName, _Y("%s%d"), YAFFS_LOSTNFOUND_PREFIX,
			      obj->objectId);
		yaffs_strncpy(name, locName, buffSize - 1);

	}
#ifdef CONFIG_YAFFS_SHORT_NAMES_IN_RAM
	else if (obj->shortName[0]) {
		yaffs_strcpy(name, obj->shortName);
	}
#endif
	else {
		int result;
		__u8 *buffer = yaffs_GetTempBuffer(obj->myDev, __LINE__);

		yaffs_ObjectHeader *oh = (yaffs_ObjectHeader *) buffer;

		memset(buffer, 0, obj->myDev->nDataBytesPerChunk);

		if (obj->chunkId >= 0) {
			result = yaffs_ReadChunkWithTagsFromNAND(obj->myDev,
							obj->chunkId, buffer,
							NULL);
		}
		yaffs_strncpy(name, oh->name, buffSize - 1);

		yaffs_ReleaseTempBuffer(obj->myDev, buffer, __LINE__);
	}

	return yaffs_strlen(name);
}

int yaffs_GetObjectFileLength(yaffs_Object * obj)
{

	/* Dereference any hard linking */
	obj = yaffs_GetEquivalentObject(obj);

	if (obj->variantType == YAFFS_OBJECT_TYPE_FILE) {
		return obj->variant.fileVariant.fileSize;
	}
	if (obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK) {
		return yaffs_strlen(obj->variant.symLinkVariant.alias);
	} else {
		/* Only a directory should drop through to here */
		return obj->myDev->nDataBytesPerChunk;
	}
}

int yaffs_GetObjectLinkCount(yaffs_Object * obj)
{
	int count = 0;
	struct list_head *i;

	if (!obj->unlinked) {
		count++;	/* the object itself */
	}
	list_for_each(i, &obj->hardLinks) {
		count++;	/* add the hard links; */
	}
	return count;

}

int yaffs_GetObjectInode(yaffs_Object * obj)
{
	obj = yaffs_GetEquivalentObject(obj);

	return obj->objectId;
}

unsigned yaffs_GetObjectType(yaffs_Object * obj)
{
	obj = yaffs_GetEquivalentObject(obj);

	switch (obj->variantType) {
	case YAFFS_OBJECT_TYPE_FILE:
		return DT_REG;
		break;
	case YAFFS_OBJECT_TYPE_DIRECTORY:
		return DT_DIR;
		break;
	case YAFFS_OBJECT_TYPE_SYMLINK:
		return DT_LNK;
		break;
	case YAFFS_OBJECT_TYPE_HARDLINK:
		return DT_REG;
		break;
	case YAFFS_OBJECT_TYPE_SPECIAL:
		if (S_ISFIFO(obj->yst_mode))
			return DT_FIFO;
		if (S_ISCHR(obj->yst_mode))
			return DT_CHR;
		if (S_ISBLK(obj->yst_mode))
			return DT_BLK;
		if (S_ISSOCK(obj->yst_mode))
			return DT_SOCK;
	default:
		return DT_REG;
		break;
	}
}

YCHAR *yaffs_GetSymlinkAlias(yaffs_Object * obj)
{
	obj = yaffs_GetEquivalentObject(obj);
	if (obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK) {
		return yaffs_CloneString(obj->variant.symLinkVariant.alias);
	} else {
		return yaffs_CloneString(_Y(""));
	}
}

#ifndef CONFIG_YAFFS_WINCE

int yaffs_SetAttributes(yaffs_Object * obj, struct iattr *attr)
{
	unsigned int valid = attr->ia_valid;

	if (valid & ATTR_MODE)
		obj->yst_mode = attr->ia_mode;
	if (valid & ATTR_UID)
		obj->yst_uid = attr->ia_uid;
	if (valid & ATTR_GID)
		obj->yst_gid = attr->ia_gid;

	if (valid & ATTR_ATIME)
		obj->yst_atime = Y_TIME_CONVERT(attr->ia_atime);
	if (valid & ATTR_CTIME)
		obj->yst_ctime = Y_TIME_CONVERT(attr->ia_ctime);
	if (valid & ATTR_MTIME)
		obj->yst_mtime = Y_TIME_CONVERT(attr->ia_mtime);

	if (valid & ATTR_SIZE)
		yaffs_ResizeFile(obj, attr->ia_size);

	yaffs_UpdateObjectHeader(obj, NULL, 1, 0, 0);

	return YAFFS_OK;

}
int yaffs_GetAttributes(yaffs_Object * obj, struct iattr *attr)
{
	unsigned int valid = 0;

	attr->ia_mode = obj->yst_mode;
	valid |= ATTR_MODE;
	attr->ia_uid = obj->yst_uid;
	valid |= ATTR_UID;
	attr->ia_gid = obj->yst_gid;
	valid |= ATTR_GID;

	Y_TIME_CONVERT(attr->ia_atime) = obj->yst_atime;
	valid |= ATTR_ATIME;
	Y_TIME_CONVERT(attr->ia_ctime) = obj->yst_ctime;
	valid |= ATTR_CTIME;
	Y_TIME_CONVERT(attr->ia_mtime) = obj->yst_mtime;
	valid |= ATTR_MTIME;

	attr->ia_size = yaffs_GetFileSize(obj);
	valid |= ATTR_SIZE;

	attr->ia_valid = valid;

	return YAFFS_OK;

}

#endif

#if 0
int yaffs_DumpObject(yaffs_Object * obj)
{
	YCHAR name[257];

	yaffs_GetObjectName(obj, name, 256);

	T(YAFFS_TRACE_ALWAYS,
	  (TSTR
	   ("Object %d, inode %d \"%s\"\n dirty %d valid %d serial %d sum %d"
	    " chunk %d type %d size %d\n"
	    TENDSTR), obj->objectId, yaffs_GetObjectInode(obj), name,
	   obj->dirty, obj->valid, obj->serial, obj->sum, obj->chunkId,
	   yaffs_GetObjectType(obj), yaffs_GetObjectFileLength(obj)));

	return YAFFS_OK;
}
#endif

/*---------------------------- Initialisation code -------------------------------------- */

static int yaffs_CheckDevFunctions(const yaffs_Device * dev)
{

	/* Common functions, gotta have */
	if (!dev->eraseBlockInNAND || !dev->initialiseNAND)
		return 0;

#ifdef CONFIG_YAFFS_YAFFS2

	/* Can use the "with tags" style interface for yaffs1 or yaffs2 */
	if (dev->writeChunkWithTagsToNAND &&
	    dev->readChunkWithTagsFromNAND &&
	    !dev->writeChunkToNAND &&
	    !dev->readChunkFromNAND &&
	    dev->markNANDBlockBad && dev->queryNANDBlock)
		return 1;
#endif

	/* Can use the "spare" style interface for yaffs1 */
	if (!dev->isYaffs2 &&
	    !dev->writeChunkWithTagsToNAND &&
	    !dev->readChunkWithTagsFromNAND &&
	    dev->writeChunkToNAND &&
	    dev->readChunkFromNAND &&
	    !dev->markNANDBlockBad && !dev->queryNANDBlock)
		return 1;

	return 0;		/* bad */
}


static int yaffs_CreateInitialDirectories(yaffs_Device *dev)
{
	/* Initialise the unlinked, deleted, root and lost and found directories */

	dev->lostNFoundDir = dev->rootDir =  NULL;
	dev->unlinkedDir = dev->deletedDir = NULL;

	dev->unlinkedDir =
	    yaffs_CreateFakeDirectory(dev, YAFFS_OBJECTID_UNLINKED, S_IFDIR);

	dev->deletedDir =
	    yaffs_CreateFakeDirectory(dev, YAFFS_OBJECTID_DELETED, S_IFDIR);

	dev->rootDir =
	    yaffs_CreateFakeDirectory(dev, YAFFS_OBJECTID_ROOT,
				      YAFFS_ROOT_MODE | S_IFDIR);
	dev->lostNFoundDir =
	    yaffs_CreateFakeDirectory(dev, YAFFS_OBJECTID_LOSTNFOUND,
				      YAFFS_LOSTNFOUND_MODE | S_IFDIR);

	if(dev->lostNFoundDir && dev->rootDir && dev->unlinkedDir && dev->deletedDir){
		yaffs_AddObjectToDirectory(dev->rootDir, dev->lostNFoundDir);
		return YAFFS_OK;
	}

	return YAFFS_FAIL;
}

int yaffs_GutsInitialise(yaffs_Device * dev)
{
	int init_failed = 0;
	unsigned x;
	int bits;

	T(YAFFS_TRACE_TRACING, (TSTR("yaffs: yaffs_GutsInitialise()" TENDSTR)));

	/* Check stuff that must be set */

	if (!dev) {
		T(YAFFS_TRACE_ALWAYS, (TSTR("yaffs: Need a device" TENDSTR)));
		return YAFFS_FAIL;
	}

	dev->internalStartBlock = dev->startBlock;
	dev->internalEndBlock = dev->endBlock;
	dev->blockOffset = 0;
	dev->chunkOffset = 0;
	dev->nFreeChunks = 0;

	if (dev->startBlock == 0) {
		dev->internalStartBlock = dev->startBlock + 1;
		dev->internalEndBlock = dev->endBlock + 1;
		dev->blockOffset = 1;
		dev->chunkOffset = dev->nChunksPerBlock;
	}

	/* Check geometry parameters. */

	if ((dev->isYaffs2 && dev->nDataBytesPerChunk < 1024) ||
	    (!dev->isYaffs2 && dev->nDataBytesPerChunk != 512) ||
	     dev->nChunksPerBlock < 2 ||
	     dev->nReservedBlocks < 2 ||
	     dev->internalStartBlock <= 0 ||
	     dev->internalEndBlock <= 0 ||
	     dev->internalEndBlock <= (dev->internalStartBlock + dev->nReservedBlocks + 2)	// otherwise it is too small
	    ) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("yaffs: NAND geometry problems: chunk size %d, type is yaffs%s "
		    TENDSTR), dev->nDataBytesPerChunk, dev->isYaffs2 ? "2" : ""));
		return YAFFS_FAIL;
	}

	if (yaffs_InitialiseNAND(dev) != YAFFS_OK) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR("yaffs: InitialiseNAND failed" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Got the right mix of functions? */
	if (!yaffs_CheckDevFunctions(dev)) {
		/* Function missing */
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("yaffs: device function(s) missing or wrong\n" TENDSTR)));

		return YAFFS_FAIL;
	}

	/* This is really a compilation check. */
	if (!yaffs_CheckStructures()) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR("yaffs_CheckStructures failed\n" TENDSTR)));
		return YAFFS_FAIL;
	}

	if (dev->isMounted) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR("yaffs: device already mounted\n" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Finished with most checks. One or two more checks happen later on too. */

	dev->isMounted = 1;



	/* OK now calculate a few things for the device */

	/*
	 *  Calculate all the chunk size manipulation numbers:
	 */
	 /* Start off assuming it is a power of 2 */
	 dev->chunkShift = ShiftDiv(dev->nDataBytesPerChunk);
	 dev->chunkMask = (1<<dev->chunkShift) - 1;

	 if(dev->nDataBytesPerChunk == (dev->chunkMask + 1)){
	 	/* Yes it is a power of 2, disable crumbs */
		dev->crumbMask = 0;
		dev->crumbShift = 0;
		dev->crumbsPerChunk = 0;
	 } else {
	 	/* Not a power of 2, use crumbs instead */
		dev->crumbShift = ShiftDiv(sizeof(yaffs_PackedTags2TagsPart));
		dev->crumbMask = (1<<dev->crumbShift)-1;
		dev->crumbsPerChunk = dev->nDataBytesPerChunk/(1 << dev->crumbShift);
		dev->chunkShift = 0;
		dev->chunkMask = 0;
	}


	/*
	 * Calculate chunkGroupBits.
	 * We need to find the next power of 2 > than internalEndBlock
	 */

	x = dev->nChunksPerBlock * (dev->internalEndBlock + 1);

	bits = ShiftsGE(x);

	/* Set up tnode width if wide tnodes are enabled. */
	if(!dev->wideTnodesDisabled){
		/* bits must be even so that we end up with 32-bit words */
		if(bits & 1)
			bits++;
		if(bits < 16)
			dev->tnodeWidth = 16;
		else
			dev->tnodeWidth = bits;
	}
	else
		dev->tnodeWidth = 16;

	dev->tnodeMask = (1<<dev->tnodeWidth)-1;

	/* Level0 Tnodes are 16 bits or wider (if wide tnodes are enabled),
	 * so if the bitwidth of the
	 * chunk range we're using is greater than 16 we need
	 * to figure out chunk shift and chunkGroupSize
	 */

	if (bits <= dev->tnodeWidth)
		dev->chunkGroupBits = 0;
	else
		dev->chunkGroupBits = bits - dev->tnodeWidth;


	dev->chunkGroupSize = 1 << dev->chunkGroupBits;

	if (dev->nChunksPerBlock < dev->chunkGroupSize) {
		/* We have a problem because the soft delete won't work if
		 * the chunk group size > chunks per block.
		 * This can be remedied by using larger "virtual blocks".
		 */
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR("yaffs: chunk group too large\n" TENDSTR)));

		return YAFFS_FAIL;
	}

	/* OK, we've finished verifying the device, lets continue with initialisation */

	/* More device initialisation */
	dev->garbageCollections = 0;
	dev->passiveGarbageCollections = 0;
	dev->currentDirtyChecker = 0;
	dev->bufferedBlock = -1;
	dev->doingBufferedBlockRewrite = 0;
	dev->nDeletedFiles = 0;
	dev->nBackgroundDeletions = 0;
	dev->nUnlinkedFiles = 0;
	dev->eccFixed = 0;
	dev->eccUnfixed = 0;
	dev->tagsEccFixed = 0;
	dev->tagsEccUnfixed = 0;
	dev->nErasureFailures = 0;
	dev->nErasedBlocks = 0;
	dev->isDoingGC = 0;
	dev->hasPendingPrioritisedGCs = 1; /* Assume the worst for now, will get fixed on first GC */

	/* Initialise temporary buffers and caches. */
	if(!yaffs_InitialiseTempBuffers(dev))
		init_failed = 1;

	dev->srCache = NULL;
	dev->gcCleanupList = NULL;


	if (!init_failed &&
	    dev->nShortOpCaches > 0) {
		int i;
		__u8 *buf;
		int srCacheBytes = dev->nShortOpCaches * sizeof(yaffs_ChunkCache);

		if (dev->nShortOpCaches > YAFFS_MAX_SHORT_OP_CACHES) {
			dev->nShortOpCaches = YAFFS_MAX_SHORT_OP_CACHES;
		}

		buf = dev->srCache =  YMALLOC(srCacheBytes);

		if(dev->srCache)
			memset(dev->srCache,0,srCacheBytes);

		for (i = 0; i < dev->nShortOpCaches && buf; i++) {
			dev->srCache[i].object = NULL;
			dev->srCache[i].lastUse = 0;
			dev->srCache[i].dirty = 0;
			dev->srCache[i].data = buf = YMALLOC_DMA(dev->nDataBytesPerChunk);
		}
		if(!buf)
			init_failed = 1;

		dev->srLastUse = 0;
	}

	dev->cacheHits = 0;

	if(!init_failed){
		dev->gcCleanupList = YMALLOC(dev->nChunksPerBlock * sizeof(__u32));
		if(!dev->gcCleanupList)
			init_failed = 1;
	}

	if (dev->isYaffs2) {
		dev->useHeaderFileSize = 1;
	}
	if(!init_failed && !yaffs_InitialiseBlocks(dev))
		init_failed = 1;

	yaffs_InitialiseTnodes(dev);
	yaffs_InitialiseObjects(dev);

	if(!init_failed && !yaffs_CreateInitialDirectories(dev))
		init_failed = 1;


	if(!init_failed){
		/* Now scan the flash. */
		if (dev->isYaffs2) {
			if(yaffs_CheckpointRestore(dev)) {
				T(YAFFS_TRACE_ALWAYS,
				  (TSTR("yaffs: restored from checkpoint" TENDSTR)));
			} else {

				/* Clean up the mess caused by an aborted checkpoint load
				 * and scan backwards.
				 */
				yaffs_DeinitialiseBlocks(dev);
				yaffs_DeinitialiseTnodes(dev);
				yaffs_DeinitialiseObjects(dev);


				dev->nErasedBlocks = 0;
				dev->nFreeChunks = 0;
				dev->allocationBlock = -1;
				dev->allocationPage = -1;
				dev->nDeletedFiles = 0;
				dev->nUnlinkedFiles = 0;
				dev->nBackgroundDeletions = 0;
				dev->oldestDirtySequence = 0;

				if(!init_failed && !yaffs_InitialiseBlocks(dev))
					init_failed = 1;

				yaffs_InitialiseTnodes(dev);
				yaffs_InitialiseObjects(dev);

				if(!init_failed && !yaffs_CreateInitialDirectories(dev))
					init_failed = 1;

				if(!init_failed && !yaffs_ScanBackwards(dev))
					init_failed = 1;
			}
		}else
			if(!yaffs_Scan(dev))
				init_failed = 1;
	}

	if(init_failed){
		/* Clean up the mess */
		T(YAFFS_TRACE_TRACING,
		  (TSTR("yaffs: yaffs_GutsInitialise() aborted.\n" TENDSTR)));

		yaffs_Deinitialise(dev);
		return YAFFS_FAIL;
	}

	/* Zero out stats */
	dev->nPageReads = 0;
	dev->nPageWrites = 0;
	dev->nBlockErasures = 0;
	dev->nGCCopies = 0;
	dev->nRetriedWrites = 0;

	dev->nRetiredBlocks = 0;

	yaffs_VerifyFreeChunks(dev);
	yaffs_VerifyBlocks(dev);


	T(YAFFS_TRACE_TRACING,
	  (TSTR("yaffs: yaffs_GutsInitialise() done.\n" TENDSTR)));
	return YAFFS_OK;

}

void yaffs_Deinitialise(yaffs_Device * dev)
{
	if (dev->isMounted) {
		int i;

		yaffs_DeinitialiseBlocks(dev);
		yaffs_DeinitialiseTnodes(dev);
		yaffs_DeinitialiseObjects(dev);
		if (dev->nShortOpCaches > 0 &&
		    dev->srCache) {

			for (i = 0; i < dev->nShortOpCaches; i++) {
				if(dev->srCache[i].data)
					YFREE(dev->srCache[i].data);
				dev->srCache[i].data = NULL;
			}

			YFREE(dev->srCache);
			dev->srCache = NULL;
		}

		YFREE(dev->gcCleanupList);

		for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
			YFREE(dev->tempBuffer[i].buffer);
		}

		dev->isMounted = 0;
	}

}

static int yaffs_CountFreeChunks(yaffs_Device * dev)
{
	int nFree;
	int b;

	yaffs_BlockInfo *blk;

	for (nFree = 0, b = dev->internalStartBlock; b <= dev->internalEndBlock;
	     b++) {
		blk = yaffs_GetBlockInfo(dev, b);

		switch (blk->blockState) {
		case YAFFS_BLOCK_STATE_EMPTY:
		case YAFFS_BLOCK_STATE_ALLOCATING:
		case YAFFS_BLOCK_STATE_COLLECTING:
		case YAFFS_BLOCK_STATE_FULL:
			nFree +=
			    (dev->nChunksPerBlock - blk->pagesInUse +
			     blk->softDeletions);
			break;
		default:
			break;
		}

	}

	return nFree;
}

int yaffs_GetNumberOfFreeChunks(yaffs_Device * dev)
{
	/* This is what we report to the outside world */

	int nFree;
	int nDirtyCacheChunks;
	int blocksForCheckpoint;

#if 1
	nFree = dev->nFreeChunks;
#else
	nFree = yaffs_CountFreeChunks(dev);
#endif

	nFree += dev->nDeletedFiles;

	/* Now count the number of dirty chunks in the cache and subtract those */

	{
		int i;
		for (nDirtyCacheChunks = 0, i = 0; i < dev->nShortOpCaches; i++) {
			if (dev->srCache[i].dirty)
				nDirtyCacheChunks++;
		}
	}

	nFree -= nDirtyCacheChunks;

	nFree -= ((dev->nReservedBlocks + 1) * dev->nChunksPerBlock);

	/* Now we figure out how much to reserve for the checkpoint and report that... */
	blocksForCheckpoint = yaffs_CalcCheckpointBlocksRequired(dev) - dev->blocksInCheckpoint;
	if(blocksForCheckpoint < 0)
		blocksForCheckpoint = 0;

	nFree -= (blocksForCheckpoint * dev->nChunksPerBlock);

	if (nFree < 0)
		nFree = 0;

	return nFree;

}

static int yaffs_freeVerificationFailures;

static void yaffs_VerifyFreeChunks(yaffs_Device * dev)
{
	int counted;
	int difference;

	if(yaffs_SkipVerification(dev))
		return;

	counted = yaffs_CountFreeChunks(dev);

	difference = dev->nFreeChunks - counted;

	if (difference) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR("Freechunks verification failure %d %d %d" TENDSTR),
		   dev->nFreeChunks, counted, difference));
		yaffs_freeVerificationFailures++;
	}
}

/*---------------------------------------- YAFFS test code ----------------------*/

#define yaffs_CheckStruct(structure,syze, name) \
           if(sizeof(structure) != syze) \
	       { \
	         T(YAFFS_TRACE_ALWAYS,(TSTR("%s should be %d but is %d\n" TENDSTR),\
		 name,syze,sizeof(structure))); \
	         return YAFFS_FAIL; \
		}

static int yaffs_CheckStructures(void)
{
/*      yaffs_CheckStruct(yaffs_Tags,8,"yaffs_Tags") */
/*      yaffs_CheckStruct(yaffs_TagsUnion,8,"yaffs_TagsUnion") */
/*      yaffs_CheckStruct(yaffs_Spare,16,"yaffs_Spare") */
#ifndef CONFIG_YAFFS_TNODE_LIST_DEBUG
	yaffs_CheckStruct(yaffs_Tnode, 2 * YAFFS_NTNODES_LEVEL0, "yaffs_Tnode")
#endif
	    yaffs_CheckStruct(yaffs_ObjectHeader, 512, "yaffs_ObjectHeader")

	    return YAFFS_OK;
}
#if 0
/**********************************************yaffs_flashif.c********************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


//const char *yaffs_flashif_c_version = "$Id: yaffs_flashif.c,v 1.3 2007-02-14 01:09:06 wookey Exp $";
//const char *yaffs_flashif_c_version = "$Id: yaffs_fileem2k.c,v 1.12 2007-02-14 01:09:06 wookey Exp $";

#include "yportenv.h"

#include "yaffs_flashif.h"
#include "yaffs_guts.h"
#include "devextras.h"


#define SIZE_IN_MB 16

#define BLOCK_SIZE (32 * 528)
#define BLOCKS_PER_MEG ((1024*1024)/(32 * 512))



typedef struct 
{
	__u8 data[528]; // Data + spare
} yflash_Page;

typedef struct
{
	yflash_Page page[32]; // The pages in the block
	
} yflash_Block;



typedef struct
{
	yflash_Block **block;
	int nBlocks;
} yflash_Device;

static yflash_Device ramdisk;

static int  CheckInit(yaffs_Device *dev)
{
	static int initialised = 0;
	
	int i;
	int fail = 0;
	int nAllocated = 0;
	
	if(initialised) 
	{
		return YAFFS_OK;
	}

	initialised = 1;
	
	
	ramdisk.nBlocks = (SIZE_IN_MB * 1024 * 1024)/(16 * 1024);
	
	ramdisk.block = YMALLOC(sizeof(yflash_Block *) * ramdisk.nBlocks);
	
	if(!ramdisk.block) return 0;
	
	for(i=0; i <ramdisk.nBlocks; i++)
	{
		ramdisk.block[i] = NULL;
	}
	
	for(i=0; i <ramdisk.nBlocks && !fail; i++)
	{
		if((ramdisk.block[i] = YMALLOC(sizeof(yflash_Block))) == 0)
		{
			fail = 1;
		}
		else
		{
			yflash_EraseBlockInNAND(dev,i);
			nAllocated++;
		}
	}
	
	if(fail)
	{
		for(i = 0; i < nAllocated; i++)
		{
			YFREE(ramdisk.block[i]);
		}
		YFREE(ramdisk.block);
		
		T(YAFFS_TRACE_ALWAYS,("Allocation failed, could only allocate %dMB of %dMB requested.\n",
		   nAllocated/64,ramdisk.nBlocks * YAFFS_BYTES_PER_BLOCK));
		return 0;
	}
	
	
	
	return 1;
}

int yflash_WriteChunkWithTagsToNAND(yaffs_Device *dev,int chunkInNAND,const __u8 *data, yaffs_ExtendedTags *tags)
{
	int blk;
	int pg;
	

	CheckInit(dev);
	
	blk = chunkInNAND/32;
	pg = chunkInNAND%32;
	
	
	if(data)
	{
		memcpy(ramdisk.block[blk]->page[pg].data,data,512);
	}
	
	
	if(tags)
	{
		yaffs_PackedTags2 pt;
		yaffs_PackTags2(&pt,tags);
		memcpy(&ramdisk.block[blk]->page[pg].data[512],&pt,sizeof(pt));
	}

	return YAFFS_OK;	

}


int yflash_ReadChunkWithTagsFromNAND(yaffs_Device *dev,int chunkInNAND, __u8 *data, yaffs_Tags *tags)
{
	int blk;
	int pg;

	
	CheckInit(dev);
	
	blk = chunkInNAND/32;
	pg = chunkInNAND%32;
	
	
	if(data)
	{
		memcpy(data,ramdisk.block[blk]->page[pg].data,512);
	}
	
	
	if(tags)
	{
		yaffs_PackedTags2 pt;
		memcpy(&pt,&ramdisk.block[blk]->page[pg].data[512],sizeof(yaffs_PackedTags2));
		yaffs_UnpackTags2(tags,&pt);
	}

	return YAFFS_OK;
}


int yflash_CheckChunkErased(yaffs_Device *dev,int chunkInNAND)
{
	int blk;
	int pg;
	int i;

	
	CheckInit(dev);
	
	blk = chunkInNAND/32;
	pg = chunkInNAND%32;
	
	
	for(i = 0; i < 528; i++)
	{
		if(ramdisk.block[blk]->page[pg].data[i] != 0xFF)
		{
			return YAFFS_FAIL;
		}
	}

	return YAFFS_OK;

}

int yflash_EraseBlockInNAND(yaffs_Device *dev, int blockNumber)
{
	
	CheckInit(dev);
	
	if(blockNumber < 0 || blockNumber >= ramdisk.nBlocks)
	{
		T(YAFFS_TRACE_ALWAYS,("Attempt to erase non-existant block %d\n",blockNumber));
		return YAFFS_FAIL;
	}
	else
	{
		memset(ramdisk.block[blockNumber],0xFF,sizeof(yflash_Block));
		return YAFFS_OK;
	}
	
}

int yflash_MarkNANDBlockBad(struct yaffs_DeviceStruct *dev, int blockNo)
{
	return YAFFS_OK;
	
}
int yflash_QueryNANDBlock(struct yaffs_DeviceStruct *dev, int blockNo, yaffs_BlockState *state, int *sequenceNumber)
{
	*state = YAFFS_BLOCK_STATE_EMPTY;
	*sequenceNumber = 0;
}


int yflash_InitialiseNAND(yaffs_Device *dev)
{
	return YAFFS_OK;
}
#endif
#if 0
/****************************************************yaffs_fileem2k.c************************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This provides a YAFFS nand emulation on a file for emulating 2kB pages.
 * This is only intended as test code to test persistence etc.
 */

//const char *yaffs_flashif_c_version = "$Id: yaffs_fileem2k.c,v 1.12 2007-02-14 01:09:06 wookey Exp $";


#include "yportenv.h"

#include "yaffs_flashif.h"
#include "yaffs_guts.h"
#include "devextras.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> 

#include "yaffs_fileem2k.h"
#include "yaffs_packedtags2.h"

//#define SIMULATE_FAILURES

typedef struct 
{
	__u8 data[PAGE_SIZE]; // Data + spare
} yflash_Page;

typedef struct
{
	yflash_Page page[PAGES_PER_BLOCK]; // The pages in the block
	
} yflash_Block;



#define MAX_HANDLES 20
#define BLOCKS_PER_HANDLE 8000

typedef struct
{
	int handle[MAX_HANDLES];
	int nBlocks;
} yflash_Device;

static yflash_Device filedisk;

int yaffs_testPartialWrite = 0;




static __u8 localBuffer[PAGE_SIZE];

static char *NToName(char *buf,int n)
{
	sprintf(buf,"emfile%d",n);
	return buf;
}

static char dummyBuffer[BLOCK_SIZE];

static int GetBlockFileHandle(int n)
{
	int h;
	int requiredSize;
	
	char name[40];
	NToName(name,n);
	int fSize;
	int i;
	
	h =  open(name, O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
	if(h >= 0){
		fSize = lseek(h,0,SEEK_END);
		requiredSize = BLOCKS_PER_HANDLE * BLOCK_SIZE;
		if(fSize < requiredSize){
		   for(i = 0; i < BLOCKS_PER_HANDLE; i++)
		   	if(write(h,dummyBuffer,BLOCK_SIZE) != BLOCK_SIZE)
				return -1;
			
		}
	}
	
	return h;

}

static int  CheckInit(void)
{
	static int initialised = 0;
	int h;
	int i;

	
	off_t fSize;
	off_t requiredSize;
	int written;
	int blk;
	
	yflash_Page p;
	
	if(initialised) 
	{
		return YAFFS_OK;
	}

	initialised = 1;
	
	memset(dummyBuffer,0xff,sizeof(dummyBuffer));
	
	
	filedisk.nBlocks = SIZE_IN_MB * BLOCKS_PER_MB;

	for(i = 0; i <  MAX_HANDLES; i++)
		filedisk.handle[i] = -1;
	
	for(i = 0,blk = 0; blk < filedisk.nBlocks; blk+=BLOCKS_PER_HANDLE,i++)
		filedisk.handle[i] = GetBlockFileHandle(i);
	
	
	return 1;
}


int yflash_GetNumberOfBlocks(void)
{
	CheckInit();
	
	return filedisk.nBlocks;
}

int yflash_WriteChunkWithTagsToNAND(yaffs_Device *dev,int chunkInNAND,const __u8 *data, yaffs_ExtendedTags *tags)
{
	int written;
	int pos;
	int h;
	int i;
	int nRead;
	int error;
	
	T(YAFFS_TRACE_MTD,(TSTR("write chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));

	CheckInit();
	
	
	
	if(data)
	{
		pos = (chunkInNAND % (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE)) * PAGE_SIZE;
		h = filedisk.handle[(chunkInNAND / (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE))];
		
		lseek(h,pos,SEEK_SET);
		nRead =  read(h, localBuffer,dev->nDataBytesPerChunk);
		for(i = error = 0; i < dev->nDataBytesPerChunk && !error; i++){
			if(localBuffer[i] != 0xFF){
				printf("nand simulation: chunk %d data byte %d was %0x2\n",
					chunkInNAND,i,localBuffer[i]);
				error = 1;
			}
		}
		
		for(i = 0; i < dev->nDataBytesPerChunk; i++)
		  localBuffer[i] &= data[i];
		  
		if(memcmp(localBuffer,data,dev->nDataBytesPerChunk))
			printf("nand simulator: data does not match\n");
			
		lseek(h,pos,SEEK_SET);
		written = write(h,localBuffer,dev->nDataBytesPerChunk);
		
		if(yaffs_testPartialWrite){
			close(h);
			exit(1);
		}
		
#ifdef SIMULATE_FAILURES
			if((chunkInNAND >> 6) == 100) 
			  written = 0;

			if((chunkInNAND >> 6) == 110) 
			  written = 0;
#endif


		if(written != dev->nDataBytesPerChunk) return YAFFS_FAIL;
	}
	
	if(tags)
	{
		pos = (chunkInNAND % (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE)) * PAGE_SIZE + PAGE_DATA_SIZE ;
		h = filedisk.handle[(chunkInNAND / (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE))];
		
		lseek(h,pos,SEEK_SET);

		if( 0 && dev->isYaffs2)
		{
			
			written = write(h,tags,sizeof(yaffs_ExtendedTags));
			if(written != sizeof(yaffs_ExtendedTags)) return YAFFS_FAIL;
		}
		else
		{
			yaffs_PackedTags2 pt;
			yaffs_PackTags2(&pt,tags);
			__u8 * ptab = (__u8 *)&pt;

			nRead = read(h,localBuffer,sizeof(pt));
			for(i = error = 0; i < sizeof(pt) && !error; i++){
				if(localBuffer[i] != 0xFF){
					printf("nand simulation: chunk %d oob byte %d was %0x2\n",
						chunkInNAND,i,localBuffer[i]);
						error = 1;
				}
			}
		
			for(i = 0; i < sizeof(pt); i++)
			  localBuffer[i] &= ptab[i];
			 
			if(memcmp(localBuffer,&pt,sizeof(pt)))
				printf("nand sim: tags corruption\n");
				
			lseek(h,pos,SEEK_SET);
			
			written = write(h,localBuffer,sizeof(pt));
			if(written != sizeof(pt)) return YAFFS_FAIL;
		}
	}
	

	return YAFFS_OK;	

}

int yaffs_CheckAllFF(const __u8 *ptr, int n)
{
	while(n)
	{
		n--;
		if(*ptr!=0xFF) return 0;
		ptr++;
	}
	return 1;
}


static int fail300 = 1;
static int fail320 = 1;

static int failRead10 = 2;

int yflash_ReadChunkWithTagsFromNAND(yaffs_Device *dev,int chunkInNAND, __u8 *data, yaffs_ExtendedTags *tags)
{
	int nread;
	int pos;
	int h;
	
	T(YAFFS_TRACE_MTD,(TSTR("read chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));
	
	CheckInit();
	
	
	
	if(data)
	{

		pos = (chunkInNAND % (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE)) * PAGE_SIZE;
		h = filedisk.handle[(chunkInNAND / (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE))];		
		lseek(h,pos,SEEK_SET);
		nread = read(h,data,dev->nDataBytesPerChunk);
		
		
		if(nread != dev->nDataBytesPerChunk) return YAFFS_FAIL;
	}
	
	if(tags)
	{
		pos = (chunkInNAND % (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE)) * PAGE_SIZE + PAGE_DATA_SIZE;
		h = filedisk.handle[(chunkInNAND / (PAGES_PER_BLOCK * BLOCKS_PER_HANDLE))];		
		lseek(h,pos,SEEK_SET);

		if(0 && dev->isYaffs2)
		{
			nread= read(h,tags,sizeof(yaffs_ExtendedTags));
			if(nread != sizeof(yaffs_ExtendedTags)) return YAFFS_FAIL;
			if(yaffs_CheckAllFF((__u8 *)tags,sizeof(yaffs_ExtendedTags)))
			{
				yaffs_InitialiseTags(tags);
			}
			else
			{
				tags->chunkUsed = 1;
			}
		}
		else
		{
			yaffs_PackedTags2 pt;
			nread= read(h,&pt,sizeof(pt));
			yaffs_UnpackTags2(tags,&pt);
#ifdef SIMULATE_FAILURES
			if((chunkInNAND >> 6) == 100) {
			    if(fail300 && tags->eccResult == YAFFS_ECC_RESULT_NO_ERROR){
			       tags->eccResult = YAFFS_ECC_RESULT_FIXED;
			       fail300 = 0;
			    }
			    
			}
			if((chunkInNAND >> 6) == 110) {
			    if(fail320 && tags->eccResult == YAFFS_ECC_RESULT_NO_ERROR){
			       tags->eccResult = YAFFS_ECC_RESULT_FIXED;
			       fail320 = 0;
			    }
			}
#endif
			if(failRead10>0 && chunkInNAND == 10){
				failRead10--;
				nread = 0;
			}
			
			if(nread != sizeof(pt)) return YAFFS_FAIL;
		}
	}
	

	return YAFFS_OK;	

}


int yflash_MarkNANDBlockBad(struct yaffs_DeviceStruct *dev, int blockNo)
{
	int written;
	int h;
	
	yaffs_PackedTags2 pt;

	CheckInit();
	
	memset(&pt,0,sizeof(pt));
	h = filedisk.handle[(blockNo / ( BLOCKS_PER_HANDLE))];
	lseek(h,((blockNo % BLOCKS_PER_HANDLE) * dev->nChunksPerBlock) * PAGE_SIZE + PAGE_DATA_SIZE,SEEK_SET);
	written = write(h,&pt,sizeof(pt));
		
	if(written != sizeof(pt)) return YAFFS_FAIL;
	
	
	return YAFFS_OK;
	
}

int yflash_EraseBlockInNAND(yaffs_Device *dev, int blockNumber)
{

	int i;
	int h;
		
	CheckInit();
	
	printf("erase block %d\n",blockNumber);
	
	if(blockNumber == 320)
		fail320 = 1;
	
	if(blockNumber < 0 || blockNumber >= filedisk.nBlocks)
	{
		T(YAFFS_TRACE_ALWAYS,("Attempt to erase non-existant block %d\n",blockNumber));
		return YAFFS_FAIL;
	}
	else
	{
	
		__u8 pg[PAGE_SIZE];
		int syz = PAGE_SIZE;
		int pos;
		
		memset(pg,0xff,syz);
		

		h = filedisk.handle[(blockNumber / ( BLOCKS_PER_HANDLE))];
		lseek(h,((blockNumber % BLOCKS_PER_HANDLE) * dev->nChunksPerBlock) * PAGE_SIZE,SEEK_SET);		
		for(i = 0; i < dev->nChunksPerBlock; i++)
		{
			write(h,pg,PAGE_SIZE);
		}
		pos = lseek(h, 0,SEEK_CUR);
		
		return YAFFS_OK;
	}
	
}

int yflash_InitialiseNAND(yaffs_Device *dev)
{
	CheckInit();
	
	return YAFFS_OK;
}




int yflash_QueryNANDBlock(struct yaffs_DeviceStruct *dev, int blockNo, yaffs_BlockState *state, int *sequenceNumber)
{
	yaffs_ExtendedTags tags;
	int chunkNo;

	*sequenceNumber = 0;
	
	chunkNo = blockNo * dev->nChunksPerBlock;
	
	yflash_ReadChunkWithTagsFromNAND(dev,chunkNo,NULL,&tags);
	if(tags.blockBad)
	{
		*state = YAFFS_BLOCK_STATE_DEAD;
	}
	else if(!tags.chunkUsed)
	{
		*state = YAFFS_BLOCK_STATE_EMPTY;
	}
	else if(tags.chunkUsed)
	{
		*state = YAFFS_BLOCK_STATE_NEEDS_SCANNING;
		*sequenceNumber = tags.sequenceNumber;
	}
	return YAFFS_OK;
}
#endif
#if 0
/***********************************************************yaffs_fileem.c**********************************/

/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This provides a YAFFS nand emulation on a file.
 * This is only intended as test code to test persistence etc.
 */

//const char *yaffs_flashif_c_version = "$Id: yaffs_fileem.c,v 1.3 2007-02-14 01:09:06 wookey Exp $";


#include "yportenv.h"

#include "yaffs_flashif.h"
#include "yaffs_guts.h"

#include "devextras.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> 



//#define SIZE_IN_MB 16

#define BLOCK_SIZE (32 * 528)
#define BLOCKS_PER_MEG ((1024*1024)/(32 * 512))



typedef struct 
{
	__u8 data[528]; // Data + spare
} yflash_Page;

typedef struct
{
	yflash_Page page[32]; // The pages in the block
	
} yflash_Block;



typedef struct
{
	int handle;
	int nBlocks;
} yflash_Device;

static yflash_Device filedisk;

static int  CheckInit(yaffs_Device *dev)
{
	static int initialised = 0;
	
	int i;

	
	int fSize;
	int written;
	
	yflash_Page p;
	
	if(initialised) 
	{
		return YAFFS_OK;
	}

	initialised = 1;
	
	
	filedisk.nBlocks = (SIZE_IN_MB * 1024 * 1024)/(16 * 1024);
	
	filedisk.handle = open("yaffsemfile", O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
	
	if(filedisk.handle < 0)
	{
		perror("Failed to open yaffs emulation file");
		return YAFFS_FAIL;
	}
	
	
	fSize = lseek(filedisk.handle,0,SEEK_END);
	
	if(fSize < SIZE_IN_MB * 1024 * 1024)
	{
		printf("Creating yaffs emulation file\n");
		
		lseek(filedisk.handle,0,SEEK_SET);
		
		memset(&p,0xff,sizeof(yflash_Page));
		
		for(i = 0; i < SIZE_IN_MB * 1024 * 1024; i+= 512)
		{
			written = write(filedisk.handle,&p,sizeof(yflash_Page));
			
			if(written != sizeof(yflash_Page))
			{
				printf("Write failed\n");
				return YAFFS_FAIL;
			}
		}		
	}
	
	return 1;
}

int yflash_WriteChunkToNAND(yaffs_Device *dev,int chunkInNAND,const __u8 *data, const yaffs_Spare *spare)
{
	int written;

	CheckInit(dev);
	
	
	
	if(data)
	{
		lseek(filedisk.handle,chunkInNAND * 528,SEEK_SET);
		written = write(filedisk.handle,data,512);
		
		if(written != 512) return YAFFS_FAIL;
	}
	
	if(spare)
	{
		lseek(filedisk.handle,chunkInNAND * 528 + 512,SEEK_SET);
		written = write(filedisk.handle,spare,16);
		
		if(written != 16) return YAFFS_FAIL;
	}
	

	return YAFFS_OK;	

}


int yflash_ReadChunkFromNAND(yaffs_Device *dev,int chunkInNAND, __u8 *data, yaffs_Spare *spare)
{
	int nread;

	CheckInit(dev);
	
	
	
	if(data)
	{
		lseek(filedisk.handle,chunkInNAND * 528,SEEK_SET);
		nread = read(filedisk.handle,data,512);
		
		if(nread != 512) return YAFFS_FAIL;
	}
	
	if(spare)
	{
		lseek(filedisk.handle,chunkInNAND * 528 + 512,SEEK_SET);
		nread= read(filedisk.handle,spare,16);
		
		if(nread != 16) return YAFFS_FAIL;
	}
	

	return YAFFS_OK;	

}


int yflash_EraseBlockInNAND(yaffs_Device *dev, int blockNumber)
{

	int i;
		
	CheckInit(dev);
	
	if(blockNumber < 0 || blockNumber >= filedisk.nBlocks)
	{
		T(YAFFS_TRACE_ALWAYS,("Attempt to erase non-existant block %d\n",blockNumber));
		return YAFFS_FAIL;
	}
	else
	{
	
		yflash_Page pg;
		
		memset(&pg,0xff,sizeof(yflash_Page));
		
		lseek(filedisk.handle, blockNumber * 32 * 528, SEEK_SET);
		
		for(i = 0; i < 32; i++)
		{
			write(filedisk.handle,&pg,528);
		}
		return YAFFS_OK;
	}
	
}

int yflash_InitialiseNAND(yaffs_Device *dev)
{
	dev->useNANDECC = 1; // force on useNANDECC which gets faked. 
						 // This saves us doing ECC checks.
	
	return YAFFS_OK;
}
#endif
/*************************************************************yaffs_ecc.c******************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This code implements the ECC algorithm used in SmartMedia.
 *
 * The ECC comprises 22 bits of parity information and is stuffed into 3 bytes.
 * The two unused bit are set to 1.
 * The ECC can correct single bit errors in a 256-byte page of data. Thus, two such ECC
 * blocks are used on a 512-byte NAND page.
 *
 */

/* Table generated by gen-ecc.c
 * Using a table means we do not have to calculate p1..p4 and p1'..p4'
 * for each byte of data. These are instead provided in a table in bits7..2.
 * Bit 0 of each entry indicates whether the entry has an odd or even parity, and therefore
 * this bytes influence on the line parity.
 */

const char *yaffs_ecc_c_version =
    "$Id: yaffs_ecc.c,v 1.10 2007-12-13 15:35:17 wookey Exp $";

#include "yportenv.h"

#include "yaffs_ecc.h"

static const unsigned char column_parity_table[] = {
	0x00, 0x55, 0x59, 0x0c, 0x65, 0x30, 0x3c, 0x69,
	0x69, 0x3c, 0x30, 0x65, 0x0c, 0x59, 0x55, 0x00,
	0x95, 0xc0, 0xcc, 0x99, 0xf0, 0xa5, 0xa9, 0xfc,
	0xfc, 0xa9, 0xa5, 0xf0, 0x99, 0xcc, 0xc0, 0x95,
	0x99, 0xcc, 0xc0, 0x95, 0xfc, 0xa9, 0xa5, 0xf0,
	0xf0, 0xa5, 0xa9, 0xfc, 0x95, 0xc0, 0xcc, 0x99,
	0x0c, 0x59, 0x55, 0x00, 0x69, 0x3c, 0x30, 0x65,
	0x65, 0x30, 0x3c, 0x69, 0x00, 0x55, 0x59, 0x0c,
	0xa5, 0xf0, 0xfc, 0xa9, 0xc0, 0x95, 0x99, 0xcc,
	0xcc, 0x99, 0x95, 0xc0, 0xa9, 0xfc, 0xf0, 0xa5,
	0x30, 0x65, 0x69, 0x3c, 0x55, 0x00, 0x0c, 0x59,
	0x59, 0x0c, 0x00, 0x55, 0x3c, 0x69, 0x65, 0x30,
	0x3c, 0x69, 0x65, 0x30, 0x59, 0x0c, 0x00, 0x55,
	0x55, 0x00, 0x0c, 0x59, 0x30, 0x65, 0x69, 0x3c,
	0xa9, 0xfc, 0xf0, 0xa5, 0xcc, 0x99, 0x95, 0xc0,
	0xc0, 0x95, 0x99, 0xcc, 0xa5, 0xf0, 0xfc, 0xa9,
	0xa9, 0xfc, 0xf0, 0xa5, 0xcc, 0x99, 0x95, 0xc0,
	0xc0, 0x95, 0x99, 0xcc, 0xa5, 0xf0, 0xfc, 0xa9,
	0x3c, 0x69, 0x65, 0x30, 0x59, 0x0c, 0x00, 0x55,
	0x55, 0x00, 0x0c, 0x59, 0x30, 0x65, 0x69, 0x3c,
	0x30, 0x65, 0x69, 0x3c, 0x55, 0x00, 0x0c, 0x59,
	0x59, 0x0c, 0x00, 0x55, 0x3c, 0x69, 0x65, 0x30,
	0xa5, 0xf0, 0xfc, 0xa9, 0xc0, 0x95, 0x99, 0xcc,
	0xcc, 0x99, 0x95, 0xc0, 0xa9, 0xfc, 0xf0, 0xa5,
	0x0c, 0x59, 0x55, 0x00, 0x69, 0x3c, 0x30, 0x65,
	0x65, 0x30, 0x3c, 0x69, 0x00, 0x55, 0x59, 0x0c,
	0x99, 0xcc, 0xc0, 0x95, 0xfc, 0xa9, 0xa5, 0xf0,
	0xf0, 0xa5, 0xa9, 0xfc, 0x95, 0xc0, 0xcc, 0x99,
	0x95, 0xc0, 0xcc, 0x99, 0xf0, 0xa5, 0xa9, 0xfc,
	0xfc, 0xa9, 0xa5, 0xf0, 0x99, 0xcc, 0xc0, 0x95,
	0x00, 0x55, 0x59, 0x0c, 0x65, 0x30, 0x3c, 0x69,
	0x69, 0x3c, 0x30, 0x65, 0x0c, 0x59, 0x55, 0x00,
};

/* Count the bits in an unsigned char or a U32 */

static int yaffs_CountBits(unsigned char x)
{
	int r = 0;
	while (x) {
		if (x & 1)
			r++;
		x >>= 1;
	}
	return r;
}

static int yaffs_CountBits32(unsigned x)
{
	int r = 0;
	while (x) {
		if (x & 1)
			r++;
		x >>= 1;
	}
	return r;
}

/* Calculate the ECC for a 256-byte block of data */
void yaffs_ECCCalculate(const unsigned char *data, unsigned char *ecc)
{
	unsigned int i;

	unsigned char col_parity = 0;
	unsigned char line_parity = 0;
	unsigned char line_parity_prime = 0;
	unsigned char t;
	unsigned char b;

	for (i = 0; i < 256; i++) {
		b = column_parity_table[*data++];
		col_parity ^= b;

		if (b & 0x01)	// odd number of bits in the byte
		{
			line_parity ^= i;
			line_parity_prime ^= ~i;
		}

	}

	ecc[2] = (~col_parity) | 0x03;

	t = 0;
	if (line_parity & 0x80)
		t |= 0x80;
	if (line_parity_prime & 0x80)
		t |= 0x40;
	if (line_parity & 0x40)
		t |= 0x20;
	if (line_parity_prime & 0x40)
		t |= 0x10;
	if (line_parity & 0x20)
		t |= 0x08;
	if (line_parity_prime & 0x20)
		t |= 0x04;
	if (line_parity & 0x10)
		t |= 0x02;
	if (line_parity_prime & 0x10)
		t |= 0x01;
	ecc[1] = ~t;

	t = 0;
	if (line_parity & 0x08)
		t |= 0x80;
	if (line_parity_prime & 0x08)
		t |= 0x40;
	if (line_parity & 0x04)
		t |= 0x20;
	if (line_parity_prime & 0x04)
		t |= 0x10;
	if (line_parity & 0x02)
		t |= 0x08;
	if (line_parity_prime & 0x02)
		t |= 0x04;
	if (line_parity & 0x01)
		t |= 0x02;
	if (line_parity_prime & 0x01)
		t |= 0x01;
	ecc[0] = ~t;

#ifdef CONFIG_YAFFS_ECC_WRONG_ORDER
	// Swap the bytes into the wrong order
	t = ecc[0];
	ecc[0] = ecc[1];
	ecc[1] = t;
#endif
}


/* Correct the ECC on a 256 byte block of data */

int yaffs_ECCCorrect(unsigned char *data, unsigned char *read_ecc,
		     const unsigned char *test_ecc)
{
	unsigned char d0, d1, d2;	/* deltas */

	d0 = read_ecc[0] ^ test_ecc[0];
	d1 = read_ecc[1] ^ test_ecc[1];
	d2 = read_ecc[2] ^ test_ecc[2];

	if ((d0 | d1 | d2) == 0)
		return 0; /* no error */

	if (((d0 ^ (d0 >> 1)) & 0x55) == 0x55 &&
	    ((d1 ^ (d1 >> 1)) & 0x55) == 0x55 &&
	    ((d2 ^ (d2 >> 1)) & 0x54) == 0x54) {
		/* Single bit (recoverable) error in data */

		unsigned byte;
		unsigned bit;

#ifdef CONFIG_YAFFS_ECC_WRONG_ORDER
		// swap the bytes to correct for the wrong order
		unsigned char t;

		t = d0;
		d0 = d1;
		d1 = t;
#endif

		bit = byte = 0;

		if (d1 & 0x80)
			byte |= 0x80;
		if (d1 & 0x20)
			byte |= 0x40;
		if (d1 & 0x08)
			byte |= 0x20;
		if (d1 & 0x02)
			byte |= 0x10;
		if (d0 & 0x80)
			byte |= 0x08;
		if (d0 & 0x20)
			byte |= 0x04;
		if (d0 & 0x08)
			byte |= 0x02;
		if (d0 & 0x02)
			byte |= 0x01;

		if (d2 & 0x80)
			bit |= 0x04;
		if (d2 & 0x20)
			bit |= 0x02;
		if (d2 & 0x08)
			bit |= 0x01;

		data[byte] ^= (1 << bit);

		return 1; /* Corrected the error */
	}

	if ((yaffs_CountBits(d0) +
	     yaffs_CountBits(d1) +
	     yaffs_CountBits(d2)) ==  1) {
		/* Reccoverable error in ecc */

		read_ecc[0] = test_ecc[0];
		read_ecc[1] = test_ecc[1];
		read_ecc[2] = test_ecc[2];

		return 1; /* Corrected the error */
	}

	/* Unrecoverable error */

	return -1;

}


/*
 * ECCxxxOther does ECC calcs on arbitrary n bytes of data
 */
void yaffs_ECCCalculateOther(const unsigned char *data, unsigned nBytes,
			     yaffs_ECCOther * eccOther)
{
	unsigned int i;

	unsigned char col_parity = 0;
	unsigned line_parity = 0;
	unsigned line_parity_prime = 0;
	unsigned char b;

	for (i = 0; i < nBytes; i++) {
		b = column_parity_table[*data++];
		col_parity ^= b;

		if (b & 0x01)	 {
			/* odd number of bits in the byte */
			line_parity ^= i;
			line_parity_prime ^= ~i;
		}

	}

	eccOther->colParity = (col_parity >> 2) & 0x3f;
	eccOther->lineParity = line_parity;
	eccOther->lineParityPrime = line_parity_prime;
}

int yaffs_ECCCorrectOther(unsigned char *data, unsigned nBytes,
			  yaffs_ECCOther * read_ecc,
			  const yaffs_ECCOther * test_ecc)
{
	unsigned char cDelta;	/* column parity delta */
	unsigned lDelta;	/* line parity delta */
	unsigned lDeltaPrime;	/* line parity delta */
	unsigned bit;

	cDelta = read_ecc->colParity ^ test_ecc->colParity;
	lDelta = read_ecc->lineParity ^ test_ecc->lineParity;
	lDeltaPrime = read_ecc->lineParityPrime ^ test_ecc->lineParityPrime;

	if ((cDelta | lDelta | lDeltaPrime) == 0)
		return 0; /* no error */

	if (lDelta == ~lDeltaPrime &&
	    (((cDelta ^ (cDelta >> 1)) & 0x15) == 0x15))
	{
		/* Single bit (recoverable) error in data */

		bit = 0;

		if (cDelta & 0x20)
			bit |= 0x04;
		if (cDelta & 0x08)
			bit |= 0x02;
		if (cDelta & 0x02)
			bit |= 0x01;

		if(lDelta >= nBytes)
			return -1;

		data[lDelta] ^= (1 << bit);

		return 1; /* corrected */
	}

	if ((yaffs_CountBits32(lDelta) + yaffs_CountBits32(lDeltaPrime) +
	     yaffs_CountBits(cDelta)) == 1) {
		/* Reccoverable error in ecc */

		*read_ecc = *test_ecc;
		return 1; /* corrected */
	}

	/* Unrecoverable error */

	return -1;

}

/******************************************************************yaffs_checkptrw.c**************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

const char *yaffs_checkptrw_c_version =
    "$Id: yaffs_checkptrw.c,v 1.15 2007-12-13 15:35:17 wookey Exp $";


#include "yaffs_checkptrw.h"


static int yaffs_CheckpointSpaceOk(yaffs_Device *dev)
{

	int blocksAvailable = dev->nErasedBlocks - dev->nReservedBlocks;

	T(YAFFS_TRACE_CHECKPOINT,
		(TSTR("checkpt blocks available = %d" TENDSTR),
		blocksAvailable));


	return (blocksAvailable <= 0) ? 0 : 1;
}


static int yaffs_CheckpointErase(yaffs_Device *dev)
{

	int i;


	if(!dev->eraseBlockInNAND)
		return 0;
	T(YAFFS_TRACE_CHECKPOINT,(TSTR("checking blocks %d to %d"TENDSTR),
		dev->internalStartBlock,dev->internalEndBlock));

	for(i = dev->internalStartBlock; i <= dev->internalEndBlock; i++) {
		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev,i);
		if(bi->blockState == YAFFS_BLOCK_STATE_CHECKPOINT){
			T(YAFFS_TRACE_CHECKPOINT,(TSTR("erasing checkpt block %d"TENDSTR),i));
			if(dev->eraseBlockInNAND(dev,i- dev->blockOffset /* realign */)){
				bi->blockState = YAFFS_BLOCK_STATE_EMPTY;
				dev->nErasedBlocks++;
				dev->nFreeChunks += dev->nChunksPerBlock;
			}
			else {
				dev->markNANDBlockBad(dev,i);
				bi->blockState = YAFFS_BLOCK_STATE_DEAD;
			}
		}
	}

	dev->blocksInCheckpoint = 0;

	return 1;
}


static void yaffs_CheckpointFindNextErasedBlock(yaffs_Device *dev)
{
	int  i;
	int blocksAvailable = dev->nErasedBlocks - dev->nReservedBlocks;
	T(YAFFS_TRACE_CHECKPOINT,
		(TSTR("allocating checkpt block: erased %d reserved %d avail %d next %d "TENDSTR),
		dev->nErasedBlocks,dev->nReservedBlocks,blocksAvailable,dev->checkpointNextBlock));

	if(dev->checkpointNextBlock >= 0 &&
	   dev->checkpointNextBlock <= dev->internalEndBlock &&
	   blocksAvailable > 0){

		for(i = dev->checkpointNextBlock; i <= dev->internalEndBlock; i++){
			yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev,i);
			if(bi->blockState == YAFFS_BLOCK_STATE_EMPTY){
				dev->checkpointNextBlock = i + 1;
				dev->checkpointCurrentBlock = i;
				T(YAFFS_TRACE_CHECKPOINT,(TSTR("allocating checkpt block %d"TENDSTR),i));
				return;
			}
		}
	}
	T(YAFFS_TRACE_CHECKPOINT,(TSTR("out of checkpt blocks"TENDSTR)));

	dev->checkpointNextBlock = -1;
	dev->checkpointCurrentBlock = -1;
}

static void yaffs_CheckpointFindNextCheckpointBlock(yaffs_Device *dev)
{
	int  i;
	yaffs_ExtendedTags tags;

	T(YAFFS_TRACE_CHECKPOINT,(TSTR("find next checkpt block: start:  blocks %d next %d" TENDSTR),
		dev->blocksInCheckpoint, dev->checkpointNextBlock));

	if(dev->blocksInCheckpoint < dev->checkpointMaxBlocks)
		for(i = dev->checkpointNextBlock; i <= dev->internalEndBlock; i++){
			int chunk = i * dev->nChunksPerBlock;
			int realignedChunk = chunk - dev->chunkOffset;

			dev->readChunkWithTagsFromNAND(dev,realignedChunk,NULL,&tags);
			T(YAFFS_TRACE_CHECKPOINT,(TSTR("find next checkpt block: search: block %d oid %d seq %d eccr %d" TENDSTR),
				i, tags.objectId,tags.sequenceNumber,tags.eccResult));

			if(tags.sequenceNumber == YAFFS_SEQUENCE_CHECKPOINT_DATA){
				/* Right kind of block */
				dev->checkpointNextBlock = tags.objectId;
				dev->checkpointCurrentBlock = i;
				dev->checkpointBlockList[dev->blocksInCheckpoint] = i;
				dev->blocksInCheckpoint++;
				T(YAFFS_TRACE_CHECKPOINT,(TSTR("found checkpt block %d"TENDSTR),i));
				return;
			}
		}

	T(YAFFS_TRACE_CHECKPOINT,(TSTR("found no more checkpt blocks"TENDSTR)));

	dev->checkpointNextBlock = -1;
	dev->checkpointCurrentBlock = -1;
}


int yaffs_CheckpointOpen(yaffs_Device *dev, int forWriting)
{

	/* Got the functions we need? */
	if (!dev->writeChunkWithTagsToNAND ||
	    !dev->readChunkWithTagsFromNAND ||
	    !dev->eraseBlockInNAND ||
	    !dev->markNANDBlockBad)
		return 0;

	if(forWriting && !yaffs_CheckpointSpaceOk(dev))
		return 0;

	if(!dev->checkpointBuffer)
		dev->checkpointBuffer = YMALLOC_DMA(dev->nDataBytesPerChunk);
	if(!dev->checkpointBuffer)
		return 0;


	dev->checkpointPageSequence = 0;

	dev->checkpointOpenForWrite = forWriting;

	dev->checkpointByteCount = 0;
	dev->checkpointSum = 0;
	dev->checkpointXor = 0;
	dev->checkpointCurrentBlock = -1;
	dev->checkpointCurrentChunk = -1;
	dev->checkpointNextBlock = dev->internalStartBlock;

	/* Erase all the blocks in the checkpoint area */
	if(forWriting){
		memset(dev->checkpointBuffer,0,dev->nDataBytesPerChunk);
		dev->checkpointByteOffset = 0;
		return yaffs_CheckpointErase(dev);


	} else {
		int i;
		/* Set to a value that will kick off a read */
		dev->checkpointByteOffset = dev->nDataBytesPerChunk;
		/* A checkpoint block list of 1 checkpoint block per 16 block is (hopefully)
		 * going to be way more than we need */
		dev->blocksInCheckpoint = 0;
		dev->checkpointMaxBlocks = (dev->internalEndBlock - dev->internalStartBlock)/16 + 2;
		dev->checkpointBlockList = YMALLOC(sizeof(int) * dev->checkpointMaxBlocks);
		for(i = 0; i < dev->checkpointMaxBlocks; i++)
			dev->checkpointBlockList[i] = -1;
	}

	return 1;
}

int yaffs_GetCheckpointSum(yaffs_Device *dev, __u32 *sum)
{
	__u32 compositeSum;
	compositeSum =  (dev->checkpointSum << 8) | (dev->checkpointXor & 0xFF);
	*sum = compositeSum;
	return 1;
}

static int yaffs_CheckpointFlushBuffer(yaffs_Device *dev)
{

	int chunk;
	int realignedChunk;

	yaffs_ExtendedTags tags;

	if(dev->checkpointCurrentBlock < 0){
		yaffs_CheckpointFindNextErasedBlock(dev);
		dev->checkpointCurrentChunk = 0;
	}

	if(dev->checkpointCurrentBlock < 0)
		return 0;

	tags.chunkDeleted = 0;
	tags.objectId = dev->checkpointNextBlock; /* Hint to next place to look */
	tags.chunkId = dev->checkpointPageSequence + 1;
	tags.sequenceNumber =  YAFFS_SEQUENCE_CHECKPOINT_DATA;
	tags.byteCount = dev->nDataBytesPerChunk;
	if(dev->checkpointCurrentChunk == 0){
		/* First chunk we write for the block? Set block state to
		   checkpoint */
		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev,dev->checkpointCurrentBlock);
		bi->blockState = YAFFS_BLOCK_STATE_CHECKPOINT;
		dev->blocksInCheckpoint++;
	}

	chunk = dev->checkpointCurrentBlock * dev->nChunksPerBlock + dev->checkpointCurrentChunk;


	T(YAFFS_TRACE_CHECKPOINT,(TSTR("checkpoint wite buffer nand %d(%d:%d) objid %d chId %d" TENDSTR),
		chunk, dev->checkpointCurrentBlock, dev->checkpointCurrentChunk,tags.objectId,tags.chunkId));

	realignedChunk = chunk - dev->chunkOffset;

	dev->writeChunkWithTagsToNAND(dev,realignedChunk,dev->checkpointBuffer,&tags);
	dev->checkpointByteOffset = 0;
	dev->checkpointPageSequence++;
	dev->checkpointCurrentChunk++;
	if(dev->checkpointCurrentChunk >= dev->nChunksPerBlock){
		dev->checkpointCurrentChunk = 0;
		dev->checkpointCurrentBlock = -1;
	}
	memset(dev->checkpointBuffer,0,dev->nDataBytesPerChunk);

	return 1;
}


int yaffs_CheckpointWrite(yaffs_Device *dev,const void *data, int nBytes)
{
	int i=0;
	int ok = 1;


	__u8 * dataBytes = (__u8 *)data;



	if(!dev->checkpointBuffer)
		return 0;

	if(!dev->checkpointOpenForWrite)
		return -1;

	while(i < nBytes && ok) {



		dev->checkpointBuffer[dev->checkpointByteOffset] = *dataBytes ;
		dev->checkpointSum += *dataBytes;
		dev->checkpointXor ^= *dataBytes;

		dev->checkpointByteOffset++;
		i++;
		dataBytes++;
		dev->checkpointByteCount++;


		if(dev->checkpointByteOffset < 0 ||
		   dev->checkpointByteOffset >= dev->nDataBytesPerChunk)
			ok = yaffs_CheckpointFlushBuffer(dev);

	}

	return 	i;
}

int yaffs_CheckpointRead(yaffs_Device *dev, void *data, int nBytes)
{
	int i=0;
	int ok = 1;
	yaffs_ExtendedTags tags;


	int chunk;
	int realignedChunk;

	__u8 *dataBytes = (__u8 *)data;

	if(!dev->checkpointBuffer)
		return 0;

	if(dev->checkpointOpenForWrite)
		return -1;

	while(i < nBytes && ok) {


		if(dev->checkpointByteOffset < 0 ||
		   dev->checkpointByteOffset >= dev->nDataBytesPerChunk) {

		   	if(dev->checkpointCurrentBlock < 0){
				yaffs_CheckpointFindNextCheckpointBlock(dev);
				dev->checkpointCurrentChunk = 0;
			}

			if(dev->checkpointCurrentBlock < 0)
				ok = 0;
			else {

				chunk = dev->checkpointCurrentBlock * dev->nChunksPerBlock +
				          dev->checkpointCurrentChunk;

				realignedChunk = chunk - dev->chunkOffset;

	   			/* read in the next chunk */
	   			/* printf("read checkpoint page %d\n",dev->checkpointPage); */
				dev->readChunkWithTagsFromNAND(dev, realignedChunk,
							       dev->checkpointBuffer,
							      &tags);

				if(tags.chunkId != (dev->checkpointPageSequence + 1) ||
				   tags.sequenceNumber != YAFFS_SEQUENCE_CHECKPOINT_DATA)
				   ok = 0;

				dev->checkpointByteOffset = 0;
				dev->checkpointPageSequence++;
				dev->checkpointCurrentChunk++;

				if(dev->checkpointCurrentChunk >= dev->nChunksPerBlock)
					dev->checkpointCurrentBlock = -1;
			}
		}

		if(ok){
			*dataBytes = dev->checkpointBuffer[dev->checkpointByteOffset];
			dev->checkpointSum += *dataBytes;
			dev->checkpointXor ^= *dataBytes;
			dev->checkpointByteOffset++;
			i++;
			dataBytes++;
			dev->checkpointByteCount++;
		}
	}

	return 	i;
}

int yaffs_CheckpointClose(yaffs_Device *dev)
{

	if(dev->checkpointOpenForWrite){
		if(dev->checkpointByteOffset != 0)
			yaffs_CheckpointFlushBuffer(dev);
	} else {
		int i;
		for(i = 0; i < dev->blocksInCheckpoint && dev->checkpointBlockList[i] >= 0; i++){
			yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev,dev->checkpointBlockList[i]);
			if(bi->blockState == YAFFS_BLOCK_STATE_EMPTY)
				bi->blockState = YAFFS_BLOCK_STATE_CHECKPOINT;
			else {
				// Todo this looks odd...
			}
		}
		YFREE(dev->checkpointBlockList);
		dev->checkpointBlockList = NULL;
	}

	dev->nFreeChunks -= dev->blocksInCheckpoint * dev->nChunksPerBlock;
	dev->nErasedBlocks -= dev->blocksInCheckpoint;


	T(YAFFS_TRACE_CHECKPOINT,(TSTR("checkpoint byte count %d" TENDSTR),
			dev->checkpointByteCount));

	if(dev->checkpointBuffer){
		/* free the buffer */
		YFREE(dev->checkpointBuffer);
		dev->checkpointBuffer = NULL;
		return 1;
	}
	else
		return 0;

}

int yaffs_CheckpointInvalidateStream(yaffs_Device *dev)
{
	/* Erase the first checksum block */

	T(YAFFS_TRACE_CHECKPOINT,(TSTR("checkpoint invalidate"TENDSTR)));

	if(!yaffs_CheckpointSpaceOk(dev))
		return 0;

	return yaffs_CheckpointErase(dev);
}

/*********************************************************yaffsfs.c***********************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include "yaffsfs.h"
#include "yaffs_guts.h"
#include "yaffscfg.h"
#include <string.h> // for memset
#include "yportenv.h"

#define YAFFSFS_MAX_SYMLINK_DEREFERENCES 5

#ifndef NULL
#define NULL ((void *)0)
#endif


const char *yaffsfs_c_version="$Id: yaffsfs.c,v 1.18 2007-07-18 19:40:38 charles Exp $";

// configurationList is the list of devices that are supported
static yaffsfs_DeviceConfiguration *yaffsfs_configurationList;


/* Some forward references */
static yaffs_Object *yaffsfs_FindObject(yaffs_Object *relativeDirectory, const char *path, int symDepth);
static void yaffsfs_RemoveObjectCallback(yaffs_Object *obj);


// Handle management.
// 


unsigned int yaffs_wr_attempts;

typedef struct
{
	__u8  inUse:1;		// this handle is in use
	__u8  readOnly:1;	// this handle is read only
	__u8  append:1;		// append only
	__u8  exclusive:1;	// exclusive
	__u32 position;		// current position in file
	yaffs_Object *obj;	// the object
}yaffsfs_Handle;


static yaffsfs_Handle yaffsfs_handle[YAFFSFS_N_HANDLES];

// yaffsfs_InitHandle
/// Inilitalise handles on start-up.
//
static int yaffsfs_InitHandles(void)
{
	int i;
	for(i = 0; i < YAFFSFS_N_HANDLES; i++)
	{
		yaffsfs_handle[i].inUse = 0;
		yaffsfs_handle[i].obj = NULL;
	}
	return 0;
}

yaffsfs_Handle *yaffsfs_GetHandlePointer(int h)
{
	if(h < 0 || h >= YAFFSFS_N_HANDLES)
	{
		return NULL;
	}
	
	return &yaffsfs_handle[h];
}

yaffs_Object *yaffsfs_GetHandleObject(int handle)
{
	yaffsfs_Handle *h = yaffsfs_GetHandlePointer(handle);

	if(h && h->inUse)
	{
		return h->obj;
	}
	
	return NULL;
}


//yaffsfs_GetHandle
// Grab a handle (when opening a file)
//

static int yaffsfs_GetHandle(void)
{
	int i;
	yaffsfs_Handle *h;
	
	for(i = 0; i < YAFFSFS_N_HANDLES; i++)
	{
		h = yaffsfs_GetHandlePointer(i);
		if(!h)
		{
			// todo bug: should never happen
		}
		if(!h->inUse)
		{
			memset(h,0,sizeof(yaffsfs_Handle));
			h->inUse=1;
			return i;
		}
	}
	return -1;
}

// yaffs_PutHandle
// Let go of a handle (when closing a file)
//
static int yaffsfs_PutHandle(int handle)
{
	yaffsfs_Handle *h = yaffsfs_GetHandlePointer(handle);
	
	if(h)
	{
		h->inUse = 0;
		h->obj = NULL;
	}
	return 0;
}



// Stuff to search for a directory from a path


int yaffsfs_Match(char a, char b)
{
	// case sensitive
	return (a == b);
}

// yaffsfs_FindDevice
// yaffsfs_FindRoot
// Scan the configuration list to find the root.
// Curveballs: Should match paths that end in '/' too
// Curveball2 Might have "/x/ and "/x/y". Need to return the longest match
static yaffs_Device *yaffsfs_FindDevice(const char *path, char **restOfPath)
{
	yaffsfs_DeviceConfiguration *cfg = yaffsfs_configurationList;
	const char *leftOver;
	const char *p;
	yaffs_Device *retval = NULL;
	int thisMatchLength;
	int longestMatch = -1;
	
	// Check all configs, choose the one that:
	// 1) Actually matches a prefix (ie /a amd /abc will not match
	// 2) Matches the longest.
	while(cfg && cfg->prefix && cfg->dev)
	{
		leftOver = path;
		p = cfg->prefix;
		thisMatchLength = 0;
		
		while(*p &&  //unmatched part of prefix 
		      strcmp(p,"/") && // the rest of the prefix is not / (to catch / at end)
		      *leftOver && 
		      yaffsfs_Match(*p,*leftOver))
		{
			p++;
			leftOver++;
			thisMatchLength++;
		}
		if((!*p || strcmp(p,"/") == 0) &&      // end of prefix
		   (!*leftOver || *leftOver == '/') && // no more in this path name part
		   (thisMatchLength > longestMatch))
		{
			// Matched prefix
			*restOfPath = (char *)leftOver;
			retval = cfg->dev;
			longestMatch = thisMatchLength;
		}
		cfg++;
	}
	return retval;
}

static yaffs_Object *yaffsfs_FindRoot(const char *path, char **restOfPath)
{

	yaffs_Device *dev;
	
	dev= yaffsfs_FindDevice(path,restOfPath);
	if(dev && dev->isMounted)
	{
		return dev->rootDir;
	}
	return NULL;
}

static yaffs_Object *yaffsfs_FollowLink(yaffs_Object *obj,int symDepth)
{

	while(obj && obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK)
	{
		char *alias = obj->variant.symLinkVariant.alias;
						
		if(*alias == '/')
		{
			// Starts with a /, need to scan from root up
			obj = yaffsfs_FindObject(NULL,alias,symDepth++);
		}
		else
		{
			// Relative to here, so use the parent of the symlink as a start
			obj = yaffsfs_FindObject(obj->parent,alias,symDepth++);
		}
	}
	return obj;
}


// yaffsfs_FindDirectory
// Parse a path to determine the directory and the name within the directory.
//
// eg. "/data/xx/ff" --> puts name="ff" and returns the directory "/data/xx"
static yaffs_Object *yaffsfs_DoFindDirectory(yaffs_Object *startDir,const char *path,char **name,int symDepth)
{
	yaffs_Object *dir;
	char *restOfPath;
	char str[YAFFS_MAX_NAME_LENGTH+1];
	int i;
	
	if(symDepth > YAFFSFS_MAX_SYMLINK_DEREFERENCES)
	{
		return NULL;
	}
	
	if(startDir)
	{
		dir = startDir;
		restOfPath = (char *)path;
	}
	else
	{
		dir = yaffsfs_FindRoot(path,&restOfPath);
	}
	
	while(dir)
	{	
		// parse off /.
		// curve ball: also throw away surplus '/' 
		// eg. "/ram/x////ff" gets treated the same as "/ram/x/ff"
		while(*restOfPath == '/')
		{
			restOfPath++; // get rid of '/'
		}
		
		*name = restOfPath;
		i = 0;
		
		while(*restOfPath && *restOfPath != '/')
		{
			if (i < YAFFS_MAX_NAME_LENGTH)
			{
				str[i] = *restOfPath;
				str[i+1] = '\0';
				i++;
			}
			restOfPath++;
		}
		
		if(!*restOfPath)
		{
			// got to the end of the string
			return dir;
		}
		else
		{
			if(strcmp(str,".") == 0)
			{
				// Do nothing
			}
			else if(strcmp(str,"..") == 0)
			{
				dir = dir->parent;
			}
			else
			{
				dir = yaffs_FindObjectByName(dir,str);
				
				while(dir && dir->variantType == YAFFS_OBJECT_TYPE_SYMLINK)
				{
				
					dir = yaffsfs_FollowLink(dir,symDepth);
		
				}
				
				if(dir && dir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY)
				{
					dir = NULL;
				}
			}
		}
	}
	// directory did not exist.
	return NULL;
}

static yaffs_Object *yaffsfs_FindDirectory(yaffs_Object *relativeDirectory,const char *path,char **name,int symDepth)
{
	return yaffsfs_DoFindDirectory(relativeDirectory,path,name,symDepth);
}

// yaffsfs_FindObject turns a path for an existing object into the object
// 
static yaffs_Object *yaffsfs_FindObject(yaffs_Object *relativeDirectory, const char *path,int symDepth)
{
	yaffs_Object *dir;
	char *name;
	
	dir = yaffsfs_FindDirectory(relativeDirectory,path,&name,symDepth);
	
	if(dir && *name)
	{
		return yaffs_FindObjectByName(dir,name);
	}
	
	return dir;
}



int yaffs_open(const char *path, int oflag, int mode)
{
	yaffs_Object *obj = NULL;
	yaffs_Object *dir = NULL;
	char *name;
	int handle = -1;
	yaffsfs_Handle *h = NULL;
	int alreadyOpen = 0;
	int alreadyExclusive = 0;
	int openDenied = 0;
	int symDepth = 0;
	int errorReported = 0;
	
	int i;
	
	
	// todo sanity check oflag (eg. can't have O_TRUNC without WRONLY or RDWR
	
	
	yaffsfs_Lock();
	
	handle = yaffsfs_GetHandle();
	
	if(handle >= 0)
	{

		h = yaffsfs_GetHandlePointer(handle);
	
	
		// try to find the exisiting object
		obj = yaffsfs_FindObject(NULL,path,0);
		
		if(obj && obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK)
		{
		
			obj = yaffsfs_FollowLink(obj,symDepth++);
		}

		if(obj)
		{
			// Check if the object is already in use
			alreadyOpen = alreadyExclusive = 0;
			
			for(i = 0; i <= YAFFSFS_N_HANDLES; i++)
			{
				
				if(i != handle &&
				   yaffsfs_handle[i].inUse &&
				    obj == yaffsfs_handle[i].obj)
				 {
				 	alreadyOpen = 1;
					if(yaffsfs_handle[i].exclusive)
					{
						alreadyExclusive = 1;
					}
				 }
			}

			if(((oflag & O_EXCL) && alreadyOpen) || alreadyExclusive)
			{
				openDenied = 1;
			}
			
			// Open should fail if O_CREAT and O_EXCL are specified
			if((oflag & O_EXCL) && (oflag & O_CREAT))
			{
				openDenied = 1;
				yaffsfs_SetError(-EEXIST);
				errorReported = 1;
			}
			
			// Check file permissions
			if( (oflag & (O_RDWR | O_WRONLY)) == 0 &&     // ie O_RDONLY
			   !(obj->yst_mode & S_IREAD))
			{
				openDenied = 1;
			}

			if( (oflag & O_RDWR) && 
			   !(obj->yst_mode & S_IREAD))
			{
				openDenied = 1;
			}

			if( (oflag & (O_RDWR | O_WRONLY)) && 
			   !(obj->yst_mode & S_IWRITE))
			{
				openDenied = 1;
			}
			
		}
		
		else if((oflag & O_CREAT))
		{
			// Let's see if we can create this file
			dir = yaffsfs_FindDirectory(NULL,path,&name,0);
			if(dir)
			{
				obj = yaffs_MknodFile(dir,name,mode,0,0);	
			}
			else
			{
				yaffsfs_SetError(-ENOTDIR);
			}
		}
		
		if(obj && !openDenied)
		{
			h->obj = obj;
			h->inUse = 1;
	    	h->readOnly = (oflag & (O_WRONLY | O_RDWR)) ? 0 : 1;
			h->append =  (oflag & O_APPEND) ? 1 : 0;
			h->exclusive = (oflag & O_EXCL) ? 1 : 0;
			h->position = 0;
			
			obj->inUse++;
			if((oflag & O_TRUNC) && !h->readOnly)
			{
				//todo truncate
				yaffs_ResizeFile(obj,0);
			}
			
		}
		else
		{
			yaffsfs_PutHandle(handle);
			if(!errorReported)
			{
				yaffsfs_SetError(-EACCESS);
				errorReported = 1;
			}
			handle = -1;
		}
		
	}
	
	yaffsfs_Unlock();
	
	return handle;		
}

int yaffs_close(int fd)
{
	yaffsfs_Handle *h = NULL;
	int retVal = 0;
	
	yaffsfs_Lock();

	h = yaffsfs_GetHandlePointer(fd);
	
	if(h && h->inUse)
	{
		// clean up
		yaffs_FlushFile(h->obj,1);
		h->obj->inUse--;
                if(h->obj->inUse <= 0 && h->obj->unlinked)
		{
			yaffs_DeleteFile(h->obj);
		}
		yaffsfs_PutHandle(fd);
		retVal = 0;
	}
	else
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
		retVal = -1;
	}
	
	yaffsfs_Unlock();
	
	return retVal;
}

int yaffs_read(int fd, void *buf, unsigned int nbyte)
{
	yaffsfs_Handle *h = NULL;
	yaffs_Object *obj = NULL;
	int pos = 0;
	int nRead = -1;
	int maxRead;
	
	yaffsfs_Lock();
	h = yaffsfs_GetHandlePointer(fd);
	obj = yaffsfs_GetHandleObject(fd);
	
	if(!h || !obj)
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
	}
	else if( h && obj)
	{
		pos=  h->position;
		if(yaffs_GetObjectFileLength(obj) > pos)
		{
			maxRead = yaffs_GetObjectFileLength(obj) - pos;
		}
		else
		{
			maxRead = 0;
		}

		if(nbyte > maxRead)
		{
			nbyte = maxRead;
		}

		
		if(nbyte > 0)
		{
			nRead = yaffs_ReadDataFromFile(obj,buf,pos,nbyte);
			if(nRead >= 0)
			{
			       h->position = pos + nRead;
                              
			}
			else
			{
				//todo error
			}
		}
		else
		{
			nRead = 0;
		}
		
	}
	
	yaffsfs_Unlock();
	
	
	return (nRead >= 0) ? nRead : -1;
		
}

int yaffs_write(int fd, const void *buf, unsigned int nbyte)
{
	yaffsfs_Handle *h = NULL;
	yaffs_Object *obj = NULL;
	int pos = 0;
	int nWritten = -1;
	int writeThrough = 0;
	
	yaffsfs_Lock();
	h = yaffsfs_GetHandlePointer(fd);
	obj = yaffsfs_GetHandleObject(fd);
	
	if(!h || !obj)
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
	}
	else if( h && obj && h->readOnly)
	{
		// todo error
	}
	else if( h && obj)
	{
		if(h->append)
		{
			pos =  yaffs_GetObjectFileLength(obj);
		}
		else
		{
			pos = h->position;
		}
		
		nWritten = yaffs_WriteDataToFile(obj,buf,pos,nbyte,writeThrough);
		
		if(nWritten >= 0)
		{
			h->position = pos + nWritten;
		}
		else
		{
			//todo error
		}
		
	}
	
	yaffsfs_Unlock();
	
	
	return (nWritten >= 0) ? nWritten : -1;

}

int yaffs_truncate(int fd, off_t newSize)
{
	yaffsfs_Handle *h = NULL;
	yaffs_Object *obj = NULL;
	int result = 0;
	
	yaffsfs_Lock();
	h = yaffsfs_GetHandlePointer(fd);
	obj = yaffsfs_GetHandleObject(fd);
	
	if(!h || !obj)
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
	}
	else
	{
		// resize the file
		result = yaffs_ResizeFile(obj,newSize);
	}	
	yaffsfs_Unlock();
	
	
	return (result) ? 0 : -1;

}

off_t yaffs_lseek(int fd, off_t offset, int whence) 
{
	yaffsfs_Handle *h = NULL;
	yaffs_Object *obj = NULL;
	int pos = -1;
	int fSize = -1;
	
	yaffsfs_Lock();
	h = yaffsfs_GetHandlePointer(fd);
	obj = yaffsfs_GetHandleObject(fd);
	
	if(!h || !obj)
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
	}
	else if(whence == SEEK_SET)
	{
		if(offset >= 0)
		{
			pos = offset;
		}
	}
	else if(whence == SEEK_CUR)
	{
		if( (h->position + offset) >= 0)
		{
			pos = (h->position + offset);
		}
	}
	else if(whence == SEEK_END)
	{
		fSize = yaffs_GetObjectFileLength(obj);
		if(fSize >= 0 && (fSize + offset) >= 0)
		{
			pos = fSize + offset;
		}
	}
	
	if(pos >= 0)
	{
		h->position = pos;
	}
	else
	{
		// todo error
	}

	
	yaffsfs_Unlock();
	
	return pos;
}


int yaffsfs_DoUnlink(const char *path,int isDirectory) 
{
	yaffs_Object *dir = NULL;
	yaffs_Object *obj = NULL;
	char *name;
	int result = YAFFS_FAIL;
	
	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0);
	dir = yaffsfs_FindDirectory(NULL,path,&name,0);
	if(!dir)
	{
		yaffsfs_SetError(-ENOTDIR);
	}
	else if(!obj)
	{
		yaffsfs_SetError(-ENOENT);
	}
	else if(!isDirectory && obj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY)
	{
		yaffsfs_SetError(-EISDIR);
	}
	else if(isDirectory && obj->variantType != YAFFS_OBJECT_TYPE_DIRECTORY)
	{
		yaffsfs_SetError(-ENOTDIR);
	}
	else
	{
		result = yaffs_Unlink(dir,name);
		
		if(result == YAFFS_FAIL && isDirectory)
		{
			yaffsfs_SetError(-ENOTEMPTY);
		}
	}
	
	yaffsfs_Unlock();
	
	// todo error
	
	return (result == YAFFS_FAIL) ? -1 : 0;
}
int yaffs_rmdir(const char *path) 
{
	return yaffsfs_DoUnlink(path,1);
}

int yaffs_unlink(const char *path) 
{
	return yaffsfs_DoUnlink(path,0);
}

int yaffs_rename(const char *oldPath, const char *newPath)
{
	yaffs_Object *olddir = NULL;
	yaffs_Object *newdir = NULL;
	yaffs_Object *obj = NULL;
	char *oldname;
	char *newname;
	int result= YAFFS_FAIL;
	int renameAllowed = 1;
	
	yaffsfs_Lock();
	
	olddir = yaffsfs_FindDirectory(NULL,oldPath,&oldname,0);
	newdir = yaffsfs_FindDirectory(NULL,newPath,&newname,0);
	obj = yaffsfs_FindObject(NULL,oldPath,0);
	
	if(!olddir || !newdir || !obj)
	{
		// bad file
		yaffsfs_SetError(-EBADF);	
		renameAllowed = 0;	
	}
	else if(olddir->myDev != newdir->myDev)
	{
		// oops must be on same device
		// todo error
		yaffsfs_SetError(-EXDEV);
		renameAllowed = 0;	
	}
	else if(obj && obj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY)
	{
		// It is a directory, check that it is not being renamed to 
		// being its own decendent.
		// Do this by tracing from the new directory back to the root, checking for obj
		
		yaffs_Object *xx = newdir;
		
		while( renameAllowed && xx)
		{
			if(xx == obj)
			{
				renameAllowed = 0;
			}
			xx = xx->parent;
		}
		if(!renameAllowed) yaffsfs_SetError(-EACCESS);
	}
	
	if(renameAllowed)
	{
		result = yaffs_RenameObject(olddir,oldname,newdir,newname);
	}
	
	yaffsfs_Unlock();
	
	return (result == YAFFS_FAIL) ? -1 : 0;	
}


static int yaffsfs_DoStat(yaffs_Object *obj,struct yaffs_stat *buf)
{
	int retVal = -1;

	if(obj)
	{
		obj = yaffs_GetEquivalentObject(obj);
	}

	if(obj && buf)
	{
    	buf->st_dev = (int)obj->myDev->genericDevice;
    	buf->st_ino = obj->objectId;
    	buf->st_mode = obj->yst_mode & ~S_IFMT; // clear out file type bits
	
		if(obj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY) 
		{
			buf->st_mode |= S_IFDIR;
		}
		else if(obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK) 
		{
			buf->st_mode |= S_IFLNK;
		}
		else if(obj->variantType == YAFFS_OBJECT_TYPE_FILE)
		{
			buf->st_mode |= S_IFREG;
		}
		
    	buf->st_nlink = yaffs_GetObjectLinkCount(obj);
    	buf->st_uid = 0;    
    	buf->st_gid = 0;;     
    	buf->st_rdev = obj->yst_rdev;
    	buf->st_size = yaffs_GetObjectFileLength(obj);
		buf->st_blksize = obj->myDev->nDataBytesPerChunk;
    	buf->st_blocks = (buf->st_size + buf->st_blksize -1)/buf->st_blksize;
    	buf->yst_atime = obj->yst_atime; 
    	buf->yst_ctime = obj->yst_ctime; 
    	buf->yst_mtime = obj->yst_mtime; 
		retVal = 0;
	}
	return retVal;
}

static int yaffsfs_DoStatOrLStat(const char *path, struct yaffs_stat *buf,int doLStat)
{
	yaffs_Object *obj;
	
	int retVal = -1;
	
	yaffsfs_Lock();
	obj = yaffsfs_FindObject(NULL,path,0);
	
	if(!doLStat && obj)
	{
		obj = yaffsfs_FollowLink(obj,0);
	}
	
	if(obj)
	{
		retVal = yaffsfs_DoStat(obj,buf);
	}
	else
	{
		// todo error not found
		yaffsfs_SetError(-ENOENT);
	}
	
	yaffsfs_Unlock();
	
	return retVal;
	
}

int yaffs_stat(const char *path, struct yaffs_stat *buf)
{
	return yaffsfs_DoStatOrLStat(path,buf,0);
}

int yaffs_lstat(const char *path, struct yaffs_stat *buf)
{
	return yaffsfs_DoStatOrLStat(path,buf,1);
}

int yaffs_fstat(int fd, struct yaffs_stat *buf)
{
	yaffs_Object *obj;
	
	int retVal = -1;
	
	yaffsfs_Lock();
	obj = yaffsfs_GetHandleObject(fd);
	
	if(obj)
	{
		retVal = yaffsfs_DoStat(obj,buf);
	}
	else
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
	}
	
	yaffsfs_Unlock();
	
	return retVal;
}

static int yaffsfs_DoChMod(yaffs_Object *obj,mode_t mode)
{
	int result;

	if(obj)
	{
		obj = yaffs_GetEquivalentObject(obj);
	}
	
	if(obj)
	{
		obj->yst_mode = mode;
		obj->dirty = 1;
		result = yaffs_FlushFile(obj,0);
	}
	
	return result == YAFFS_OK ? 0 : -1;
}


int yaffs_chmod(const char *path, mode_t mode)
{
	yaffs_Object *obj;
	
	int retVal = -1;
	
	yaffsfs_Lock();
	obj = yaffsfs_FindObject(NULL,path,0);
	
	if(obj)
	{
		retVal = yaffsfs_DoChMod(obj,mode);
	}
	else
	{
		// todo error not found
		yaffsfs_SetError(-ENOENT);
	}
	
	yaffsfs_Unlock();
	
	return retVal;
	
}


int yaffs_fchmod(int fd, mode_t mode)
{
	yaffs_Object *obj;
	
	int retVal = -1;
	
	yaffsfs_Lock();
	obj = yaffsfs_GetHandleObject(fd);
	
	if(obj)
	{
		retVal = yaffsfs_DoChMod(obj,mode);
	}
	else
	{
		// bad handle
		yaffsfs_SetError(-EBADF);		
	}
	
	yaffsfs_Unlock();
	
	return retVal;
}


int yaffs_mkdir(const char *path, mode_t mode)
{
	yaffs_Object *parent = NULL;
	yaffs_Object *dir = NULL;
	char *name;
	int retVal= -1;
	
	yaffsfs_Lock();
	parent = yaffsfs_FindDirectory(NULL,path,&name,0);
	if(parent)
		dir = yaffs_MknodDirectory(parent,name,mode,0,0);
	if(dir)
	{
		retVal = 0;
	}
	else
	{
		yaffsfs_SetError(-ENOSPC); // just assume no space for now
		retVal = -1;
	}
	
	yaffsfs_Unlock();
	
	return retVal;
}

int yaffs_mount(const char *path)
{
	int retVal=-1;
	int result=YAFFS_FAIL;
	yaffs_Device *dev=NULL;
	char *dummy;
	
	T(YAFFS_TRACE_ALWAYS,("yaffs: Mounting %s\n",path));
	
	yaffsfs_Lock();
	dev = yaffsfs_FindDevice(path,&dummy);
	if(dev)
	{
		if(!dev->isMounted)
		{
			result = yaffs_GutsInitialise(dev);
			if(result == YAFFS_FAIL)
			{
				// todo error - mount failed
				yaffsfs_SetError(-ENOMEM);
			}
			retVal = result ? 0 : -1;
			
		}
		else
		{
			//todo error - already mounted.
			yaffsfs_SetError(-EBUSY);
		}
	}
	else
	{
		// todo error - no device
		yaffsfs_SetError(-ENODEV);
	}
	yaffsfs_Unlock();
	return retVal;
	
}

int yaffs_unmount(const char *path)
{
	int retVal=-1;
	yaffs_Device *dev=NULL;
	char *dummy;
	
	yaffsfs_Lock();
	dev = yaffsfs_FindDevice(path,&dummy);
	if(dev)
	{
		if(dev->isMounted)
		{
			int i;
			int inUse;
			
			yaffs_FlushEntireDeviceCache(dev);
			yaffs_CheckpointSave(dev);
			
			for(i = inUse = 0; i < YAFFSFS_N_HANDLES && !inUse; i++)
			{
				if(yaffsfs_handle[i].inUse && yaffsfs_handle[i].obj->myDev == dev)
				{
					inUse = 1; // the device is in use, can't unmount
				}
			}
			
			if(!inUse)
			{
				yaffs_Deinitialise(dev);
					
				retVal = 0;
			}
			else
			{
				// todo error can't unmount as files are open
				yaffsfs_SetError(-EBUSY);
			}
			
		}
		else
		{
			//todo error - not mounted.
			yaffsfs_SetError(-EINVAL);
			
		}
	}
	else
	{
		// todo error - no device
		yaffsfs_SetError(-ENODEV);
	}	
	yaffsfs_Unlock();
	return retVal;
	
}

loff_t yaffs_freespace(const char *path)
{
	loff_t retVal=-1;
	yaffs_Device *dev=NULL;
	char *dummy;
	
	yaffsfs_Lock();
	dev = yaffsfs_FindDevice(path,&dummy);
	if(dev  && dev->isMounted)
	{
		retVal = yaffs_GetNumberOfFreeChunks(dev);
		retVal *= dev->nDataBytesPerChunk;
		
	}
	else
	{
		yaffsfs_SetError(-EINVAL);
	}
	
	yaffsfs_Unlock();
	return retVal;	
}



void yaffs_initialise(yaffsfs_DeviceConfiguration *cfgList)
{
	
	yaffsfs_DeviceConfiguration *cfg;
	
	yaffsfs_configurationList = cfgList;
	
	yaffsfs_InitHandles();
	
	cfg = yaffsfs_configurationList;
	
	while(cfg && cfg->prefix && cfg->dev)
	{
		cfg->dev->isMounted = 0;
		cfg->dev->removeObjectCallback = yaffsfs_RemoveObjectCallback;
		cfg++;
	}
	
	
}


//
// Directory search stuff.

//
// Directory search context
//
// NB this is an opaque structure.


typedef struct
{
	__u32 magic;
	yaffs_dirent de;		/* directory entry being used by this dsc */
	char name[NAME_MAX+1];		/* name of directory being searched */
	yaffs_Object *dirObj;		/* ptr to directory being searched */
	yaffs_Object *nextReturn;	/* obj to be returned by next readddir */
	int offset;
	struct list_head others;	
} yaffsfs_DirectorySearchContext;



static struct list_head search_contexts;


static void yaffsfs_SetDirRewound(yaffsfs_DirectorySearchContext *dsc)
{
	if(dsc &&
	   dsc->dirObj &&
	   dsc->dirObj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY){
	   
	   dsc->offset = 0;
	   
	   if( list_empty(&dsc->dirObj->variant.directoryVariant.children)){
	   	dsc->nextReturn = NULL;
	   } else {
	      	dsc->nextReturn = list_entry(dsc->dirObj->variant.directoryVariant.children.next,
						yaffs_Object,siblings);
	   }
	} else {
		/* Hey someone isn't playing nice! */
	}
}

static void yaffsfs_DirAdvance(yaffsfs_DirectorySearchContext *dsc)
{
	if(dsc &&
	   dsc->dirObj &&
	   dsc->dirObj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY){
	   
	   if( dsc->nextReturn == NULL ||
	       list_empty(&dsc->dirObj->variant.directoryVariant.children)){
	   	dsc->nextReturn = NULL;
	   } else {
		   struct list_head *next = dsc->nextReturn->siblings.next;
   
		   if( next == &dsc->dirObj->variant.directoryVariant.children)
	   		dsc->nextReturn = NULL; /* end of list */
	   	   else 
		   	dsc->nextReturn = list_entry(next,yaffs_Object,siblings);
	   }
	} else {
		/* Hey someone isn't playing nice! */
	}
}

static void yaffsfs_RemoveObjectCallback(yaffs_Object *obj)
{

	struct list_head *i;
	yaffsfs_DirectorySearchContext *dsc;
	
	/* if search contexts not initilised then skip */
	if(!search_contexts.next)
		return;
		
	/* Iteratethrough the directory search contexts.
	 * If any are the one being removed, then advance the dsc to
	 * the next one to prevent a hanging ptr.
	 */
	 list_for_each(i, &search_contexts) {
		if (i) {
			dsc = list_entry(i, yaffsfs_DirectorySearchContext,others);
			if(dsc->nextReturn == obj)
				yaffsfs_DirAdvance(dsc);
		}
	}
				
}

yaffs_DIR *yaffs_opendir(const char *dirname)
{
	yaffs_DIR *dir = NULL;
 	yaffs_Object *obj = NULL;
	yaffsfs_DirectorySearchContext *dsc = NULL;
	
	yaffsfs_Lock();
	
	obj = yaffsfs_FindObject(NULL,dirname,0);
	
	if(obj && obj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY)
	{
		
		dsc = YMALLOC(sizeof(yaffsfs_DirectorySearchContext));
		dir = (yaffs_DIR *)dsc;
		if(dsc)
		{
			memset(dsc,0,sizeof(yaffsfs_DirectorySearchContext));
			dsc->magic = YAFFS_MAGIC;
			dsc->dirObj = obj;
			strncpy(dsc->name,dirname,NAME_MAX);
			INIT_LIST_HEAD(&dsc->others);
			
			if(!search_contexts.next)
				INIT_LIST_HEAD(&search_contexts);
				
			list_add(&dsc->others,&search_contexts);	
			yaffsfs_SetDirRewound(dsc);		}
	
	}
	
	yaffsfs_Unlock();
	
	return dir;
}

struct yaffs_dirent *yaffs_readdir(yaffs_DIR *dirp)
{
	yaffsfs_DirectorySearchContext *dsc = (yaffsfs_DirectorySearchContext *)dirp;
	struct yaffs_dirent *retVal = NULL;
		
	yaffsfs_Lock();
	
	if(dsc && dsc->magic == YAFFS_MAGIC){
		yaffsfs_SetError(0);
		if(dsc->nextReturn){
			dsc->de.d_ino = yaffs_GetEquivalentObject(dsc->nextReturn)->objectId;
			dsc->de.d_dont_use = (unsigned)dsc->nextReturn;
			dsc->de.d_off = dsc->offset++;
			yaffs_GetObjectName(dsc->nextReturn,dsc->de.d_name,NAME_MAX);
			if(strlen(dsc->de.d_name) == 0)
			{
				// this should not happen!
				strcpy(dsc->de.d_name,"zz");
			}
			dsc->de.d_reclen = sizeof(struct yaffs_dirent);
			retVal = &dsc->de;
			yaffsfs_DirAdvance(dsc);
		} else
			retVal = NULL;
	}
	else
	{
		yaffsfs_SetError(-EBADF);
	}
	
	yaffsfs_Unlock();
	
	return retVal;
	
}


void yaffs_rewinddir(yaffs_DIR *dirp)
{
	yaffsfs_DirectorySearchContext *dsc = (yaffsfs_DirectorySearchContext *)dirp;
	
	yaffsfs_Lock();
	
	yaffsfs_SetDirRewound(dsc);

	yaffsfs_Unlock();
}


int yaffs_closedir(yaffs_DIR *dirp)
{
	yaffsfs_DirectorySearchContext *dsc = (yaffsfs_DirectorySearchContext *)dirp;
		
	yaffsfs_Lock();
	dsc->magic = 0;
	list_del(&dsc->others); /* unhook from list */
	YFREE(dsc);
	yaffsfs_Unlock();
	return 0;
}

// end of directory stuff


int yaffs_symlink(const char *oldpath, const char *newpath)
{
	yaffs_Object *parent = NULL;
	yaffs_Object *obj;
	char *name;
	int retVal= -1;
	int mode = 0; // ignore for now
	
	yaffsfs_Lock();
	parent = yaffsfs_FindDirectory(NULL,newpath,&name,0);
	obj = yaffs_MknodSymLink(parent,name,mode,0,0,oldpath);
	if(obj)
	{
		retVal = 0;
	}
	else
	{
		yaffsfs_SetError(-ENOSPC); // just assume no space for now
		retVal = -1;
	}
	
	yaffsfs_Unlock();
	
	return retVal;
	
}

int yaffs_readlink(const char *path, char *buf, int bufsiz)
{
	yaffs_Object *obj = NULL;
	int retVal;

		
	yaffsfs_Lock();
	
	obj = yaffsfs_FindObject(NULL,path,0);
	
	if(!obj)
	{
		yaffsfs_SetError(-ENOENT);
		retVal = -1;
	}
	else if(obj->variantType != YAFFS_OBJECT_TYPE_SYMLINK)
	{
		yaffsfs_SetError(-EINVAL);
		retVal = -1;
	}
	else
	{
		char *alias = obj->variant.symLinkVariant.alias;
		memset(buf,0,bufsiz);
		strncpy(buf,alias,bufsiz - 1);
		retVal = 0;
	}
	yaffsfs_Unlock();
	return retVal;
}

int yaffs_link(const char *oldpath, const char *newpath)
{
	// Creates a link called newpath to existing oldpath
	yaffs_Object *obj = NULL;
	yaffs_Object *target = NULL;
	int retVal = 0;

		
	yaffsfs_Lock();
	
	obj = yaffsfs_FindObject(NULL,oldpath,0);
	target = yaffsfs_FindObject(NULL,newpath,0);
	
	if(!obj)
	{
		yaffsfs_SetError(-ENOENT);
		retVal = -1;
	}
	else if(target)
	{
		yaffsfs_SetError(-EEXIST);
		retVal = -1;
	}
	else	
	{
		yaffs_Object *newdir = NULL;
		yaffs_Object *link = NULL;
		
		char *newname;
		
		newdir = yaffsfs_FindDirectory(NULL,newpath,&newname,0);
		
		if(!newdir)
		{
			yaffsfs_SetError(-ENOTDIR);
			retVal = -1;
		}
		else if(newdir->myDev != obj->myDev)
		{
			yaffsfs_SetError(-EXDEV);
			retVal = -1;
		}
		if(newdir && strlen(newname) > 0)
		{
			link = yaffs_Link(newdir,newname,obj);
			if(link)
				retVal = 0;
			else
			{
				yaffsfs_SetError(-ENOSPC);
				retVal = -1;
			}

		}
	}
	yaffsfs_Unlock();
	
	return retVal;
}

int yaffs_mknod(const char *pathname, mode_t mode, dev_t dev);

int yaffs_DumpDevStruct(const char *path)
{
	char *rest;
	
	yaffs_Object *obj = yaffsfs_FindRoot(path,&rest);
	
	if(obj)
	{
		yaffs_Device *dev = obj->myDev;
		
		printf("\n"
			   "nPageWrites.......... %d\n"
			   "nPageReads........... %d\n"
			   "nBlockErasures....... %d\n"
			   "nGCCopies............ %d\n"
			   "garbageCollections... %d\n"
			   "passiveGarbageColl'ns %d\n"
			   "\n",
				dev->nPageWrites,
				dev->nPageReads,
				dev->nBlockErasures,
				dev->nGCCopies,
				dev->garbageCollections,
				dev->passiveGarbageCollections
		);
		
	}
	return 0;
}

/*****************************************************************yaffscfg2k.c************************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * yaffscfg2k.c  The configuration for the "direct" use of yaffs.
 *
 * This file is intended to be modified to your requirements.
 * There is no need to redistribute this file.
 */

#include "yaffscfg.h"
#include "yaffsfs.h"
#include "yaffs_fileem2k.h"
#include "yaffs_nandemul2k.h"

#include <errno.h>

unsigned yaffs_traceMask = 

	YAFFS_TRACE_SCAN |  
	YAFFS_TRACE_GC | YAFFS_TRACE_GC_DETAIL | 
	YAFFS_TRACE_ERASE | 
	YAFFS_TRACE_TRACING | 
	YAFFS_TRACE_ALLOCATE | 
	YAFFS_TRACE_CHECKPOINT |
	YAFFS_TRACE_BAD_BLOCKS |
	YAFFS_TRACE_VERIFY |
	YAFFS_TRACE_VERIFY_NAND |
	YAFFS_TRACE_VERIFY_FULL |
//	(~0) |
	
	0;
        


void yaffsfs_SetError(int err)
{
	//Do whatever to set error
	errno = err;
}

void yaffsfs_Lock(void)
{
}

void yaffsfs_Unlock(void)
{
}

__u32 yaffsfs_CurrentTime(void)
{
	return 0;
}


static int yaffs_kill_alloc = 0;
static size_t total_malloced = 0;
static size_t malloc_limit = 0 & 6000000;

void *yaffs_malloc(size_t size)
{
	size_t this;
	if(yaffs_kill_alloc)
		return NULL;
	if(malloc_limit && malloc_limit <(total_malloced + size) )
		return NULL;

	this = malloc(size);
	if(this)
		total_malloced += size;
	return this;
}

void yaffs_free(void *ptr)
{
	free(ptr);
}

void yaffsfs_LocalInitialisation(void)
{
	// Define locking semaphore.
}

// Configuration for:
// /ram  2MB ramdisk
// /boot 2MB boot disk (flash)
// /flash 14MB flash disk (flash)
// NB Though /boot and /flash occupy the same physical device they
// are still disticnt "yaffs_Devices. You may think of these as "partitions"
// using non-overlapping areas in the same device.
// 

#include "yaffs_ramdisk.h"
#include "yaffs_flashif.h"
#include "yaffs_nandemul2k.h"

static yaffs_Device ramDev;
static yaffs_Device bootDev;
static yaffs_Device flashDev;
static yaffs_Device ram2kDev;

static yaffsfs_DeviceConfiguration yaffsfs_config[] = {
#if 0
	{ "/ram", &ramDev},
	{ "/ram2k", &ram2kDev},
	{(void *)0,(void *)0}
#else
	{ "/", &ramDev},
	{ "/ram2k", &ram2kDev},
	{(void *)0,(void *)0} /* Null entry to terminate list */
#endif
};


int yaffs_StartUp(void)
{
	// Stuff to configure YAFFS
	// Stuff to initialise anything special (eg lock semaphore).
	yaffsfs_LocalInitialisation();
	
	// Set up devices
	// /ram
	memset(&ramDev,0,sizeof(ramDev));
	ramDev.nDataBytesPerChunk = 512;
	ramDev.nChunksPerBlock = 32;
	ramDev.nReservedBlocks = 2; // Set this smaller for RAM
	ramDev.startBlock = 0; // Can use block 0
	ramDev.endBlock = 127; // Last block in 2MB.	
	//ramDev.useNANDECC = 1;
	ramDev.nShortOpCaches = 0;	// Disable caching on this device.
	ramDev.genericDevice = (void *) 0;	// Used to identify the device in fstat.
	ramDev.writeChunkWithTagsToNAND = yramdisk_WriteChunkWithTagsToNAND;
	ramDev.readChunkWithTagsFromNAND = yramdisk_ReadChunkWithTagsFromNAND;
	ramDev.eraseBlockInNAND = yramdisk_EraseBlockInNAND;
	ramDev.initialiseNAND = yramdisk_InitialiseNAND;

	/*
	// /boot
	memset(&bootDev,0,sizeof(bootDev));
	bootDev.nDataBytesPerChunk = 512;
	bootDev.nChunksPerBlock = 32;
	bootDev.nReservedBlocks = 5;
	bootDev.startBlock = 0; // Can use block 0
	bootDev.endBlock = 63; // Last block
	//bootDev.useNANDECC = 0; // use YAFFS's ECC
	bootDev.nShortOpCaches = 10; // Use caches
	bootDev.genericDevice = (void *) 1;	// Used to identify the device in fstat.
	bootDev.writeChunkWithTagsToNAND = yflash_WriteChunkWithTagsToNAND;
	bootDev.readChunkWithTagsFromNAND = yflash_ReadChunkWithTagsFromNAND;
	bootDev.eraseBlockInNAND = yflash_EraseBlockInNAND;
	bootDev.initialiseNAND = yflash_InitialiseNAND;
	bootDev.markNANDBlockBad = yflash_MarkNANDBlockBad;
	bootDev.queryNANDBlock = yflash_QueryNANDBlock;



	// /flash
	// Set this puppy up to use
	// the file emulation space as
	// 2kpage/64chunk per block/128MB device
	memset(&flashDev,0,sizeof(flashDev));

	flashDev.nDataBytesPerChunk = 2048;
	flashDev.nChunksPerBlock = 64;
	flashDev.nReservedBlocks = 5;
	//flashDev.checkpointStartBlock = 1;
	//flashDev.checkpointEndBlock = 20;
	flashDev.startBlock = 0;
	flashDev.endBlock = 200; // Make it smaller
	//flashDev.endBlock = yflash_GetNumberOfBlocks()-1;
	flashDev.isYaffs2 = 1;
	flashDev.wideTnodesDisabled=0;
	flashDev.nShortOpCaches = 10; // Use caches
	flashDev.genericDevice = (void *) 2;	// Used to identify the device in fstat.
	flashDev.writeChunkWithTagsToNAND = yflash_WriteChunkWithTagsToNAND;
	flashDev.readChunkWithTagsFromNAND = yflash_ReadChunkWithTagsFromNAND;
	flashDev.eraseBlockInNAND = yflash_EraseBlockInNAND;
	flashDev.initialiseNAND = yflash_InitialiseNAND;
	flashDev.markNANDBlockBad = yflash_MarkNANDBlockBad;
	flashDev.queryNANDBlock = yflash_QueryNANDBlock;

	*/

	// /ram2k
	// Set this puppy up to use
	// the file emulation space as
	// 2kpage/64chunk per block/128MB device
	memset(&ram2kDev,0,sizeof(ram2kDev));

	ram2kDev.nDataBytesPerChunk = nandemul2k_GetBytesPerChunk();
	ram2kDev.nChunksPerBlock = nandemul2k_GetChunksPerBlock();
	ram2kDev.nReservedBlocks = 5;
	ram2kDev.startBlock = 0; // First block after /boot
	//ram2kDev.endBlock = 127; // Last block in 16MB
	ram2kDev.endBlock = nandemul2k_GetNumberOfBlocks() - 1; // Last block in 512MB
	ram2kDev.isYaffs2 = 1;
	ram2kDev.nShortOpCaches = 10; // Use caches
	ram2kDev.genericDevice = (void *) 3;	// Used to identify the device in fstat.
	ram2kDev.writeChunkWithTagsToNAND = nandemul2k_WriteChunkWithTagsToNAND;
	ram2kDev.readChunkWithTagsFromNAND = nandemul2k_ReadChunkWithTagsFromNAND;
	ram2kDev.eraseBlockInNAND = nandemul2k_EraseBlockInNAND;
	ram2kDev.initialiseNAND = nandemul2k_InitialiseNAND;
	ram2kDev.markNANDBlockBad = nandemul2k_MarkNANDBlockBad;
	ram2kDev.queryNANDBlock = nandemul2k_QueryNANDBlock;

	yaffs_initialise(yaffsfs_config);
	
	return 0;
}



void SetCheckpointReservedBlocks(int n)
{
//	flashDev.nCheckpointReservedBlocks = n;
}
#if 0
/***************************************************************yaffscfg.c*************************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * yaffscfg.c  The configuration for the "direct" use of yaffs.
 *
 * This file is intended to be modified to your requirements.
 * There is no need to redistribute this file.
 */

#include "yaffscfg.h"
#include "yaffsfs.h"
#include <errno.h>

unsigned yaffs_traceMask = 0xFFFFFFFF;


void yaffsfs_SetError(int err)
{
	//Do whatever to set error
	errno = err;
}

void yaffsfs_Lock(void)
{
}

void yaffsfs_Unlock(void)
{
}

__u32 yaffsfs_CurrentTime(void)
{
	return 0;
}

void *yaffs_malloc(size_t size)
{
	return malloc(size);
}

void yaffs_free(void *ptr)
{
	free(ptr);
}

void yaffsfs_LocalInitialisation(void)
{
	// Define locking semaphore.
}

// Configuration for:
// /ram  2MB ramdisk
// /boot 2MB boot disk (flash)
// /flash 14MB flash disk (flash)
// NB Though /boot and /flash occupy the same physical device they
// are still disticnt "yaffs_Devices. You may think of these as "partitions"
// using non-overlapping areas in the same device.
// 

#include "yaffs_ramdisk.h"
#include "yaffs_flashif.h"

static yaffs_Device ramDev;
static yaffs_Device bootDev;
static yaffs_Device flashDev;

static yaffsfs_DeviceConfiguration yaffsfs_config[] = {

	{ "/ram", &ramDev},
	{ "/boot", &bootDev},
	{ "/flash", &flashDev},
	{(void *)0,(void *)0}
};


int yaffs_StartUp(void)
{
	// Stuff to configure YAFFS
	// Stuff to initialise anything special (eg lock semaphore).
	yaffsfs_LocalInitialisation();
	
	// Set up devices

	// /ram
	ramDev.nDataBytesPerChunk = 512;
	ramDev.nChunksPerBlock = 32;
	ramDev.nReservedBlocks = 2; // Set this smaller for RAM
	ramDev.startBlock = 1; // Can't use block 0
	ramDev.endBlock = 127; // Last block in 2MB.	
	ramDev.useNANDECC = 1;
	ramDev.nShortOpCaches = 0;	// Disable caching on this device.
	ramDev.genericDevice = (void *) 0;	// Used to identify the device in fstat.
	ramDev.writeChunkWithTagsToNAND = yramdisk_WriteChunkWithTagsToNAND;
	ramDev.readChunkWithTagsFromNAND = yramdisk_ReadChunkWithTagsFromNAND;
	ramDev.eraseBlockInNAND = yramdisk_EraseBlockInNAND;
	ramDev.initialiseNAND = yramdisk_InitialiseNAND;

	// /boot
	bootDev.nDataBytesPerChunk = 512;
	bootDev.nChunksPerBlock = 32;
	bootDev.nReservedBlocks = 5;
	bootDev.startBlock = 1; // Can't use block 0
	bootDev.endBlock = 127; // Last block in 2MB.	
	bootDev.useNANDECC = 0; // use YAFFS's ECC
	bootDev.nShortOpCaches = 10; // Use caches
	bootDev.genericDevice = (void *) 1;	// Used to identify the device in fstat.
	bootDev.writeChunkToNAND = yflash_WriteChunkToNAND;
	bootDev.readChunkFromNAND = yflash_ReadChunkFromNAND;
	bootDev.eraseBlockInNAND = yflash_EraseBlockInNAND;
	bootDev.initialiseNAND = yflash_InitialiseNAND;

		// /flash
	flashDev.nDataBytesPerChunk =  512;
	flashDev.nChunksPerBlock = 32;
	flashDev.nReservedBlocks = 5;
	flashDev.startBlock = 128; // First block after 2MB
	flashDev.endBlock = 1023; // Last block in 16MB
	flashDev.useNANDECC = 0; // use YAFFS's ECC
	flashDev.nShortOpCaches = 10; // Use caches
	flashDev.genericDevice = (void *) 2;	// Used to identify the device in fstat.
	flashDev.writeChunkToNAND = yflash_WriteChunkToNAND;
	flashDev.readChunkFromNAND = yflash_ReadChunkFromNAND;
	flashDev.eraseBlockInNAND = yflash_EraseBlockInNAND;
	flashDev.initialiseNAND = yflash_InitialiseNAND;

	yaffs_initialise(yaffsfs_config);
	
	return 0;
}
#endif
/*****************************************yaffs_tagsvalidity.c************************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "yaffs_tagsvalidity.h"

void yaffs_InitialiseTags(yaffs_ExtendedTags * tags)
{
	memset(tags, 0, sizeof(yaffs_ExtendedTags));
	tags->validMarker0 = 0xAAAAAAAA;
	tags->validMarker1 = 0x55555555;
}

int yaffs_ValidateTags(yaffs_ExtendedTags * tags)
{
	return (tags->validMarker0 == 0xAAAAAAAA &&
		tags->validMarker1 == 0x55555555);

}

/****************************************************yaffs_tagscompat.c***************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "yaffs_guts.h"
#include "yaffs_tagscompat.h"
#include "yaffs_ecc.h"

static void yaffs_HandleReadDataError(yaffs_Device * dev, int chunkInNAND);
#ifdef NOTYET
static void yaffs_CheckWrittenBlock(yaffs_Device * dev, int chunkInNAND);
static void yaffs_HandleWriteChunkOk(yaffs_Device * dev, int chunkInNAND,
				     const __u8 * data,
				     const yaffs_Spare * spare);
static void yaffs_HandleUpdateChunk(yaffs_Device * dev, int chunkInNAND,
				    const yaffs_Spare * spare);
static void yaffs_HandleWriteChunkError(yaffs_Device * dev, int chunkInNAND);
#endif

static const char yaffs_countBitsTable[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};
#if 0
static int yaffs_CountBits(__u8 x)
{
	int retVal;
	retVal = yaffs_countBitsTable[x];
	return retVal;
}
#endif
/********** Tags ECC calculations  *********/

void yaffs_CalcECC(const __u8 * data, yaffs_Spare * spare)
{
	yaffs_ECCCalculate(data, spare->ecc1);
	yaffs_ECCCalculate(&data[256], spare->ecc2);
}

void yaffs_CalcTagsECC(yaffs_Tags * tags)
{
	/* Calculate an ecc */

	unsigned char *b = ((yaffs_TagsUnion *) tags)->asBytes;
	unsigned i, j;
	unsigned ecc = 0;
	unsigned bit = 0;

	tags->ecc = 0;

	for (i = 0; i < 8; i++) {
		for (j = 1; j & 0xff; j <<= 1) {
			bit++;
			if (b[i] & j) {
				ecc ^= bit;
			}
		}
	}

	tags->ecc = ecc;

}

int yaffs_CheckECCOnTags(yaffs_Tags * tags)
{
	unsigned ecc = tags->ecc;

	yaffs_CalcTagsECC(tags);

	ecc ^= tags->ecc;

	if (ecc && ecc <= 64) {
		/* TODO: Handle the failure better. Retire? */
		unsigned char *b = ((yaffs_TagsUnion *) tags)->asBytes;

		ecc--;

		b[ecc / 8] ^= (1 << (ecc & 7));

		/* Now recvalc the ecc */
		yaffs_CalcTagsECC(tags);

		return 1;	/* recovered error */
	} else if (ecc) {
		/* Wierd ecc failure value */
		/* TODO Need to do somethiong here */
		return -1;	/* unrecovered error */
	}

	return 0;
}

/********** Tags **********/

static void yaffs_LoadTagsIntoSpare(yaffs_Spare * sparePtr,
				    yaffs_Tags * tagsPtr)
{
	yaffs_TagsUnion *tu = (yaffs_TagsUnion *) tagsPtr;

	yaffs_CalcTagsECC(tagsPtr);

	sparePtr->tagByte0 = tu->asBytes[0];
	sparePtr->tagByte1 = tu->asBytes[1];
	sparePtr->tagByte2 = tu->asBytes[2];
	sparePtr->tagByte3 = tu->asBytes[3];
	sparePtr->tagByte4 = tu->asBytes[4];
	sparePtr->tagByte5 = tu->asBytes[5];
	sparePtr->tagByte6 = tu->asBytes[6];
	sparePtr->tagByte7 = tu->asBytes[7];
}

static void yaffs_GetTagsFromSpare(yaffs_Device * dev, yaffs_Spare * sparePtr,
				   yaffs_Tags * tagsPtr)
{
	yaffs_TagsUnion *tu = (yaffs_TagsUnion *) tagsPtr;
	int result;

	tu->asBytes[0] = sparePtr->tagByte0;
	tu->asBytes[1] = sparePtr->tagByte1;
	tu->asBytes[2] = sparePtr->tagByte2;
	tu->asBytes[3] = sparePtr->tagByte3;
	tu->asBytes[4] = sparePtr->tagByte4;
	tu->asBytes[5] = sparePtr->tagByte5;
	tu->asBytes[6] = sparePtr->tagByte6;
	tu->asBytes[7] = sparePtr->tagByte7;

	result = yaffs_CheckECCOnTags(tagsPtr);
	if (result > 0) {
		dev->tagsEccFixed++;
	} else if (result < 0) {
		dev->tagsEccUnfixed++;
	}
}

static void yaffs_SpareInitialise(yaffs_Spare * spare)
{
	memset(spare, 0xFF, sizeof(yaffs_Spare));
}

static int yaffs_WriteChunkToNAND(struct yaffs_DeviceStruct *dev,
				  int chunkInNAND, const __u8 * data,
				  yaffs_Spare * spare)
{
	if (chunkInNAND < dev->startBlock * dev->nChunksPerBlock) {
		T(YAFFS_TRACE_ERROR,
		  (TSTR("**>> yaffs chunk %d is not valid" TENDSTR),
		   chunkInNAND));
		return YAFFS_FAIL;
	}

	dev->nPageWrites++;
	return dev->writeChunkToNAND(dev, chunkInNAND, data, spare);
}

static int yaffs_ReadChunkFromNAND(struct yaffs_DeviceStruct *dev,
				   int chunkInNAND,
				   __u8 * data,
				   yaffs_Spare * spare,
				   yaffs_ECCResult * eccResult,
				   int doErrorCorrection)
{
	int retVal;
	yaffs_Spare localSpare;

	dev->nPageReads++;

	if (!spare && data) {
		/* If we don't have a real spare, then we use a local one. */
		/* Need this for the calculation of the ecc */
		spare = &localSpare;
	}

	if (!dev->useNANDECC) {
		retVal = dev->readChunkFromNAND(dev, chunkInNAND, data, spare);
		if (data && doErrorCorrection) {
			/* Do ECC correction */
			/* Todo handle any errors */
			int eccResult1, eccResult2;
			__u8 calcEcc[3];

			yaffs_ECCCalculate(data, calcEcc);
			eccResult1 =
			    yaffs_ECCCorrect(data, spare->ecc1, calcEcc);
			yaffs_ECCCalculate(&data[256], calcEcc);
			eccResult2 =
			    yaffs_ECCCorrect(&data[256], spare->ecc2, calcEcc);

			if (eccResult1 > 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>yaffs ecc error fix performed on chunk %d:0"
				    TENDSTR), chunkInNAND));
				dev->eccFixed++;
			} else if (eccResult1 < 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>yaffs ecc error unfixed on chunk %d:0"
				    TENDSTR), chunkInNAND));
				dev->eccUnfixed++;
			}

			if (eccResult2 > 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>yaffs ecc error fix performed on chunk %d:1"
				    TENDSTR), chunkInNAND));
				dev->eccFixed++;
			} else if (eccResult2 < 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>yaffs ecc error unfixed on chunk %d:1"
				    TENDSTR), chunkInNAND));
				dev->eccUnfixed++;
			}

			if (eccResult1 || eccResult2) {
				/* We had a data problem on this page */
				yaffs_HandleReadDataError(dev, chunkInNAND);
			}

			if (eccResult1 < 0 || eccResult2 < 0)
				*eccResult = YAFFS_ECC_RESULT_UNFIXED;
			else if (eccResult1 > 0 || eccResult2 > 0)
				*eccResult = YAFFS_ECC_RESULT_FIXED;
			else
				*eccResult = YAFFS_ECC_RESULT_NO_ERROR;
		}
	} else {
		/* Must allocate enough memory for spare+2*sizeof(int) */
		/* for ecc results from device. */
		struct yaffs_NANDSpare nspare;
		retVal =
		    dev->readChunkFromNAND(dev, chunkInNAND, data,
					   (yaffs_Spare *) & nspare);
		memcpy(spare, &nspare, sizeof(yaffs_Spare));
		if (data && doErrorCorrection) {
			if (nspare.eccres1 > 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>mtd ecc error fix performed on chunk %d:0"
				    TENDSTR), chunkInNAND));
			} else if (nspare.eccres1 < 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>mtd ecc error unfixed on chunk %d:0"
				    TENDSTR), chunkInNAND));
			}

			if (nspare.eccres2 > 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>mtd ecc error fix performed on chunk %d:1"
				    TENDSTR), chunkInNAND));
			} else if (nspare.eccres2 < 0) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("**>>mtd ecc error unfixed on chunk %d:1"
				    TENDSTR), chunkInNAND));
			}

			if (nspare.eccres1 || nspare.eccres2) {
				/* We had a data problem on this page */
				yaffs_HandleReadDataError(dev, chunkInNAND);
			}

			if (nspare.eccres1 < 0 || nspare.eccres2 < 0)
				*eccResult = YAFFS_ECC_RESULT_UNFIXED;
			else if (nspare.eccres1 > 0 || nspare.eccres2 > 0)
				*eccResult = YAFFS_ECC_RESULT_FIXED;
			else
				*eccResult = YAFFS_ECC_RESULT_NO_ERROR;

		}
	}
	return retVal;
}

#ifdef NOTYET
static int yaffs_CheckChunkErased(struct yaffs_DeviceStruct *dev,
				  int chunkInNAND)
{

	static int init = 0;
	static __u8 cmpbuf[YAFFS_BYTES_PER_CHUNK];
	static __u8 data[YAFFS_BYTES_PER_CHUNK];
	/* Might as well always allocate the larger size for */
	/* dev->useNANDECC == true; */
	static __u8 spare[sizeof(struct yaffs_NANDSpare)];

	dev->readChunkFromNAND(dev, chunkInNAND, data, (yaffs_Spare *) spare);

	if (!init) {
		memset(cmpbuf, 0xff, YAFFS_BYTES_PER_CHUNK);
		init = 1;
	}

	if (memcmp(cmpbuf, data, YAFFS_BYTES_PER_CHUNK))
		return YAFFS_FAIL;
	if (memcmp(cmpbuf, spare, 16))
		return YAFFS_FAIL;

	return YAFFS_OK;

}
#endif

/*
 * Functions for robustisizing
 */

static void yaffs_HandleReadDataError(yaffs_Device * dev, int chunkInNAND)
{
	int blockInNAND = chunkInNAND / dev->nChunksPerBlock;

	/* Mark the block for retirement */
	yaffs_GetBlockInfo(dev, blockInNAND)->needsRetiring = 1;
	T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
	  (TSTR("**>>Block %d marked for retirement" TENDSTR), blockInNAND));

	/* TODO:
	 * Just do a garbage collection on the affected block
	 * then retire the block
	 * NB recursion
	 */
}

#ifdef NOTYET
static void yaffs_CheckWrittenBlock(yaffs_Device * dev, int chunkInNAND)
{
}

static void yaffs_HandleWriteChunkOk(yaffs_Device * dev, int chunkInNAND,
				     const __u8 * data,
				     const yaffs_Spare * spare)
{
}

static void yaffs_HandleUpdateChunk(yaffs_Device * dev, int chunkInNAND,
				    const yaffs_Spare * spare)
{
}

static void yaffs_HandleWriteChunkError(yaffs_Device * dev, int chunkInNAND)
{
	int blockInNAND = chunkInNAND / dev->nChunksPerBlock;

	/* Mark the block for retirement */
	yaffs_GetBlockInfo(dev, blockInNAND)->needsRetiring = 1;
	/* Delete the chunk */
	yaffs_DeleteChunk(dev, chunkInNAND, 1, __LINE__);
}

static int yaffs_VerifyCompare(const __u8 * d0, const __u8 * d1,
			       const yaffs_Spare * s0, const yaffs_Spare * s1)
{

	if (memcmp(d0, d1, YAFFS_BYTES_PER_CHUNK) != 0 ||
	    s0->tagByte0 != s1->tagByte0 ||
	    s0->tagByte1 != s1->tagByte1 ||
	    s0->tagByte2 != s1->tagByte2 ||
	    s0->tagByte3 != s1->tagByte3 ||
	    s0->tagByte4 != s1->tagByte4 ||
	    s0->tagByte5 != s1->tagByte5 ||
	    s0->tagByte6 != s1->tagByte6 ||
	    s0->tagByte7 != s1->tagByte7 ||
	    s0->ecc1[0] != s1->ecc1[0] ||
	    s0->ecc1[1] != s1->ecc1[1] ||
	    s0->ecc1[2] != s1->ecc1[2] ||
	    s0->ecc2[0] != s1->ecc2[0] ||
	    s0->ecc2[1] != s1->ecc2[1] || s0->ecc2[2] != s1->ecc2[2]) {
		return 0;
	}

	return 1;
}
#endif				/* NOTYET */

int yaffs_TagsCompatabilityWriteChunkWithTagsToNAND(yaffs_Device * dev,
						    int chunkInNAND,
						    const __u8 * data,
						    const yaffs_ExtendedTags *
						    eTags)
{
	yaffs_Spare spare;
	yaffs_Tags tags;

	yaffs_SpareInitialise(&spare);

	if (eTags->chunkDeleted) {
		spare.pageStatus = 0;
	} else {
		tags.objectId = eTags->objectId;
		tags.chunkId = eTags->chunkId;
		tags.byteCount = eTags->byteCount;
		tags.serialNumber = eTags->serialNumber;

		if (!dev->useNANDECC && data) {
			yaffs_CalcECC(data, &spare);
		}
		yaffs_LoadTagsIntoSpare(&spare, &tags);

	}

	return yaffs_WriteChunkToNAND(dev, chunkInNAND, data, &spare);
}

int yaffs_TagsCompatabilityReadChunkWithTagsFromNAND(yaffs_Device * dev,
						     int chunkInNAND,
						     __u8 * data,
						     yaffs_ExtendedTags * eTags)
{

	yaffs_Spare spare;
	yaffs_Tags tags;
	yaffs_ECCResult eccResult;

	static yaffs_Spare spareFF;
	static int init;

	if (!init) {
		memset(&spareFF, 0xFF, sizeof(spareFF));
		init = 1;
	}

	if (yaffs_ReadChunkFromNAND
	    (dev, chunkInNAND, data, &spare, &eccResult, 1)) {
		/* eTags may be NULL */
		if (eTags) {

			int deleted =
			    (yaffs_CountBits(spare.pageStatus) < 7) ? 1 : 0;

			eTags->chunkDeleted = deleted;
			eTags->eccResult = eccResult;
			eTags->blockBad = 0;	/* We're reading it */
			/* therefore it is not a bad block */
			eTags->chunkUsed =
			    (memcmp(&spareFF, &spare, sizeof(spareFF)) !=
			     0) ? 1 : 0;

			if (eTags->chunkUsed) {
				yaffs_GetTagsFromSpare(dev, &spare, &tags);

				eTags->objectId = tags.objectId;
				eTags->chunkId = tags.chunkId;
				eTags->byteCount = tags.byteCount;
				eTags->serialNumber = tags.serialNumber;
			}
		}

		return YAFFS_OK;
	} else {
		return YAFFS_FAIL;
	}
}

int yaffs_TagsCompatabilityMarkNANDBlockBad(struct yaffs_DeviceStruct *dev,
					    int blockInNAND)
{

	yaffs_Spare spare;

	memset(&spare, 0xff, sizeof(yaffs_Spare));

	spare.blockStatus = 'Y';

	yaffs_WriteChunkToNAND(dev, blockInNAND * dev->nChunksPerBlock, NULL,
			       &spare);
	yaffs_WriteChunkToNAND(dev, blockInNAND * dev->nChunksPerBlock + 1,
			       NULL, &spare);

	return YAFFS_OK;

}

int yaffs_TagsCompatabilityQueryNANDBlock(struct yaffs_DeviceStruct *dev,
					  int blockNo, yaffs_BlockState *
					  state,
					  int *sequenceNumber)
{

	yaffs_Spare spare0, spare1;
	static yaffs_Spare spareFF;
	static int init;
	yaffs_ECCResult dummy;

	if (!init) {
		memset(&spareFF, 0xFF, sizeof(spareFF));
		init = 1;
	}

	*sequenceNumber = 0;

	yaffs_ReadChunkFromNAND(dev, blockNo * dev->nChunksPerBlock, NULL,
				&spare0, &dummy, 1);
	yaffs_ReadChunkFromNAND(dev, blockNo * dev->nChunksPerBlock + 1, NULL,
				&spare1, &dummy, 1);

	if (yaffs_CountBits(spare0.blockStatus & spare1.blockStatus) < 7)
		*state = YAFFS_BLOCK_STATE_DEAD;
	else if (memcmp(&spareFF, &spare0, sizeof(spareFF)) == 0)
		*state = YAFFS_BLOCK_STATE_EMPTY;
	else
		*state = YAFFS_BLOCK_STATE_NEEDS_SCANNING;

	return YAFFS_OK;
}

/*************************************************************yaffs_ramem2k.c*************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * yaffs_ramem2k.c: RAM emulation in-kernel for 2K pages (YAFFS2)
 */


const char *yaffs_ramem2k_c_version = "$Id: yaffs_ramem2k.c,v 1.3 2007-02-14 01:09:06 wookey Exp $";

#ifndef __KERNEL__
#define CONFIG_YAFFS_RAM_ENABLED
#else
#include <linux/config.h>
#endif

#ifdef CONFIG_YAFFS_RAM_ENABLED

#include "yportenv.h"

#include "yaffs_nandemul2k.h"
#include "yaffs_guts.h"
#include "yaffsinterface.h"
#include "devextras.h"
#include "yaffs_packedtags2.h"



#define EM_SIZE_IN_MEG (32)
#define PAGE_DATA_SIZE  (2048)
#define PAGE_SPARE_SIZE (64)
#define PAGES_PER_BLOCK (64)



#define EM_SIZE_IN_BYTES (EM_SIZE_IN_MEG * (1<<20))

#define PAGE_TOTAL_SIZE (PAGE_DATA_SIZE+PAGE_SPARE_SIZE)

#define BLOCK_TOTAL_SIZE (PAGES_PER_BLOCK * PAGE_TOTAL_SIZE)

#define BLOCKS_PER_MEG ((1<<20)/(PAGES_PER_BLOCK * PAGE_DATA_SIZE))


typedef struct 
{
	__u8 data[PAGE_TOTAL_SIZE]; // Data + spare
	int empty;      // is this empty?
} nandemul_Page;


typedef struct
{
	nandemul_Page *page[PAGES_PER_BLOCK];
	int damaged;	
} nandemul_Block;



typedef struct
{
	nandemul_Block**block;
	int nBlocks;
} nandemul_Device;

static nandemul_Device ned;

static int sizeInMB = EM_SIZE_IN_MEG;


static void nandemul_yield(int n)
{
#ifdef __KERNEL__
	if(n > 0) schedule_timeout(n);
#endif

}


static void nandemul_ReallyEraseBlock(int blockNumber)
{
	int i;
	
	nandemul_Block *blk;
	
	if(blockNumber < 0 || blockNumber >= ned.nBlocks)
	{
		return;
	}
	
	blk = ned.block[blockNumber];
	
	for(i = 0; i < PAGES_PER_BLOCK; i++)
	{
		memset(blk->page[i],0xff,sizeof(nandemul_Page));
		blk->page[i]->empty = 1;
	}
	nandemul_yield(2);
}


static int nandemul2k_CalcNBlocks(void)
{
	return EM_SIZE_IN_MEG * BLOCKS_PER_MEG;
}



static int  CheckInit(void)
{
	static int initialised = 0;
	
	int i,j;
	
	int fail = 0;
	int nBlocks; 

	int nAllocated = 0;
	
	if(initialised) 
	{
		return YAFFS_OK;
	}
	
	
	ned.nBlocks = nBlocks = nandemul2k_CalcNBlocks();

	
	ned.block = YMALLOC(sizeof(nandemul_Block*) * nBlocks );
	
	if(!ned.block) return YAFFS_FAIL;
	
	
	

		
	for(i=fail=0; i <nBlocks; i++)
	{
		
		nandemul_Block *blk;
		
		if(!(blk = ned.block[i] = YMALLOC(sizeof(nandemul_Block))))
		{
		 fail = 1;
		}  
		else
		{
			for(j = 0; j < PAGES_PER_BLOCK; j++)
			{
				if((blk->page[j] = YMALLOC(sizeof(nandemul_Page))) == 0)
				{
					fail = 1;
				}
			}
			nandemul_ReallyEraseBlock(i);
			ned.block[i]->damaged = 0;
			nAllocated++;
		}
	}
	
	if(fail)
	{
		//Todo thump pages
		
		for(i = 0; i < nAllocated; i++)
		{
			YFREE(ned.block[i]);
		}
		YFREE(ned.block);
		
		T(YAFFS_TRACE_ALWAYS,("Allocation failed, could only allocate %dMB of %dMB requested.\n",
		   nAllocated/64,sizeInMB));
		return 0;
	}
	
	ned.nBlocks = nBlocks;
	
	initialised = 1;
	
	return 1;
}

int nandemul2k_WriteChunkWithTagsToNAND(yaffs_Device *dev,int chunkInNAND,const __u8 *data, yaffs_ExtendedTags *tags)
{
	int blk;
	int pg;
	int i;
	
	__u8 *x;

	
	blk = chunkInNAND/PAGES_PER_BLOCK;
	pg = chunkInNAND%PAGES_PER_BLOCK;
	
	
	if(data)
	{
		x = ned.block[blk]->page[pg]->data;
		
		for(i = 0; i < PAGE_DATA_SIZE; i++)
		{
			x[i] &=data[i];
		}

		ned.block[blk]->page[pg]->empty = 0;
	}
	
	
	if(tags)
	{
		x = &ned.block[blk]->page[pg]->data[PAGE_DATA_SIZE];
		
		yaffs_PackTags2((yaffs_PackedTags2 *)x,tags);
			
	}
	
	if(tags || data)
	{
		nandemul_yield(1);
	}

	return YAFFS_OK;
}


int nandemul2k_ReadChunkWithTagsFromNAND(yaffs_Device *dev,int chunkInNAND, __u8 *data, yaffs_ExtendedTags *tags)
{
	int blk;
	int pg;
	
	__u8 *x;

	
	
	blk = chunkInNAND/PAGES_PER_BLOCK;
	pg = chunkInNAND%PAGES_PER_BLOCK;
	
	
	if(data)
	{
		memcpy(data,ned.block[blk]->page[pg]->data,PAGE_DATA_SIZE);
	}
	
	
	if(tags)
	{
		x = &ned.block[blk]->page[pg]->data[PAGE_DATA_SIZE];
		
		yaffs_UnpackTags2(tags,(yaffs_PackedTags2 *)x);
	}

	return YAFFS_OK;
}


static int nandemul2k_CheckChunkErased(yaffs_Device *dev,int chunkInNAND)
{
	int blk;
	int pg;
	int i;

	
	
	blk = chunkInNAND/PAGES_PER_BLOCK;
	pg = chunkInNAND%PAGES_PER_BLOCK;
	
	
	for(i = 0; i < PAGE_TOTAL_SIZE; i++)
	{
		if(ned.block[blk]->page[pg]->data[i] != 0xFF)
		{
			return YAFFS_FAIL;
		}
	}

	return YAFFS_OK;

}

int nandemul2k_EraseBlockInNAND(yaffs_Device *dev, int blockNumber)
{
	
	
	if(blockNumber < 0 || blockNumber >= ned.nBlocks)
	{
		T(YAFFS_TRACE_ALWAYS,("Attempt to erase non-existant block %d\n",blockNumber));
	}
	else if(ned.block[blockNumber]->damaged)
	{
		T(YAFFS_TRACE_ALWAYS,("Attempt to erase damaged block %d\n",blockNumber));
	}
	else
	{
		nandemul_ReallyEraseBlock(blockNumber);
	}
	
	return YAFFS_OK;
}

int nandemul2k_InitialiseNAND(yaffs_Device *dev)
{
	CheckInit();
	return YAFFS_OK;
}
 
int nandemul2k_MarkNANDBlockBad(struct yaffs_DeviceStruct *dev, int blockNo)
{
	
	__u8 *x;
	
	x = &ned.block[blockNo]->page[0]->data[PAGE_DATA_SIZE];
	
	memset(x,0,sizeof(yaffs_PackedTags2));
	
	
	return YAFFS_OK;
	
}

int nandemul2k_QueryNANDBlock(struct yaffs_DeviceStruct *dev, int blockNo, yaffs_BlockState *state, int *sequenceNumber)
{
	yaffs_ExtendedTags tags;
	int chunkNo;

	*sequenceNumber = 0;
	
	chunkNo = blockNo * dev->nChunksPerBlock;
	
	nandemul2k_ReadChunkWithTagsFromNAND(dev,chunkNo,NULL,&tags);
	if(tags.blockBad)
	{
		*state = YAFFS_BLOCK_STATE_DEAD;
	}
	else if(!tags.chunkUsed)
	{
		*state = YAFFS_BLOCK_STATE_EMPTY;
	}
	else if(tags.chunkUsed)
	{
		*state = YAFFS_BLOCK_STATE_NEEDS_SCANNING;
		*sequenceNumber = tags.sequenceNumber;
	}
	return YAFFS_OK;
}

int nandemul2k_GetBytesPerChunk(void) { return PAGE_DATA_SIZE;}

int nandemul2k_GetChunksPerBlock(void) { return PAGES_PER_BLOCK; }
int nandemul2k_GetNumberOfBlocks(void) {return nandemul2k_CalcNBlocks();}


#endif //YAFFS_RAM_ENABLED


/*******************************************yaffs_qsort.c**********************************/
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "yportenv.h"
//#include <linux/string.h>

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) { 		\
	long i = (n) / sizeof (TYPE); 			\
	register TYPE *pi = (TYPE *) (parmi); 		\
	register TYPE *pj = (TYPE *) (parmj); 		\
	do { 						\
		register TYPE	t = *pi;		\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static __inline void
swapfunc(char *a, char *b, int n, int swaptype)
{
	if (swaptype <= 1)
		swapcode(long, a, b, n)
	else
		swapcode(char, a, b, n)
}

#define swap(a, b)					\
	if (swaptype == 0) {				\
		long t = *(long *)(a);			\
		*(long *)(a) = *(long *)(b);		\
		*(long *)(b) = t;			\
	} else						\
		swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n) 	if ((n) > 0) swapfunc(a, b, n, swaptype)

static __inline char *
med3(char *a, char *b, char *c, int (*cmp)(const void *, const void *))
{
	return cmp(a, b) < 0 ?
	       (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
              :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

void
yaffs_qsort(void *aa, size_t n, size_t es,
	int (*cmp)(const void *, const void *))
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	int d, r, swaptype, swap_cnt;
	register char *a = aa;

loop:	SWAPINIT(a, es);
	swap_cnt = 0;
	if (n < 7) {
		for (pm = (char *)a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *)a + (n / 2) * es;
	if (n > 7) {
		pl = (char *)a;
		pn = (char *)a + (n - 1) * es;
		if (n > 40) {
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp);
			pm = med3(pm - d, pm, pm + d, cmp);
			pn = med3(pn - 2 * d, pn - d, pn, cmp);
		}
		pm = med3(pl, pm, pn, cmp);
	}
	swap(a, pm);
	pa = pb = (char *)a + es;

	pc = pd = (char *)a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (r = cmp(pb, a)) <= 0) {
			if (r == 0) {
				swap_cnt = 1;
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (r = cmp(pc, a)) >= 0) {
			if (r == 0) {
				swap_cnt = 1;
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		swap_cnt = 1;
		pb += es;
		pc -= es;
	}
	if (swap_cnt == 0) {  /* Switch to insertion sort */
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}

	pn = (char *)a + n * es;
	r = min(pa - (char *)a, pb - pa);
	vecswap(a, pb - r, r);
	r = min((long)(pd - pc), (long)(pn - pd - es));
	vecswap(pb, pn - r, r);
	if ((r = pb - pa) > es)
		yaffs_qsort(a, r / es, es, cmp);
	if ((r = pd - pc) > es) {
		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		n = r / es;
		goto loop;
	}
/*		yaffs_qsort(pn - r, r / es, es, cmp);*/
}
/*****************************************************yaffs_packedtags2.c************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "yaffs_packedtags2.h"
#include "yportenv.h"
#include "yaffs_tagsvalidity.h"

/* This code packs a set of extended tags into a binary structure for
 * NAND storage
 */

/* Some of the information is "extra" struff which can be packed in to
 * speed scanning
 * This is defined by having the EXTRA_HEADER_INFO_FLAG set.
 */

/* Extra flags applied to chunkId */

#define EXTRA_HEADER_INFO_FLAG	0x80000000
#define EXTRA_SHRINK_FLAG	0x40000000
#define EXTRA_SHADOWS_FLAG	0x20000000
#define EXTRA_SPARE_FLAGS	0x10000000

#define ALL_EXTRA_FLAGS		0xF0000000

/* Also, the top 4 bits of the object Id are set to the object type. */
#define EXTRA_OBJECT_TYPE_SHIFT (28)
#define EXTRA_OBJECT_TYPE_MASK  ((0x0F) << EXTRA_OBJECT_TYPE_SHIFT)

static void yaffs_DumpPackedTags2(const yaffs_PackedTags2 * pt)
{
	T(YAFFS_TRACE_MTD,
	  (TSTR("packed tags obj %d chunk %d byte %d seq %d" TENDSTR),
	   pt->t.objectId, pt->t.chunkId, pt->t.byteCount,
	   pt->t.sequenceNumber));
}

static void yaffs_DumpTags2(const yaffs_ExtendedTags * t)
{
	T(YAFFS_TRACE_MTD,
	  (TSTR
	   ("ext.tags eccres %d blkbad %d chused %d obj %d chunk%d byte "
	    "%d del %d ser %d seq %d"
	    TENDSTR), t->eccResult, t->blockBad, t->chunkUsed, t->objectId,
	   t->chunkId, t->byteCount, t->chunkDeleted, t->serialNumber,
	   t->sequenceNumber));

}

void yaffs_PackTags2(yaffs_PackedTags2 * pt, const yaffs_ExtendedTags * t)
{
	pt->t.chunkId = t->chunkId;
	pt->t.sequenceNumber = t->sequenceNumber;
	pt->t.byteCount = t->byteCount;
	pt->t.objectId = t->objectId;

	if (t->chunkId == 0 && t->extraHeaderInfoAvailable) {
		/* Store the extra header info instead */
		/* We save the parent object in the chunkId */
		pt->t.chunkId = EXTRA_HEADER_INFO_FLAG
			| t->extraParentObjectId;
		if (t->extraIsShrinkHeader) {
			pt->t.chunkId |= EXTRA_SHRINK_FLAG;
		}
		if (t->extraShadows) {
			pt->t.chunkId |= EXTRA_SHADOWS_FLAG;
		}

		pt->t.objectId &= ~EXTRA_OBJECT_TYPE_MASK;
		pt->t.objectId |=
		    (t->extraObjectType << EXTRA_OBJECT_TYPE_SHIFT);

		if (t->extraObjectType == YAFFS_OBJECT_TYPE_HARDLINK) {
			pt->t.byteCount = t->extraEquivalentObjectId;
		} else if (t->extraObjectType == YAFFS_OBJECT_TYPE_FILE) {
			pt->t.byteCount = t->extraFileLength;
		} else {
			pt->t.byteCount = 0;
		}
	}

	yaffs_DumpPackedTags2(pt);
	yaffs_DumpTags2(t);

#ifndef YAFFS_IGNORE_TAGS_ECC
	{
		yaffs_ECCCalculateOther((unsigned char *)&pt->t,
					sizeof(yaffs_PackedTags2TagsPart),
					&pt->ecc);
	}
#endif
}

void yaffs_UnpackTags2(yaffs_ExtendedTags * t, yaffs_PackedTags2 * pt)
{

	memset(t, 0, sizeof(yaffs_ExtendedTags));

	yaffs_InitialiseTags(t);

	if (pt->t.sequenceNumber != 0xFFFFFFFF) {
		/* Page is in use */
#ifdef YAFFS_IGNORE_TAGS_ECC
		{
			t->eccResult = YAFFS_ECC_RESULT_NO_ERROR;
		}
#else
		{
			yaffs_ECCOther ecc;
			int result;
			yaffs_ECCCalculateOther((unsigned char *)&pt->t,
						sizeof
						(yaffs_PackedTags2TagsPart),
						&ecc);
			result =
			    yaffs_ECCCorrectOther((unsigned char *)&pt->t,
						  sizeof
						  (yaffs_PackedTags2TagsPart),
						  &pt->ecc, &ecc);
			switch(result){
				case 0:
					t->eccResult = YAFFS_ECC_RESULT_NO_ERROR;
					break;
				case 1:
					t->eccResult = YAFFS_ECC_RESULT_FIXED;
					break;
				case -1:
					t->eccResult = YAFFS_ECC_RESULT_UNFIXED;
					break;
				default:
					t->eccResult = YAFFS_ECC_RESULT_UNKNOWN;
			}
		}
#endif
		t->blockBad = 0;
		t->chunkUsed = 1;
		t->objectId = pt->t.objectId;
		t->chunkId = pt->t.chunkId;
		t->byteCount = pt->t.byteCount;
		t->chunkDeleted = 0;
		t->serialNumber = 0;
		t->sequenceNumber = pt->t.sequenceNumber;

		/* Do extra header info stuff */

		if (pt->t.chunkId & EXTRA_HEADER_INFO_FLAG) {
			t->chunkId = 0;
			t->byteCount = 0;

			t->extraHeaderInfoAvailable = 1;
			t->extraParentObjectId =
			    pt->t.chunkId & (~(ALL_EXTRA_FLAGS));
			t->extraIsShrinkHeader =
			    (pt->t.chunkId & EXTRA_SHRINK_FLAG) ? 1 : 0;
			t->extraShadows =
			    (pt->t.chunkId & EXTRA_SHADOWS_FLAG) ? 1 : 0;
			t->extraObjectType =
			    pt->t.objectId >> EXTRA_OBJECT_TYPE_SHIFT;
			t->objectId &= ~EXTRA_OBJECT_TYPE_MASK;

			if (t->extraObjectType == YAFFS_OBJECT_TYPE_HARDLINK) {
				t->extraEquivalentObjectId = pt->t.byteCount;
			} else {
				t->extraFileLength = pt->t.byteCount;
			}
		}
	}

	yaffs_DumpPackedTags2(pt);
	yaffs_DumpTags2(t);

}

/*******************************************************yaffs_packedtags1.c********************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "yaffs_packedtags1.h"
#include "yportenv.h"

void yaffs_PackTags1(yaffs_PackedTags1 * pt, const yaffs_ExtendedTags * t)
{
	pt->chunkId = t->chunkId;
	pt->serialNumber = t->serialNumber;
	pt->byteCount = t->byteCount;
	pt->objectId = t->objectId;
	pt->ecc = 0;
	pt->deleted = (t->chunkDeleted) ? 0 : 1;
	pt->unusedStuff = 0;
	pt->shouldBeFF = 0xFFFFFFFF;

}

void yaffs_UnpackTags1(yaffs_ExtendedTags * t, const yaffs_PackedTags1 * pt)
{
	static const __u8 allFF[] =
	    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (memcmp(allFF, pt, sizeof(yaffs_PackedTags1))) {
		t->blockBad = 0;
		if (pt->shouldBeFF != 0xFFFFFFFF) {
			t->blockBad = 1;
		}
		t->chunkUsed = 1;
		t->objectId = pt->objectId;
		t->chunkId = pt->chunkId;
		t->byteCount = pt->byteCount;
		t->eccResult = YAFFS_ECC_RESULT_NO_ERROR;
		t->chunkDeleted = (pt->deleted) ? 0 : 1;
		t->serialNumber = pt->serialNumber;
	} else {
		memset(t, 0, sizeof(yaffs_ExtendedTags));

	}
}

/****************************************************yaffs_ramdisk.c**************************/
/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * yaffs_ramdisk.c: yaffs ram disk component
 * This provides a ram disk under yaffs.
 * NB this is not intended for NAND emulation.
 * Use this with dev->useNANDECC enabled, then ECC overheads are not required.
 */

const char *yaffs_ramdisk_c_version = "$Id: yaffs_ramdisk.c,v 1.4 2007-02-14 01:09:06 wookey Exp $";


#include "yportenv.h"

#include "yaffs_ramdisk.h"
#include "yaffs_guts.h"
#include "devextras.h"
#include "yaffs_packedtags1.h"



#define SIZE_IN_MB2 2

//#define BLOCK_SIZE (32 * 528)
//#define BLOCKS_PER_MEG ((1024*1024)/(32 * 512))





typedef struct 
{
	__u8 data[528]; // Data + spare
} yramdisk_Page;

typedef struct
{
	yramdisk_Page page[32]; // The pages in the block
	
} yramdisk_Block;



typedef struct
{
	yramdisk_Block **block;
	int nBlocks;
} yramdisk_Device;

static yramdisk_Device ramdisk;

static int  CheckInit1(yaffs_Device *dev)
{
	static int initialised = 0;
	
	int i;
	int fail = 0;
	//int nBlocks; 
	int nAllocated = 0;
	
	if(initialised) 
	{
		return YAFFS_OK;
	}

	initialised = 1;
	
	
	ramdisk.nBlocks = (SIZE_IN_MB2 * 1024 * 1024)/(16 * 1024);
	
	ramdisk.block = YMALLOC(sizeof(yramdisk_Block *) * ramdisk.nBlocks);
	
	if(!ramdisk.block) return 0;
	
	for(i=0; i <ramdisk.nBlocks; i++)
	{
		ramdisk.block[i] = NULL;
	}
	
	for(i=0; i <ramdisk.nBlocks && !fail; i++)
	{
		if((ramdisk.block[i] = YMALLOC(sizeof(yramdisk_Block))) == 0)
		{
			fail = 1;
		}
		else
		{
			yramdisk_EraseBlockInNAND(dev,i);
			nAllocated++;
		}
	}
	
	if(fail)
	{
		for(i = 0; i < nAllocated; i++)
		{
			YFREE(ramdisk.block[i]);
		}
		YFREE(ramdisk.block);
		
		T(YAFFS_TRACE_ALWAYS,("Allocation failed, could only allocate %dMB of %dMB requested.\n",
		   nAllocated/64,ramdisk.nBlocks * 528));
		return 0;
	}
	
	
	return 1;
}

int yramdisk_WriteChunkWithTagsToNAND(yaffs_Device *dev,int chunkInNAND,const __u8 *data, yaffs_ExtendedTags *tags)
{
	int blk;
	int pg;
	

	CheckInit1(dev);
	
	blk = chunkInNAND/32;
	pg = chunkInNAND%32;
	
	
	if(data)
	{
		memcpy(ramdisk.block[blk]->page[pg].data,data,512);
	}
	
	
	if(tags)
	{
		yaffs_PackedTags1 pt;
		
		yaffs_PackTags1(&pt,tags);
		memcpy(&ramdisk.block[blk]->page[pg].data[512],&pt,sizeof(pt));
	}

	return YAFFS_OK;	

}


int yramdisk_ReadChunkWithTagsFromNAND(yaffs_Device *dev,int chunkInNAND, __u8 *data, yaffs_ExtendedTags *tags)
{
	int blk;
	int pg;

	
	CheckInit1(dev);
	
	blk = chunkInNAND/32;
	pg = chunkInNAND%32;
	
	
	if(data)
	{
		memcpy(data,ramdisk.block[blk]->page[pg].data,512);
	}
	
	
	if(tags)
	{
		yaffs_PackedTags1 pt;
		
		memcpy(&pt,&ramdisk.block[blk]->page[pg].data[512],sizeof(pt));
		yaffs_UnpackTags1(tags,&pt);
		
	}

	return YAFFS_OK;
}


int yramdisk_CheckChunkErased(yaffs_Device *dev,int chunkInNAND)
{
	int blk;
	int pg;
	int i;

	
	CheckInit1(dev);
	
	blk = chunkInNAND/32;
	pg = chunkInNAND%32;
	
	
	for(i = 0; i < 528; i++)
	{
		if(ramdisk.block[blk]->page[pg].data[i] != 0xFF)
		{
			return YAFFS_FAIL;
		}
	}

	return YAFFS_OK;

}

int yramdisk_EraseBlockInNAND(yaffs_Device *dev, int blockNumber)
{
	
	CheckInit1(dev);
	
	if(blockNumber < 0 || blockNumber >= ramdisk.nBlocks)
	{
		T(YAFFS_TRACE_ALWAYS,("Attempt to erase non-existant block %d\n",blockNumber));
		return YAFFS_FAIL;
	}
	else
	{
		memset(ramdisk.block[blockNumber],0xFF,sizeof(yramdisk_Block));
		return YAFFS_OK;
	}
	
}

int yramdisk_InitialiseNAND(yaffs_Device *dev)
{
	//dev->useNANDECC = 1; // force on useNANDECC which gets faked. 
						 // This saves us doing ECC checks.
	
	return YAFFS_OK;
}


