/*
  This is a updated versoin (v2) based on log-simulater.
  There are two most important new designs:
  1. FIFO Write adopts APPEND_ONLY, instead of in-place updates.
  2. When the block choosing to evict out of FIFO is a "old version",
     then drop it off，won't activate write-back, and move head pointer to deal with next one.
*/
#define _GNU_SOURCE 1
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <memory.h>
#include <aio.h>
#include <fcntl.h>

#include "report.h"
#include "cache.h"
#include "smr-simulator/simulator_logfifo.h"
#include "inner_ssd_buf_table.h"
#include "timerUtils.h"
#include "statusDef.h"
#include "shmlib.h"
/*int  fd_fifo_part;
int  fd_smr_part;

static FIFOCtrl global_fifo_ctrl;*/
static FIFODesc* fifo_desp_array;

char* BandBuffer;
static blksize_t NSMRBands = 194180;		// smr band cnt = 194180;
static unsigned long BNDSZ = 36*1024*1024;      // bandsize = 36MB  (18MB~36MB)

/*static off_t SMR_DISK_OFFSET;

int ACCESS_FLAG = 1;

pthread_mutex_t simu_smr_fifo_mutex;

static long	band_size_num;
static long	num_each_size;*/

static FIFODesc* getFIFODesp();
static volatile void flushFIFO();
/*
static long simu_read_smr_bands;
static long simu_flush_bands;
static long simu_flush_band_size;

static blkcnt_t simu_n_collect_fifo;
static blkcnt_t simu_n_read_fifo;
static blkcnt_t simu_n_write_fifo;
static blkcnt_t simu_n_read_smr;

static blkcnt_t simu_n_fifo_write_HIT = 0;
static double simu_time_read_fifo;
static double simu_time_read_smr;
static double simu_time_write_smr;
static double simu_time_write_fifo;
*/
//static double simu_time_collectFIFO = 0; /** To monitor the efficent of read all blocks in same band **/

SIMU_STAT *simu_stat;

static void* smr_fifo_monitor_thread();

static void removeFromArray(FIFODesc* desp);

static unsigned long GetSMRActualBandSizeFromSSD(unsigned long offset);
static unsigned long GetSMRBandNumFromSSD(unsigned long offset);
static off_t GetSMROffsetInBandFromSSD(FIFODesc * ssd_hdr);

/** AIO related for blocks collected in FIFO **/
#define max_aio_count 6000
struct aiocb aiolist[max_aio_count];
struct aiocb* aiocb_addr_list[max_aio_count];/* >= band block size */


/*
 * init inner ssd buffer hash table, strategy_control, buffer, work_mem
 */
