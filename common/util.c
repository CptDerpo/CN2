//Zahir Bingen s2436647

#include "util.h"
#include <string.h>

void error(int err)
{
    if(err < 0)
    {
        exit(0);
    }
}

void random_delay(int min_delay, int max_delay)
{
    int delay = (rand() % (max_delay - min_delay + 1)) + min_delay;

    useconds_t delay_us = delay * 1000;

    usleep(delay_us);
}