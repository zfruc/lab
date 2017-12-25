#include <stdio.h>
#include <stdlib.h>
#include <global.h>
#include "shmlib.h"
#include "most.h"

#define MOST_DIRTY 1
long GetSMRBandNumFromSSD(unsigned long offset);

BandDescForMost	*EvictedBand;

int
initSSDBufferForMost()
{


    int stat = SHM_lock_n_check("LOCK_SSDBUF_STRATEGY_Most");
    if(stat == 0)
    {
        initBandTable(NBANDTables);
        SSDBufDespForMost *ssd_buf_hdr_for_most;
        BandDescForMost *band_hdr_for_most;
        ssd_buf_desps_for_most = (SSDBufDespForMost *) SHM_alloc(SHM_SSDBUF_STRATEGY_DESP,sizeof(SSDBufDespForMost) * NBLOCK_SSD_CACHE);
        long		i;
        ssd_buf_hdr_for_most = ssd_buf_desps_for_most;
        for (i = 0; i < NBLOCK_SSD_CACHE; ssd_buf_hdr_for_most++, i++) {
            ssd_buf_hdr_for_most->ssd_buf_id = i;
            ssd_buf_hdr_for_most->next_ssd_buf = -1;
        }

        band_descriptors_for_most = (BandDescForMost *) SHM_alloc(SHM_BAND_STRATEGY_DESP,sizeof(BandDescForMost) * NZONES);
        band_hdr_for_most = band_descriptors_for_most;
        for (i = 0; i < NZONES; band_hdr_for_most++, i++) {
            band_hdr_for_most->band_num = -1;
            band_hdr_for_most->current_pages = 0;
            band_hdr_for_most->first_page = -1;
        }

        ssd_buf_strategy_ctrl_for_most = (SSDBufferStrategyControlForMost *) SHM_alloc(SHM_SSDBUF_STRATEGY_CTRL,sizeof(SSDBufferStrategyControlForMost));
        ssd_buf_strategy_ctrl_for_most->nbands = 0;
		SHM_mutex_init(&ssd_buf_strategy_ctrl_for_most->lock);

        EvictedBand = (BandDescForMost *)SHM_alloc(SHM_STRATEGY_EVICTED_BAND,sizeof(BandDescForMost));
        EvictedBand->band_num = 0;
        EvictedBand->first_page = -1;
        EvictedBand->current_pages = 0;
    }
    else
    {
        initBandTable(NBANDTables);
        ssd_buf_desps_for_most = (SSDBufDespForMost *) SHM_get(SHM_SSDBUF_STRATEGY_DESP,sizeof(SSDBufDespForMost) * NBLOCK_SSD_CACHE);
        band_descriptors_for_most = (BandDescForMost *) SHM_get(SHM_BAND_STRATEGY_DESP,sizeof(BandDescForMost) * NZONES);
        ssd_buf_strategy_ctrl_for_most = (SSDBufferStrategyControlForMost *) SHM_get(SHM_SSDBUF_STRATEGY_CTRL,sizeof(SSDBufferStrategyControlForMost));
        EvictedBand = (BandDescForMost *)SHM_get(SHM_STRATEGY_EVICTED_BAND,sizeof(BandDescForMost));
    }
    SHM_unlock("LOCK_SSDBUF_STRATEGY_Most");
    return stat;
}

int
HitMostBuffer()
{
	return 1;
}

long LogOutDesp_most()
{
	long band_hash = 0;
  //  printf("try to get lock in LogOutDesp_most.n.\n");
	_LOCK(&ssd_buf_strategy_ctrl_for_most->lock);
//	printf("now LogOutDesp_most hold lock ssd_buf_strategy_ctrl_for_most->lock.\n");
//	printf("before while loop checking EvictedBand->first_page, EvictedBand->first_page = %d.\n",EvictedBand->first_page);
	long loop_standard = EvictedBand->first_page;
//	printf("out of while, loop_standard = %d\n",loop_standard);
	while(loop_standard < 0){
//	printf("in loop checking EvictedBand->first_page,now EvictedBand->first_page = %d.\n",EvictedBand->first_page);
 /*       _LOCK(&ssd_buf_strategy_ctrl_for_most->lock);
        if(EvictedBand->first_page >=0 )
        {
            _UNLOCK(&ssd_buf_strategy_ctrl_for_most->lock);
            break;
        }
*/

		*EvictedBand = band_descriptors_for_most[0];
        long del_val = bandtableDelete(EvictedBand->band_num, bandtableHashcode(EvictedBand->band_num));

        BandDescForMost	temp;
		temp = band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1];
		long		parent = 0;
		long		child = parent * 2 + 1;
		while (child < ssd_buf_strategy_ctrl_for_most->nbands) {
			if (child < ssd_buf_strategy_ctrl_for_most->nbands && band_descriptors_for_most[child].current_pages < band_descriptors_for_most[child + 1].current_pages)
				child++;
			if (temp.current_pages >= band_descriptors_for_most[child].current_pages)
				break;
			else {
				band_descriptors_for_most[parent] = band_descriptors_for_most[child];
				long		band_hash = bandtableHashcode(band_descriptors_for_most[child].band_num);
				bandtableDelete(band_descriptors_for_most[child].band_num, band_hash);
				bandtableInsert(band_descriptors_for_most[child].band_num, band_hash, parent);
				parent = child;
				child = child * 2 + 1;
			}
		}
		band_descriptors_for_most[parent] = temp;
		band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1].band_num = -1;
		band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1].current_pages = 0;
		band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1].first_page = -1;
		ssd_buf_strategy_ctrl_for_most->nbands--;
		band_hash = bandtableHashcode(temp.band_num);
		bandtableDelete(temp.band_num, band_hash);
		bandtableInsert(temp.band_num, band_hash, parent);
  //      _UNLOCK(&ssd_buf_strategy_ctrl_for_most->lock);
		loop_standard = EvictedBand->first_page;
	}

	long		band_num = EvictedBand->band_num;
	band_hash = bandtableHashcode(band_num);
	long		band_id = bandtableLookup(band_num, band_hash);
	long		first_page = EvictedBand->first_page;

