#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "lru.h"
#include "shmlib.h"
#include "cache.h"
/********
 ** SHM**
 ********/
static StrategyCtrl_LRU_global *strategy_ctrl[2];
static StrategyDesp_LRU_global	*strategy_desp[2];

static volatile void *addToLRUHead(StrategyDesp_LRU_global * ssd_buf_hdr_for_lru,unsigned int rw);
static volatile void *deleteFromLRU(StrategyDesp_LRU_global * ssd_buf_hdr_for_lru,unsigned int rw);
static volatile void *moveToLRUHead(StrategyDesp_LRU_global * ssd_buf_hdr_for_lru,unsigned int rw);
static int hasBeenDeleted(StrategyDesp_LRU_global* ssd_buf_hdr_for_lru,unsigned int rw);
/*
 * init buffer hash table, Strategy_control, buffer, work_mem
 */
int
initSSDBufferForLRU()
{
    int stat = SHM_lock_n_check("LOCK_SSDBUF_STRATEGY_LRU");
    if(stat == 0)
    {
        strategy_ctrl[0] =(StrategyCtrl_LRU_global *)SHM_alloc(SHM_SSDBUF_STRATEGY_CTRL_READ,sizeof(StrategyCtrl_LRU_global));
	strategy_ctrl[1] =(StrategyCtrl_LRU_global *)SHM_alloc(SHM_SSDBUF_STRATEGY_CTRL_WRITE,sizeof(StrategyCtrl_LRU_global));	
	strategy_desp[0] = (StrategyDesp_LRU_global *)SHM_alloc(SHM_SSDBUF_STRATEGY_DESP_READ, sizeof(StrategyDesp_LRU_global) * NBLOCK_SSD_CACHE);
        strategy_desp[1] = (StrategyDesp_LRU_global *)SHM_alloc(SHM_SSDBUF_STRATEGY_DESP_WRITE, sizeof(StrategyDesp_LRU_global) * NBLOCK_SSD_CACHE);

        strategy_ctrl[0]->first_lru = -1;
        strategy_ctrl[0]->last_lru = -1;
	strategy_ctrl[1]->first_lru = -1;
        strategy_ctrl[1]->last_lru = -1;
        SHM_mutex_init(&strategy_ctrl[0]->lock);
	SHM_mutex_init(&strategy_ctrl[1]->lock);

        StrategyDesp_LRU_global *ssd_buf_hdr_for_lru = strategy_desp[0];
        long i;
        for (i = 0; i < NBLOCK_SSD_CACHE; ssd_buf_hdr_for_lru++, i++)
        {
            ssd_buf_hdr_for_lru->serial_id = i;
            ssd_buf_hdr_for_lru->next_lru = -1;
            ssd_buf_hdr_for_lru->last_lru = -1;
            SHM_mutex_init(&ssd_buf_hdr_for_lru->lock);
        }
	ssd_buf_hdr_for_lru = strategy_desp[1];
        for (i = 0; i < NBLOCK_SSD_CACHE; ssd_buf_hdr_for_lru++, i++)
        {
            ssd_buf_hdr_for_lru->serial_id = i;
            ssd_buf_hdr_for_lru->next_lru = -1;
            ssd_buf_hdr_for_lru->last_lru = -1;
            SHM_mutex_init(&ssd_buf_hdr_for_lru->lock);
        }
    }
    else
    {
        strategy_ctrl[0] =(StrategyCtrl_LRU_global *)SHM_get(SHM_SSDBUF_STRATEGY_CTRL_READ,sizeof(StrategyCtrl_LRU_global));
	strategy_ctrl[1] =(StrategyCtrl_LRU_global *)SHM_get(SHM_SSDBUF_STRATEGY_CTRL_WRITE,sizeof(StrategyCtrl_LRU_global));
        strategy_desp[0] = (StrategyDesp_LRU_global *)SHM_get(SHM_SSDBUF_STRATEGY_DESP_READ, sizeof(StrategyDesp_LRU_global) * NBLOCK_SSD_CACHE);
	strategy_desp[1] = (StrategyDesp_LRU_global *)SHM_get(SHM_SSDBUF_STRATEGY_DESP_WRITE, sizeof(StrategyDesp_LRU_global) * NBLOCK_SSD_CACHE);
    }
    SHM_unlock("LOCK_SSDBUF_STRATEGY_LRU");
    return stat;
}

