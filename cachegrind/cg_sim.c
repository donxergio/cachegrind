/*--------------------------------------------------------------------*/
/*--- Cache simulation                                    cg_sim.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Cachegrind, a Valgrind tool for cache
   profiling programs.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

/* Notes:
  - simulates a write-allocate cache
  - (block --> set) hash function uses simple bit selection
  - handling of references straddling two cache blocks:
      - counts as only one cache access (not two)
      - both blocks hit                  --> one hit
      - one block hits, the other misses --> one miss
      - both blocks miss                 --> one miss (not two)
*/

#define CYCLES 5000
#define ONLINE_DIFERENCE 100

typedef struct
{
   Int size; /* bytes */
   Int assoc;
   Int line_size; /* bytes */
   Int sets;
   Int sets_min_1;
   Int line_size_bits;
   Int tag_shift;
   HChar desc_line[128]; /* large enough */
   UWord *tags;
} cache_t2;

Bool status_lru;
Bool status_fifo;
Bool status_random;
Bool status_bip;
Bool status_active;
Bool status_adaptative;
Bool using_data = False;
Bool print_output = False;
static cache_t2 LL;
static cache_t2 I1;
static cache_t2 D1;
cache_t2 I1_lru;
cache_t2 LL_lru;
cache_t2 D1_lru;
cache_t2 I1_fifo;
cache_t2 LL_fifo;
cache_t2 D1_fifo;
cache_t2 I1_random;
cache_t2 LL_random;
cache_t2 D1_random;
cache_t2 I1_bip;
cache_t2 LL_bip;
cache_t2 D1_bip;
cache_t2 I1_best;
cache_t2 LL_best;
cache_t2 D1_best;
cache_t2 LL_lru_temp;
cache_t2 D1_lru_temp;
cache_t2 LL_fifo_temp;
cache_t2 D1_fifo_temp;
cache_t2 LL_random_temp;
cache_t2 D1_random_temp;
cache_t2 LL_bip_temp;
cache_t2 D1_bip_temp;
int hits_lru[CYCLES];
int hits_fifo[CYCLES];
int hits_random[CYCLES];
int hits_bip[CYCLES];
unsigned long long access_counter = 0;
unsigned long long hit_counter_LRU = 0;
unsigned long long hit_counter_FIFO = 0;
unsigned long long hit_counter_RANDOM = 0;
unsigned long long hit_counter_BIP = 0;
unsigned long long hit_counter_ADAPTATIVE = 0;
unsigned long long hit_counter_ACTIVE = 0;
unsigned long long temp_hit_counter_LRU = 0;
unsigned long long temp_hit_counter_FIFO = 0;
unsigned long long temp_hit_counter_RANDOM = 0;
unsigned long long temp_hit_counter_BIP = 0;
unsigned long long miss_counter_LRU = 0;
unsigned long long miss_counter_FIFO = 0;
unsigned long long miss_counter_RANDOM = 0;
unsigned long long miss_counter_BIP = 0;
unsigned long long miss_counter_ADAPTATIVE = 0;
unsigned long long miss_counter_ACTIVE = 0;
unsigned long long temp_D1_miss_counter_LRU = 0;
unsigned long long temp_D1_miss_counter_FIFO = 0;
unsigned long long temp_D1_miss_counter_RANDOM = 0;
unsigned long long temp_D1_miss_counter_BIP = 0;
unsigned long long temp_D1_miss_counter_ADAPTATIVE = 0;
unsigned long long D1_miss_counter_LRU = 0;
unsigned long long D1_miss_counter_FIFO = 0;
unsigned long long D1_miss_counter_RANDOM = 0;
unsigned long long D1_miss_counter_BIP = 0;
unsigned long long D1_miss_counter_ADAPTATIVE = 0;
unsigned long long D1_miss_counter_ACTIVE = 0;
unsigned long long temp_counter = 0;
unsigned long long D1_only_BIP_counter = 0;
unsigned long long D1_only_LRU_counter = 0;
unsigned long long D1_only_FIFO_counter = 0;
unsigned long long D1_only_RANDOM_counter = 0;
unsigned long long D1_all_misses_counter = 0;
unsigned long long blocks = 0 ;
unsigned long long blocks_LRU = 0 ;
unsigned long long blocks_BIP = 0 ;
unsigned long long blocks_FIFO = 0 ;
unsigned long long blocks_RANDOM = 0 ;
unsigned long long online_D1_miss_counter_LRU = 512;
unsigned long long online_D1_miss_counter_FIFO = 512;
unsigned long long online_D1_miss_counter_RANDOM = 512;
unsigned long long online_D1_miss_counter_BIP = 512;
unsigned long long average_miss_LRU;
unsigned long long average_miss_FIFO;
unsigned long long average_miss_RANDOM;
unsigned long long average_miss_BIP;