//	printf("first_page = %lf,NBLOCK_SSD_CACHE = %lf\n",first_page,NBLOCK_SSD_CACHE);
    //_LOCK(&ssd_buf_strategy_ctrl_for_most->lock);
//	printf("first_page = %d,NBLOCK_SSD_CACHE = %d\n",first_page,NBLOCK_SSD_CACHE);
	EvictedBand->first_page = ssd_buf_desps_for_most[first_page].next_ssd_buf;
	ssd_buf_desps_for_most[first_page].next_ssd_buf = -1;

//printf("now LogOutDesp_most release lock ssd_buf_strategy_ctrl_for_most->lock.\n");
    _UNLOCK(&ssd_buf_strategy_ctrl_for_most->lock);
	return ssd_buf_desps_for_most[first_page].ssd_buf_id;
}

long LogInMostBuffer(long despId, SSDBufTag tag,unsigned despflag)
{
	long		band_num = GetSMRBandNumFromSSD(tag.offset);
	unsigned long	band_hash = bandtableHashcode(band_num);
	long		band_id = bandtableLookup(band_num, band_hash);
    long flag= 0;

	SSDBufDespForMost *ssd_buf_for_most;
	BandDescForMost *band_hdr_for_most;
  //  printf("try to get lock in LogInMostBuffer.n.\n");
    _LOCK(&ssd_buf_strategy_ctrl_for_most->lock);
 //   printf("now LogInMostBuffer hold lock ssd_buf_strategy_ctrl_for_most->lock.\n");

	if (band_id >= 0) {
		SSDBufDespForMost *new_ssd_buf_for_most;
		new_ssd_buf_for_most = &ssd_buf_desps_for_most[despId];
		new_ssd_buf_for_most->next_ssd_buf = band_descriptors_for_most[band_id].first_page;
		band_descriptors_for_most[band_id].first_page = despId;

        if(MOST_DIRTY)
        {
            if((despflag & SSD_BUF_DIRTY) != 0)
                band_descriptors_for_most[band_id].current_pages++;
        }
        else
            band_descriptors_for_most[band_id].current_pages++;
		BandDescForMost	temp;
		long		parent = (band_id - 1) / 2;
		long		child = band_id;
		while (parent >= 0 && band_descriptors_for_most[child].current_pages > band_descriptors_for_most[parent].current_pages) {
			temp = band_descriptors_for_most[child];
			band_descriptors_for_most[child] = band_descriptors_for_most[parent];
			band_hash = bandtableHashcode(band_descriptors_for_most[parent].band_num);
			bandtableDelete(band_descriptors_for_most[parent].band_num, band_hash);
			bandtableInsert(band_descriptors_for_most[parent].band_num, band_hash, child);
			band_descriptors_for_most[parent] = temp;
			band_hash = bandtableHashcode(temp.band_num);
			bandtableDelete(temp.band_num, band_hash);
			bandtableInsert(temp.band_num, band_hash, parent);

			child = parent;
			parent = (child - 1) / 2;
		}
	} else {
		ssd_buf_strategy_ctrl_for_most->nbands++;
		band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1].band_num = band_num;
		band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1].current_pages = 1;
		band_descriptors_for_most[ssd_buf_strategy_ctrl_for_most->nbands - 1].first_page = despId;
        bandtableInsert(band_num, band_hash, ssd_buf_strategy_ctrl_for_most->nbands - 1);
        SSDBufDespForMost *new_ssd_buf_for_most;
		new_ssd_buf_for_most = &ssd_buf_desps_for_most[despId];
		new_ssd_buf_for_most->next_ssd_buf = -1;
	}

   // printf("now LogOutDesp_most release lock ssd_buf_strategy_ctrl_for_most->lock.\n");
    _UNLOCK(&ssd_buf_strategy_ctrl_for_most->lock);
	return band_id;
}

long GetSMRBandNumFromSSD(unsigned long offset)
{
    long BNDSZ = 36*1024*1024;      // bandsize = 36MB  (18MB~36MB)
    long band_size_num = BNDSZ / 1024 / 1024 / 2 + 1;
    long num_each_size = NZONES / band_size_num;
    long        i, size, total_size = 0;
    for (i = 0; i < band_size_num; i++)
    {
        size = BNDSZ / 2 + i * 1024 * 1024;
        if (total_size + size * num_each_size > offset)
            return num_each_size * i + (offset - total_size) / size;
        total_size += size * num_each_size;
    }

    return 0;
}
