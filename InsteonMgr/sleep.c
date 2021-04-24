//
//  sleep.c
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/6/21.
//

#include "sleep.h"

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(unsigned int milliseconds){ // cross-platform sleep function
#ifdef WIN32
	 Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
	 struct timespec ts;
	 ts.tv_sec = milliseconds / 1000;
	 ts.tv_nsec = (milliseconds % 1000) * 1000000;
	 nanosleep(&ts, NULL);
#else
	 if (milliseconds >= 1000)
		sleep(milliseconds / 1000);
	 usleep((milliseconds % 1000) * 1000);
#endif
}
