#ifndef SMR_SSD_CACHE_SMR_SIMULATOR_H
#define SMR_SSD_CACHE_SMR_SIMULATOR_H

#include "global.h"
#include "statusDef.h"

#define DEBUG 0
/* ---------------------------smr simulator---------------------------- */
#include <pthread.h>

typedef struct
{
    off_t offset;
} DespTag;

typedef struct
{
    DespTag tag;
    long    despId;
    int     isValid;
} FIFODesc;

typedef struct
{
	unsigned long	n_used;
             long   head, tail;
} FIFOCtrl;

typedef struct
{
    int  fd_fifo_part;
    int  fd_smr_part;
    FIFOCtrl global_fifo_ctrl;
    off_t SMR_DISK_OFFSET;

    int ACCESS_FLAG;

    pthread_mutex_t simu_smr_fifo_mutex;

    long	band_size_num;
    long	num_each_size;

    long simu_read_smr_bands;
    long simu_flush_bands;
    long simu_flush_band_size;

    blkcnt_t simu_n_collect_fifo;
    blkcnt_t simu_n_read_fifo;
    blkcnt_t simu_n_write_fifo;
    blkcnt_t simu_n_read_smr;

    blkcnt_t simu_n_fifo_write_HIT;
    double simu_time_read_fifo;
    double simu_time_read_smr;
    double simu_time_write_smr;
    double simu_time_write_fifo;

    double simu_time_collectFIFO; /** To monitor the efficent of read all blocks in same band **/
    pthread_mutex_t lock;
} SIMU_STAT;

//extern int  fd_fifo_part;
//extern int  fd_smr_part;
extern SIMU_STAT *simu_stat;
extern void InitSimulator();
extern int smrread(char* buffer, size_t size, off_t offset);
extern int smrwrite(char* buffer, size_t size, off_t offset);
extern void PrintSimulatorStatistic();

#endif
