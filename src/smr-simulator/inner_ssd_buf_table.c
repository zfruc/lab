#include <stdio.h>
#include <stdlib.h>
#include "shmlib.h"
#include "inner_ssd_buf_table.h"
#include "smr-simulator/simulator_v2.h"
#include "cache.h"

static SSDHashBucket* HashTable;
static SSDHashBucket* Freelist;
#define GetSSDHashBucket(hash_code) ((SSDHashBucket *) (HashTable + (unsigned long) (hash_code)))

#define IsSame(T1, T2) ((T1.offset == T2.offset) ? 1 : 0)

void initSSDTable(size_t size)
{
    int stat = SHM_lock_n_check("LOCK_INIT_SMR_INNER_TABLE");
    if(stat == 0)
    {
        HashTable = (SSDHashBucket*)SHM_alloc(SHM_FIFO_HASH_BUCKET,sizeof(SSDHashBucket)*size);
	if(HashTable == NULL)
	{
		//printf("HashTable allocation fail.\n");
		exit(-1);
	}
	//printf("in initSSDTable,play SHM_alloc, HashTable is %lu.\n",HashTable);
        Freelist = (SSDHashBucket*)SHM_alloc(SHM_FIFO_HASH_FREELIST,sizeof(SSDHashBucket)*(NBLOCK_SMR_FIFO + 1));
        if(Freelist == NULL)
        {
                printf("Freelist allocation fail.\n");
                exit(-1);
        }
	size_t i;
        SSDHashBucket *bucket = HashTable;
        for (i = 0; i < size; bucket++, i++){
            //printf("in initSSDTable , now in loop %lu.\n",i);
	        bucket->despId = -1;
            bucket->hash_key.offset = -1;
            bucket->next_item_id = -1;
            bucket->selfid = i;
        }
        bucket = Freelist;
        for (i = 0; i < NBLOCK_SMR_FIFO; bucket++, i++){
            bucket->despId = -1;
            bucket->hash_key.offset = -1;
            bucket->next_item_id = i + 1;
            bucket->selfid = i;
        }
        Freelist[NBLOCK_SMR_FIFO].despId = -1;
        Freelist[NBLOCK_SMR_FIFO].hash_key.offset = -1;
        Freelist[NBLOCK_SMR_FIFO].next_item_id = -1;
        Freelist[NBLOCK_SMR_FIFO].selfid = NBLOCK_SMR_FIFO;
    }
    else
    {
        HashTable = (SSDHashBucket*)SHM_get(SHM_FIFO_HASH_BUCKET,sizeof(SSDHashBucket)*size);
        //printf("in initSSDTable,play SHM_get, HashTable is %lu.\n",HashTable);
	Freelist = (SSDHashBucket*)SHM_get(SHM_FIFO_HASH_FREELIST,sizeof(SSDHashBucket)*(NBLOCK_SMR_FIFO + 1));
    }
    // printf("in initSSDTable, Table is %lu.\n",HashTable);
    printf("in initSSDTable, Freelist is %lu.\n",Freelist);
    SHM_unlock("LOCK_INIT_SMR_INNER_TABLE");
}

unsigned long ssdtableHashcode(DespTag tag)
{
	unsigned long ssd_hash = (tag.offset / SSD_BUFFER_SIZE) % NBLOCK_SMR_FIFO;
	return ssd_hash;
}

long ssdtableLookup(DespTag tag, unsigned long hash_code)
{
	if (DEBUG)
		printf("[INFO] Lookup tag: %lu\n",tag.offset);
	SSDHashBucket *nowbucket = GetSSDHashBucket(hash_code);
	//printf("in ssdtableLoopup(), hash_code = %lu.\n",hash_code);
	//printf("nowbucket = %lu,HashTable = %lu,hash_code = %lu.\n",nowbucket,HashTable,hash_code);
	while (nowbucket != NULL) {
	//	printf("nowbucket->buf_id = %u %u %u\n", nowbucket->hash_key.rel.database, nowbucket->hash_key.rel.relation, nowbucket->hash_key.block_num);
		//long offset_buc = nowbucket->hash_key.offset;
	//	printf("Freelist = %lu,nowbucket = %lu.\n",Freelist,nowbucket);
		if (IsSame(nowbucket->hash_key, tag)) {
	//		printf("find\n");
			return nowbucket->despId;
		}
        if(nowbucket->next_item_id == -1)
            break;
		nowbucket = &Freelist[nowbucket->next_item_id];
	}
//	printf("no find\n");

	return -1;
}