__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_lru(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_lip(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_random(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_fifo(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_bip(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_all(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_runtime(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_exhaustive(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_online(cache_t2 *c, UInt set_no, UWord tag);

Bool (*cachesim_setref_is_miss)(cache_t2 *, UInt, UWord) = &cachesim_setref_is_miss_lip;

static Bool isInteger(double val)
{
    int truncated = (int)val;
    return (val == truncated);
}

__attribute__((always_inline)) static __inline__ void init_caches(cache_t2 *c, cache_t config)
{
   Int i;
   c->size = config.size;
   c->assoc = config.assoc;
   c->line_size = config.line_size;
   double sets = (c->size / c->line_size) / c->assoc;
   if(isInteger(VG_(log2)(sets))){//Changed to allow set not power of 2  
      c->sets = (c->size / c->line_size) / c->assoc;
      c->sets_min_1 = c->sets - 1;  
      c->line_size_bits = VG_(log2)(c->line_size);
      c->tag_shift = c->line_size_bits + VG_(log2)(c->sets);
   }
   else{
      int power2Sets = 2^((int)VG_(log2)(sets))+1;
      c->sets = (c->size / c->line_size) / c->assoc;
      c->sets_min_1 = power2Sets - 1;  
      c->line_size_bits = VG_(log2)(c->line_size);
      c->tag_shift = c->line_size_bits + VG_(log2)(power2Sets);
   }

   if (c->assoc == 1)
   {
      VG_(sprintf)
      (c->desc_line, "%d B, %d B, direct-mapped",
       c->size, c->line_size);
   }
   else
   {
      VG_(sprintf)
      (c->desc_line, "%d B, %d B, %d-way associative",
       c->size, c->line_size, c->assoc);
   }

   c->tags = VG_(malloc)("cg.sim.ci.1", sizeof(UWord) * c->sets * c->assoc);

   for (i = 0; i < c->sets * c->assoc; i++)
      c->tags[i] = 0;
}

__attribute__((always_inline)) static __inline__ void copy_caches(cache_t2 *c_ori, cache_t2 *c_dest)
{
   Int i;
   c_dest->size = c_ori->size;
   c_dest->assoc = c_ori->assoc;
   c_dest->line_size = c_ori->line_size;

   c_dest->sets = c_ori->sets;
   c_dest->sets_min_1 = c_ori->sets_min_1;
   c_dest->line_size_bits = c_ori->line_size_bits;
   c_dest->tag_shift = c_ori->tag_shift;

   for (i = 0; i < c_dest->sets * c_dest->assoc; i++)
      c_dest->tags[i] = c_ori->tags[i];
}

/* By this point, the size/assoc/line_size has been checked. */
static void cachesim_initcache(cache_t config, cache_t2 *c)
{
   Int i;

   c->size = config.size;
   c->assoc = config.assoc;
   c->line_size = config.line_size;

/*    c->sets = (c->size / c->line_size) / c->assoc;
   c->sets_min_1 = c->sets - 1;
   c->line_size_bits = VG_(log2)(c->line_size);
   c->tag_shift = c->line_size_bits + VG_(log2)(c->sets); */

      double sets = (c->size / c->line_size) / c->assoc;
   if(isInteger(VG_(log2)(sets))){//Changed to allow set not power of 2  
      c->sets = (c->size / c->line_size) / c->assoc;
      c->sets_min_1 = c->sets - 1;  
      c->line_size_bits = VG_(log2)(c->line_size);
      c->tag_shift = c->line_size_bits + VG_(log2)(c->sets);
   }
   else{
      int power2Sets = 2^((int)VG_(log2)(sets))+1;
      c->sets = (c->size / c->line_size) / c->assoc;
      c->sets_min_1 = power2Sets - 1;  
      c->line_size_bits = VG_(log2)(c->line_size);
      c->tag_shift = c->line_size_bits + VG_(log2)(power2Sets);
   }

   if (c->assoc == 1)
   {
      VG_(sprintf)
      (c->desc_line, "%d B, %d B, direct-mapped",
       c->size, c->line_size);
   }
   else
   {
      VG_(sprintf)
      (c->desc_line, "%d B, %d B, %d-way associative",
       c->size, c->line_size, c->assoc);
   }

   c->tags = VG_(malloc)("cg.sim.ci.1",
                         sizeof(UWord) * c->sets * c->assoc);

   for (i = 0; i < c->sets * c->assoc; i++)
      c->tags[i] = 0;

   /* For cache_lru */
   /*cache_lru.size      = config.size;
   cache_lru.assoc     = config.assoc;
   cache_lru.line_size = config.line_size;

   cache_lru.sets           = (cache_lru.size / cache_lru.line_size) / cache_lru.assoc;  
   cache_lru.sets_min_1     = cache_lru.sets - 1;
   cache_lru.line_size_bits = VG_(log2)(cache_lru.line_size);
   cache_lru.tag_shift      = cache_lru.line_size_bits + VG_(log2)(cache_lru.sets);

   if (cache_lru.assoc == 1) {
      VG_(sprintf)(cache_lru.desc_line, "%d B, %d B, direct-mapped", 
                                 cache_lru.size, cache_lru.line_size);
   } else {
      VG_(sprintf)(cache_lru.desc_line, "%d B, %d B, %d-way associative",
                                 cache_lru.size, cache_lru.line_size, cache_lru.assoc);
   }


   cache_lru.tags = VG_(malloc)("cg.sim.ci.1",
                         sizeof(UWord) * cache_lru.sets * cache_lru.assoc);

   for (i = 0; i < cache_lru.sets * cache_lru.assoc; i++)
      cache_lru.tags[i] = 0;*/

   /* For cache_bip */
   /*cache_bip.size      = config.size;
   cache_bip.assoc     = config.assoc;
   cache_bip.line_size = config.line_size;

   cache_bip.sets           = (cache_bip.size / cache_bip.line_size) / cache_bip.assoc;  
   cache_bip.sets_min_1     = cache_bip.sets - 1;
   cache_bip.line_size_bits = VG_(log2)(cache_bip.line_size);
   cache_bip.tag_shift      = cache_bip.line_size_bits + VG_(log2)(cache_bip.sets);

   if (cache_bip.assoc == 1) {
      VG_(sprintf)(cache_bip.desc_line, "%d B, %d B, direct-mapped", 
                                 cache_bip.size, cache_bip.line_size);
   } else {
      VG_(sprintf)(cache_bip.desc_line, "%d B, %d B, %d-way associative",
                                 cache_bip.size, cache_bip.line_size, cache_bip.assoc);
   }


   cache_bip.tags = VG_(malloc)("cg.sim.ci.1",
                         sizeof(UWord) * cache_bip.sets * cache_bip.assoc);

   for (i = 0; i < cache_bip.sets * cache_bip.assoc; i++)
      cache_bip.tags[i] = 0; */
}

/* This attribute forces GCC to inline the function, getting rid of a
 * lot of indirection around the cache_t2 pointer, if it is known to be
 * constant in the caller (the caller is inlined itself).
 * Without inlining of simulator functions, cachegrind can get 40% slower.
 */
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_lru(cache_t2 *c, UInt set_no, UWord tag)
{
   int i, j;
   UWord *set;
   set = &(c->tags[set_no * c->assoc]);

   /* This loop is unrolled for just the first case, which is the most */
   /* common.  We can't unroll any further because it would screw up   */
   /* if we have a direct-mapped (1-way) cache.                        */
   if (tag == set[0])
   {
      if (using_data && print_output)
      {
         //VG_(printf)("LRU_Hit\n");
         VG_(printf)("H");
      }
      return False;
   }

   /* If the tag is one other than the MRU, move it into the MRU spot  */
   /* and shuffle the rest down.                                       */
   for (i = 1; i < c->assoc; i++)
   {
      if (tag == set[i])
      {
         for (j = i; j > 0; j--)
         {
            set[j] = set[j - 1];
         }
         set[0] = tag;
         if (using_data && print_output)
         {
            //VG_(printf)("LRU_Hit\n");
            VG_(printf)("H");
         }
         return False;
      }
   }

   /* A miss;  install this tag as MRU, shuffle rest down. */
   for (j = c->assoc - 1; j > 0; j--)
   {
      set[j] = set[j - 1];
   }
   set[0] = tag;
   if (using_data && print_output)
      VG_(printf)("M");
   return True;
}

/* This attribute forces GCC to inline the function, getting rid of a
 * lot of indirection around the cache_t2 pointer, if it is known to be
 * constant in the caller (the caller is inlined itself).
 * Without inlining of simulator functions, cachegrind can get 40% slower.
 */
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_lip(cache_t2 *c, UInt set_no, UWord tag)
{
   int i, j;
   UWord *set;

   set = &(c->tags[set_no * c->assoc]);

   /* This loop is unrolled for just the first case, which is the most */
   /* common.  We can't unroll any further because it would screw up   */
   /* if we have a direct-mapped (1-way) cache.                        */
   if (tag == set[0])
      return False;

   /* If the tag is one other than the MRU, move it into the MRU spot  */
   /* and shuffle the rest down.                                       */
   for (i = 1; i < c->assoc; i++)
   {
      if (tag == set[i])
      {
         for (j = i; j > 0; j--)
         {
            set[j] = set[j - 1];
         }
         set[0] = tag;

         return False;
      }
   }

   /* A miss;  install this tag as LRU. */
   set[c->assoc - 1] = tag;

   return True;
}

/* This attribute forces GCC to inline the function, getting rid of a
 * lot of indirection around the cache_t2 pointer, if it is known to be
 * constant in the caller (the caller is inlined itself).
 * Without inlining of simulator functions, cachegrind can get 40% slower.
 */
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_random(cache_t2 *c, UInt set_no, UWord tag)
{
   int i;
   UWord *set;

   set = &(c->tags[set_no * c->assoc]);

   /* A hit;  */
   for (i = 0; i < c->assoc; i++)
   {
      if (tag == set[i])
      {
         if (using_data && print_output)
         {
            //VG_(printf)("Random_Hit\n");
            VG_(printf)("H");
         }
         return False;
      }
   }

   /* A miss;  install this tag at random position */
   i = VG_(random)(NULL) % c->assoc;
   set[i] = tag;
   if (using_data && print_output)
      VG_(printf)("M");
   //VG_(printf)("Random_Miss\n");
   return True;
}

/* This attribute forces GCC to inline the function, getting rid of a
 * lot of indirection around the cache_t2 pointer, if it is known to be
 * constant in the caller (the caller is inlined itself).
 * Without inlining of simulator functions, cachegrind can get 40% slower.
 */
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_fifo(cache_t2 *c, UInt set_no, UWord tag)
{
   int i, j;
   UWord *set;

   set = &(c->tags[set_no * c->assoc]);

   /* A hit; */
   for (i = 0; i < c->assoc; i++)
   {
      if (tag == set[i])
      {
         if (using_data && print_output)
         {
            //VG_(printf)("FIFO_Hit\n");
            VG_(printf)("H");
         }
         return False;
      }
   }

   /* A miss;  install this tag as MRU, shuffle rest down. */
   for (j = c->assoc - 1; j > 0; j--)
   {
      set[j] = set[j - 1];
   }
   set[0] = tag;
   if (using_data && print_output)
      VG_(printf)("M");
   //VG_(printf)("FIFO_Miss\n");
   return True;
}

/* This attribute forces GCC to inline the function, getting rid of a
 * lot of indirection around the cache_t2 pointer, if it is known to be
 * constant in the caller (the caller is inlined itself).
 * Without inlining of simulator functions, cachegrind can get 40% slower.
 */
/*
 * Bimodal insertion policy (BIP) put most blocks at the tail; with a small probability, insert at head; for thrashing
 * workloads, it can retain part of the working set and yield hits on them.
 * Source: https://my.eng.utah.edu/~cs7810/pres/11-7810-09.pdf
 */
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_bip(cache_t2 *c, UInt set_no, UWord tag)
{
   //VG_(printf)("BIP Throttle parameter: %f\n", bip_throttle_parameter); //bip_throttle_parameter is correct here.
   /*
    TODO: from Giovani: the code below is a copy from the LIP Policy.
    Should modify to implement BIP instead.
   */

   int i, j;
   UWord *set;

   set = &(c->tags[set_no * c->assoc]);

   /* This loop is unrolled for just the first case, which is the most */
   /* common.  We can't unroll any further because it would screw up   */
   /* if we have a direct-mapped (1-way) cache.                        */
   if (tag == set[0])
   {
      if (using_data && print_output)
         VG_(printf)("H");
      //VG_(printf)("BIP_Hit\n");
      return False;
   }

   /* If the tag is one other than the MRU, move it into the MRU spot  */
   /* and shuffle the rest down.                                       */
   for (i = 1; i < c->assoc; i++)
   {
      if (tag == set[i])
      {
         for (j = i; j > 0; j--)
         {
            set[j] = set[j - 1];
         }
         set[0] = tag;
         if (using_data && print_output)
         {
            //VG_(printf)("BIP_Hit\n");
            VG_(printf)("H");
         }
         return False;
      }
   }

   //if bimodal throttle parameter is 1.0, behaves like LRU
   if (bip_throttle_parameter == 1.0)
   {

      /* A miss;  install this tag as MRU, shuffle rest down. */
      for (j = c->assoc - 1; j > 0; j--)
      {
         set[j] = set[j - 1];
      }
      set[0] = tag;
   }
   else if (bip_throttle_parameter == 0.0)
   { //if bimodal throttle parameter is 0.0, behaves like LIP

      /* A miss;  install this tag as LRU. */
      set[c->assoc - 1] = tag;
   }
   else
   { //uses the probability, either LRU or LIP

      double prob = (double)(VG_(random)(NULL) % 100 + 1.0);

      if (prob >= (bip_throttle_parameter * 100))
      { //LIP
         /* A miss;  install this tag as LRU. */
         set[c->assoc - 1] = tag;
      }
      else
      { //LRU
         /* A miss;  install this tag as MRU, shuffle rest down. */
         for (j = c->assoc - 1; j > 0; j--)
         {
            set[j] = set[j - 1];
         }
         set[0] = tag;
      }
   }
   if (using_data && print_output)
      VG_(printf)("M");
   //   VG_(printf)("BIP_Miss\n");
   return True;
}

__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip(cache_t2 *c, UInt set_no, UWord tag)
{

   //static unsigned int psel_size = 4; //number of bits of psel

   //psel_msb = 2 << (psel_size - 1)
   static unsigned int psel_msb = 8; // decimal value when there is 1 in the MSB of psel (1000)

   //psel_max = 2 << (psel_size) - 1
   static unsigned int psel_max = 15; // maximum value of psel (1111)

   static unsigned int psel = 8; // Policy selector, must be iniatialize in the middle of the range [0,psel_max]

   Bool is_miss_lru = False;
   Bool is_miss_bip = False;
   if (c == &D1)
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&D1_lru, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&D1_bip, set_no, tag);
   }
   else if (c == &LL)
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&LL_lru, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&LL_bip, set_no, tag);
   }
   else
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&I1_lru, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&I1_bip, set_no, tag);
   }

   if (is_miss_lru)
   {
      if (psel < psel_max)
      {
         psel++;
         //VG_(printf)("%u \n", psel);
      }
   }
   if (is_miss_bip)
   {
      if (psel > 0)
      {
         psel--;
         //VG_(printf)("%u \n", psel);
      }
   }

   if (psel >= psel_msb)
   { //if there is 1 in the MSB apply BIP
      //VG_(printf)("BIP used \n");
      return cachesim_setref_is_miss_bip(c, set_no, tag);
   }
   else
   {
      //VG_(printf)("LRU used \n");
      return cachesim_setref_is_miss_lru(c, set_no, tag);
   }
}

/*void calculate_ratio(unsigned int counter, unsigned int hits, float *ratio)
{
   if (counter >= CYCLES)
   {
      *ratio = (float)hits / (float)counter;
   }
   else{
      *ratio = -1;
   }
}*/

/*This function rotates the array provided and returns the sum of all the values inside it after the rotation and insertion of last_value
*/
__attribute__((always_inline)) static __inline__ int rotate_array(int *array, int array_length, int last_value)
{
   int temp = 0;
   if (array_length == 1)
   {
      return last_value;
   }
   for (int i = 0; i < array_length - 1; i++)
   {
      array[i] = array[i + 1];
      temp = temp + array[i];
      //VG_(printf) ("%d", array[i]);
   }
   array[array_length - 1] = last_value;
   temp = temp + last_value;
   //VG_(printf) ("%d \n", array[array_length - 1]);
   return temp;
}

__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_all(cache_t2 *c, UInt set_no, UWord tag)
{
   Bool in_LL = (c == &LL);

   if (status_lru == True)
   {
      if (c == &I1)
         status_lru = cachesim_setref_is_miss_lru(&I1_lru, set_no, tag);
      if (c == &LL)
         status_lru = cachesim_setref_is_miss_lru(&LL_lru, set_no, tag);
      if (c == &D1){
         status_lru = cachesim_setref_is_miss_lru(&D1_lru, set_no, tag);
         if (status_lru){
            temp_D1_miss_counter_LRU++;
            D1_miss_counter_LRU++;
            rotate_array(&hits_lru, CYCLES, 0);
         }
      }
      if (!status_lru && using_data){
         temp_hit_counter_LRU++;
         rotate_array(&hits_lru, CYCLES, 1);
      }

   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_fifo == True)
   {
      if (c == &I1)
         status_fifo = cachesim_setref_is_miss_fifo(&I1_fifo, set_no, tag);
      if (c == &LL)
         status_fifo = cachesim_setref_is_miss_fifo(&LL_fifo, set_no, tag);
      if (c == &D1){
         status_fifo = cachesim_setref_is_miss_fifo(&D1_fifo, set_no, tag);
         if (status_fifo){
            temp_D1_miss_counter_FIFO++;
            D1_miss_counter_FIFO++;
            rotate_array(&hits_fifo, CYCLES, 0);
         }
      }
      if (!status_fifo && using_data)
      {
         temp_hit_counter_FIFO++;
         rotate_array(&hits_fifo, CYCLES, 0);
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_random == True)
   {
      if (c == &I1)
         status_random = cachesim_setref_is_miss_random(&I1_random, set_no, tag);
      if (c == &LL)
         status_random = cachesim_setref_is_miss_random(&LL_random, set_no, tag);
      if (c == &D1){
         status_random = cachesim_setref_is_miss_random(&D1_random, set_no, tag);
         if (status_random){
            temp_D1_miss_counter_RANDOM++;
            D1_miss_counter_RANDOM++;
            rotate_array(&hits_random, CYCLES, 0);
         }
      }
      if(!status_random && using_data){
         temp_hit_counter_RANDOM++;
         rotate_array(&hits_random, CYCLES, 1);
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_bip == True)
   {
      if (c == &I1)
         status_bip = cachesim_setref_is_miss_bip(&I1_bip, set_no, tag);
      if (c == &LL)
         status_bip = cachesim_setref_is_miss_bip(&LL_bip, set_no, tag);
      if (c == &D1){
         status_bip = cachesim_setref_is_miss_bip(&D1_bip, set_no, tag);
         if(status_bip){
            D1_miss_counter_BIP++;
            temp_D1_miss_counter_BIP++;
            rotate_array(&hits_bip, CYCLES, 0);
         }
      }
      if(!status_bip && using_data){
         temp_hit_counter_BIP++;
         rotate_array(&hits_bip, CYCLES, 1);
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_adaptative == True)
   {
      if (using_data && print_output){
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
         VG_(printf)("-");
      }
      switch (current_adaptative_cache_replacement_policy)
      {
      case ALL_POLICY:         
      status_adaptative = True;
         break;
      case EXHAUSTIVE_POLICY:
         status_adaptative = True;
         //Start searching
         current_adaptative_cache_replacement_policy = ALL_POLICY;
         break;
      default:
         break;
      }
   }
   else
   {
      if (using_data && print_output){
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
         VG_(printf)("-");
      }
   }

   if(c == &D1 && (!status_bip && status_lru && status_fifo && status_random)){
      D1_only_BIP_counter++;
   }
   if(c == &D1 && (status_bip && !status_lru && status_fifo && status_random)){
      D1_only_LRU_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && !status_fifo && status_random)){
      D1_only_FIFO_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && !status_random)){
      D1_only_RANDOM_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && status_random)){
      D1_all_misses_counter++;
   }

   return status_lru || status_fifo || status_random || status_bip || status_adaptative; 
   
}

__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_exhaustive(cache_t2 *c, UInt set_no, UWord tag)
{
      Bool in_LL = (c == &LL);

   if (status_lru == True)
   {
      if (c == &I1)
         status_lru = cachesim_setref_is_miss_lru(&I1_lru, set_no, tag);
      if (c == &LL)
         status_lru = cachesim_setref_is_miss_lru(&LL_lru, set_no, tag);
      if (c == &D1){
         status_lru = cachesim_setref_is_miss_lru(&D1_lru, set_no, tag);
         if (status_lru){
            temp_D1_miss_counter_LRU++;
            D1_miss_counter_LRU++;
            average_miss_LRU = rotate_array(&hits_lru, CYCLES, 1);
         }
         else
            average_miss_LRU = rotate_array(&hits_lru, CYCLES, 0);
      }
      if (!status_lru && using_data){
         temp_hit_counter_LRU++;
      }

   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_fifo == True)
   {
      if (c == &I1)
         status_fifo = cachesim_setref_is_miss_fifo(&I1_fifo, set_no, tag);
      if (c == &LL)
         status_fifo = cachesim_setref_is_miss_fifo(&LL_fifo, set_no, tag);
      if (c == &D1){
         status_fifo = cachesim_setref_is_miss_fifo(&D1_fifo, set_no, tag);
         if (status_fifo){
            temp_D1_miss_counter_FIFO++;
            D1_miss_counter_FIFO++;
            average_miss_FIFO = rotate_array(&hits_fifo, CYCLES, 1);
         }
         else
            average_miss_FIFO = rotate_array(&hits_fifo, CYCLES, 0);
      }
      if (!status_fifo && using_data)
      {
         temp_hit_counter_FIFO++;
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_random == True)
   {
      if (c == &I1)
         status_random = cachesim_setref_is_miss_random(&I1_random, set_no, tag);
      if (c == &LL)
         status_random = cachesim_setref_is_miss_random(&LL_random, set_no, tag);
      if (c == &D1){
         status_random = cachesim_setref_is_miss_random(&D1_random, set_no, tag);
         if (status_random){
            temp_D1_miss_counter_RANDOM++;
            D1_miss_counter_RANDOM++;
            average_miss_RANDOM = rotate_array(&hits_random, CYCLES, 1);
         }
         else
            average_miss_RANDOM = rotate_array(&hits_random, CYCLES, 0);
      }
      if(!status_random && using_data){
         temp_hit_counter_RANDOM++;
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_bip == True)
   {
      if (c == &I1)
         status_bip = cachesim_setref_is_miss_bip(&I1_bip, set_no, tag);
      if (c == &LL)
         status_bip = cachesim_setref_is_miss_bip(&LL_bip, set_no, tag);
      if (c == &D1){
         status_bip = cachesim_setref_is_miss_bip(&D1_bip, set_no, tag);
         if(status_bip){
            D1_miss_counter_BIP++;
            temp_D1_miss_counter_BIP++;
            average_miss_BIP =  rotate_array(&hits_bip, CYCLES, 1);
         }
         else
             average_miss_BIP =  rotate_array(&hits_bip, CYCLES, 0);
      }
      if(!status_bip && using_data){
         temp_hit_counter_BIP++;
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_adaptative == True)
   {
      if (using_data && print_output){
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
         VG_(printf)("-");
      }
   }
   else
   {
      if (using_data && print_output){
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
         VG_(printf)("-");
      }
   }

   if(c == &D1 && (!status_bip && status_lru && status_fifo && status_random)){
      D1_only_BIP_counter++;
   }
   if(c == &D1 && (status_bip && !status_lru && status_fifo && status_random)){
      D1_only_LRU_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && !status_fifo && status_random)){
      D1_only_FIFO_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && !status_random)){
      D1_only_RANDOM_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && status_random)){
      D1_all_misses_counter++;
   }

   return status_lru || status_fifo || status_random || status_bip || status_adaptative; 
}
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_runtime(cache_t2 *c, UInt set_no, UWord tag)
{
   Bool in_LL = (c == &LL);

   if (status_lru == True)
   {
      if (c == &I1)
         status_lru = cachesim_setref_is_miss_lru(&I1_lru, set_no, tag);
      if (c == &LL)
         status_lru = cachesim_setref_is_miss_lru(&LL_lru, set_no, tag);
      if (c == &D1){
         status_lru = cachesim_setref_is_miss_lru(&D1_lru, set_no, tag);
         if(status_lru){
            temp_D1_miss_counter_LRU++;
            D1_miss_counter_LRU++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_fifo == True)
   {
      if (c == &I1)
         status_fifo = cachesim_setref_is_miss_fifo(&I1_fifo, set_no, tag);
      if (c == &LL)
         status_fifo = cachesim_setref_is_miss_fifo(&LL_fifo, set_no, tag);
      if (c == &D1){
         status_fifo = cachesim_setref_is_miss_fifo(&D1_fifo, set_no, tag);
         if(status_fifo){
            temp_D1_miss_counter_FIFO++;
            D1_miss_counter_FIFO++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_random == True)
   {
      if (c == &I1)
         status_random = cachesim_setref_is_miss_random(&I1_random, set_no, tag);
      if (c == &LL)
         status_random = cachesim_setref_is_miss_random(&LL_random, set_no, tag);
      if (c == &D1){
         status_random = cachesim_setref_is_miss_random(&D1_random, set_no, tag);
         if(status_random){
            temp_D1_miss_counter_RANDOM++;
            D1_miss_counter_RANDOM++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_bip == True)
   {
      if (c == &I1)
         status_bip = cachesim_setref_is_miss_bip(&I1_bip, set_no, tag);
      if (c == &LL)
         status_bip = cachesim_setref_is_miss_bip(&LL_bip, set_no, tag);
      if (c == &D1){
         status_bip = cachesim_setref_is_miss_bip(&D1_bip, set_no, tag);
         if(status_bip){
            temp_D1_miss_counter_BIP++;
            D1_miss_counter_BIP++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_adaptative == True)
   {
      if (using_data && print_output)
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
      //Check better eviction policy
      /*LRU_POLICY,
         LIP_POLICY,
         RANDOM_POLICY,
         FIFO_POLICY,
         BIP_POLICY,
         DIP_POLICY,
         ALL_POLICY,
         ADAPTATIVE_POLICY*/
      switch (current_adaptative_cache_replacement_policy)
      {
      case 0:
         status_adaptative = cachesim_setref_is_miss_lru(c, set_no, tag);
         break;
      case 1:
         status_adaptative = cachesim_setref_is_miss_lru(c, set_no, tag);
         break;
      case 2:
         status_adaptative = cachesim_setref_is_miss_random(c, set_no, tag);
         break;
      case 3:
         status_adaptative = cachesim_setref_is_miss_fifo(c, set_no, tag);
         break;
      case 4:
         status_adaptative = cachesim_setref_is_miss_bip(c, set_no, tag);
         break;
      default:
         status_adaptative = cachesim_setref_is_miss_lru(c, set_no, tag);
      }
      if (c == &D1){
         if (status_adaptative){\
            temp_D1_miss_counter_ADAPTATIVE++;
            D1_miss_counter_ADAPTATIVE++;
            D1_miss_counter_ACTIVE++;
         }
      }
   }
   else
   {
      if (using_data && print_output){
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
         VG_(printf)("-");
      }
   }

   if(c == &D1 && (!status_bip && status_lru && status_fifo && status_random)){
      D1_only_BIP_counter++;
   }
   if(c == &D1 && (status_bip && !status_lru && status_fifo && status_random)){
      D1_only_LRU_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && !status_fifo && status_random)){
      D1_only_FIFO_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && !status_random)){
      D1_only_RANDOM_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && status_random)){
      D1_all_misses_counter++;
   }

   return status_lru || status_fifo || status_random || status_bip || status_adaptative;
}
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_online(cache_t2 *c, UInt set_no, UWord tag){
      Bool in_LL = (c == &LL);

   if (status_lru == True)
   {
      if (c == &I1)
         status_lru = cachesim_setref_is_miss_lru(&I1_lru, set_no, tag);
      if (c == &LL)
         status_lru = cachesim_setref_is_miss_lru(&LL_lru, set_no, tag);
      if (c == &D1){
         status_lru = cachesim_setref_is_miss_lru(&D1_lru, set_no, tag);
         if(status_lru){
            temp_D1_miss_counter_LRU++;
            D1_miss_counter_LRU++;
            if(online_D1_miss_counter_LRU <1024)
               online_D1_miss_counter_LRU++;
         }
         else{
            if (online_D1_miss_counter_LRU > 0)
               online_D1_miss_counter_LRU--;
            temp_hit_counter_LRU++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_fifo == True)
   {
      if (c == &I1)
         status_fifo = cachesim_setref_is_miss_fifo(&I1_fifo, set_no, tag);
      if (c == &LL)
         status_fifo = cachesim_setref_is_miss_fifo(&LL_fifo, set_no, tag);
      if (c == &D1){
         status_fifo = cachesim_setref_is_miss_fifo(&D1_fifo, set_no, tag);
         if(status_fifo){
            temp_D1_miss_counter_FIFO++;
            D1_miss_counter_FIFO++;
            if(online_D1_miss_counter_FIFO < 1024)
               online_D1_miss_counter_FIFO++;
         }
         else{
            if(online_D1_miss_counter_FIFO > 0)
           online_D1_miss_counter_FIFO--; 
           temp_hit_counter_FIFO++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_random == True)
   {
      if (c == &I1)
         status_random = cachesim_setref_is_miss_random(&I1_random, set_no, tag);
      if (c == &LL)
         status_random = cachesim_setref_is_miss_random(&LL_random, set_no, tag);
      if (c == &D1){
         status_random = cachesim_setref_is_miss_random(&D1_random, set_no, tag);
         if(status_random){
            temp_D1_miss_counter_RANDOM++;
            D1_miss_counter_RANDOM++;
            if(online_D1_miss_counter_RANDOM < 1024)
               online_D1_miss_counter_RANDOM++;
         }
         else{
            if(online_D1_miss_counter_RANDOM > 0)
            online_D1_miss_counter_RANDOM--;
            temp_hit_counter_RANDOM++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_bip == True)
   {
      if (c == &I1)
         status_bip = cachesim_setref_is_miss_bip(&I1_bip, set_no, tag);
      if (c == &LL)
         status_bip = cachesim_setref_is_miss_bip(&LL_bip, set_no, tag);
      if (c == &D1){
         status_bip = cachesim_setref_is_miss_bip(&D1_bip, set_no, tag);
         if(status_bip){
            temp_D1_miss_counter_BIP++;
            D1_miss_counter_BIP++;
            if(online_D1_miss_counter_BIP < 1024)
               online_D1_miss_counter_BIP++;
         }
         else{
            if(online_D1_miss_counter_BIP > 0)
               online_D1_miss_counter_BIP--;
            temp_hit_counter_BIP++;
         }
      }
   }
   else
   {
      if (using_data && print_output)
         VG_(printf)("-");
   }
   if (status_adaptative == True)
   {
      if (using_data && print_output)
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
      //Check selected eviction policy
      /*LRU_POLICY,
         LIP_POLICY,
         RANDOM_POLICY,
         FIFO_POLICY,
         BIP_POLICY,
         DIP_POLICY,
         ALL_POLICY,
         ADAPTATIVE_POLICY*/
      switch (current_adaptative_cache_replacement_policy)
      {
      case 0:
         status_adaptative = cachesim_setref_is_miss_lru(c, set_no, tag);
         break;
      case 1:
         status_adaptative = cachesim_setref_is_miss_lru(c, set_no, tag);
         break;
      case 2:
         status_adaptative = cachesim_setref_is_miss_random(c, set_no, tag);
         break;
      case 3:
         status_adaptative = cachesim_setref_is_miss_fifo(c, set_no, tag);
         break;
      case 4:
         status_adaptative = cachesim_setref_is_miss_bip(c, set_no, tag);
         break;
      default:
         status_adaptative = cachesim_setref_is_miss_lru(c, set_no, tag);
      }
      if (c == &D1){
         if (status_adaptative)
            D1_miss_counter_ADAPTATIVE++;
      }
   }
   else
   {
      if (using_data && print_output){
         VG_(printf)("%d",current_adaptative_cache_replacement_policy);
         VG_(printf)("-");
      }
   }

   if(c == &D1 && (!status_bip && status_lru && status_fifo && status_random)){
      D1_only_BIP_counter++;
   }
   if(c == &D1 && (status_bip && !status_lru && status_fifo && status_random)){
      D1_only_LRU_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && !status_fifo && status_random)){
      D1_only_FIFO_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && !status_random)){
      D1_only_RANDOM_counter++;
   }
   if(c == &D1 && (status_bip && status_lru && status_fifo && status_random)){
      D1_all_misses_counter++;
   }

   return status_lru || status_fifo || status_random || status_bip || status_adaptative;
}
__attribute__((always_inline)) static __inline__ Bool cachesim_ref_is_miss(cache_t2 *c, Addr a, UChar size)
{
   /* A memory block has the size of a cache line */
   UWord block1 = a >> c->line_size_bits;
   UWord block2 = (a + size - 1) >> c->line_size_bits;
   UInt set1 = block1 & c->sets_min_1;

   /* Tags used in real caches are minimal to save space.
    * As the last bits of the block number of addresses mapping
    * into one cache set are the same, real caches use as tag
    *   tag = block >> log2(#sets)
    * But using the memory block as more specific tag is fine,
    * and saves instructions.
    */
   UWord tag1 = block1;

   /* Access entirely within line. */
   if (block1 == block2)
      return cachesim_setref_is_miss(c, set1, tag1);

   /* Access straddles two lines. */
   else if (block1 + 1 == block2)
   {
      UInt set2 = block2 & c->sets_min_1;
      UWord tag2 = block2;

      /* always do both, as state is updated as side effect */
      if (cachesim_setref_is_miss(c, set1, tag1))
      {
         cachesim_setref_is_miss(c, set2, tag2);
         return True;
      }
      return cachesim_setref_is_miss(c, set2, tag2);
   }
   VG_(printf)
   ("addr: %lx  size: %u  blocks: %lu %lu",
    a, size, block1, block2);
   VG_(tool_panic)
   ("item straddles more than two cache sets");
   /* not reached */
   return True;
}

static void cachesim_initcaches(cache_t I1c, cache_t D1c, cache_t LLc)
{
   if (cache_replacement_policy == LRU_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_lru;
   else if (cache_replacement_policy == LIP_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_lip;
   else if (cache_replacement_policy == RANDOM_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_random;
   else if (cache_replacement_policy == FIFO_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_fifo;
   else if (cache_replacement_policy == BIP_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_bip;
   else if (cache_replacement_policy == DIP_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_dip;
   else if (cache_replacement_policy == ALL_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_all;
   else if (cache_replacement_policy == ADAPTATIVE_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_runtime;
   else if (cache_replacement_policy ==EXHAUSTIVE_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_exhaustive;
   else if (cache_replacement_policy ==ONLINE_POLICY)
      cachesim_setref_is_miss = &cachesim_setref_is_miss_online;
      

   cachesim_initcache(I1c, &I1);
   cachesim_initcache(D1c, &D1);
   cachesim_initcache(LLc, &LL);
   cachesim_initcache(D1c, &D1_lru);
   cachesim_initcache(LLc, &LL_lru);
   cachesim_initcache(I1c, &I1_lru);
   cachesim_initcache(D1c, &D1_fifo);
   cachesim_initcache(LLc, &LL_fifo);
   cachesim_initcache(I1c, &I1_fifo);
   cachesim_initcache(D1c, &D1_random);
   cachesim_initcache(LLc, &LL_random);
   cachesim_initcache(I1c, &I1_random);
   cachesim_initcache(D1c, &D1_bip);
   cachesim_initcache(LLc, &LL_bip);
   cachesim_initcache(I1c, &I1_bip);
   cachesim_initcache(D1c, &D1_best);
   cachesim_initcache(LLc, &LL_best);
   cachesim_initcache(I1c, &I1_best);
   cachesim_initcache(LLc, &LL_lru_temp);
   cachesim_initcache(D1c, &D1_lru_temp);
   cachesim_initcache(LLc, &LL_fifo_temp);
   cachesim_initcache(D1c, &D1_fifo_temp);
   cachesim_initcache(LLc, &LL_random_temp);
   cachesim_initcache(D1c, &D1_random_temp);
   cachesim_initcache(LLc, &LL_bip_temp);
   cachesim_initcache(D1c, &D1_bip_temp);
}

__attribute__((always_inline)) static __inline__ void cachesim_I1_doref_Gen(Addr a, UChar size, ULong *m1, ULong *mL)
{
   status_lru = True;
   status_fifo = True;
   status_random = True;
   status_bip = True;
   status_active = True;
   status_adaptative = True;
   using_data = False;

/*    if (current_cache_replacement_policy == ONLINE_POLICY)
   {
      if(online_D1_miss_counter_LRU <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }
         //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
         else if(online_D1_miss_counter_FIFO <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_FIFO <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_FIFO <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = FIFO_POLICY;
            if(print_output)
               VG_(printf)("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
         }
         //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
         else if(online_D1_miss_counter_RANDOM <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = RANDOM_POLICY;
            if(print_output)
               VG_(printf)("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
         }
         //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
         else if(online_D1_miss_counter_BIP <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_RANDOM - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = BIP_POLICY;
            if(print_output)
               VG_(printf)("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n",temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
         }
         else{
            //current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }
   } */
/*    if (current_cache_replacement_policy == ADAPTATIVE_POLICY || current_cache_replacement_policy == EXHAUSTIVE_POLICY)
   {
      if (current_cache_replacement_policy == EXHAUSTIVE_POLICY && current_adaptative_cache_replacement_policy == ADAPTATIVE_POLICY){
         if(print_output)
            VG_(printf)("*-*-*\n");
         //if(temp_hit_counter_LRU > temp_hit_counter_FIFO && temp_hit_counter_LRU > temp_hit_counter_RANDOM && temp_hit_counter_LRU > temp_hit_counter_BIP){
         if(temp_D1_miss_counter_LRU <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_LRU <= temp_D1_miss_counter_RANDOM && temp_D1_miss_counter_LRU <= temp_D1_miss_counter_BIP){
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }
         //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
         else if(temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_RANDOM && temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_BIP){
            current_adaptative_cache_replacement_policy = FIFO_POLICY;
            if(print_output)
               VG_(printf)("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
         }
         //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
         else if(temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_BIP){
            current_adaptative_cache_replacement_policy = RANDOM_POLICY;
            if(print_output)
               VG_(printf)("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
         }
         //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
         else if(temp_D1_miss_counter_BIP <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_BIP <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_BIP <= temp_D1_miss_counter_RANDOM){
            current_adaptative_cache_replacement_policy = BIP_POLICY;
            if(print_output)
               VG_(printf)("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n",temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
         }
         else{
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }
      }
      switch (current_adaptative_cache_replacement_policy)
      {
      case LRU_POLICY: 
         copy_caches(&I1_lru, &I1);
         copy_caches(&LL_lru, &LL);
         break;
      case LIP_POLICY:
         copy_caches(&I1_lru, &I1);
         copy_caches(&LL_lru, &LL);
         break;
      case RANDOM_POLICY:
         copy_caches(&I1_random, &I1);
         copy_caches(&LL_random, &LL);
         break;
      case FIFO_POLICY:
         copy_caches(&I1_fifo, &I1);
         copy_caches(&LL_fifo, &LL);
         break;
      case BIP_POLICY:
         copy_caches(&I1_bip, &I1);
         copy_caches(&LL_bip, &LL);
         break;
      case DIP_POLICY:
         copy_caches(&I1_bip, &I1);
         copy_caches(&LL_bip, &LL);
         break;
       case EXHAUSTIVE_POLICY:
         temp_hit_counter_LRU = 0;
         temp_hit_counter_BIP = 0;
         temp_hit_counter_FIFO = 0;
         temp_hit_counter_RANDOM =0;
         temp_D1_miss_counter_LRU = 0;
         temp_D1_miss_counter_BIP = 0;
         temp_D1_miss_counter_FIFO = 0;
         temp_D1_miss_counter_RANDOM =0;
         temp_counter = 0;
         break; 
      default:
         break;
      }
   } */

   if (cachesim_ref_is_miss(&I1, a, size))
   {
      (*m1)++;
      if (cachesim_ref_is_miss(&LL, a, size))
         (*mL)++;
   }
}

// common special case IrNoX
__attribute__((always_inline)) static __inline__ void cachesim_I1_doref_NoX(Addr a, UChar size, ULong *m1, ULong *mL)
{
   status_lru = True;
   status_fifo = True;
   status_random = True;
   status_bip = True;
   status_active = True;
   status_adaptative = True;
   using_data = False;

/*    if (current_cache_replacement_policy == ONLINE_POLICY)
   {
         if(online_D1_miss_counter_LRU <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }
         //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
         else if(online_D1_miss_counter_FIFO <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_FIFO <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_FIFO <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = FIFO_POLICY;
            if(print_output)
               VG_(printf)("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
         }
         //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
         else if(online_D1_miss_counter_RANDOM <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = RANDOM_POLICY;
            if(print_output)
               VG_(printf)("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
         }
         //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
         else if(online_D1_miss_counter_BIP <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_RANDOM - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = BIP_POLICY;
            if(print_output)
               VG_(printf)("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n",temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
         }
         else{
            //current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }
   } */

//    if (current_cache_replacement_policy == ADAPTATIVE_POLICY || current_cache_replacement_policy == EXHAUSTIVE_POLICY /* || current_cache_replacement_policy == ALL_POLICY  */)
//    {
//       if ((current_cache_replacement_policy == EXHAUSTIVE_POLICY || current_cache_replacement_policy == ALL_POLICY) && current_adaptative_cache_replacement_policy == ADAPTATIVE_POLICY)
//       {
//          if (print_output)
//             VG_(printf)
//             ("*-*-*\n");
//          // if(temp_hit_counter_LRU > temp_hit_counter_FIFO && temp_hit_counter_LRU > temp_hit_counter_RANDOM && temp_hit_counter_LRU > temp_hit_counter_BIP){
//          if (temp_D1_miss_counter_LRU <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_LRU <= temp_D1_miss_counter_RANDOM && temp_D1_miss_counter_LRU <= temp_D1_miss_counter_BIP)
//          {
//             current_adaptative_cache_replacement_policy = LRU_POLICY;
//             if (print_output)
//                VG_(printf)
//                ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
//             hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
//             D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
//          }
//          // else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
//          else if (temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_RANDOM && temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_BIP)
//             current_adaptative_cache_replacement_policy = FIFO_POLICY;
//          if (print_output)
//             VG_(printf)
//             ("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
//          hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
//          D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
//       }
//       // else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
//       else if (temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_BIP)
//       {
//          current_adaptative_cache_replacement_policy = RANDOM_POLICY;
//          if (print_output)
//             VG_(printf)
//             ("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
//          hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
//          D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
//       }
//       // else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
//       else if (temp_D1_miss_counter_BIP <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_BIP <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_BIP <= temp_D1_miss_counter_RANDOM)
//       {
//          current_adaptative_cache_replacement_policy = BIP_POLICY;
//          if (print_output)
//             VG_(printf)
//             ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
//          hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
//          D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
//       }
//       else
//       {
//          current_adaptative_cache_replacement_policy = LRU_POLICY;
//          if (print_output)
//             VG_(printf)
//             ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
//          hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
//          D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
//       }
   
//    switch (current_adaptative_cache_replacement_policy)
//    {
//    case LRU_POLICY:
//       copy_caches(&I1_lru, &I1);
//       copy_caches(&LL_lru, &LL);
//       break;
//    case LIP_POLICY:
//       copy_caches(&I1_lru, &I1);
//       copy_caches(&LL_lru, &LL);
//       break;
//    case RANDOM_POLICY:
//       copy_caches(&I1_random, &I1);
//       copy_caches(&LL_random, &LL);
//       break;
//    case FIFO_POLICY:
//       copy_caches(&I1_fifo, &I1);
//       copy_caches(&LL_fifo, &LL);
//       break;
//    case BIP_POLICY:
//       copy_caches(&I1_bip, &I1);
//       copy_caches(&LL_bip, &LL);
//       break;
//    case DIP_POLICY:
//       copy_caches(&I1_bip, &I1);
//       copy_caches(&LL_bip, &LL);
//       break;
//    case EXHAUSTIVE_POLICY:
//       temp_hit_counter_LRU = 0;
//       temp_hit_counter_BIP = 0;
//       temp_hit_counter_FIFO = 0;
//       temp_hit_counter_RANDOM = 0;
//       temp_D1_miss_counter_LRU = 0;
//       temp_D1_miss_counter_BIP = 0;
//       temp_D1_miss_counter_FIFO = 0;
//       temp_D1_miss_counter_RANDOM = 0;
//       temp_counter = 0;
//       break;
//    default:
//       break;
//    }
// }
   UWord block = a >> I1.line_size_bits;
   UInt I1_set = block & I1.sets_min_1;

   // use block as tag
   if (cachesim_setref_is_miss(&I1, I1_set, block))
   {
      UInt LL_set = block & LL.sets_min_1;
      (*m1)++;
      // can use block as tag as L1I and LL cache line sizes are equal
      if (cachesim_setref_is_miss(&LL, LL_set, block))
         (*mL)++;
   }
}

__attribute__((always_inline)) static __inline__ void cachesim_D1_doref(Addr a, UChar size, ULong *m1, ULong *mL)
{
   if(current_cache_replacement_policy == ALL_POLICY){
      if (temp_counter >= density_access_counter)
      {
         current_adaptative_cache_replacement_policy = ADAPTATIVE_POLICY;
      }
   }

   if (current_cache_replacement_policy == ALL_POLICY && current_adaptative_cache_replacement_policy == ADAPTATIVE_POLICY){
      blocks++;
      if(print_output)
         VG_(printf)("*-*-*\n");
      //if(temp_hit_counter_LRU > temp_hit_counter_FIFO && temp_hit_counter_LRU > temp_hit_counter_RANDOM && temp_hit_counter_LRU > temp_hit_counter_BIP){
      if(temp_D1_miss_counter_LRU <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_LRU <= temp_D1_miss_counter_RANDOM && temp_D1_miss_counter_LRU <= temp_D1_miss_counter_BIP){
         current_adaptative_cache_replacement_policy = LRU_POLICY;
         if(print_output)
            VG_(printf)("LRU SELECTED\n");
         blocks_LRU++;
         hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
         D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
      }
      //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
      else if(temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_RANDOM && temp_D1_miss_counter_FIFO <= temp_D1_miss_counter_BIP){
         current_adaptative_cache_replacement_policy = FIFO_POLICY;
         if(print_output)
            VG_(printf)("FIFO SELECTED\n");
         blocks_FIFO++;
         hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
         D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
      }
      //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
      else if(temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_RANDOM <= temp_D1_miss_counter_BIP){
         current_adaptative_cache_replacement_policy = RANDOM_POLICY;
         if(print_output)
            VG_(printf)("RANDOM SELECTED\n");
         blocks_RANDOM++;
         hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
         D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
      }
      //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
      else if(temp_D1_miss_counter_BIP <= temp_D1_miss_counter_LRU && temp_D1_miss_counter_BIP <= temp_D1_miss_counter_FIFO && temp_D1_miss_counter_BIP <= temp_D1_miss_counter_RANDOM){
         current_adaptative_cache_replacement_policy = BIP_POLICY;
         if(print_output)
            VG_(printf)("BIP SELECTED\n");
         blocks_BIP++;
         hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
         D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
      }
      else{
         current_adaptative_cache_replacement_policy = LRU_POLICY;
         if(print_output)
            VG_(printf)("LRU_d SELECTED\n");
         blocks_LRU++;
         hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
         D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
      }

      if (print_output)
      {
         VG_(printf)
         ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
         VG_(printf)
         ("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
         VG_(printf)
         ("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
         VG_(printf)
         ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
         VG_(printf)
         ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
      }
      if (current_cache_replacement_policy == ALL_POLICY)
      {
         current_adaptative_cache_replacement_policy = EXHAUSTIVE_POLICY;
         temp_hit_counter_LRU = 0;
         temp_hit_counter_BIP = 0;
         temp_hit_counter_FIFO = 0;
         temp_hit_counter_RANDOM =0;
         temp_D1_miss_counter_LRU = 0;
         temp_D1_miss_counter_BIP = 0;
         temp_D1_miss_counter_FIFO = 0;
         temp_D1_miss_counter_RANDOM =0;
         temp_counter = 0;
      }
   }
   
   if (current_cache_replacement_policy == EXHAUSTIVE_POLICY){
      blocks++;
      if (temp_counter >= CYCLES){
         if(print_output)
            VG_(printf)("*-*-*\n");
         //if(temp_hit_counter_LRU > temp_hit_counter_FIFO && temp_hit_counter_LRU > temp_hit_counter_RANDOM && temp_hit_counter_LRU > temp_hit_counter_BIP){
         if(average_miss_LRU <= average_miss_FIFO && average_miss_LRU <= average_miss_RANDOM && average_miss_LRU <= average_miss_BIP){
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU SELECTED\n");
            blocks_LRU++;
            if (hits_lru[CYCLES/2] == 0)
               hit_counter_ACTIVE = hit_counter_ACTIVE + 1;
            else
               D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + 1;
         }
         //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
         else if(average_miss_FIFO <= average_miss_LRU && average_miss_FIFO <= average_miss_RANDOM && average_miss_FIFO <= average_miss_BIP){
            current_adaptative_cache_replacement_policy = FIFO_POLICY;
            if(print_output)
               VG_(printf)("FIFO SELECTED\n");
            blocks_FIFO++;
            if (hits_fifo[CYCLES/2] == 0)
               hit_counter_ACTIVE = hit_counter_ACTIVE + 1;
            else
               D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + 1;
         }
         //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
         else if(average_miss_RANDOM <= average_miss_LRU && average_miss_RANDOM <= average_miss_FIFO && average_miss_RANDOM <= average_miss_BIP){
            current_adaptative_cache_replacement_policy = RANDOM_POLICY;
            if(print_output)
               VG_(printf)("RANDOM SELECTED\n");
            blocks_RANDOM++;
            if (hits_random[CYCLES/2] == 0)
               hit_counter_ACTIVE = hit_counter_ACTIVE + 1;
            else
               D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + 1;
         }
         //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
         else if(average_miss_BIP <= average_miss_LRU && average_miss_BIP <= average_miss_FIFO && average_miss_BIP <= average_miss_RANDOM){
            current_adaptative_cache_replacement_policy = BIP_POLICY;
            if(print_output)
               VG_(printf)("BIP SELECTED\n");
            blocks_BIP++;
            if (hits_bip[CYCLES/2] == 0)
               hit_counter_ACTIVE = hit_counter_ACTIVE + 1;
            else
               D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + 1;
         }
         else{
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU_d SELECTED\n");
            blocks_LRU++;
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
         }

         if (print_output)
         {
            VG_(printf)
            ("LRU window MISSES D1 %lld\n", average_miss_LRU);
            VG_(printf)
            ("FIFO window MISSES D1 %lld\n", average_miss_FIFO);
            VG_(printf)
            ("RANDOM window MISSES D1 %lld\n", average_miss_RANDOM);
            VG_(printf)
            ("BIP window MISSES D1%lld\n", average_miss_BIP);
            VG_(printf)
            ("LRU window MISSES D1 %lld\n", temp_hit_counter_LRU);
         }
      }
      else if (temp_counter < CYCLES/2){//By default LRU used at the begining and end
            current_adaptative_cache_replacement_policy = LRU_POLICY;
            if(print_output)
               VG_(printf)("LRU SELECTED\n");
            blocks_LRU++;
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
      }

      temp_hit_counter_LRU = 0;
      temp_hit_counter_BIP = 0;
      temp_hit_counter_FIFO = 0;
      temp_hit_counter_RANDOM =0;
      temp_D1_miss_counter_LRU = 0;
      temp_D1_miss_counter_BIP = 0;
      temp_D1_miss_counter_FIFO = 0;
      temp_D1_miss_counter_RANDOM =0;
      //temp_counter = 0;
      
   }

   if (current_cache_replacement_policy == ONLINE_POLICY)
   {
      //VG_(printf)("LRU: %lld    FIFO: %lld    RANDOM: %lld     BIP: %lld\n", online_D1_miss_counter_LRU, online_D1_miss_counter_FIFO, online_D1_miss_counter_RANDOM, online_D1_miss_counter_BIP);
      blocks++;
      if(online_D1_miss_counter_LRU <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = LRU_POLICY;            
            if(print_output)
               VG_(printf)("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
            blocks_LRU++;
         }
         //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
         else if(online_D1_miss_counter_FIFO <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_FIFO <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_FIFO <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = FIFO_POLICY;
            if(print_output)
               VG_(printf)("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
            blocks_FIFO++;
         }
         //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
         else if(online_D1_miss_counter_RANDOM <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_BIP - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = RANDOM_POLICY;
            if(print_output)
               VG_(printf)("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n",temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
            blocks_RANDOM++;
         }
         //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
         else if(online_D1_miss_counter_BIP <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_RANDOM - online_threshold_parameter){
            current_adaptative_cache_replacement_policy = BIP_POLICY;
            if (print_output)
               VG_(printf)
               ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
            hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
            D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
            blocks_BIP++;
         }
         else
         {
            if (current_adaptative_cache_replacement_policy == LRU_POLICY)
            {
               if (online_D1_miss_counter_FIFO <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_FIFO < online_D1_miss_counter_RANDOM && online_D1_miss_counter_FIFO < online_D1_miss_counter_BIP)
               {
                  current_adaptative_cache_replacement_policy = FIFO_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
                  blocks_FIFO++;
               }
               //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
               else if (online_D1_miss_counter_RANDOM <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_RANDOM < online_D1_miss_counter_FIFO && online_D1_miss_counter_RANDOM < online_D1_miss_counter_BIP)
               {
                  current_adaptative_cache_replacement_policy = RANDOM_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
                  blocks_RANDOM++;
               }
               //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
               else if (online_D1_miss_counter_BIP <= online_D1_miss_counter_LRU - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_FIFO && online_D1_miss_counter_BIP <= online_D1_miss_counter_RANDOM)
               {
                  current_adaptative_cache_replacement_policy = BIP_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
                  blocks_BIP++;
               }
               else
               {
                  if (print_output)
                     VG_(printf)
                  ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
                  blocks_LRU++;
               }
            }
            if (current_adaptative_cache_replacement_policy == FIFO_POLICY)
            {
               if (online_D1_miss_counter_LRU < online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_RANDOM && online_D1_miss_counter_LRU < online_D1_miss_counter_BIP)
               {
                  current_adaptative_cache_replacement_policy = LRU_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
                  blocks_LRU++;
               }
               //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
               else if (online_D1_miss_counter_RANDOM < online_D1_miss_counter_LRU && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_RANDOM < online_D1_miss_counter_BIP)
               {
                  current_adaptative_cache_replacement_policy = RANDOM_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
                  blocks_RANDOM++;
               }
               //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
               else if (online_D1_miss_counter_BIP <= online_D1_miss_counter_LRU && online_D1_miss_counter_BIP <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_RANDOM)
               {
                  current_adaptative_cache_replacement_policy = BIP_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
                  blocks_BIP++;
               }
               else
               {
                  if (print_output)
                     VG_(printf)
                  ("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
                  blocks_FIFO++;
               }
            }
            if (current_adaptative_cache_replacement_policy == RANDOM_POLICY)
            {
               if (online_D1_miss_counter_LRU <= online_D1_miss_counter_FIFO && online_D1_miss_counter_LRU <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_LRU <= online_D1_miss_counter_BIP)
               {
                  current_adaptative_cache_replacement_policy = LRU_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
                  blocks_LRU++;
               }
               //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
               else if (online_D1_miss_counter_FIFO < online_D1_miss_counter_LRU && online_D1_miss_counter_FIFO <= online_D1_miss_counter_RANDOM - online_threshold_parameter && online_D1_miss_counter_FIFO < online_D1_miss_counter_BIP)
               {
                  current_adaptative_cache_replacement_policy = FIFO_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
                  blocks_FIFO++;
               }
               //else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
               else if (online_D1_miss_counter_BIP <= online_D1_miss_counter_LRU && online_D1_miss_counter_BIP <= online_D1_miss_counter_FIFO - online_threshold_parameter && online_D1_miss_counter_BIP <= online_D1_miss_counter_RANDOM - online_threshold_parameter)
               {
                  current_adaptative_cache_replacement_policy = BIP_POLICY;
                  if (print_output)
                     VG_(printf)
                  ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
                  blocks_BIP++;
               }
               else
               {
                  if (print_output)
                     VG_(printf)
                  ("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
                  blocks_RANDOM++;
               }
            }
            if (current_adaptative_cache_replacement_policy == BIP_POLICY)
            {
               if (online_D1_miss_counter_LRU < online_D1_miss_counter_FIFO && online_D1_miss_counter_LRU < online_D1_miss_counter_RANDOM && online_D1_miss_counter_LRU <= online_D1_miss_counter_BIP - online_threshold_parameter)
               {
                  current_adaptative_cache_replacement_policy = LRU_POLICY;
                  if (print_output)
                     VG_(printf)
                     ("LRU HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_LRU, temp_counter, temp_D1_miss_counter_LRU);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_LRU;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_LRU;
                  blocks_LRU++;
               }
               //else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
               else if (online_D1_miss_counter_FIFO < online_D1_miss_counter_LRU && online_D1_miss_counter_FIFO < online_D1_miss_counter_RANDOM && online_D1_miss_counter_FIFO <= online_D1_miss_counter_BIP - online_threshold_parameter)
               {
                  current_adaptative_cache_replacement_policy = FIFO_POLICY;
                  if (print_output)
                     VG_(printf)
                     ("FIFO HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_FIFO, temp_counter, temp_D1_miss_counter_FIFO);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_FIFO;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_FIFO;
                  blocks_FIFO++;
               }
               //else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
               else if (online_D1_miss_counter_RANDOM < online_D1_miss_counter_LRU && online_D1_miss_counter_RANDOM < online_D1_miss_counter_FIFO && online_D1_miss_counter_RANDOM <= online_D1_miss_counter_BIP - online_threshold_parameter)
               {
                  current_adaptative_cache_replacement_policy = RANDOM_POLICY;
                  if (print_output)
                     VG_(printf)
                     ("RANDOM HITS: %lld COUNTER: %lld MISSES D1 %lld\n", temp_hit_counter_RANDOM, temp_counter, temp_D1_miss_counter_RANDOM);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_RANDOM;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_RANDOM;
                  blocks_RANDOM++;
               }
               else
               {
                  if (print_output)
                     VG_(printf)
                     ("BIP HITS: %lld COUNTER: %lld MISSES D1%lld\n", temp_hit_counter_BIP, temp_counter, temp_D1_miss_counter_BIP);
                  hit_counter_ACTIVE = hit_counter_ACTIVE + temp_hit_counter_BIP;
                  D1_miss_counter_ACTIVE = D1_miss_counter_ACTIVE + temp_D1_miss_counter_BIP;
                  blocks_BIP++;
               }
            }
         }

         temp_hit_counter_LRU = 0;
         temp_hit_counter_BIP = 0;
         temp_hit_counter_FIFO = 0;
         temp_hit_counter_RANDOM =0;
         temp_D1_miss_counter_LRU = 0;
         temp_D1_miss_counter_BIP = 0;
         temp_D1_miss_counter_FIFO = 0;
         temp_D1_miss_counter_RANDOM =0;
         temp_counter = 0;
   }

   if (current_cache_replacement_policy == ADAPTATIVE_POLICY)
   {
      // VG_(printf)("LRU: %lld    FIFO: %lld    RANDOM: %lld     BIP: %lld\n", online_D1_miss_counter_LRU, online_D1_miss_counter_FIFO, online_D1_miss_counter_RANDOM, online_D1_miss_counter_BIP);
      blocks++;
      if (current_adaptative_cache_replacement_policy == LRU_POLICY)
      {
         blocks_LRU++;
      }
      // else if(temp_hit_counter_FIFO> temp_hit_counter_LRU && temp_hit_counter_FIFO > temp_hit_counter_RANDOM && temp_hit_counter_FIFO > temp_hit_counter_BIP){
      else if (current_adaptative_cache_replacement_policy == FIFO_POLICY)
      {
         blocks_FIFO++;
      }
      // else if(temp_hit_counter_RANDOM> temp_hit_counter_LRU && temp_hit_counter_RANDOM > temp_hit_counter_FIFO&& temp_hit_counter_RANDOM > temp_hit_counter_BIP){
      else if (current_adaptative_cache_replacement_policy == RANDOM_POLICY)
      {
         blocks_RANDOM++;
      }
      // else if(temp_hit_counter_BIP> temp_hit_counter_LRU && temp_hit_counter_BIP > temp_hit_counter_FIFO && temp_hit_counter_BIP > temp_hit_counter_RANDOM){
      else if (current_adaptative_cache_replacement_policy == BIP_POLICY)
      {
         blocks_BIP++;
      }
      else
      {
         blocks_LRU++;
      }

      temp_hit_counter_LRU = 0;
      temp_hit_counter_BIP = 0;
      temp_hit_counter_FIFO = 0;
      temp_hit_counter_RANDOM = 0;
      temp_D1_miss_counter_LRU = 0;
      temp_D1_miss_counter_BIP = 0;
      temp_D1_miss_counter_FIFO = 0;
      temp_D1_miss_counter_RANDOM = 0;
      temp_counter = 0;
   }

   status_lru = True;
   status_fifo = True;
   status_random = True;
   status_bip = True;
   status_active = True;
   status_adaptative = True;
   using_data = True;
   //Saving caches before modifying them
/*    copy_caches(&LL_lru, &LL_lru_temp);
   copy_caches(&D1_lru, &D1_lru_temp);
   copy_caches(&LL_fifo, &LL_fifo_temp);
   copy_caches(&D1_fifo, &D1_fifo_temp);
   copy_caches(&LL_random, &LL_random_temp);
   copy_caches(&D1_random, &D1_random_temp);
   copy_caches(&LL_bip, &LL_bip_temp);
   copy_caches(&D1_bip, &D1_bip_temp); */

   access_counter++;
   temp_counter++;
   if(print_output)
      VG_(printf)("%lx ", a);
   if (cachesim_ref_is_miss(&D1, a, size))
   {
      (*m1)++;
      if (print_output)
         VG_(printf)("|");
      if (cachesim_ref_is_miss(&LL, a, size))
         (*mL)++;
   }
   if (!status_lru)
      hit_counter_LRU++;
   else
      miss_counter_LRU++;
   if (!status_fifo)
      hit_counter_FIFO++;
   else
      miss_counter_FIFO++;
   if (!status_random)
      hit_counter_RANDOM++;
   else
      miss_counter_RANDOM++;
   if (!status_bip)
      hit_counter_BIP++;
   else
      miss_counter_BIP++;
   if (!status_adaptative){
      hit_counter_ADAPTATIVE++;
      if (current_cache_replacement_policy == ADAPTATIVE_POLICY)
         hit_counter_ACTIVE++; //Not processed in the ADAPTATIVE BLOCK as other policies
   }
   else
      miss_counter_ADAPTATIVE++;

   using_data = False;
   if(print_output)
      VG_(printf)("\n");
}

/* Check for special case IrNoX. Called at instrumentation time.
 *
 * Does this Ir only touch one cache line, and are L1I/LL cache
 * line sizes the same? This allows to get rid of a runtime check.
 *
 * Returning false is always fine, as this calls the generic case
 */
static Bool cachesim_is_IrNoX(Addr a, UChar size)
{
   UWord block1, block2;

   if (I1.line_size_bits != LL.line_size_bits)
      return False;
   block1 = a >> I1.line_size_bits;
   block2 = (a + size - 1) >> I1.line_size_bits;
   if (block1 != block2)
      return False;

   return True;
}

 __attribute__((always_inline)) static __inline__ void print_stats(VgFile* fp)
{
   if(current_cache_replacement_policy == ALL_POLICY){
      VG_(fprintf)(fp, "\n...Density approach...\n");
      VG_(fprintf)(fp, "\n...Block size: %llu...\n", density_access_counter);
   }
   if(current_cache_replacement_policy == EXHAUSTIVE_POLICY)
      VG_(fprintf)(fp, "\n...Exhaustive search approach...\n");
   VG_(fprintf)(fp, "BIP Throttle parameter %f\n", bip_throttle_parameter);
   VG_(fprintf)(fp,
   "\n\nSummary hits with LL [Total accesses %llu]:\n"
    " LRU %llu - %f \n"
    " FIFO %llu - %f \n"
    " RANDOM %llu - %f \n"
    " BIP %llu - %f \n"
    " ACTIVE %llu - %f \n \n",
    access_counter,
    hit_counter_LRU, (float)hit_counter_LRU * 100 / (float)access_counter,
    hit_counter_FIFO, (float)hit_counter_FIFO * 100 / (float)access_counter,
    hit_counter_RANDOM, (float)hit_counter_RANDOM * 100 / (float)access_counter,
    hit_counter_BIP, (float)hit_counter_BIP * 100 / (float)access_counter,
    hit_counter_ACTIVE, (float)hit_counter_ACTIVE * 100 / (float)access_counter);
    VG_(fprintf)(fp,
   "\n\nSummary misses considering only D1 [Total accesses %llu]:\n"
    " LRU %llu - %f \n"
    " FIFO %llu - %f \n"
    " RANDOM %llu - %f \n"
    " BIP %llu - %f \n"
    " ACTIVE %llu - %f \n \n"
    " ALL MISSES %llu - %f \n",
    access_counter,
    D1_miss_counter_LRU, (float)D1_miss_counter_LRU * 100 / (float)access_counter,
    D1_miss_counter_FIFO, (float)D1_miss_counter_FIFO * 100 / (float)access_counter,
    D1_miss_counter_RANDOM, (float)D1_miss_counter_RANDOM * 100 / (float)access_counter,
    D1_miss_counter_BIP, (float)D1_miss_counter_BIP * 100 / (float)access_counter,
    D1_miss_counter_ACTIVE, (float)D1_miss_counter_ACTIVE * 100 / (float)access_counter,
    D1_all_misses_counter,(float)D1_all_misses_counter * 100 / (float)access_counter);
   VG_(fprintf)(fp,
   "\n\nSummary hits in only one policy considering only D1 [Total accesses %llu]:\n"
    " LRU %llu - %f \n"
    " FIFO %llu - %f \n"
    " RANDOM %llu - %f \n"
    " BIP %llu - %f \n",
    access_counter,
    D1_only_LRU_counter, (float)D1_only_LRU_counter * 100 / (float)access_counter,
    D1_only_FIFO_counter, (float)D1_only_FIFO_counter * 100 / (float)access_counter,
    D1_only_RANDOM_counter, (float)D1_only_RANDOM_counter * 100 / (float)access_counter,
    D1_only_BIP_counter, (float)D1_only_BIP_counter * 100 / (float)access_counter);
   VG_(fprintf)(fp,
   "\n\nSummary policy used by blocks [Total blocks %llu]:\n"
    " LRU %llu - %f \n"
    " FIFO %llu - %f \n"
    " RANDOM %llu - %f \n"
    " BIP %llu - %f \n",
    blocks,
    blocks_LRU, (float)blocks_LRU * 100 / (float)blocks,
    blocks_FIFO, (float)blocks_FIFO * 100 / (float)blocks,
    blocks_RANDOM, (float)blocks_RANDOM * 100 / (float)blocks,
    blocks_BIP, (float)blocks_BIP * 100 / (float)blocks);
} 
/*--------------------------------------------------------------------*/
/*--- end                                                 cg_sim.c ---*/
/*--------------------------------------------------------------------*/
