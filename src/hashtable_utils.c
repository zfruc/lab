#include <stdio.h>
#include <stdlib.h>
#include "shmlib.h"
#include "cache.h"
#include "hashtable_utils.h"
#include "global.h"
#include "mcheck.h"

#define GetSSDBufHashBucket(hash_code,rw) ((SSDBufHashBucket *) (ssd_buf_hashtable[rw] + (unsigned) (hash_code)))
#define isSameTag(tag1,tag2) ((tag1 == tag2) ? 1 : 0)
extern void _LOCK(pthread_mutex_t* lock);
extern void _UNLOCK(pthread_mutex_t* lock);

SSDBufHashBucket* ssd_buf_hashtable[2];

static SSDBufHashBucket* hashitem_freelist[2];
static SSDBufHashBucket* bucket_alloc(unsigned int rw);

static void releasebucket(SSDBufHashBucket* bucket, unsigned int rw);
int HashTab_Init()
{
    int stat = SHM_lock_n_check("LOCK_INIT_SSD_HASH_TAB");
    if(stat == 0)
    {
        ssd_buf_hashtable[0] = (SSDBufHashBucket*)SHM_alloc(SHM_SSD_HASH_TAB_READ,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE);
	ssd_buf_hashtable[1] = (SSDBufHashBucket*)SHM_alloc(SHM_SSD_HASH_TAB_WRITE,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE);
        hashitem_freelist[0] = (SSDBufHashBucket*)SHM_alloc(SHM_SSD_FREE_LIST_READ,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE + 1);
	hashitem_freelist[1] = (SSDBufHashBucket*)SHM_alloc(SHM_SSD_FREE_LIST_WRITE,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE + 1);
        if(ssd_buf_hashtable[0] == NULL || hashitem_freelist[0] == NULL || ssd_buf_hashtable[1] == NULL || hashitem_freelist[1] == NULL)
            return -1;

        SSDBufHashBucket* bucket = ssd_buf_hashtable[0];
        SSDBufHashBucket* freebucket = hashitem_freelist[0];
        size_t i = 0;
        for(i = 0; i < NBLOCK_SSD_CACHE; bucket++, freebucket++, i++)
        {
            bucket->desp_serial_id = freebucket->desp_serial_id = -1;
            bucket->hash_key.offset = freebucket->hash_key.offset = -1;
            bucket->selfid = freebucket->selfid = i;
            bucket->next_item_id = -1;
            freebucket->next_item_id = i + 1;
        }
        hashitem_freelist[0][NBLOCK_SSD_CACHE + 1 - 1].selfid = NBLOCK_SSD_CACHE;
	hashitem_freelist[0][NBLOCK_SSD_CACHE + 1 - 1].desp_serial_id = -1;
	hashitem_freelist[0][NBLOCK_SSD_CACHE + 1 - 1].hash_key.offset = -1;
        hashitem_freelist[0][NBLOCK_SSD_CACHE + 1 - 1].next_item_id = -1;
        hashitem_freelist[0]->remaining = NTABLE_SSD_CACHE;
        hashitem_freelist[0]->insertCnt = hashitem_freelist[0]->deleteCnt = 0;

	bucket = ssd_buf_hashtable[1];
        freebucket = hashitem_freelist[1];
        for(i = 0; i < NBLOCK_SSD_CACHE; bucket++, freebucket++, i++)
        {
            bucket->desp_serial_id = freebucket->desp_serial_id = -1;
            bucket->hash_key.offset = freebucket->hash_key.offset = -1;
            bucket->selfid = freebucket->selfid = i;
            bucket->next_item_id = -1;
            freebucket->next_item_id = i + 1;
        }
        hashitem_freelist[1][NBLOCK_SSD_CACHE + 1 - 1].selfid = NBLOCK_SSD_CACHE;
	hashitem_freelist[1][NBLOCK_SSD_CACHE + 1 - 1].desp_serial_id = -1;
	hashitem_freelist[1][NBLOCK_SSD_CACHE + 1 - 1].hash_key.offset = -1;
        hashitem_freelist[1][NBLOCK_SSD_CACHE + 1 - 1].next_item_id = -1;
        hashitem_freelist[1]->remaining = NTABLE_SSD_CACHE;
        hashitem_freelist[1]->insertCnt = hashitem_freelist[1]->deleteCnt = 0;
    }
    else
    {
        ssd_buf_hashtable[0] = (SSDBufHashBucket*)SHM_get(SHM_SSD_HASH_TAB_READ,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE);
	ssd_buf_hashtable[1] = (SSDBufHashBucket*)SHM_get(SHM_SSD_HASH_TAB_WRITE,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE);
        hashitem_freelist[0] = (SSDBufHashBucket*)SHM_get(SHM_SSD_FREE_LIST_READ,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE + 1);
	hashitem_freelist[1] = (SSDBufHashBucket*)SHM_get(SHM_SSD_FREE_LIST_WRITE,sizeof(SSDBufHashBucket)*NTABLE_SSD_CACHE + 1);
    }
    SHM_unlock("LOCK_INIT_SSD_HASH_TAB");
    if(ssd_buf_hashtable[0] == NULL || ssd_buf_hashtable[1] == NULL)
	    printf("ssd_buf_hashtable == NULL.\n");
    if(hashitem_freelist[0] == NULL || hashitem_freelist[1] == NULL)
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
   // printf("into HashTab_GetHashCode().\n");
    unsigned long hashcode = (ssd_buf_tag.offset / SSD_BUFFER_SIZE) % NTABLE_SSD_CACHE;
    return hashcode;
}