void InitSimulator()
{
    printf("into InitSimulator funcation.\n");
    int stat = SHM_lock_n_check("LOCK_INIT_SIMULATOR");
    printf("stat = lock LOCK_INIT_SIMULATOR, stat = %d.\n",stat);
    if(stat == 0)
    {
        /* initialliz related constants */
        simu_stat = (SIMU_STAT *)SHM_alloc(SHM_SIMU_STAT,sizeof(SIMU_STAT));

        simu_stat->fd_fifo_part = open(simu_smr_fifo_device, O_RDWR | O_DIRECT);
        simu_stat->fd_smr_part = open(simu_smr_smr_device, O_RDWR | O_DIRECT | O_FSYNC);
        printf("Simulator Device: fifo part=%d, smr part=%d\n",simu_stat->fd_fifo_part,simu_stat->fd_smr_part);
        if(simu_stat->fd_fifo_part<0 || simu_stat->fd_smr_part<0) exit(-1);

        simu_stat->SMR_DISK_OFFSET = NBLOCK_SMR_FIFO * BLCKSZ;
        simu_stat->band_size_num = BNDSZ / 1024 / 1024 / 2 + 1;
        simu_stat->num_each_size = NSMRBands / simu_stat->band_size_num;

        simu_stat->global_fifo_ctrl.n_used = 0;
        simu_stat->global_fifo_ctrl.head = simu_stat->global_fifo_ctrl.tail = 0;

        fifo_desp_array = (FIFODesc *)SHM_alloc(SHM_FIFO_DESP_ARRAY,sizeof(FIFODesc) * NBLOCK_SMR_FIFO);
        if(fifo_desp_array==NULL)
        {
            printf("error in shm alloc.\n");
            exit(0);
        }
        printf("check fifo_desp_arary alloc done.\n");
        FIFODesc* fifo_hdr = fifo_desp_array;
        long i;
        for (i = 0; i < NBLOCK_SMR_FIFO; fifo_hdr++, i++)
        {
            fifo_hdr->despId = i;
            fifo_hdr->isValid = 0;
        }

        posix_memalign(&BandBuffer, 512, sizeof(char) * BNDSZ);

        SHM_mutex_init(&simu_stat->simu_smr_fifo_mutex);
	    SHM_mutex_lock(&simu_stat->simu_smr_fifo_mutex);
	    SHM_mutex_unlock(&simu_stat->simu_smr_fifo_mutex);

        printf("SHM_mutex_init done.\n");
        /** AIO related **/

        /** statistic related **/
        simu_stat->ACCESS_FLAG = 1;
        simu_stat->simu_n_fifo_write_HIT = 0;
        simu_stat->simu_time_collectFIFO = 0;

        simu_stat->simu_read_smr_bands = 0;
        simu_stat->simu_flush_bands = 0;
  //      simu_stat->simu_flush_band_size = 0;

        simu_stat->simu_n_collect_fifo = 0;
        simu_stat->simu_n_read_fifo = 0;
  //      simu_stat->simu_n_write_fifo = 0;
        simu_stat->simu_n_read_smr = 0;
        simu_stat->simu_time_read_fifo = 0;
        simu_stat->simu_time_read_smr = 0;
        simu_stat->simu_time_write_smr = 0;
        simu_stat->simu_time_write_fifo = 0;
        
        int uId = 0;
        for(uId = 0;uId < 5;uId++)
        {
            simu_stat->simu_flush_band_size[uId] = 0;
            simu_stat->simu_n_write_fifo[uId] = 0;
        }

        printf("begin to initSSDTable.\n");
        initSSDTable(NBLOCK_SMR_FIFO);
        printf("initSSDTable done.\n");

        //pthread_t tid;
        //int err = pthread_create(&tid, NULL, smr_fifo_monitor_thread, NULL);
        //if (err != 0)
        //{
        //    printf("[ERROR] initSSD: fail to create thread: %s\n", strerror(err));
        //    exit(-1);
        //}
        //printf("pthread_create done.\n");

        /* cgroup write fifo throttle */
    //    pid_t mypid = getpid();
    //    int bps_write = 1048576 * 11;
    //    char cmd_cg[512];
    //
    //    int r;
    //    r = system("rm -rf /sys/fs/cgroup/blkio/smr_simu");
    //    r = system("mkdir /sys/fs/cgroup/blkio/smr_simu");
    //    sprintf(cmd_cg,"echo \"8:16 %d\" > /sys/fs/cgroup/blkio/smr_simu/blkio.throttle.write_bps_device",bps_write);
    //    r = system(cmd_cg);
    //    sprintf(cmd_cg,"echo %d > /sys/fs/cgroup/blkio/smr_simu/tasks",mypid);
    //    r = system(cmd_cg);
    }
    else
    {
        initSSDTable(NBLOCK_SMR_FIFO);
        simu_stat = (SIMU_STAT *)SHM_get(SHM_SIMU_STAT,sizeof(SIMU_STAT));
        fifo_desp_array = (FIFODesc *)SHM_get(SHM_FIFO_DESP_ARRAY,sizeof(FIFODesc) * NBLOCK_SMR_FIFO);
    }
    SHM_unlock("LOCK_INIT_SIMULATOR");

}

/** \brief
 *  To monitor the FIFO in SMR, and do cleanning operation when idle status.
 */
