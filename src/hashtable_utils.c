#include <stdio.h>
#include <stdlib.h>
#include "shmlib.h"
#include "cache.h"
#include "hashtable_utils.h"
#include "global.h"
#include "mcheck.h"

#define GetSSDBufHashBucket(hash_code) ((SSDBufHashBucket *) (ssd_buf_hashtable + (unsigned) (hash_code)))
#define isSameTag(tag1,tag2) ((tag1.offset == tag2.offset) ? 1 : 0)
extern void _LOCK(pthread_mutex_t* lock);
extern void _UNLOCK(pthread_mutex_t* lock);

SSDBufHashBucket* ssd_buf_hashtable;

static SSDBufHashBucket* hashitem_freelist;
static SSDBufHashBucket* buckect_alloc();

static long insertCnt,deleteCnt;

static void releasebucket(SSDBufHashBucket* bucket);
int HashTab_Init()
{
    insertCnt = deleteCnt = 0;
    int stat = SHM_lock_n_check("LOCK_INIT_SSD_HASH_TAB");
    if(stat == 0)
    {
        ssd_buf_hashtable = (SSDBufHashBucket*)SHM_alloc(SHM_SSD_HASH_TAB,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE);
        hashitem_freelist = (SSDBufHashBucket*)SHM_alloc(SHM_SSD_FREE_LIST,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE + 1);
        if(ssd_buf_hashtable == NULL || hashitem_freelist == NULL)
            return -1;

        SSDBufHashBucket* bucket = ssd_buf_hashtable;
        SSDBufHashBucket* freebucket = hashitem_freelist;
        size_t i = 0;
        for(i = 0; i < NBLOCK_SSD_CACHE; bucket++, freebucket++, i++)
        {
            bucket->desp_serial_id = freebucket->desp_serial_id = -1;
            bucket->hash_key.offset = freebucket->hash_key.offset = -1;
            bucket->selfid = freebucket->selfid = i;
            bucket->next_item_id = -1;
            freebucket->next_item_id = i + 1;
        }
        hashitem_freelist[NBLOCK_SSD_CACHE + 1 - 1].selfid = NBLOCK_SSD_CACHE;
	hashitem_freelist[NBLOCK_SSD_CACHE + 1 - 1].desp_serial_id = -1;
	hashitem_freelist[NBLOCK_SSD_CACHE + 1 - 1].hash_key.offset = -1;
        hashitem_freelist[NBLOCK_SSD_CACHE + 1 - 1].next_item_id = -1;
    }
    else
    {
        ssd_buf_hashtable = (SSDBufHashBucket*)SHM_get(SHM_SSD_HASH_TAB,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE);
        hashitem_freelist = (SSDBufHashBucket*)SHM_get(SHM_SSD_FREE_LIST,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE + 1);
    }
    SHM_unlock("LOCK_INIT_SSD_HASH_TAB");
    if(ssd_buf_hashtable == NULL)
	printf("ssd_buf_hashtable == NULL.\n");
    if(hashitem_freelist == NULL)
        printf("hashitem_freelist == NULL.\n");
    return stat;
//    int stat = SHM_lock_n_check("LOCK_SSDBUF_HASHTABLE");
//    if(stat == 0)
//    {
//        ssd_buf_hash_ctrl = (SSDBufHashCtrl*)SHM_alloc(SHM_SSDBUF_HASHTABLE_CTRL,sizeof(SSDBufHashCtrl));
//        ssd_buf_hashtable = (SSDBufHashBucket*)SHM_alloc(SHM_SSDBUF_HASHTABLE,sizeof(SSDBufHashBucket)*size);
//        ssd_buf_hashdesps = (SSDBufHashBucket*)SHM_alloc(SHM_SSDBUF_HASHDESPS,sizeof(SSDBufHashBucket)*NBLOCK_SSD_CACHE);
//
//        SHM_mutex_init(&ssd_buf_hash_ctrl->lock);
//
//        size_t i;
//        SSDBufHashBucket *ssd_buf_hash = ssd_buf_hashtable;
//        for (i = 0; i < size; ssd_buf_hash++, i++)
//        {
//            ssd_buf_hash->ssd_buf_id = -1;
//            ssd_buf_hash->hash_key.offset = -1;
//            ssd_buf_hash->next_item = NULL;
//        }
//        SSDBufHashBucket *hash_desp = ssd_buf_hashdesps;
//        for(i = 0; i <NBLOCK_SSD_CACHE; i++)
//        {
//            ssd_buf_hash->ssd_buf_id = i;
//            ssd_buf_hash->hash_key.offset = -1;
//            ssd_buf_hash->next_item = NULL;
//        }
//    }
//    else
//    {
//        ssd_buf_hash_ctrl = (SSDBufHashCtrl*)SHM_get(SHM_SSDBUF_HASHTABLE_CTRL,sizeof(SSDBufHashCtrl));
//        ssd_buf_hashtable = (SSDBufHashBucket *)SHM_get(SHM_SSDBUF_HASHTABLE,sizeof(SSDBufHashBucket)*size);
//        ssd_buf_hashdesps = (SSDBufHashBucket*)SHM_get(SHM_SSDBUF_HASHDESPS,sizeof(SSDBufHashBucket)*NBLOCK_SSD_CACHE);
//
//    }
//    SHM_unlock("LOCK_SSDBUF_HASHTABLE");
//    return stat;
}