long
LogOutDesp_lru(unsigned int rw)
{
    _LOCK(&strategy_ctrl[rw]->lock);

    long frozen_id = strategy_ctrl[rw]->last_lru;
    deleteFromLRU(&strategy_desp[rw][frozen_id],rw);

    _UNLOCK(&strategy_ctrl[rw]->lock);
    return frozen_id;
}

int
HitLruBuffer(long serial_id,unsigned int rw)
{
    _LOCK(&strategy_ctrl[rw]->lock);

    StrategyDesp_LRU_global* ssd_buf_hdr_for_lru = &strategy_desp[rw][serial_id];
    if(hasBeenDeleted(ssd_buf_hdr_for_lru,rw))
    {
        _UNLOCK(&strategy_ctrl[rw]->lock);
        return -1;
    }
    moveToLRUHead(ssd_buf_hdr_for_lru,rw);
    _UNLOCK(&strategy_ctrl[rw]->lock);

    return 0;
}

void LogInLruBuffer(long serial_id,unsigned int rw)
{
    _LOCK(&strategy_ctrl[rw]->lock);

    addToLRUHead(&strategy_desp[rw][serial_id],rw);
    _UNLOCK(&strategy_ctrl[rw]->lock);
    return 0;
}


static volatile void *
addToLRUHead(StrategyDesp_LRU_global* ssd_buf_hdr_for_lru,unsigned int rw)
{
    if (strategy_ctrl[rw]->last_lru < 0)
    {
        strategy_ctrl[rw]->first_lru = ssd_buf_hdr_for_lru->serial_id;
        strategy_ctrl[rw]->last_lru = ssd_buf_hdr_for_lru->serial_id;
    }
    else
    {
        ssd_buf_hdr_for_lru->next_lru = strategy_desp[rw][strategy_ctrl[rw]->first_lru].serial_id;
        ssd_buf_hdr_for_lru->last_lru = -1;
        strategy_desp[rw][strategy_ctrl[rw]->first_lru].last_lru = ssd_buf_hdr_for_lru->serial_id;
        strategy_ctrl[rw]->first_lru = ssd_buf_hdr_for_lru->serial_id;
    }
    return NULL;
}

static volatile void *
deleteFromLRU(StrategyDesp_LRU_global * ssd_buf_hdr_for_lru,unsigned int rw)
{
    if (ssd_buf_hdr_for_lru->last_lru >= 0)
    {
        strategy_desp[rw][ssd_buf_hdr_for_lru->last_lru].next_lru = ssd_buf_hdr_for_lru->next_lru;
    }
    else                    //the newest one was chosen
    {
        strategy_ctrl[rw]->first_lru = ssd_buf_hdr_for_lru->next_lru;
    }
    if (ssd_buf_hdr_for_lru->next_lru >= 0)
    {
        strategy_desp[rw][ssd_buf_hdr_for_lru->next_lru].last_lru = ssd_buf_hdr_for_lru->last_lru;
    }
    else                    //the oldest one was chosen
    {
        strategy_ctrl[rw]->last_lru = ssd_buf_hdr_for_lru->last_lru;
    }

    ssd_buf_hdr_for_lru->last_lru = ssd_buf_hdr_for_lru->next_lru = -1;

    return NULL;
}

static volatile void *
moveToLRUHead(StrategyDesp_LRU_global * ssd_buf_hdr_for_lru,unsigned int rw)
{
    deleteFromLRU(ssd_buf_hdr_for_lru,rw);
    addToLRUHead(ssd_buf_hdr_for_lru,rw);
    return NULL;
}

static int
hasBeenDeleted(StrategyDesp_LRU_global* ssd_buf_hdr_for_lru,unsigned int rw)
{
    if(ssd_buf_hdr_for_lru->last_lru < 0 && ssd_buf_hdr_for_lru->next_lru < 0)
        return 1;
    else
        return 0;
}
