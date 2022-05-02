#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

#include "ngscope/hdr/dciLib/time_stamp.h"
/* Timestamp related function */
int64_t timestamp_ns()
{
  struct timespec ts;

  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;
  return ret;
}

int64_t timestamp_us( )
{
  struct timespec ts;

  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;

  return ret / 1000;
}

int64_t timestamp_ms( )
{
  struct timespec ts;

  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;

  return ret / 1000000;
}