//1:before inert;2:after insert;3:before delete;4:after delete;5:before read_block;6:after read_block;7:before write_block;8:after write_block
unsigned long Check_Bucket(unsigned long flag)
{
    unsigned long i;
    SSDBufHashBucket *checkbucket;
//    for(i = 0;i<NBLOCK_SSD_CACHE;i++)
//    {
//        checkbucket = ssd_buf_hashtable + (unsigned)i;
//        if(checkbucket->next_item_id == 0)
//        {
//            printf("bucket %ld error. flag = %ld.\n",i,flag);
//        }
//    }
}

long HashTab_Lookup(SSDBufTag ssd_buf_tag, unsigned long hash_code,unsigned int rw)
{
//    printf("into HashTab_Lookup().\n");
    if (DEBUG)
        printf("[INFO] Lookup ssd_buf_tag: %lu\n",ssd_buf_tag.offset);
    SSDBufHashBucket *nowbucket = GetSSDBufHashBucket(hash_code,rw);
    long loop = 0;
    while (nowbucket->next_item_id != -1)
    {
     //   printf("now HashTab_Lookup loop %ld.\n",loop);
        loop++;
        //printf("in HashTab_Lookup, nowbucket = %lu, hashitem_freelist = %lu.\n");
	    if (isSameTag(hashitem_freelist[rw][nowbucket->next_item_id].hash_key.offset, ssd_buf_tag.offset))
        {
            return hashitem_freelist[rw][nowbucket->next_item_id].desp_serial_id;
        }
        nowbucket = &hashitem_freelist[rw][nowbucket->next_item_id];
    }
/*    SSDBufHashBucket *nowbucket = ssd_buf_hashtable;
    while (nowbucket->next_item_id != -1)
    {
	    if (isSameTag(hashitem_freelist[nowbucket->next_item_id].hash_key.offset, ssd_buf_tag.offset))
        {
            return hashitem_freelist[nowbucket->next_item_id].desp_serial_id;
        }
        nowbucket = &hashitem_freelist[nowbucket->next_item_id];
    }
    return -1;*/
}