unsigned long HashTab_GetHashCode(SSDBufTag ssd_buf_tag)
{
    unsigned long hashcode = (ssd_buf_tag.offset / SSD_BUFFER_SIZE) % NTABLE_SSD_CACHE;
    return hashcode;
}

long HashTab_Lookup(SSDBufTag ssd_buf_tag, unsigned long hash_code)
{
    if (DEBUG)
        printf("[INFO] Lookup ssd_buf_tag: %lu\n",ssd_buf_tag.offset);
    SSDBufHashBucket *nowbucket = GetSSDBufHashBucket(hash_code);
    while (nowbucket != NULL)
    {
        //printf("in HashTab_Lookup, nowbucket = %lu, hashitem_freelist = %lu.\n");
	if (isSameTag(nowbucket->hash_key, ssd_buf_tag))
        {
            return nowbucket->desp_serial_id;
        }
        if(nowbucket->next_item_id == -1)
            break;
    	//printf("nowbucket->next_item_id = %ld.\n",nowbucket->next_item_id);
	    nowbucket = &hashitem_freelist[nowbucket->next_item_id];
    }

    return -1;
}

long HashTab_Insert(SSDBufTag ssd_buf_tag, unsigned long hash_code, long desp_serial_id)
{
    if (DEBUG)
        printf("[INFO] Insert buf_tag: %lu\n",ssd_buf_tag.offset);

    insertCnt++;
    SSDBufHashBucket *nowbucket = GetSSDBufHashBucket(hash_code);
    if(nowbucket == NULL)
    {
        printf("[ERROR] Insert HashBucket: Cannot get HashBucket.\n");
        exit(1);
    }
    while (nowbucket->next_item_id != -1)
    {
        nowbucket = &hashitem_freelist[nowbucket->next_item_id];
    }

    SSDBufHashBucket* newitem;
    if((newitem  = buckect_alloc()) == NULL)
    {
        printf("hash bucket alloc failure\n");
        exit(-1);
    }
    newitem->hash_key = ssd_buf_tag;
    newitem->desp_serial_id = desp_serial_id;
    newitem->next_item_id = -1;

    nowbucket->next_item_id = newitem->selfid;
    return 0;
}

long HashTab_Delete(SSDBufTag ssd_buf_tag, unsigned long hash_code)
{
    if (DEBUG)
        printf("[INFO] Delete buf_tag: %lu\n",ssd_buf_tag.offset);
    deleteCnt++;
    long del_id;
    SSDBufHashBucket *delitem;
    SSDBufHashBucket *nowbucket = GetSSDBufHashBucket(hash_code);

    while (nowbucket->next_item_id != -1)
    {
        if (isSameTag(hashitem_freelist[nowbucket->next_item_id].hash_key, ssd_buf_tag))
        {
            delitem = &hashitem_freelist[nowbucket->next_item_id];
            del_id = delitem->desp_serial_id;
            nowbucket->next_item_id = delitem->next_item_id;
            releasebucket(delitem);
            return del_id;
        }
        nowbucket = &hashitem_freelist[nowbucket->next_item_id];
    }
    printf("return value of HashTab_Delete is -1.\n");
    return -1;
}

static SSDBufHashBucket* buckect_alloc()
{
    if(hashitem_freelist->next_item_id < 0)
    {
	printf("insertCnt - deleteCnt = %d.\n",insertCnt-deleteCnt);
        return NULL;
    }
    SSDBufHashBucket* freebucket = &hashitem_freelist[hashitem_freelist->next_item_id];
    hashitem_freelist->next_item_id = freebucket->next_item_id;
    freebucket->next_item_id = -1;
    return freebucket;
}

static void releasebucket(SSDBufHashBucket* bucket)
{
    //printf("bucket->next_item_id = %ld.\n",bucket->next_item_id);
    bucket->next_item_id = hashitem_freelist->next_item_id;
    hashitem_freelist->next_item_id = bucket->selfid;
}
