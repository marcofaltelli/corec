//
// Created by marco on 10/02/22.
//

#ifndef MT_DPDK_READ_DONE_H
#define MT_DPDK_READ_DONE_H

#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "limits.h"
#include <stdint.h>
#include "stdbool.h"
#include "assert.h"
#include "rte_log.h"
#include "rte_atomic.h"
#include <math.h>

//#define SIZE_ULL log2((sizeof(unsigned int) * 8))

#define wrap_ring_no_incr(index, ring_size) (uint16_t) ((index) & (ring_size - 1))
#define wrap_ring(index, ring_size) (uint16_t) ((index + 1) & (ring_size - 1))
#define wrap_ring_decrease(index, ring_size) (uint16_t) ((index - 1) & (ring_size - 1))
#define wrap_ring_n(index, n, size) (uint16_t) ((index + (n)) & (size - 1))

 static void write_batch64(unsigned int *array, uint32_t start, uint32_t end, uint16_t size) {
    //printf("Writing to tail from start %u until end %u\n", start, end);
    uint8_t size_ull = sizeof(unsigned int) * 8;
    unsigned int start_var = start/size_ull;
    uint8_t size_array = size/size_ull;
    unsigned int step = start;
    //RTE_LOG(CRIT, EAL, "Write starts from %u\t", start);
    while(step != end) {
        //__sync_synchronize();
        unsigned int var = __atomic_load_n(array + start_var, __ATOMIC_ACQUIRE);
        if ( var != UINT_MAX) {
            printf("Error! var %u is not UINT_MAX but %u\n", start_var, var);
            exit(1);
        }
        //__sync_synchronize();
        __atomic_store_n(array + start_var, 0, __ATOMIC_RELEASE);
        //  printf("batch of packets position %u is now %u\t", start_var, array[start_var]);
        start_var = wrap_ring(start_var, size_array);
        step = wrap_ring_n(step, size_ull, size);
    }
    //RTE_LOG(CRIT, EAL, "ends at %u original end was %u\n", step, end);
    //printf("Wrote whole %u variable final step is %u \n", start_var, step);
}

 static void write_bit(unsigned int *array, uint32_t bit) {
    uint8_t size_ull = sizeof(unsigned int) * 8;
    unsigned int start_var = bit/size_ull;
    unsigned int offset = bit % size_ull;
    // printf("offset is %d bit %lu\t", offset, bit);
    unsigned int one = 1;
    unsigned int varr =  (one << offset);
    //printf("var is %llu\t", varr);
    unsigned int var2, var3;
    do {
        var2 = __atomic_load_n(array + start_var, __ATOMIC_ACQUIRE);
        /* if ((varr & var2) != 0) {
             printf("Error! var is %llu var2 is %llu\n", varr, var2);
             exit(1);
         }*/
        var3 = varr | var2;
    } while(!__atomic_compare_exchange_n(array + start_var, &var2, var3, true, __ATOMIC_RELEASE, __ATOMIC_SEQ_CST));
    //printf("Wrote register %llu\n", var3);
}

/*
 * array: the pointer to the READ_DONE array
 * uint32_t start: the start in the 0...NUM_RX_QUEUE_ENTRIES - 1 range
 * uint16_t size: the size of our RX queue (should be equal to NUM_RX_QUEUE_ENTRIES)
 */
 static uint32_t read_most_significant_bit(unsigned long long *array, uint32_t start, uint16_t size) {

    //we need size_ull to understand whether we need to check also the next ull variable in the array or not
    uint8_t size_ull = sizeof(unsigned long long) * 8;
    //The first variable to be checked is the one in the start/size_ull position
    unsigned int start_var = start/size_ull;
    uint32_t clz;

    clz = __builtin_clzll(*(array + start_var));
    //if clz == 0,
    while (clz == 0) {
        start_var = wrap_ring(start_var, size/(size_ull));
        clz = __builtin_clzll(*(array + start_var));
    }
    //start_var will now indicate the variable in the array containing the most significant bit
    //the position of our most significant 1-bit is actually at size_ull - clz - 1
    //We return the position in the RX queue of the last descriptor processed, that is start_var * size_ull - clz;
    if (start_var == 0)
        //in this case, returning the MSB is enough
        return size_ull - clz - 1;
    else
        //Otherwise, we return the descriptor ID for this queue
        return (start_var * size_ull) - clz;

}

 static uint32_t read_end_batch64(unsigned int *array, uint32_t start, uint16_t size) {
    uint8_t size_ull = sizeof(unsigned int) * 8;
    unsigned int start_var = start/size_ull;
    //__sync_synchronize();
    while (__atomic_load_n(array + start_var, __ATOMIC_ACQUIRE) == UINT_MAX) {
        start_var = wrap_ring(start_var, size/size_ull);
        //  __sync_synchronize();
    }
    return start_var * size_ull;
}

 static uint32_t read_batch64(unsigned int *array, uint32_t start, uint16_t size) {
    //RTE_LOG(CRIT, EAL, "entering\n");
    uint8_t size_ull = sizeof(unsigned int) * 8;
    unsigned int start_var = start/size_ull;
    unsigned int processed = 0;
    //__sync_synchronize();
    while (__atomic_load_n(array + start_var, __ATOMIC_ACQUIRE) == UINT_MAX) {
        start_var = wrap_ring(start_var, size/size_ull);
        processed += size_ull;
     //   RTE_LOG(CRIT, EAL, "start_var %u processed %u\n", start_var, processed);
        //  __sync_synchronize();
    }
   // RTE_LOG(CRIT, EAL, "exit\n");
    return processed;
}

 static bool read_bit(unsigned int *array, unsigned int index)
{
    uint8_t size_ull = sizeof(unsigned int) * 8;
    unsigned int start_var = index/size_ull;
    unsigned int offset = index % size_ull;
    unsigned int bit = 1 << offset;
    //RTE_LOG(CRIT, EAL, "Var %d offset %u\n", start_var, offset);
    return((bool) (bit & __atomic_load_n(array + start_var, __ATOMIC_ACQUIRE)));
}

static void write_batch_is_done(unsigned int *array, uint32_t start, uint32_t end, uint16_t size) {
    uint8_t size_ull = sizeof(unsigned int) * 8;
    uint8_t size_array = size/size_ull;
    unsigned int start_var = start/size_ull;
    unsigned int start_offset = start % size_ull;
    unsigned int end_var = end/size_ull;
    unsigned int end_offset = end % size_ull;
    unsigned int old_batch, new_batch;
    unsigned int var1, var2, flag_batch;
    while (start_var != end_var) {
        var1 = UINT_MAX << start_offset;
        //var2 = UINT_MAX;
        //flag_batch = var1 & var2;
        do {
            old_batch = __atomic_load_n(array + start_var, __ATOMIC_ACQUIRE);
            new_batch = old_batch | var1;
        } while (!__atomic_compare_exchange_n(array + start_var, &old_batch, new_batch, false, __ATOMIC_SEQ_CST,
                                              __ATOMIC_SEQ_CST));
        start_var = wrap_ring(start_var, size_array);
        start_offset = 0;
    }
    var1 = UINT_MAX << start_offset;
    var2 = UINT_MAX >> (size_ull - end_offset - 1);
    flag_batch = var1 & var2;
    do {
        old_batch = __atomic_load_n(array + start_var, __ATOMIC_ACQUIRE);
        new_batch = old_batch | flag_batch;
    } while (!__atomic_compare_exchange_n(array + start_var, &old_batch, new_batch, true, __ATOMIC_RELEASE,
                                          __ATOMIC_SEQ_CST));
}

#endif //MT_DPDK_READ_DONE_H
