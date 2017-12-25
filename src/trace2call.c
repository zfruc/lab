#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "global.h"
#include "statusDef.h"

#include "timerUtils.h"
#include "cache.h"
#include "strategy/lru.h"
#include "trace2call.h"
#include "report.h"

extern struct RuntimeSTAT* STT;
#define REPORT_INTERVAL 10000
#define MONITOR_INTERVAL 3          //report the count of handled trace lines & cache hit rate for this period of time every 3 seconds

static void reportCurInfo();
static void report_ontime();
static void resetStatics();

static timeval  tv_trace_start, tv_trace_end;
static timeval  tv_monitor_last,tv_monitor_now;
static double time_trace;

/** single request statistic information **/
static timeval          tv_req_start, tv_req_stop;
static microsecond_t    msec_req;
extern microsecond_t    msec_r_hdd,msec_w_hdd,msec_r_ssd,msec_w_ssd;
extern int IsHit;
char logbuf[512];

void
trace_to_iocall(char *trace_file_path, int isWriteOnly,off_t startLBA)
{
    char		action;
    off_t		offset;
    char       *ssd_buffer;
    int	        returnCode;
    int         isFullSSDcache = 0;

#ifdef REPORT_MONITOR
    char buf[30];
    sprintf(buf,"%ld",STT->userId);
    FILE *monitor_file = fopen(strcat(buf,"_monitor.csv"),"w+");

    blkcnt_t last_hitnum_s=0,last_reqcnt_s=0;

    if(monitor_file  == NULL)
    {
        error("Failed to open monitor file!\n");
        exit(1);
    }

    perror("monitor.csv");

    if(monitor_file == 0)
    {
        printf("errno = %d.\n",errno);
        exit(1);
    }
#endif

#ifdef CG_THROTTLE
    static char* cgbuf;
    int returncode = posix_memalign(&cgbuf, 512, 4096);
#endif // CG_THROTTLE

    FILE *trace;
    if ((trace = fopen(trace_file_path, "rt")) == NULL)
    {
        error("Failed to open the trace file!\n");
        exit(1);
    }

    returnCode = posix_memalign(&ssd_buffer, 1024, 16*sizeof(char) * BLCKSZ);
    if (returnCode < 0)
    {
        error("posix memalign error\n");
        free(ssd_buffer);
        exit(-1);
    }
    int i;
    for (i = 0; i < 16 * BLCKSZ; i++)
    {
        ssd_buffer[i] = '1';
    }

    _TimerLap(&tv_trace_start);
#ifdef REPORT_MONITOR
    _TimerLap(&tv_monitor_last);
#endif
    static int req_cnt = 0;

    while (!feof(trace) && req_cnt++ < 84340000)
    {
//        if(feof(trace))
//            fseek(trace,0,SEEK_SET);
#ifdef CG_THROTTLE
        if(pwrite(ram_fd,cgbuf,1024,0) <= 0)
        {
            printf("write ramdisk error:%d\n",errno);
            exit(1);
        }
#endif // CG_THROTTLE

        returnCode = fscanf(trace, "%c %d %lu\n", &action, &i, &offset);
        if (returnCode < 0)
        {
            error("error while reading trace file.");
            break;
        }
        offset = (offset + startLBA) * BLCKSZ;

        if(!isFullSSDcache && (STT->flush_clean_blocks + STT->flush_hdd_blocks) > 0)
        {
            reportCurInfo();
            resetStatics();        // Because we do not care about the statistic while the process of filling SSD cache.
            isFullSSDcache = 1;
#ifdef REPORT_MONITOR
            last_hitnum_s = last_reqcnt_s = 0;
#endif
        }

#ifdef LOG_SINGLE_REQ
        _TimerLap(&tv_req_start);
#endif // TIMER_SINGLE_REQ
        if (action == ACT_WRITE) // Write = 1
        {
            STT->reqcnt_w++;
            write_block(offset, ssd_buffer);
        }
        else if (!isWriteOnly && action == ACT_READ)    // read = 9
        {
            STT->reqcnt_r++;
            read_block(offset,ssd_buffer);
        }
#ifdef LOG_SINGLE_REQ
        _TimerLap(&tv_req_stop);
        msec_req = TimerInterval_MICRO(&tv_req_start,&tv_req_stop);
        /*
            print log
            format:
            <req_id, r/w, ishit, time cost for: one request, read_ssd, write_ssd, read_smr, write_smr>
        */
        //sprintf(logbuf,"%lu,%c,%d,%ld,%ld,%ld,%ld,%ld\n",STT->reqcnt_s,action,IsHit,msec_req,msec_r_ssd,msec_w_ssd,msec_r_hdd,msec_w_hdd);
       // WriteLog(logbuf);
        msec_r_ssd = msec_w_ssd = msec_r_hdd = msec_w_hdd = 0;
#endif // TIMER_SINGLE_REQ

        if (++STT->reqcnt_s % REPORT_INTERVAL == 0)
        {
            report_ontime();
        }


        //ResizeCacheUsage();
#ifdef REPORT_MONITOR
        _TimerLap(&tv_monitor_now);
        if(Mirco2Sec(TimerInterval_MICRO(&tv_monitor_last,&tv_monitor_now)) > MONITOR_INTERVAL)
        {
            fprintf(monitor_file,"%ld,%.2f\n",STT->reqcnt_s,(STT->hitnum_s-last_hitnum_s)/(float)(STT->reqcnt_s - last_reqcnt_s));

            last_hitnum_s = STT->hitnum_s;
            last_reqcnt_s = STT->reqcnt_s;
            tv_monitor_last = tv_monitor_now;
            fflush(monitor_file);
        }
#endif
    }

#ifdef REPORT_MONITOR
    _TimerLap(&tv_monitor_now);
    fprintf(monitor_file,"%ld,%.2f\n",STT->reqcnt_s,(STT->hitnum_s-last_hitnum_s)/(float)(STT->reqcnt_s - last_reqcnt_s));
    last_hitnum_s = STT->hitnum_s;
    last_reqcnt_s = STT->reqcnt_s;
    tv_monitor_last = tv_monitor_now;
    fflush(monitor_file);
    fclose(monitor_file);
#endif

    _TimerLap(&tv_trace_end);
    time_trace = Mirco2Sec(TimerInterval_MICRO(&tv_trace_start,&tv_trace_end));
    reportCurInfo();
    free(ssd_buffer);
    fclose(trace);
}

