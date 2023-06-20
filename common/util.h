//Zahir Bingen s2436647

#ifndef UTIL_H
#define UTIL_H


#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

//Checks for error and exits
void error(int err);

//Sleeps for a random delay value between min_delay and max_delay
void random_delay(int min_delay, int max_delay);

#endif