static void*
smr_fifo_monitor_thread()
{
    pthread_t th = pthread_self();
    pthread_detach(th);
    int interval = 10;
    printf("Simulator Auto-clean thread [%lu], clean interval %d seconds.\n",th,interval);

    while (1)
    {
	 SHM_mutex_lock(&simu_stat->simu_smr_fifo_mutex); 
//	printf("now smr_fifo_monitor_thread() get simu_stat->simu_smr_fifo_mutex.\n");
       if (!simu_stat->ACCESS_FLAG)
        {
	    flushFIFO();
//	    printf("now smr_fifo_monitor_thread() release simu_stat->simu_smr_fifo_mutex.\n");
            SHM_mutex_unlock(&simu_stat->simu_smr_fifo_mutex); 
    	   if (DEBUG)
                printf("[INFO] freeStrategySSD():--------after clean\n");
        }
        else
        {
	    simu_stat->ACCESS_FLAG = 0;
  //          printf("now smr_fifo_monitor_thread() release simu_stat->simu_smr_fifo_mutex.\n");
	    SHM_mutex_unlock(&simu_stat->simu_smr_fifo_mutex);
            sleep(interval);
        }
    }
    return NULL;
}

int
simu_smr_read(char *buffer, size_t size, off_t offset)
{
    //printf("in simu_smr_read, try to get lock simu_stat->simu_smr_fifo_mutex.\n");
    SHM_mutex_lock(&simu_stat->simu_smr_fifo_mutex);
    //printf("in simu_smr_read, have got lock simu_stat->simu_smr_fifo_mutex.\n");
    DespTag		tag;
    FIFODesc    *ssd_hdr;
    long		i;
    int	        returnCode = 0;
    long		ssd_hash;
    long		despId;
    struct timeval	tv_start,tv_stop;

    for (i = 0; i * BLCKSZ < size; i++)
    {
	//printf("now in simu_smr_read loop %lu.\n",i);
        tag.offset = offset + i * BLCKSZ;
        ssd_hash = ssdtableHashcode(tag);
        //printf("the hashcode pass to ssdtableLookup is %lu.\n",ssd_hash);
	despId = ssdtableLookup(tag, ssd_hash);
	//printf("despId in loop is %lu.\n",despId);

        if (despId >= 0)
        {
            /* read from fifo */
            simu_stat->simu_n_read_fifo++;
            ssd_hdr = fifo_desp_array + despId;

            _TimerLap(&tv_start);
    //        returnCode = pread(fd_fifo_part, buffer, BLCKSZ, ssd_hdr->despId * BLCKSZ);
            if (returnCode < 0)
            {
                printf("[ERROR] smrread():-------read from inner ssd: fd=%d, errorcode=%d, offset=%lu\n", simu_stat->fd_fifo_part, returnCode, ssd_hdr->despId * BLCKSZ);
                exit(-1);
            }
            _TimerLap(&tv_stop);
            simu_stat->simu_time_read_fifo += TimerInterval_SECOND(&tv_start,&tv_stop);
        }
        else
        {
            /* read from actual smr */
            simu_stat->simu_n_read_smr++;
            _TimerLap(&tv_start);

      //      returnCode = pread(fd_smr_part, buffer, BLCKSZ, offset + i * BLCKSZ);
            if (returnCode < 0)
            {
                printf("[ERROR] smrread():-------read from smr disk: fd=%d, errorcode=%d, offset=%lu\n", simu_stat->fd_smr_part, returnCode, offset + i * BLCKSZ);
                exit(-1);
            }
            _TimerLap(&tv_stop);
            simu_stat->simu_time_read_smr += TimerInterval_SECOND(&tv_start,&tv_stop);
        }
    }
    simu_stat->ACCESS_FLAG = 1;
    //printf("now simu_smr_read release the simu_stat->simu_smr_fifo_mutex.\n");
    SHM_mutex_unlock(&simu_stat->simu_smr_fifo_mutex);
    return 0;
}

