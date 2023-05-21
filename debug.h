/*
 * author : Shuichi TAKANO
 * since  : Tue Oct 30 2018 6:35:28
 */
#pragma once

#include <stdio.h>

#ifdef NDEBUG
#define DBOUT(x) \
    do           \
    {            \
    } while (0)
#else
#define DBOUT(x) printf x
#endif
