#ifndef _DHT_PACKED_H_
#define _DHT_PACKED_H_

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "atomic_ops.h"
#include "utils.h"

#define true 1
#define false 0

#define READ_ONLY_FAIL
/* #define DEBUG */

#if defined(DEBUG)
#  define DPP(x)	x++				
#else
#  define DPP(x)
#endif

#define CACHE_LINE_SIZE 64
#define ENTRIES_PER_BUCKET 3

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif

#if defined(__sparc__)
#  define PREFETCHW(x) 
#  define PREFETCH(x) 
#  define PREFETCHNTA(x) 
#  define PREFETCHT0(x) 
#  define PREFETCHT1(x) 
#  define PREFETCHT2(x) 

#  define PAUSE    asm volatile("rd    %%ccr, %%g0\n\t" \
				::: "memory")
#  define _mm_pause() PAUSE
#  define _mm_mfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore");
#  define _mm_lfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore");
#  define _mm_sfence() __asm__ __volatile__("membar #StoreLoad | #StoreStore");


#elif defined(__tile__)
#  define _mm_lfence() arch_atomic_read_barrier()
#  define _mm_sfence() arch_atomic_write_barrier()
#  define _mm_mfence() arch_atomic_full_barrier()
#  define _mm_pause() cycle_relax()
#endif

#define CAS_U64_BOOL(a, b, c) (CAS_U64(a, b, c) == b)

typedef uintptr_t clht_addr_t;
typedef volatile uintptr_t clht_val_t;

typedef uint8_t hyht_lock_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) bucket_s
{
  hyht_lock_t lock;
  uint32_t last;
  clht_addr_t key[ENTRIES_PER_BUCKET];
  clht_val_t  val[ENTRIES_PER_BUCKET];
  struct bucket_s* next;
} bucket_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) clht_wrapper
{
  struct
  {
    struct hashtable_s* ht;
    uint8_t next_cache_line[CACHE_LINE_SIZE - (sizeof(void*))];
  };
} clht_wrapper_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) hashtable_s
{
  union
  {
    struct
    {
      size_t num_buckets;
      ALIGNED(CACHE_LINE_SIZE) bucket_t* table;
    };
    uint8_t padding[CACHE_LINE_SIZE];
  };
} hashtable_t;


static inline void
_mm_pause_rep(uint64_t w)
{
  while (w--)
    {
      _mm_pause();
    }
}

/* #define TAS_WITH_FAI */
#define TAS_WITH_TAS
/* #define TAS_WITH_CAS */
/* #define TAS_WITH_SWAP */

#if defined(XEON) | defined(COREi7)
#  define TAS_RLS_MFENCE() _mm_mfence();
#else
#  define TAS_RLS_MFENCE()
#endif

#if defined(TAS_WITH_FAI)
#  define LOCK_ACQ(lock)			\
  while (FAI_U32((uint32_t*) lock))		\
    {						\
      _mm_pause();				\
      DPP(put_num_restarts);			\
    }						
#elif defined(TAS_WITH_TAS)			
#  define LOCK_ACQ(lock)			\
  while (TAS_U8((uint8_t*) (lock)))		\
    {						\
      _mm_pause();				\
      DPP(put_num_restarts);			\
    }						
#elif defined(TAS_WITH_SWAP)			
#  define LOCK_ACQ(lock)			\
  while (SWAP_U32((uint32_t*) lock, 1))		\
    {						\
      _mm_pause();				\
      DPP(put_num_restarts);			\
    }						
#endif

#define LOCK_RLS(lock)				\
  TAS_RLS_MFENCE();				\
  *lock = 0;	  

/* Create a new hashtable. */
hashtable_t* ht_create(uint32_t num_buckets );
clht_wrapper_t* clht_wrapper_create(uint32_t num_buckets);

/* Hash a key for a particular hashtable. */
uint32_t ht_hash(hashtable_t* hashtable, clht_addr_t key );

/* Insert a key-value pair into a hashtable. */
int ht_put(clht_wrapper_t* h, clht_addr_t key, clht_val_t val);

/* Retrieve a key-value pair from a hashtable. */
clht_val_t ht_get(hashtable_t* hashtable, clht_addr_t key);

/* Remove a key-value pair from a hashtable. */
clht_val_t ht_remove(clht_wrapper_t* hashtable, clht_addr_t key);

/* Dealloc the hashtable */
void ht_destroy(hashtable_t* hashtable);

uint32_t ht_size(hashtable_t* hashtable);

void ht_print(hashtable_t* hashtable, uint32_t num_buckets);

bucket_t* create_bucket();


#endif /* _DHT_PACKED_H_ */