int
simu_smr_write(char *buffer, size_t size, off_t offset)
{
    //printf("try to get lock simu_stat->simu_smr_fifo_mutex in simu_smr_write.\n");
    SHM_mutex_lock(&simu_stat->simu_smr_fifo_mutex);
    //printf("now simu_smr_write() get simu_stat->simu_smr_fifo_mutex.\n");
    DespTag		tag;
    FIFODesc        *ssd_hdr;
    long		i;
    int		returnCode = 0;
    long		ssd_hash;
    long		despId;
    struct timeval	tv_start,tv_stop;

    for (i = 0; i * BLCKSZ < size; i++)
    {
        tag.offset = offset + i * BLCKSZ;

        /* APPEND_ONLY */
        ssd_hdr = getFIFODesp();
        ssd_hdr->tag = tag;

        /* Update HashTable and Descriptor array */
        ssd_hash = ssdtableHashcode(tag);
        long old_despId = ssdtableUpdate(tag, ssd_hash, ssd_hdr->despId);
       // printf("old_despId for removeFromArray = %ld.\n",old_despId);
	    if(old_despId >= 0)
	    {
	        FIFODesc* oldDesp = fifo_desp_array + old_despId;
	        removeFromArray(oldDesp); ///invalid the old desp;
	    }
        _TimerLap(&tv_start);
    //    returnCode = pwrite(fd_fifo_part, buffer, BLCKSZ, ssd_hdr->despId * BLCKSZ);
        if (returnCode < 0)
        {
            printf("[ERROR] smrwrite():-------write to smr disk: fd=%d, errorcode=%d, offset=%lu\n", simu_stat->fd_fifo_part, returnCode, offset + i * BLCKSZ);
            exit(-1);
        }
        _TimerLap(&tv_stop);
        simu_stat->simu_time_write_fifo += TimerInterval_SECOND(&tv_start,&tv_stop);
        
        int uId = tag.offset/4096/20000000;
        simu_stat->simu_n_write_fifo[uId]++;
    }
    simu_stat->ACCESS_FLAG = 1;
    SHM_mutex_unlock(&simu_stat->simu_smr_fifo_mutex);
    //printf("simu_smr_write() release lock simu_stat->simu_smr_fifo_mutex.\n");
    return 0;
}

static void
removeFromArray(FIFODesc* desp) //get lock out of this function
{
    int tmp1 = desp->despId;
    int tmp2 = simu_stat->global_fifo_ctrl.head;
    if(desp->despId == simu_stat->global_fifo_ctrl.head)
    {
        simu_stat->global_fifo_ctrl.head = (simu_stat->global_fifo_ctrl.head + 1) % NBLOCK_SMR_FIFO;
    }
    desp->isValid = 0;
    simu_stat->global_fifo_ctrl.n_used--;

}

static FIFODesc *
getFIFODesp()	// get lock  out of this function
{
    FIFODesc* newDesp;
    if((simu_stat->global_fifo_ctrl.tail + 1) % NBLOCK_SMR_FIFO == simu_stat->global_fifo_ctrl.head)
    {
        /* Log structure array is full fill */
        flushFIFO();
    }

    /* Append to tail */
    newDesp = fifo_desp_array + simu_stat->global_fifo_ctrl.tail;
    newDesp->isValid = 1;
    simu_stat->global_fifo_ctrl.tail = (simu_stat->global_fifo_ctrl.tail + 1) % NBLOCK_SMR_FIFO;
    return newDesp;
}

