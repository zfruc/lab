#define DEBUG 0
/*----------------------------------Most---------------------------------*/
#include "band_table.h"
#include "cache.h"

typedef struct
{
	long ssd_buf_id;//ssd buffer location in shared buffer
	long next_ssd_buf;
} SSDBufDespForMost;

typedef struct
{
    long band_num;
	long current_pages;
	long first_page;
} BandDescForMost;

typedef struct
{
    long        nbands;          // # of cached bands
    pthread_mutex_t lock;
} SSDBufferStrategyControlForMost;


SSDBufDespForMost *ssd_buf_desps_for_most;
BandDescForMost *band_descriptors_for_most;
SSDBufferStrategyControlForMost *ssd_buf_strategy_ctrl_for_most;

extern int initSSDBufferForMost();
extern int HitMostBuffer();
extern long LogOutDesp_most();
extern long LogInMostBuffer();
