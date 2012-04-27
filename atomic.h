typedef struct {
    volatile int counter;
} atomic_t;

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v.
 */
static inline void atomic_add(int i, atomic_t *v)
{
    asm volatile("lock" "addl %1,%0"
                 : "+m" (v->counter)
                 : "ir" (i));
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static inline int atomic_sub_and_test(int i, atomic_t *v)
{
    unsigned char c;

    asm volatile("lock " "subl %2,%0; sete %1"
                 : "+m" (v->counter), "=qm" (c)
                 : "ir" (i) : "memory");
    return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1.
 */
static inline void atomic_inc(atomic_t *v)
{
    asm volatile("lock " "incl %0"
                 : "+m" (v->counter));
}

static inline int atomic_read(atomic_t *v) 
{
    return v->counter;
}

static inline int atomic_set(atomic_t *v, int i) 
{
    return v->counter = i;
}

#define SPIN_LOCK_UNLOCKED 0

#define ADDR (*(volatile int *) addr)

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int test_and_set_bit(int nr, volatile void *addr)
{
    int oldbit;

    asm volatile (
        "btsl %2,%1\n\tsbbl %0,%0"
        : "=r" (oldbit), "=m" (ADDR)
        : "Ir" (nr), "m" (ADDR) : "memory");
    return oldbit;
}

typedef int disk_spinlock_t;

static inline void disk_spin_lock(disk_spinlock_t *lock)
{
    while ( test_and_set_bit(1, lock) );
}

static inline void disk_spin_lock_init(disk_spinlock_t *lock)
{
    *lock = SPIN_LOCK_UNLOCKED;
}

static inline void disk_spin_unlock(disk_spinlock_t *lock)
{
    *lock = SPIN_LOCK_UNLOCKED;
}

static inline int disk_spin_trylock(disk_spinlock_t *lock)
{
    return !test_and_set_bit(1, lock);
}
