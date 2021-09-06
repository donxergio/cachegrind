#ifndef __CACHEGRIND_H
#define __CACHEGRIND_H


/* This file is for inclusion into client (your!) code.

   You can use these macros to manipulate and query memory permissions
   inside your own programs.

   See comment near the top of valgrind.h on how to use them.
*/

#include "valgrind.h"

typedef enum { 
    CG_CHANGE_CACHE_REPL_POLICY = 0x3000
} Cg_ClientRequest;

typedef enum { 
    LRU_POLICY = 0,
    LIP_POLICY,
    RANDOM_POLICY,
    FIFO_POLICY,
    BIP_POLICY,
    DIP_POLICY,
    ALL_POLICY,
} Cg_CachePolicy;


/* Run-time change of the chosen cache replacementpolicy */
#define CACHEGRIND_CHANGE_CACHE_REPL_POLICY(_policy)                          \
    VALGRIND_DO_CLIENT_REQUEST_STMT(CG_CHANGE_CACHE_REPL_POLICY,                 \
                            ((_policy)), 0, 0, 0, 0)

#endif
