#ifndef _GLOBAL_H
#define _GLOBAL_H 1

#include <sys/types.h>

#include "statusDef.h"


#define SSD_BUF_VALID 0x01
#define SSD_BUF_DIRTY 0x02

struct RuntimeSTAT
{
    /** This user basic info */
    unsigned int batchId;
    unsigned int userId;
    unsigned int traceId;
    unsigned long startLBA;
    unsigned int isWriteOnly;

    /** Runtime strategy refered parameter **/
    //union StratetyUnion strategyRef;

    /** Runtime Statistic **/
    blkcnt_t cacheLimit;
    blkcnt_t cacheUsage;

    blkcnt_t reqcnt_s;
    blkcnt_t reqcnt_r;
    blkcnt_t reqcnt_w;

    blkcnt_t hitnum_s;
    blkcnt_t hitnum_r;
    blkcnt_t hitnum_w;

    blkcnt_t load_ssd_blocks;
    blkcnt_t load_hdd_blocks;
    blkcnt_t flush_hdd_blocks;
    blkcnt_t flush_ssd_blocks;
    blkcnt_t flush_clean_blocks;

    double time_read_ssd;
    double time_read_hdd;
    double time_write_ssd;
    double time_write_hdd;

    blksize_t hashmiss_sum;
    blksize_t hashmiss_read;
    blksize_t hashmiss_write;
};

typedef enum
{
//    CLOCK = 0,
    LRU,
    LRUofBand,
    Most,
//    Most_Dirty,
//    SCAN,
//    CMR,
//    SMR,
//    WA,
//    MaxCold,
//    MaxAll,
//    AvgBandHot,
//    HotDivSize,
    /** add for multiuser **/
    LRU_global,
    LRU_private,
    LRU_batch,
    PORE,
    PORE_PLUS
}SSDEvictionStrategy;

/** This user basic info */
extern int BatchId;
extern int UserId;
extern int TraceId;
extern off_t StartLBA;
extern int WriteOnly;
extern SSDEvictionStrategy EvictStrategy;
extern unsigned long Param1;
extern unsigned long Param2;
extern int BatchSize;

/** All users basic setup **/
extern blkcnt_t NBLOCK_SSD_CACHE;
extern blkcnt_t NTABLE_SSD_CACHE;
extern blkcnt_t SSD_BUFFER_SIZE;
extern blkcnt_t NBLOCK_SMR_FIFO;
//extern blkcnt_t NSMRBands;		// 194180*(18MB+36MB)/2~5TB
//extern blkcnt_t NSMRBlocks;		// 2621952*8KB~20GB
//extern blkcnt_t NSSDs;
extern blkcnt_t NSSDTables;
//extern blkcnt_t NBANDTables;
extern blkcnt_t SSD_SIZE;
extern const blksize_t BLCKSZ;
extern const blkcnt_t  NZONES;
extern const blksize_t ZONESZ;

extern char simu_smr_fifo_device[];
extern char simu_smr_smr_device[];
extern char smr_device[];
extern char ssd_device_read[];
extern char ssd_device_write[];
extern char ram_device[1024];

extern int BandOrBlock;

/*Block = 0, Band=1*/
extern int hdd_fd;
extern int ssd_fd_read;
extern int ssd_fd_write;
extern int ram_fd;
extern struct RuntimeSTAT* STT;

/** Shared memory variable names **/
extern const char* SHM_SSDBUF_STRATEGY_CTRL_READ;
extern const char* SHM_SSDBUF_STRATEGY_CTRL_WRITE;
extern const char* SHM_SSDBUF_STRATEGY_DESP_READ;
extern const char* SHM_SSDBUF_STRATEGY_DESP_WRITE;

extern const char* SHM_BAND_STRATEGY_DESP;
extern const char* SHM_STRATEGY_EVICTED_BAND;
extern const char* SHM_BAND_HASH_BUCKET;
extern const char* SHM_SSD_HASH_BUCKET;
extern const char* SHM_FIFO_DESP_ARRAY;
extern const char* SHM_SIMU_STAT;
extern const char* SHM_SSD_BUF_HASH_BUCKET;

extern const char* SHM_SSDBUF_DESP_CTRL_READ;
extern const char* SHM_SSDBUF_DESP_CTRL_WRITE;
extern const char* SHM_SSDBUF_DESPS_READ;
extern const char* SHM_SSDBUF_DESPS_WRITE;

extern const char* SHM_SSDBUF_HASHTABLE_CTRL;
extern const char* SHM_SSDBUF_HASHTABLE;
extern const char* SHM_SSDBUF_HASHDESPS;
extern const char* SHM_PROCESS_REQ_LOCK;

extern const char* SHM_FIFO_HASH_BUCKET;
extern const char* SHM_FIFO_HASH_FREELIST;

extern const char* SHM_SSD_HASH_TAB_READ;
extern const char* SHM_SSD_HASH_TAB_WRITE;
extern const char* SHM_SSD_FREE_LIST_READ;
extern const char* SHM_SSD_FREE_LIST_WRITE;


extern const char* SHM_Most_HASHTABLE;
extern const char* SHM_Most_FREELIST;


extern const char* SHM_FINISH_PROCESS;

extern const char* PATH_LOG;
#endif