static volatile void
flushFIFO()
{
    if(simu_stat->global_fifo_ctrl.head == simu_stat->global_fifo_ctrl.tail) // Log structure array is empty.
    {
        return ;
    }

    int     returnCode = 0;
    struct  timeval	tv_start, tv_stop;
    long    dirty_n_inBand = 0;
    double  wtrAmp;

    FIFODesc* target = fifo_desp_array + simu_stat->global_fifo_ctrl.head;

    /* Create a band-sized buffer for readind and flushing whole band bytes */
    long		band_size = GetSMRActualBandSizeFromSSD(target->tag.offset);
    off_t		band_offset = target->tag.offset - GetSMROffsetInBandFromSSD(target) * BLCKSZ;

    /* read whole band from smr to buffer*/
    _TimerLap(&tv_start);
//    returnCode = pread(fd_smr_part, BandBuffer, band_size,band_offset);
    if (returnCode < 0)
    {
        printf("[ERROR] flushSSD():---------read from smr: fd=%d, errorcode=%d, offset=%lu\n", simu_stat->fd_smr_part, returnCode, band_offset);
        exit(-1);
    }
    _TimerLap(&tv_stop);
    simu_stat->simu_time_read_smr += TimerInterval_SECOND(&tv_start,&tv_stop);
    simu_stat->simu_read_smr_bands++;

    /* Combine cached pages from FIFO which are belong to the same band */
    unsigned long BandNum = GetSMRBandNumFromSSD(target->tag.offset);

    /** ---------------DEBUG BLOCK----------------------- **/
    struct timeval	tv_collect_start, tv_collect_stop;
    double collect_time;
    _TimerLap(&tv_collect_start);
    /**--------------------------------------------------- **/
    int aio_read_cnt = 0;
    long curPos = target->despId;
    while(curPos != simu_stat->global_fifo_ctrl.tail)
    {
        FIFODesc* curDesp = fifo_desp_array + curPos;
        long nextPos = (curDesp->despId + 1) % NBLOCK_SMR_FIFO;
        if (curDesp->isValid && GetSMRBandNumFromSSD(curDesp->tag.offset) == BandNum)
        {
            /* The block belongs the same band with the header of fifo. */
            off_t offset_inband = GetSMROffsetInBandFromSSD(curDesp);

#ifdef SIMULATOR_AIO
            struct aiocb* aio_n = aiolist + aio_read_cnt;
            aio_n->aio_fildes = simu_stat->fd_fifo_part;
            aio_n->aio_offset = curPos * BLCKSZ;
            aio_n->aio_buf = BandBuffer + offset_inband * BLCKSZ;
            aio_n->aio_nbytes = BLCKSZ;
            aio_n->aio_lio_opcode = LIO_READ;
            aiocb_addr_list[aio_read_cnt] = aio_n;
            aio_read_cnt++;
#else
            _TimerLap(&tv_start);
     //       returnCode = pread(fd_fifo_part, BandBuffer + offset_inband * BLCKSZ, BLCKSZ, curPos * BLCKSZ);
            if (returnCode < 0)
            {
                printf("[ERROR] flushSSD():-------read from inner ssd: fd=%d, errorcode=%d, offset=%lu\n", simu_stat->fd_fifo_part, returnCode, curPos * BLCKSZ);
                exit(-1);
            }
            _TimerLap(&tv_stop);
            simu_stat->simu_time_read_fifo += TimerInterval_SECOND(&tv_start,&tv_stop);
#endif // SIMULATOR_AIO
            /* clean the meta data */
            dirty_n_inBand++;
            unsigned long hash_code = ssdtableHashcode(curDesp->tag);
            ssdtableDelete(curDesp->tag, hash_code);

            removeFromArray(curDesp);
        }
        else if(!curDesp->isValid && curDesp->despId == simu_stat->global_fifo_ctrl.head)
        {
            simu_stat->global_fifo_ctrl.head = (simu_stat->global_fifo_ctrl.head + 1) % NBLOCK_SMR_FIFO;
        }
        curPos = nextPos;
    }
    simu_stat->simu_n_collect_fifo += dirty_n_inBand;
#ifdef SIMULATOR_AIO
    _TimerLap(&tv_start);
//    int ret_aio = lio_listio(LIO_WAIT,aiocb_addr_list,aio_read_cnt,NULL);
    int ret_aio=1;
    if(ret_aio < 0)
    {
        char log[128];
        sprintf(log,"Flush [%ld] times ERROR: AIO read list from FIFO Failure[%d].\n",simu_stat->simu_flush_bands+1,ret_aio);
        WriteLog(log);
    }
    _TimerLap(&tv_stop);
    simu_stat->simu_time_read_fifo += TimerInterval_SECOND(&tv_start,&tv_stop);
#endif // SIMULATOR_AIO
    /**--------------------------------------------------- **/
    _TimerLap(&tv_collect_stop);
    collect_time = TimerInterval_SECOND(&tv_collect_start, &tv_collect_stop);
    simu_stat->simu_time_collectFIFO += collect_time;
    /** ---------------DEBUG BLOCK----------------------- **/

    /* flush whole band to smr */
    _TimerLap(&tv_start);

//    returnCode = pwrite(fd_smr_part, BandBuffer, band_size, band_offset);
    if (returnCode < 0)
    {
        printf("[ERROR] flushSSD():-------write to smr: fd=%d, errorcode=%d, offset=%lu\n", simu_stat->fd_smr_part, returnCode, band_offset);
        exit(-1);
    }
    _TimerLap(&tv_stop);

    int uId = target->tag.offset/4096/20000000;
    simu_stat->simu_time_write_smr += TimerInterval_SECOND(&tv_start,&tv_stop);
    simu_stat->simu_flush_bands++;
    simu_stat->simu_flush_band_size[uId] += band_size;

    wtrAmp = (double)band_size / (dirty_n_inBand * BLCKSZ);
    char log[256];
    sprintf(log,"flush [%ld] times from fifo to smr,collect time:%lf, cnt=%ld,WtrAmp = %lf\n",simu_stat->simu_flush_bands,collect_time,dirty_n_inBand,wtrAmp);
    WriteLog(log);
}


