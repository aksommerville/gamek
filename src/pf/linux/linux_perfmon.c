#include "linux_internal.h"
#include <time.h>
#include <sys/time.h>

/* Begin performance monitor.
 */
 
void linux_perfmon_begin() {
  gamek_linux.starttime_real=linux_now_s();
  gamek_linux.starttime_cpu=linux_now_cpu_s();
}

/* Final performance report.
 */
 
void linux_perfmon_end() {
  double endtime_real=linux_now_s();
  double endtime_cpu=linux_now_cpu_s();
  double elapsed_real=endtime_real-gamek_linux.starttime_real;
  double elapsed_cpu=endtime_cpu-gamek_linux.starttime_cpu;
  double vrate=gamek_linux.vframec/elapsed_real;
  double urate=gamek_linux.uframec/elapsed_real;
  double arate=gamek_linux.aframec/elapsed_real;
  double cpuload=elapsed_cpu/elapsed_real;
  fprintf(stderr,
    "Run time %.03f s. video=%.03f Hz, audio=%.03f Hz, update=%.03f Hz, cpu=%.06f, clockfault=%d\n",
    elapsed_real,vrate,arate,urate,cpuload,gamek_linux.clockfaultc
  );
}

/* Current time.
 */
 
double linux_now_s() {
  struct timespec tv={0};
  clock_gettime(CLOCK_REALTIME,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

double linux_now_cpu_s() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

int64_t linux_now_us() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}