long HashTab_Insert(SSDBufTag ssd_buf_tag, unsigned long hash_code, long desp_serial_id,unsigned int rw)
{
    Check_Bucket(1);

    if(ssd_buf_tag.offset < 0)
    {
        printf("ssd_buf_tag.offset = %ld,hash_code = %ld,desp_serial_id = %ld.\n",ssd_buf_tag.offset,hash_code,desp_serial_id);
        exit(-1);
    }

    SSDBufHashBucket *nowbucket = GetSSDBufHashBucket(hash_code,rw);

    if (DEBUG)
        printf("[INFO] Insert buf_tag: %lu\n",ssd_buf_tag.offset);

    hashitem_freelist[rw]->insertCnt++;
    if(nowbucket == NULL)
    {
        printf("[ERROR] Insert HashBucket: Cannot get HashBucket.\n");
        exit(1);
    }

    SSDBufHashBucket* newitem;
    if((newitem  = bucket_alloc(rw)) == NULL)
    {
        printf("hash bucket alloc fail, remaining = %ld\n",hashitem_freelist[rw]->remaining);
        exit(-1);
    }

    newitem->hash_key = ssd_buf_tag;
    newitem->desp_serial_id = desp_serial_id;
    newitem->next_item_id = nowbucket->next_item_id;

    nowbucket->next_item_id = newitem->selfid;

//    FILE *insertfile = fopen("/dev/shm/insert_history.txt","a+");
//    fprintf(insertfile,"%ld %ld hash_bucket addr = %ld, newitem->selfid = %ld\n",ssd_buf_tag.offset,hash_code,nowbucket,newitem->selfid);
//    fclose(insertfile);

    if(hashitem_freelist[rw][nowbucket->next_item_id].hash_key.offset<0 || hashitem_freelist[rw][nowbucket->next_item_id].desp_serial_id<0)
    {
        printf("Insert error,negative number appear,ssd_buf_tag.offset = %ld,desp_serial_id = %ld.\n",ssd_buf_tag.offset,desp_serial_id);
        printf("hashitem_freelist[nowbucket->next_item_id].hash_key.offset = %ld,hashitem_freelist[nowbucket->next_item_id].desp_serial_id = %ld.\n",hashitem_freelist[rw][nowbucket->next_item_id].hash_key.offset,hashitem_freelist[rw][nowbucket->next_item_id].desp_serial_id);
        exit(-1);
    }
  //  printf("now remaining = %ld.\n",remaining);


    //check if insert right
    if(HashTab_Lookup(ssd_buf_tag,hash_code,rw)<0)
    {
        printf("not insert correctly.\n");
        exit(-1);
    }

    if(nowbucket->next_item_id == 0)
    {
        printf("in insert function, nowbucket->next_item_id == 0, nowbucket->next_item_id = %ld, newitem->selfid = %ld.\n",nowbucket->next_item_id,newitem->selfid);
        exit(-1);
    }

 //   printf("now in Insert function, operated item selfid = %ld, hash_code = %ld.\n",newitem->selfid,hash_code);

    Check_Bucket(2);


    return 0;
  /*  SSDBufHashBucket* newitem;
    SSDBufHashBucket *nowbucket = ssd_buf_hashtable;
    if((newitem  = bucket_alloc(rw)) == NULL)
    {
        printf("hash bucket alloc fail, remaining = %ld\n",hashitem_freelist->remaining);
        exit(-1);
    }

    newitem->hash_key = ssd_buf_tag;
    newitem->desp_serial_id = desp_serial_id;
    newitem->next_item_id = nowbucket->next_item_id;

    nowbucket->next_item_id = newitem->selfid;
    return 0;*/
}

