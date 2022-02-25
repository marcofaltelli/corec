//
// Created by marco on 10/02/22.
//

#ifndef MT_DPDK_TAL_HANDLER_H
#define MT_DPDK_TAL_HANDLER_H

//    TAIL_UNWRAPPED || MIN_COUNTER
//          4B       ||     4B


uint64_t read_variable(uint64_t *var) {
    return __atomic_load_n(var, __ATOMIC_ACQUIRE);
}

uint32_t read_tail_w( uint64_t *var) {
    return __atomic_load_n(((__uint32_t *)var) + 1, __ATOMIC_ACQUIRE);
}

uint32_t read_tail_w_nonatomic( uint64_t *var) {
    return (uint32_t) (*var >> 32);
}

uint32_t read_min_counter( uint64_t *var) {
    return __atomic_load_n((__uint32_t *)var, __ATOMIC_ACQUIRE);
}

uint32_t read_min_counter_nonatomic( uint64_t *var) {
    return (uint32_t) *var;
}

void write_tail_w(uint64_t *var, uint32_t value) {
    uint32_t *var2 = ((uint32_t*) var) + 1;
    __atomic_store_n(var2, value, __ATOMIC_RELEASE);
}

void write_min_counter(uint64_t *var, uint32_t value) {
    uint32_t *var2 = ((uint32_t*) var);
    __atomic_store_n(var2, value, __ATOMIC_RELEASE);

}



#endif //MT_DPDK_TAL_HANDLER_H
