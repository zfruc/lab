#include "global.h"

/** This user basic info */
int BatchId;
int UserId;
int TraceId;
off_t StartLBA;
int WriteOnly;
int BatchSize;
SSDEvictionStrategy EvictStrategy;

unsigned long Param1;
unsigned long Param2;

/** All users basic setup **/
blksize_t NBLOCK_SSD_CACHE;
blksize_t NTABLE_SSD_CACHE;
blksize_t SSD_BUFFER_SIZE = 4096;
blksize_t NBLOCK_SMR_FIFO;
//blksize_t NSMRBlocks = 2621952;		// 2621952*8KB~20GB
//blksize_t NSSDs;
//blksize_t NSSDTables;
blksize_t NBANDTables = 2621952;
blksize_t SSD_SIZE = 4096;
const blksize_t BLCKSZ = 4096;
const blkcnt_t  NZONES = 194180;    // NZONES * ZONESZ =
const blksize_t ZONESZ = 18874368;//18MB    // Unit: Byte.

char simu_smr_fifo_device[] = "/sdb1";
char simu_smr_smr_device[] = "/sdb2";
char smr_device[] = "/sdc";
char ssd_device_read[30] = "/dev/sdb";
char ssd_device_write[30] = "/dev/sdc";
char ram_device[1024];

int BandOrBlock;

/*Block = 0, Band=1*/
int hdd_fd;
int ssd_fd_read;
int ssd_fd_write;
int ram_fd;
struct RuntimeSTAT* STT;

/** Shared memory variable names **/
const char* SHM_SSDBUF_STRATEGY_CTRL_READ = "SHM_SSDBUF_STRATEGY_CTRL_READ";
const char* SHM_SSDBUF_STRATEGY_CTRL_WRITE = "SHM_SSDBUF_STRATEGY_CTRL_WRITE";
const char* SHM_SSDBUF_STRATEGY_DESP_READ = "SHM_SSDBUF_STRATEGY_DESP_READ";
const char* SHM_SSDBUF_STRATEGY_DESP_WRITE = "SHM_SSDBUF_STRATEGY_DESP_WRITE";
const char* SHM_BAND_STRATEGY_DESP = "SHM_BAND_STRATEGY_DESP";
const char* SHM_STRATEGY_EVICTED_BAND = "SHM_STRATEGY_EVICTED_BAND";
const char* SHM_BAND_HASH_BUCKET = "SHM_BAND_HASH_BUCKET";

const char* SHM_SSDBUF_DESP_CTRL_READ = "SHM_SSDBUF_DESP_CTRL_READ";
const char* SHM_SSDBUF_DESP_CTRL_WRITE = "SHM_SSDBUF_DESP_CTRL_WRITE";
const char* SHM_SSDBUF_DESPS_READ = "SHM_SSDBUF_DESPS_READ";
const char* SHM_SSDBUF_DESPS_WRITE = "SHM_SSDBUF_DESPS_WRITE";

const char* SHM_SSDBUF_HASHTABLE_CTRL = "SHM_SSDBUF_HASHTABLE_CTRL";
const char* SHM_SSDBUF_HASHTABLE = "SHM_SSDBUF_HASHTABLE";
const char* SHM_SSDBUF_HASHDESPS =  "SHM_SSDBUF_HASHDESPS";
const char* SHM_PROCESS_REQ_LOCK = "SHM_PROCESS_REQ_LOCK";
const char* SHM_SSD_BUF_HASH_BUCKET = "SHM_SSD_BUF_HASH_BUCKET";

const char* SHM_FIFO_DESP_ARRAY = "SHM_FIFO_DESP_ARRAY";
const char* SHM_SIMU_STAT = "SHM_SIMU_STAT";
const char* SHM_SSD_HASH_BUCKET = "SHM_SSD_HASH_BUCKET";

const char* SHM_FIFO_HASH_BUCKET = "SHM_FIFO_HASH_BUCKET";
const char* SHM_FIFO_HASH_FREELIST = "SHM_FIFO_HASH_FREELIST";

const char* SHM_SSD_HASH_TAB_READ = "SHM_SSD_HASH_TAB_READ";
const char* SHM_SSD_HASH_TAB_WRITE = "SHM_SSD_HASH_TAB_WRITE";
const char* SHM_SSD_FREE_LIST_READ = "SHM_SSD_FREE_LIST_READ";
const char* SHM_SSD_FREE_LIST_WRITE = "SHM_SSD_FREE_LIST_WRITE";

const char* SHM_Most_HASHTABLE = "SHM_Most_HASHTABLE";
const char* SHM_Most_FREELIST = "SHM_Most_FREELIST";

const char* SHM_FINISH_PROCESS = "SHM_FINISH_PROCESS";

const char* PATH_LOG = "/home/outputs/logs";

