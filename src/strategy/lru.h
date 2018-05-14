#ifndef _LRU_GLOBAL_H_
#define _LRU_GLOBAL_H_
#define DEBUG 0

/* ---------------------------lru---------------------------- */

typedef struct
{
	long 		serial_id;			// the corresponding descriptor serial number.
    long        next_lru;               // to link used SSD as LRU, link the ont older than me
    long        last_lru;               // to link used SSD as LRU, link the one newer than me
    pthread_mutex_t lock;
} StrategyDesp_LRU_global;

typedef struct
{
    long        first_lru;          // Head of list of LRU,the newest
    long        last_lru;           // Tail of list of LRU
    pthread_mutex_t lock;
} StrategyCtrl_LRU_global;

extern int initSSDBufferForLRU();
extern long LogOutDesp_lru(unsigned int rw);
extern int HitLruBuffer(long serial_id,unsigned int rw);
extern void LogInLruBuffer(long serial_id,unsigned int rw);
#endif // _LRU_GLOBAL_H_