static unsigned long
GetSMRActualBandSizeFromSSD(unsigned long offset)
{

    long		i, size, total_size = 0;
    for (i = 0; i < simu_stat->band_size_num; i++)
    {
        size = BNDSZ / 2 + i * 1024 * 1024;
        if (total_size + size *simu_stat-> num_each_size >= offset)
            return size;
        total_size += size * simu_stat->num_each_size;
    }

    return 0;
}

static unsigned long
GetSMRBandNumFromSSD(unsigned long offset)
{
    long		i, size, total_size = 0;
    for (i = 0; i < simu_stat->band_size_num; i++)
    {
        size = BNDSZ / 2 + i * 1024 * 1024;
        if (total_size + size *simu_stat-> num_each_size > offset)
            return simu_stat->num_each_size * i + (offset - total_size) / size;
        total_size += size * simu_stat->num_each_size;
    }

    return 0;
}

static off_t
GetSMROffsetInBandFromSSD(FIFODesc * ssd_hdr)
{
    long		i, size, total_size = 0;
    unsigned long	offset = ssd_hdr->tag.offset;

    for (i = 0; i < simu_stat->band_size_num; i++)
    {
        size = BNDSZ / 2 + i * 1024 * 1024;
        if (total_size + size * simu_stat->num_each_size > offset)
            return (offset - total_size - (offset - total_size) / size * size) / BLCKSZ;
        total_size += size * simu_stat->num_each_size;
    }

    return 0;
}

void PrintSimulatorStatistic()
{
    printf("----------------SIMULATOR------------\n");
    printf("Time:\n");
    printf("Read FIFO:\t%lf\nWrite FIFO:\t%lf\nRead SMR:\t%lf\nFlush SMR:\t%lf\n",simu_stat->simu_time_read_fifo, simu_stat->simu_time_write_fifo, simu_stat->simu_time_read_smr, simu_stat->simu_time_write_smr);
    printf("Total I/O:\t%lf\n", simu_stat->simu_time_read_fifo+simu_stat->simu_time_write_fifo+simu_stat->simu_time_read_smr+simu_stat->simu_time_write_smr);
    printf("FIFO Collect:\t%lf\n",simu_stat->simu_time_collectFIFO);
    printf("Block/Band Count:\n");
 //   printf("Read FIFO:\t%ld\nWrite FIFO:\t%ld\nFIFO Collect:\t%ld\nRead SMR:\t%ld\nFIFO Write HIT:\t%ld\n",simu_stat->simu_n_read_fifo, simu_stat->simu_n_write_fifo,simu_stat->simu_n_collect_fifo, simu_stat->simu_n_read_smr, simu_stat->simu_n_fifo_write_HIT);
    printf("Read Bands:\t%ld\nFlush Bands:\t%ld\nFlush BandSize:\t%ld\n",simu_stat->simu_read_smr_bands, simu_stat->simu_flush_bands, simu_stat->simu_flush_band_size);

    int i = 0;
    for(i = 0;i < 5;i++)        //up to 5 users
    {
        if(simu_stat->simu_flush_band_size[i] == 0)
            break;
        printf("User %d  WrtAmp:\t%lf\n",i,(double)simu_stat->simu_flush_band_size[i] / (simu_stat->simu_n_write_fifo[i] * BLCKSZ));
    }
//    printf("Total WrtAmp:\t%lf\n",(double)simu_stat->simu_flush_band_size / (simu_stat->simu_n_write_fifo * BLCKSZ));
}