static void reportCurInfo()
{
    struct timeval endtime;
    gettimeofday( &endtime, NULL );
    printf(" totalreqNum:%lu\n read_req_count: %lu\n write_req_count: %lu\n",
           STT->reqcnt_s,STT->reqcnt_r,STT->reqcnt_w);

    printf(" hit num:%lu\n hitnum_r:%lu\n hitnum_w:%lu\n",
           STT->hitnum_s,STT->hitnum_r,STT->hitnum_w);

    printf(" \n read hit rate:%.3f\n write hit rate:%.3f\n total hit rate:%.3f\n\n",
           STT->hitnum_r/(float) STT->reqcnt_r,STT->hitnum_w/(float) STT->reqcnt_w,STT->hitnum_s/(float) STT->reqcnt_s); 

    printf(" read_ssd_blocks:%lu\n flush_ssd_blocks:%lu\n read_hdd_blocks:%lu\n flush_hdd_blocks:%lu\n flush_clean_blocks:%lu\n",
           STT->load_ssd_blocks, STT->flush_ssd_blocks, STT->load_hdd_blocks, STT->flush_hdd_blocks, STT->flush_clean_blocks);

    printf(" hash_miss:%lu\n hashmiss_read:%lu\n hashmiss_write:%lu\n",
           STT->hashmiss_sum, STT->hashmiss_read, STT->hashmiss_write);

    printf(" end of time is (s) : %.6f\n",(double)endtime.tv_usec/1000000+endtime.tv_sec);

    printf(" total run time (s) : %lf\n time_read_ssd : %lf\n time_write_ssd : %lf\n time_read_smr : %lf\n time_write_smr : %lf\n",
           time_trace, STT->time_read_ssd, STT->time_write_ssd, STT->time_read_hdd, STT->time_write_hdd);
    printf("Batch flush HDD time:%lu\n",msec_bw_hdd);
}

static void report_ontime()
{
//    _TimerLap(&tv_checkpoint);
//    double timecost = Mirco2Sec(TimerInterval_SECOND(&tv_trace_start,&tv_checkpoint));
    printf("totalreq:%lu, readreq:%lu, hit:%lu, readhit:%lu, flush_ssd_blk:%lu flush_hdd_blk:%lu, hashmiss:%lu, readhassmiss:%lu writehassmiss:%lu\n",
           STT->reqcnt_s,STT->reqcnt_r, STT->hitnum_s, STT->hitnum_r, STT->flush_ssd_blocks, STT->flush_hdd_blocks, STT->hashmiss_sum, STT->hashmiss_read, STT->hashmiss_write);
}

static void resetStatics()
{

    STT->hitnum_s = 0;
    STT->hitnum_r = 0;
    STT->hitnum_w = 0;
    STT->load_ssd_blocks = 0;
    STT->flush_ssd_blocks = 0;
    STT->load_hdd_blocks = 0;
    STT->flush_hdd_blocks = 0;
    STT->flush_clean_blocks = 0;

    STT->time_read_hdd = 0.0;
    STT->time_write_hdd = 0.0;
    STT->time_read_ssd = 0.0;
    STT->time_write_ssd = 0.0;
    STT->hashmiss_sum = 0;
    STT->hashmiss_read = 0;
    STT->hashmiss_write = 0;
    msec_bw_hdd = 0;
}