SSDHashBucket* getAFreeBucket()
{
    if(Freelist->next_item_id != -1)
    {
        _LOCK(&Freelist->lock);
        //printf("now getAFreeBucket hold lock Freelist->lock.\n");
	SSDHashBucket* tmpBucket= &Freelist[Freelist->next_item_id];
        Freelist->next_item_id = Freelist[Freelist->next_item_id].next_item_id;
        _UNLOCK(&Freelist->lock);
	//printf("now getAFreeBucket release lock Freelist->lock.\n");
        return tmpBucket;
    }
    else
        return NULL;
}

void PutBucketBack(SSDHashBucket* backBucket)
{
    _LOCK(&Freelist->lock);
    //printf("now PutBucketBack hold lock Freelist->lock.\n");
    backBucket->next_item_id = Freelist->next_item_id;
    Freelist->next_item_id = backBucket->selfid;
    _UNLOCK(&Freelist->lock);
     //printf("now PutBucketBack release lock Freelist->lock.\n");
}

long ssdtableInsert(DespTag tag, unsigned long hash_code, long despId)
{
	if (DEBUG)
		printf("[INFO] Insert tag: %lu, hash_code=%lu\n",tag.offset, hash_code);
	SSDHashBucket *nowbucket = GetSSDHashBucket(hash_code);
	while (nowbucket->next_item_id != -1) {
		nowbucket = &Freelist[nowbucket->next_item_id];
	}
    SSDHashBucket *newitem = getAFreeBucket();
    newitem->hash_key = tag;
    newitem->despId = despId;
    newitem->next_item_id = -1;
    nowbucket->next_item_id = newitem->selfid;

	return 0;
}

long ssdtableDelete(DespTag tag, unsigned long hash_code)
{
	if (DEBUG)
		printf("[INFO] Delete tag: %lu, hash_code=%lu\n",tag.offset, hash_code);
	SSDHashBucket *nowbucket = GetSSDHashBucket(hash_code);
	long del_id;
	SSDHashBucket *delitem;

	while (nowbucket->next_item_id != -1) {
		if (IsSame(Freelist[nowbucket->next_item_id].hash_key, tag)) {
            delitem = &Freelist[nowbucket->next_item_id];
            del_id = delitem->despId;
            nowbucket->next_item_id = delitem->next_item_id;
            PutBucketBack(delitem);
            return del_id;
		}
		nowbucket = &Freelist[nowbucket->next_item_id];
	}

	return -1;
}

long ssdtableUpdate(DespTag tag, unsigned long hash_code, long despId)
{
	if (DEBUG)
		printf("[INFO] Insert tag: %lu, hash_code=%lu\n",tag.offset, hash_code);
	SSDHashBucket* nowbucket = GetSSDHashBucket(hash_code);
	SSDHashBucket* lastbucket = nowbucket;
	while (nowbucket != NULL) {
        lastbucket = nowbucket;
   //     printf("in ssdtableUpdate , Freelist = %lu, nowbucket = %lu.\n",Freelist,nowbucket);
	    if (IsSame(nowbucket->hash_key,tag)) {
            long oldId = nowbucket->despId;
            nowbucket->despId = despId;
            //printf("oldId returned by ssdtableUpdate is %ld.\n",oldId);
	        return oldId;
		}
        if(nowbucket->next_item_id == -1)
            break;
		nowbucket = &Freelist[nowbucket->next_item_id];
	}

	// if not exist in table, insert one.
    SSDHashBucket *newitem = getAFreeBucket();
    newitem->hash_key = tag;
    newitem->despId = despId;
    newitem->next_item_id = -1;
    lastbucket->next_item_id = newitem->selfid;
 //   printf("oldId returned by ssdtableUpdate is %ld.\n",-1);
    return -1;
}
