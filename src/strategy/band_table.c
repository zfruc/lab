#include <stdio.h>
#include <stdlib.h>
#include "shmlib.h"
#include "global.h"
#include "band_table.h"

BandHashBucket *band_hashtable;
BandHashBucket* freelist;

static bool isSameband(long band_id1,long band_id2);

void initBandTable(size_t size)
{
	int stat = SHM_lock_n_check("LOCK_STRATEGY_Most_HASHBAND");
	if(stat==0)
	{
		band_hashtable = (BandHashBucket *)SHM_alloc(SHM_Most_HASHTABLE,sizeof(BandHashBucket)*size);
        freelist = (BandHashBucket *)SHM_alloc(SHM_Most_FREELIST,sizeof(BandHashBucket)*NTABLE_SSD_CACHE+1);
		size_t i;
		BandHashBucket *band_hash = band_hashtable;
		for(i = 0;i < size; band_hash++,i++){
			band_hash->band_num = -1;
			band_hash->band_id = -1;
			band_hash->next_item = -1;
            band_hash->self_id = i;
		}
        BandHashBucket *freeptr = freelist;
        for(i = 0;i < NTABLE_SSD_CACHE + 1; freeptr++,i++)
        {
            freeptr->band_num = -1;
            freeptr->band_id = -1;
            freeptr->next_item = i+1;
            freeptr->self_id = i;
        }
        freelist[NTABLE_SSD_CACHE+1 -1].next_item = -1;
	}
	else
    {
        band_hashtable = (BandHashBucket *)SHM_get(SHM_Most_HASHTABLE,sizeof(BandHashBucket)*size);
        freelist = (BandHashBucket *)SHM_get(SHM_Most_FREELIST,sizeof(BandHashBucket)*NTABLE_SSD_CACHE+1);
    }


    SHM_unlock("LOCK_STRATEGY_Most_HASHBAND");
}

unsigned long bandtableHashcode(long band_num)
{
	unsigned long band_hash = band_num % NBANDTables;
	return band_hash;
}

BandHashBucket *bandtableAlloc()
{
    if(freelist->next_item < 0)
        return NULL;
    else
    {
        BandHashBucket *tmp = &freelist[freelist->next_item];
        freelist->next_item = tmp->next_item;
        tmp->next_item = -1;
        return tmp;
    }
}

size_t bandtableLookup(long band_num,unsigned long hash_code)
{
	BandHashBucket *nowbucket = GetBandHashBucket(hash_code, band_hashtable);
	while(nowbucket->next_item != -1){
		if(isSameband(freelist[nowbucket->next_item].band_num,band_num))
			return nowbucket->band_id;	
		nowbucket = &freelist[nowbucket->next_item];
	}
	return -1;
}

long bandtableInsert(long band_num,unsigned long hash_code,long band_id)
{
//	printf("insert table:band_num%ld hash_code:%ld band_id:%ld\n ",band_num,hash_code,band_id);
	BandHashBucket *nowbucket = GetBandHashBucket(hash_code, band_hashtable);
//	while(nowbucket->next_item != -1){
//		nowbucket = &freelist[nowbucket->next_item];
//	}

    BandHashBucket *newitem = bandtableAlloc();
    if(newitem == NULL)
    {
        printf("alloc error in band_table.c.\n");
        exit(-1);
    }
    newitem->band_num = band_num;
    newitem->band_id = band_id;
    newitem->next_item = nowbucket->next_item;
    nowbucket->next_item = newitem->self_id;

//    if(band_num == 0)
//    {
//        printf("an insert record, band_num = 0.\n");
//    }
//    if(hash_code == 0)
//    {
//        printf("now hash_code = 0, nowbucket->next_item = %ld.\n",nowbucket->next_item);
//    }

	return -1;
}

long bandtableDelete(long band_num,unsigned long hash_code)
{
	BandHashBucket *nowbucket = GetBandHashBucket(hash_code, band_hashtable);
	long del_val;
	BandHashBucket *delitem;
	while(nowbucket->next_item != -1){
		if(isSameband(freelist[nowbucket->next_item].band_num,band_num)){
			del_val = freelist[nowbucket->next_item].band_id;
			delitem = &freelist[nowbucket->next_item];
            nowbucket->next_item = delitem->next_item;
            bandtableRelease(delitem);
            return del_val;
		}
		nowbucket = &freelist[nowbucket->next_item];
	}
    while(nowbucket->next_item != -1){
        printf("nowbucket->next_item = %ld.\n",nowbucket->next_item);
        nowbucket = &freelist[nowbucket->next_item];
    }
    printf("band_num = %ld,hash_code = %ld.\n",band_num,hash_code);
    printf("error in bandtableDelete.\n");
    exit(-1);
	return -1;
}

static bool isSameband(long band_num1,long band_num2)
{
	if(band_num1 != band_num2)
		return 0;
	else return 1;
}



void bandtableRelease(BandHashBucket *bucket)
{
    bucket->next_item = freelist->next_item;
    freelist->next_item = bucket->self_id;
}