long HashTab_Delete(SSDBufTag ssd_buf_tag, unsigned long hash_code,unsigned int rw)
{
    Check_Bucket(3);
    if (DEBUG)
        printf("[INFO] Delete buf_tag: %lu\n",ssd_buf_tag.offset);

    long del_id,despId;
    SSDBufHashBucket *delitem;
    SSDBufHashBucket *nowbucket = GetSSDBufHashBucket(hash_code,rw);

    //check if it is in hash bucket
    if(HashTab_Lookup(ssd_buf_tag,hash_code,rw)<0)
    {
        printf("didn't found in correspond bucket, ssd_buf_tag.offset = %ld, hash_code = %ld.\n",ssd_buf_tag.offset,hash_code);
     //   exit(-1);
        while (nowbucket->next_item_id != -1)
        {
            printf("item->selfid = %ld.\n",nowbucket->next_item_id);
            nowbucket = &hashitem_freelist[rw][nowbucket->next_item_id];
        }
    }

    while (nowbucket->next_item_id != -1)
    {
        if (isSameTag(hashitem_freelist[rw][nowbucket->next_item_id].hash_key.offset, ssd_buf_tag.offset))
        {
            del_id = nowbucket->next_item_id;
            delitem = &hashitem_freelist[rw][del_id];
            despId = delitem->desp_serial_id;
            nowbucket->next_item_id = delitem->next_item_id;

//            FILE *deletefile = fopen("/dev/shm/delete_history.txt","a+");
//            fprintf(deletefile,"%ld %ld delitem add = %ld, item->selfid = %ld\n",delitem->hash_key.offset,hash_code,delitem,del_id);
//            fclose(deletefile);

            releasebucket(delitem,rw);
            hashitem_freelist[rw]->deleteCnt++;

            Check_Bucket(4);

            /*for debug*/
            nowbucket = GetSSDBufHashBucket(hash_code,rw);
            if(nowbucket->next_item_id==0)
            {
                printf("After delete,nowbucket->next_item_id = 0, hash_code = %ld\n",hash_code);
                exit(-1);
            }

            return despId;
        }
 //       printf("in HashTab_Delete(), self nowbucket item->selfid = %ld.\n",hashitem_freelist[nowbucket->next_item_id].selfid);
        long nextId = nowbucket -> next_item_id;
        nowbucket = &hashitem_freelist[rw][nextId];
    }
    printf("return value of HashTab_Delete is -1.hash_code = %ld,ssd_buf_tag.offset = %ld\n",hash_code,ssd_buf_tag.offset);

    Check_Bucket(4);

    /*for debug*/
    nowbucket = GetSSDBufHashBucket(hash_code,rw);
    if(nowbucket->next_item_id==0)
    {
        printf("After delete,nowbucket->next_item_id = 0, hash_code = %ld\n",hash_code);
        exit(-1);
    }

    return -1;

/*
    long del_id,despId;
    SSDBufHashBucket *delitem;
    SSDBufHashBucket *nowbucket = ssd_buf_hashtable;
    while (nowbucket->next_item_id != -1)
    {
        if (isSameTag(hashitem_freelist[nowbucket->next_item_id].hash_key.offset, ssd_buf_tag.offset))
        {
            del_id = nowbucket->next_item_id;
            delitem = &hashitem_freelist[del_id];
            despId = delitem->desp_serial_id;
            nowbucket->next_item_id = delitem->next_item_id;

            releasebucket(delitem);
            hashitem_freelist->deleteCnt++;
            return del_id;
        }
        long nextId = nowbucket -> next_item_id;
        nowbucket = &hashitem_freelist[nextId];
    }
    return -1;*/
}

static SSDBufHashBucket* bucket_alloc(unsigned int rw)
{
    if(hashitem_freelist[rw]->next_item_id <= 0)
    {
	    printf("hashitem_freelist->insertCnt = %ld,hashitem_freelist->deleteCnt = %ld,hashitem_freelist->insertCnt - hashitem_freelist->deleteCnt = %d.\n",hashitem_freelist[rw]->insertCnt,hashitem_freelist[rw]->deleteCnt,hashitem_freelist[rw]->insertCnt-hashitem_freelist[rw]->deleteCnt);
        return NULL;
    }
    long freeId = hashitem_freelist[rw]->next_item_id;
    SSDBufHashBucket* freebucket = &hashitem_freelist[rw][freeId];
    hashitem_freelist[rw]->next_item_id = freebucket->next_item_id;
    freebucket->next_item_id = -1;
    hashitem_freelist[rw]->remaining--;
 //   printf("hashitem_freelist->next_item_id = %ld.\n",hashitem_freelist->next_item_id);
    return freebucket;
}

static void releasebucket(SSDBufHashBucket* bucket,unsigned int rw)
{
    //printf("bucket->next_item_id = %ld.\n",bucket->next_item_id);
    bucket->next_item_id = hashitem_freelist[rw]->next_item_id;
    bucket->hash_key.offset = -1;
    bucket->desp_serial_id = -1;
    hashitem_freelist[rw]->next_item_id = bucket->selfid;
    hashitem_freelist[rw]->remaining++;
  //  printf("hashitem_freelist->next_item_id = %ld.\n",hashitem_freelist->next_item_id);
}
