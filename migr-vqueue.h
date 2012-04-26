#ifndef MIGR_VQUEUE_H
#define MIGR_VQUEUE_H

static inline uint32_t atomic_compare_exchange32(volatile uint32_t *p, uint32_t old, uint32_t newv)
{
    uint32_t out;
    __asm__ __volatile__ ( "lock; cmpxchgl %2,%1"
                           : "=a" (out), "+m" (*p)
                           : "q" (newv), "0" (old)
                           : "cc");
    return out;
}

static int
hold_page(volatile uint32_t *v_p, uint32_t old_vnum, uint32_t new_vnum) {

    return (atomic_compare_exchange32(v_p, old_vnum, new_vnum * 2 + 1) != old_vnum);
}

static void
release_page(volatile uint32_t *v_p, uint32_t new_vnum) {
    *v_p = new_vnum * 2 + 2;
}

static int
hold_block(volatile uint32_t *v_p, uint32_t old_vnum, uint32_t new_vnum) {

    return (atomic_compare_exchange32(v_p, old_vnum, new_vnum * 2 + 1) != old_vnum);
}

static void
release_block(volatile uint32_t *v_p, uint32_t new_vnum) {
    *v_p = new_vnum * 2 + 2;
}

static int 
hold_block_cb(volatile uint32_t *v_p) {
    return (atomic_compare_exchange32(v_p, 0, 1) == 0);
}
#endif
