typedef struct {
		int counter;
} atomic_t;

/**
 *  * atomic_add - add integer to atomic variable
 *   * @i: integer value to add
 *    * @v: pointer of type atomic_t
 *     *
 *      * Atomically adds @i to @v.
 *       */
static inline void atomic_add(int i, atomic_t *v)
{
		asm volatile(LOCK_PREFIX "addl %1,%0"
						     : "+m" (v->counter)
									     : "ir" (i));
}

/**
 *  * atomic_sub_and_test - subtract value from variable and test result
 *   * @i: integer value to subtract
 *    * @v: pointer of type atomic_t
 *     *
 *      * Atomically subtracts @i from @v and returns
 *       * true if the result is zero, or false for all
 *        * other cases.
 *         */
static inline int atomic_sub_and_test(int i, atomic_t *v)
{
		unsigned char c;

			asm volatile(LOCK_PREFIX "subl %2,%0; sete %1"
							     : "+m" (v->counter), "=qm" (c)
										     : "ir" (i) : "memory");
				return c;
}

/**
 *  * atomic_inc - increment atomic variable
 *   * @v: pointer of type atomic_t
 *    *
 *     * Atomically increments @v by 1.
 *      */
static inline void atomic_inc(atomic_t *v)
{
		asm volatile(LOCK_PREFIX "incl %0"
						     : "+m" (v->counter));
}
