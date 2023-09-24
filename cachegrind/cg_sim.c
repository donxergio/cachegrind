#include "limits.h"
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

#define POLICIES 4

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
   ULong *tags_miss;
} cache_t2;

typedef struct 
{
   cache_t2 I1;
   cache_t2 D1;
   cache_t2 LL;
   int* array_misses_LL; //Stores last misses in LL
   int* array_misses_D1; //Stores last misses in D1
   ULong uses ; //Number of times the policy was used
   ULong misses_DL; //Total number of misses in last data level
   ULong misses_IL; //Total number of misses in last instruction level
   ULong misses_D1; //Total number of misses in first data level
   ULong misses_I1; //Total number of misses in first instruction level
   int historic_misses_LL; //History of misses in the last accesses DL
   int historic_misses_D1; //History of misses in the last accesses DL
   int online_misses_LL;
   int online_misses_D1;
   int bit_result_counter_D1;
   int bit_result_counter_LL;
   Bool (*is_miss_func) (cache_t2 *, UInt, UWord);
   char* name;
} policy;

static policy policies[POLICIES]; //TODO adapt the numer of policies with a parameter

static cache_t2 LL;
static cache_t2 I1;
static cache_t2 D1;
static ULong* missesIL;
static ULong* missesI1; 

static ULong access_counter = 0;
static ULong learning_counter = 0;
static ULong data_blocks = 0 ;

/*Add here the pointers to the functions of the different policies*/
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_lru(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_lip(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_random(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_fifo(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_bip(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip3(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip3a(cache_t2 *c, UInt set_no, UWord tag);
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dueling(cache_t2 *c, UInt set_no, UWord tag);

static Bool (*miss_functions[POLICIES])(cache_t2 *, UInt, UWord) = {
    &cachesim_setref_is_miss_lru,
    &cachesim_setref_is_miss_fifo,
    &cachesim_setref_is_miss_bip,
    &cachesim_setref_is_miss_random};

static const char *names[POLICIES] = {"LRU",
                                      "FIFO",
                                      "BIP",
                                      "RANDOM"};

Bool (*cachesim_setref_is_miss)(cache_t2 *, UInt, UWord) = &cachesim_setref_is_miss_lru;

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
   c->tags_miss = VG_(malloc)("cg.sim.ci.1", sizeof(ULong) * c->sets);

   for (i = 0; i < c->sets * c->assoc; i++)
      c->tags[i] = 0;
   for (i = 0; i < c->sets; i++)
      c->tags_miss[i] = 0;
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
   for (i = 0; i < c_dest->sets; i++)
      c_dest->tags_miss[i] = c_ori->tags_miss[i];
   //memcpy(c_dest->tags,c_ori->tags,sizeof(UWord) * c_ori->sets * c_ori->assoc);
   //memcpy(c_dest->tags_miss,c_ori->tags_miss,sizeof(ULong) * c_ori->sets);
}

/* By this point, the size/assoc/line_size has been checked. */
static void cachesim_initcache(cache_t config, cache_t2 *c)
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

   c->tags = VG_(malloc)("cg.sim.ci.1",
                         sizeof(UWord) * c->sets * c->assoc);
   c->tags_miss = VG_(malloc)("cg.sim.ci.1",
                         sizeof(ULong) * c->sets);

   for (i = 0; i < c->sets * c->assoc; i++)
      c->tags[i] = 0;
   for (i = 0; i < c->sets; i++)
      c->tags_miss[i] = 0;
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
         return False;
      }
   }

   /* A miss;  install this tag as MRU, shuffle rest down. */
   for (j = c->assoc - 1; j > 0; j--)
   {
      set[j] = set[j - 1];
   }
   set[0] = tag;
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
         return False;
      }
   }

   /* A miss;  install this tag at random position */
   i = VG_(random)(NULL) % c->assoc;
   set[i] = tag;
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
         return False;
      }
   }

   /* A miss;  install this tag as MRU, shuffle rest down. */
   for (j = c->assoc - 1; j > 0; j--)
   {
      set[j] = set[j - 1];
   }
   set[0] = tag;
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
      is_miss_lru = cachesim_setref_is_miss_lru(&policies[0].D1, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&policies[2].D1, set_no, tag);
   }
   else if (c == &LL)
   {
      cachesim_setref_is_miss_lru(&policies[0].LL, set_no, tag);
      cachesim_setref_is_miss_bip(&policies[2].LL, set_no, tag);
      return cachesim_setref_is_miss_lru(c, set_no, tag);
   }
   else
   {
      cachesim_setref_is_miss_lru(&policies[0].I1, set_no, tag);
      cachesim_setref_is_miss_bip(&policies[2].I1, set_no, tag);
      return cachesim_setref_is_miss_lru(c, set_no, tag);
   }

   if (is_miss_lru)
   {
      if (psel < psel_max)
      {
         psel++;
      }
   }
   if (is_miss_bip)
   {
      if (psel > 0)
      {
         psel--;
      }
   }

   if (psel >= psel_msb)
   { //if there is 1 in the MSB apply BIP
      return cachesim_setref_is_miss_bip(c, set_no, tag);
   }
   else
   {
      return cachesim_setref_is_miss_lru(c, set_no, tag);
   }
}
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip3(cache_t2 *c, UInt set_no, UWord tag)
{
   //static unsigned int psel_size = 4; //number of bits of psel

   //psel_msb = 2 << (psel_size - 1)
   static unsigned int psel_msb = 8; // decimal value when there is 1 in the MSB of psel (1000)

   //psel_max = 2 << (psel_size) - 1
   static unsigned int psel_max = 15; // maximum value of psel (1111)

   static unsigned int psel_1 = 8; // Policy selector, must be iniatialize in the middle of the range [0,psel_max] +lru vs fifo-
   static unsigned int psel_2 = 8; // Policy selector, must be iniatialize in the middle of the range [0,psel_max] +lru vs bip-
   static unsigned int psel_3 = 8; // Policy selector, must be iniatialize in the middle of the range [0,psel_max] +bip vs fifo-

   Bool is_miss_lru = False;
   Bool is_miss_fifo = False;
   Bool is_miss_bip = False;
   if (c == &D1)
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&policies[0].D1, set_no, tag);
      is_miss_fifo = cachesim_setref_is_miss_lru(&policies[1].D1, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&policies[2].D1, set_no, tag);
      if (is_miss_lru)
      {
         if (psel_1 < psel_max)
         {
            psel_1++;
         }
         if (psel_2 < psel_max)
         {
            psel_2++;
         }
      }
      if (is_miss_bip)
      {
         if (psel_2 > 0)
         {
            psel_2--;
         }

         if (psel_3 < psel_max)
         {
            psel_3++;
         }
      }
      if (is_miss_fifo)
      {
         if (psel_1 > 0)
         {
            psel_1--;
         }
         if (psel_3 > 0)
         {
            psel_3--;
         }
      }
      if (psel_1 >= psel_msb)
      {
         if (psel_3 >= psel_msb)
         {
            // Fifo policy is the best
            return cachesim_setref_is_miss_fifo(c, set_no, tag);
         }
      }
      if (psel_1 < psel_msb)
      {
         if (psel_2 < psel_msb)
         {
            // Lru policy is the best
            return cachesim_setref_is_miss_lru(c, set_no, tag);
         }
      }
      if (psel_2 >= psel_msb)
      {
         if (psel_3 < psel_msb)
         {
            // Bip policy is the best
            return cachesim_setref_is_miss_bip(c, set_no, tag);
         }
      }
      //Not clear which one is better -> default LRU
      return cachesim_setref_is_miss_lru(c, set_no, tag); 
   }
   else if (c == &LL)
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&policies[0].LL, set_no, tag);
      is_miss_fifo = cachesim_setref_is_miss_lru(&policies[1].LL, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&policies[2].LL, set_no, tag);
   }
   else
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&policies[0].I1, set_no, tag);
      is_miss_fifo = cachesim_setref_is_miss_lru(&policies[1].I1, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&policies[2].I1, set_no, tag);
   }

   return cachesim_setref_is_miss_lru(c, set_no, tag);

}

/**
 * @brief Save the last result of policy (1= Miss, 0= Hit) in the array of results
 * @param policy, struct that represent the policy
 * @param last_value, 1=Miss, 0=Hit
 * @return int, number of misses in the array
 */

__attribute__((always_inline)) static __inline__ int store_misses_history(policy* policy_, int last_value, Bool LL)
{
   //TODO remove window_counter from implementation and replace it by a parameter
   int temp = 0;
   if (window_counter == 1)
   {
      return last_value;
   }
   if (LL)
   {
      if (last_value ==1){
         policy_->misses_DL++;
         policy_->online_misses_LL++; 
      }
      else{
        policy_->online_misses_LL--; 
      }
      if (last_value != policy_->array_misses_LL[policy_->bit_result_counter_LL])
      {
         // The value to be returned should be updated
         if (last_value == 1)
         {
            policy_->historic_misses_LL++; // New miss
                       
         }
         else
         {
            policy_->historic_misses_LL--; // New hit            
         }
         policy_->array_misses_LL[policy_->bit_result_counter_LL] = last_value;
      }
      policy_->bit_result_counter_LL++;
      if (policy_->bit_result_counter_LL >= window_counter)
      {
         policy_->bit_result_counter_LL = 0;
      }
      if (policy_->historic_misses_LL > window_counter)
         policy_->historic_misses_LL = window_counter;
      if (policy_->historic_misses_LL < 0)
         policy_->historic_misses_LL = 0;
      if (policy_->online_misses_LL > window_counter)
         policy_->online_misses_LL = window_counter;
      if (policy_->online_misses_LL < 0)
         policy_->online_misses_LL = 0;

      return policy_->historic_misses_LL;
   }
   else
   {
      if(last_value){
         policy_->misses_D1++;
         policy_->online_misses_D1++; 
      }
      else{
         policy_->online_misses_D1--;
      }
      if (last_value != policy_->array_misses_D1[policy_->bit_result_counter_D1])
      {
         // The value to be returned should be updated
         if (last_value == 1)
         {
            policy_->historic_misses_D1++; // New miss
                       
         }
         else
         {
            policy_->historic_misses_D1--; // New hit            
         }
         policy_->array_misses_D1[policy_->bit_result_counter_D1] = last_value;
      }
      policy_->bit_result_counter_D1++;
      if (policy_->bit_result_counter_D1 >= window_counter)
      {
         policy_->bit_result_counter_D1 = 0;
      }
      if (policy_->historic_misses_D1 > window_counter)
         policy_->historic_misses_D1 = window_counter;
      if (policy_->historic_misses_D1 < 0)
         policy_->historic_misses_D1 = 0;
      if (policy_->online_misses_D1 > window_counter)
         policy_->online_misses_D1 = window_counter;
      if (policy_->online_misses_D1 < 0)
         policy_->online_misses_D1 = 0;
      return policy_->historic_misses_D1;
   }
}
__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dip3a(cache_t2 *c, UInt set_no, UWord tag)
{
   Bool is_miss_lru = False;
   Bool is_miss_fifo = False;
   Bool is_miss_bip = False;
   if (c == &D1)
   {
      if (cachesim_setref_is_miss_lru(&policies[0].D1, set_no, tag))
      {
         store_misses_history(&policies[0], 1, False);
      }
      else
      {
         store_misses_history(&policies[0], 0, False);
      }
      if (cachesim_setref_is_miss_fifo(&policies[1].D1, set_no, tag))
      {
         store_misses_history(&policies[1], 1, False);
      }
      else
      {
         store_misses_history(&policies[1], 0, False);
      }
      if (cachesim_setref_is_miss_bip(&policies[2].D1, set_no, tag))
      {
         store_misses_history(&policies[2], 1, False);
      }
      else
      {
         store_misses_history(&policies[2], 0, False);
      }      
      if (policies[0].historic_misses_D1 <= policies[1].historic_misses_D1 && policies[0].historic_misses_D1 <= policies[2].historic_misses_D1)
      {
         return cachesim_setref_is_miss_lru(c, set_no, tag);
      }
      if (policies[1].historic_misses_D1 <= policies[0].historic_misses_D1 && policies[1].historic_misses_D1 <= policies[2].historic_misses_D1)
      {
         return cachesim_setref_is_miss_fifo(c, set_no, tag);
      }
      if (policies[2].historic_misses_D1 <= policies[1].historic_misses_D1 && policies[2].historic_misses_D1 <= policies[0].historic_misses_D1)
      {
         return cachesim_setref_is_miss_bip(c, set_no, tag);
      }
      return cachesim_setref_is_miss_lru(c, set_no, tag);
   }
   else if (c == &LL)
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&policies[0].LL, set_no, tag);
      is_miss_fifo = cachesim_setref_is_miss_lru(&policies[1].LL, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&policies[2].LL, set_no, tag);
   }
   else
   {
      is_miss_lru = cachesim_setref_is_miss_lru(&policies[0].I1, set_no, tag);
      is_miss_fifo = cachesim_setref_is_miss_lru(&policies[1].I1, set_no, tag);
      is_miss_bip = cachesim_setref_is_miss_bip(&policies[2].I1, set_no, tag);
   }

   return cachesim_setref_is_miss_lru(c, set_no, tag);

}

__attribute__((always_inline)) static __inline__ Bool cachesim_setref_is_miss_dueling(cache_t2 *c, UInt set_no, UWord tag){
   Bool in_LL = (c == &LL);
   Bool in_D1 = (c == &D1);
   Bool in_I1 = (c == &I1);
   if (in_I1)
   {
      //By default we apply LRU
      return cachesim_setref_is_miss_lru(c, set_no, tag);
   }
   if (in_LL)
   {
      if(cachesim_setref_is_miss_lru(c, set_no, tag))
      {
         store_misses_history(&policies[0], 1, True);
         return True;
      }
      else
      {
         store_misses_history(&policies[0], 0, True); 
         return False;
      }
   }
   
   /*Temp -> TODO create a struct to save the set configuration for each policy*/
   int n = 5;
   if (set_no == 1+(0*n)  || set_no == 1+(1*n)  || set_no == 1+(2*n)  || set_no == 1+(3*n)  || set_no == 1+(4*n)  || set_no == 1+(5*n)  || set_no == 1+(6*n)  || set_no == 1+(7*n)  || set_no == 1+(8*n)  || set_no == 2+(9*n)
    || set_no == 1+(10*n) || set_no == 1+(11*n) || set_no == 1+(12*n) || set_no == 1+(13*n) || set_no == 1+(14*n) || set_no == 1+(15*n) || set_no == 1+(16*n) || set_no == 1+(17*n) || set_no == 1+(18*n) || set_no == 2+(19*n) 
    || set_no == 1+(20*n) || set_no == 1+(21*n) || set_no == 1+(22*n) || set_no == 1+(23*n) || set_no == 1+(24*n) || set_no == 1+(25*n) || set_no == 1+(26*n) || set_no == 1+(27*n) || set_no == 1+(28*n) || set_no == 2+(29*n)
    || set_no == 1+(30*n) || set_no == 1+(31*n) /* || set_no == 1+(32*n) || set_no == 1+(33*n) || set_no == 1+(34*n) || set_no == 1+(35*n) || set_no == 1+(36*n) || set_no == 1+(37*n) || set_no == 1+(38*n) || set_no == 2+(39*n)
    || set_no == 1+(40*n) || set_no == 1+(41*n) || set_no == 1+(42*n) || set_no == 1+(43*n) || set_no == 1+(44*n) || set_no == 1+(45*n) || set_no == 1+(46*n) || set_no == 1+(47*n) || set_no == 1+(48*n) || set_no == 2+(49*n)
    || set_no == 1+(50*n) || set_no == 1+(51*n) || set_no == 1+(52*n) || set_no == 1+(53*n) || set_no == 1+(54*n) || set_no == 1+(55*n) || set_no == 1+(56*n) || set_no == 1+(57*n) || set_no == 1+(58*n) || set_no == 2+(59*n)
    || set_no == 1+(60*n) || set_no == 1+(61*n) || set_no == 1+(62*n) || set_no == 1+(63*n) */ )
   {
      policies[0].uses++;
      data_blocks++;
      if (cachesim_setref_is_miss_lru(c, set_no, tag))
      {
         store_misses_history(&policies[0], 1, False);
         return True;
      }
      else
      {
         store_misses_history(&policies[0], 0, False);
      }
      return False;
   }
   if (set_no == 2+(0*n)  || set_no == 2+(1*n)  || set_no == 2+(2*n)  || set_no == 2+(3*n)  || set_no == 2+(4*n)  || set_no == 2+(5*n)  || set_no == 2+(6*n)  || set_no == 2+(7*n)  || set_no == 2+(8*n)  || set_no == 2+(9*n)
      || set_no == 2+(10*n) || set_no == 2+(11*n) || set_no == 2+(12*n) || set_no == 2+(13*n) || set_no == 2+(14*n) || set_no == 2+(15*n) || set_no == 2+(16*n) || set_no == 2+(17*n) || set_no == 2+(18*n) || set_no == 2+(19*n) 
      || set_no == 2+(20*n) || set_no == 2+(21*n) || set_no == 2+(22*n) || set_no == 2+(23*n) || set_no == 2+(24*n) || set_no == 2+(25*n) || set_no == 2+(26*n) || set_no == 2+(27*n) || set_no == 2+(28*n) || set_no == 2+(29*n)
      || set_no == 2+(30*n) || set_no == 2+(31*n) /* || set_no == 2+(32*n) || set_no == 2+(33*n) || set_no == 2+(34*n) || set_no == 2+(35*n) || set_no == 2+(36*n) || set_no == 2+(37*n) || set_no == 2+(38*n) || set_no == 2+(39*n)
      || set_no == 2+(40*n) || set_no == 2+(41*n) || set_no == 2+(42*n) || set_no == 2+(43*n) || set_no == 2+(44*n) || set_no == 2+(45*n) || set_no == 2+(46*n) || set_no == 2+(47*n) || set_no == 2+(48*n) || set_no == 2+(49*n)
      || set_no == 2+(50*n) || set_no == 2+(51*n) || set_no == 2+(52*n) || set_no == 2+(53*n) || set_no == 2+(54*n) || set_no == 2+(55*n) || set_no == 2+(56*n) || set_no == 2+(57*n) || set_no == 2+(58*n) || set_no == 2+(59*n)
      || set_no == 2+(60*n) || set_no == 2+(61*n) || set_no == 2+(62*n) || set_no == 2+(63*n) */ )
      {
         policies[1].uses++;
         data_blocks++;
      if (cachesim_setref_is_miss_fifo(c, set_no, tag))
      {
         store_misses_history(&policies[1], 1, False);
         return True;
      }
      else
      {
         store_misses_history(&policies[1], 0, False);
      }
      return False;
      }
   if (set_no == 3+(0*n)  || set_no == 3+(1*n)  || set_no == 3+(2*n)  || set_no == 3+(3*n)  || set_no == 3+(4*n)  || set_no == 3+(5*n)  || set_no == 3+(6*n)  || set_no == 3+(7*n)  || set_no == 3+(8*n)  || set_no == 2+(9*n)
    || set_no == 3+(10*n) || set_no == 3+(11*n) || set_no == 3+(12*n) || set_no == 3+(13*n) || set_no == 3+(14*n) || set_no == 3+(15*n) || set_no == 3+(16*n) || set_no == 3+(17*n) || set_no == 3+(18*n) || set_no == 2+(19*n) 
    || set_no == 3+(20*n) || set_no == 3+(21*n) || set_no == 3+(22*n) || set_no == 3+(23*n) || set_no == 3+(24*n) || set_no == 3+(25*n) || set_no == 3+(26*n) || set_no == 3+(27*n) || set_no == 3+(28*n) || set_no == 2+(29*n)
    || set_no == 3+(30*n) || set_no == 3+(31*n) /* || set_no == 3+(32*n) || set_no == 3+(33*n) || set_no == 3+(34*n) || set_no == 3+(35*n) || set_no == 3+(36*n) || set_no == 3+(37*n) || set_no == 3+(38*n) || set_no == 2+(39*n)
    || set_no == 3+(40*n) || set_no == 3+(41*n) || set_no == 3+(42*n) || set_no == 3+(43*n) || set_no == 3+(44*n) || set_no == 3+(45*n) || set_no == 3+(46*n) || set_no == 3+(47*n) || set_no == 3+(48*n) || set_no == 2+(49*n)
    || set_no == 3+(50*n) || set_no == 3+(51*n) || set_no == 3+(52*n) || set_no == 3+(53*n) || set_no == 3+(54*n) || set_no == 3+(55*n) || set_no == 3+(56*n) || set_no == 3+(57*n) || set_no == 3+(58*n) || set_no == 2+(59*n)
    || set_no == 3+(60*n) || set_no == 3+(61*n) || set_no == 3+(62*n) || set_no == 3+(63*n) */ )
      {
         policies[2].uses++;
         data_blocks++;
      if (cachesim_setref_is_miss_bip(c, set_no, tag))
      {
         store_misses_history(&policies[2], 1, False);
         return True;
      }
      else
      {
         store_misses_history(&policies[2], 0, False);

      }
      return False;
      }
   /* if (set_no == 4+(0*n)  || set_no == 4+(1*n)  || set_no == 4+(2*n)  || set_no == 4+(3*n)  || set_no == 4+(4*n)  || set_no == 4+(5*n)  || set_no == 4+(6*n)  || set_no == 4+(7*n)  || set_no == 4+(8*n)  || set_no == 2+(9*n)
    || set_no == 4+(10*n) || set_no == 4+(11*n) || set_no == 4+(12*n) || set_no == 4+(13*n) || set_no == 4+(14*n) || set_no == 4+(15*n) || set_no == 4+(16*n) || set_no == 4+(17*n) || set_no == 4+(18*n) || set_no == 2+(19*n) 
    || set_no == 4+(20*n) || set_no == 4+(21*n) || set_no == 4+(22*n) || set_no == 4+(23*n) || set_no == 4+(24*n) || set_no == 4+(25*n) || set_no == 4+(26*n) || set_no == 4+(27*n) || set_no == 4+(28*n) || set_no == 2+(29*n)
    || set_no == 4+(30*n) || set_no == 4+(31*n) || set_no == 4+(32*n) || set_no == 4+(33*n) || set_no == 4+(34*n) || set_no == 4+(35*n) || set_no == 4+(36*n) || set_no == 4+(37*n) || set_no == 4+(38*n) || set_no == 2+(39*n)
    || set_no == 4+(40*n) || set_no == 4+(41*n) || set_no == 4+(42*n) || set_no == 4+(43*n) || set_no == 4+(44*n) || set_no == 4+(45*n) || set_no == 4+(46*n) || set_no == 4+(47*n) || set_no == 4+(48*n) || set_no == 2+(49*n)
    || set_no == 4+(50*n) || set_no == 4+(51*n) || set_no == 4+(52*n) || set_no == 4+(53*n) || set_no == 4+(54*n) || set_no == 4+(55*n) || set_no == 4+(56*n) || set_no == 4+(57*n) || set_no == 4+(58*n) || set_no == 2+(59*n)
    || set_no == 4+(60*n) || set_no == 4+(61*n) || set_no == 4+(62*n) || set_no == 4+(63*n) )
      {
         policies[3].uses++;
         data_blocks++;
      if (cachesim_setref_is_miss_random(c, set_no, tag))
      {
         store_misses_history(&policies[3], 1, False);
         return True;
      }
      else
      {
         store_misses_history(&policies[3], 0, False);
      }
      return False;
      } */
      //Follower sets policy selection
      if (policies[0].historic_misses_D1 <= policies[1].historic_misses_D1 && policies[0].historic_misses_D1 <= policies[2].historic_misses_D1 /* && policies[0].historic_misses_D1 >= policies[3].historic_misses_D1 */)
      {
         return cachesim_setref_is_miss_lru(c, set_no, tag);
      }
      if (policies[1].historic_misses_D1 <= policies[0].historic_misses_D1 && policies[1].historic_misses_D1 <= policies[2].historic_misses_D1 /* && policies[1].historic_misses_D1 >= policies[3].historic_misses_D1 */)
      {
         return cachesim_setref_is_miss_fifo(c, set_no, tag);
      }
      if (policies[2].historic_misses_D1 <= policies[1].historic_misses_D1 && policies[2].historic_misses_D1 <= policies[0].historic_misses_D1 /* && policies[2].historic_misses_D1 >= policies[3].historic_misses_D1 */)
      {
         return cachesim_setref_is_miss_bip(c, set_no, tag);
      }
/*       if (policies[3].historic_misses_D1 >= policies[1].historic_misses_D1 && policies[3].historic_misses_D1 >= policies[2].historic_misses_D1 && policies[3].historic_misses_D1 >= policies[0].historic_misses_D1)
      {
         return cachesim_setref_is_miss_random(c, set_no, tag);
      } */
      return cachesim_setref_is_miss_lru(c, set_no, tag);
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
   /*Here we prepare all the policies data structs*/
    if ((cache_replacement_policy == DIP3_POLICY || cache_replacement_policy == DIP3_AGEING_POLICY)&& overhead_allowed) {
      D1c.size = D1c.size - (3*(32 * D1c.assoc * D1c.line_size));
   } 
   cachesim_initcache(I1c, &I1);
   cachesim_initcache(D1c, &D1);
   cachesim_initcache(LLc, &LL);

   if (cache_replacement_policy == DIP3_POLICY || cache_replacement_policy == DIP3_AGEING_POLICY) {
      D1c.size = (32 * D1c.assoc * D1c.line_size);
   }

   for (ULong i = 0; i < POLICIES; i++)
   {
      cachesim_initcache(I1c, &policies[i].I1);
      cachesim_initcache(D1c, &policies[i].D1);
      cachesim_initcache(LLc, &policies[i].LL);
      policies[i].is_miss_func = miss_functions[i];
      policies[i].name = names[i];
      policies[i].array_misses_D1 = (int*) VG_(calloc)("array_misses_D1", window_counter, sizeof(int));
      policies[i].array_misses_LL = (int*) VG_(calloc)("array_misses_LL", window_counter, sizeof(int));
      policies[i].historic_misses_D1 = 0;
      policies[i].historic_misses_LL = 0;
      policies[i].uses = 0;
      if (cache_replacement_policy == ONLINE) //The online approach use this value for switching and starts in the 50% of the max value of misses of counter
         policies[i].misses_DL =(ULong) window_counter/2;
      else
         policies[i].misses_DL =0;
      policies[i].misses_IL =0;
      policies[i].misses_D1 =0;
      policies[i].misses_I1 =0;
      policies[i].bit_result_counter_D1 =0;
      policies[i].bit_result_counter_LL =0;
   }

   /*  Here we choose the policy that will be used for the result */
   if (cache_replacement_policy == LRU_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_lru;
   }
   else if (cache_replacement_policy == LIP_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_lip;
   }
   else if (cache_replacement_policy == RANDOM_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_random;
   }
   else if (cache_replacement_policy == FIFO_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_fifo;
   }
   else if (cache_replacement_policy == BIP_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_bip;
   }
   else if (cache_replacement_policy == DIP_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_dip;
   }
   else if (cache_replacement_policy == DIP3_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_dip3;
   }
   else if (cache_replacement_policy == DIP3_AGEING_POLICY)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_dip3a;
   }
   else if (cache_replacement_policy == CACHE_DUELING)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_dueling;
   }
   else
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_lru; // By default LRU is used for the result
   }
}

__attribute__((always_inline)) static __inline__ void cachesim_I1_doref_Gen(Addr a, UChar size, ULong *m1, ULong *mL)
{
   /*PROCESS THE DIFFERENT APPROACHES*/
   missesI1 = m1;
   missesIL = mL;
   if (current_cache_replacement_policy == FIXED_WINDOW ||
       current_cache_replacement_policy == SLIDING_WINDOW ||
       current_cache_replacement_policy == ONLINE ||
       current_cache_replacement_policy == LEARNING)

   {
      for (ULong i = 0; i < POLICIES; i++)
      {
         cachesim_setref_is_miss = policies[i].is_miss_func;
         if (cachesim_ref_is_miss(&policies[i].I1, a, size))
         {
            (policies[i].misses_I1)++;
            if (cachesim_ref_is_miss(&policies[i].LL, a, size))
            {
               (policies[i].misses_IL)++;
            }
         }
      }
   }
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
   missesI1 = m1;
   missesIL = mL;
   UWord block = a >> I1.line_size_bits;
   UInt I1_set = block & I1.sets_min_1;

   // use block as tag
   /*PROCESS THE DIFFERENT APPROACHES*/
   if (current_cache_replacement_policy == FIXED_WINDOW ||
       current_cache_replacement_policy == SLIDING_WINDOW ||
       current_cache_replacement_policy == ONLINE ||
       current_cache_replacement_policy == LEARNING)

   {
      for (ULong i = 0; i < POLICIES; i++)
      {
         if (policies[i].is_miss_func(&policies[i].I1, I1_set, block))
         {
            UInt LL_set = block & LL.sets_min_1;
            (policies[i].misses_I1)++;
            if (policies[i].is_miss_func(&policies[i].LL, LL_set, block))
            {
               (policies[i].misses_IL)++;
            }
         }
      }
   }
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
   static ULong blocks = 0 ;
   static unsigned long lastAddress=0;
   static file_counter = 0;
   access_counter++;
   blocks++;
   ULong min_misses = ULLONG_MAX;
   ULong min_misses_second = ULLONG_MAX;
   static int index_selected_policy = 0;
   static int prev_index_selected_policy = 0;
   static int index_min_misses_policy = 0;
   static VgFile *fp = NULL;
   
   // If LEARNING approach is activated all policies will run and the result will be printed in a file
   if (current_cache_replacement_policy == LEARNING)
   {
      cachesim_setref_is_miss = &cachesim_setref_is_miss_lru;
      if (cachesim_ref_is_miss(&D1, a, size))
      {
         (*m1)++;
         if (cachesim_ref_is_miss(&LL, a, size))
            (*mL)++;
      }
      UShort result[POLICIES];
      if (fp == NULL || access_counter > 10000000)
      {
         access_counter = 0;         
         HChar learn_filename [15];
         VG_(sprintf)(learn_filename, "learning_%d.dat", file_counter);
         file_counter ++;
         fp = VG_(fopen)(learn_filename, VKI_O_CREAT | VKI_O_TRUNC | VKI_O_WRONLY,
                         VKI_S_IRUSR | VKI_S_IWUSR);
         if (fp == NULL)
         {            
            // If the file can't be opened for whatever reason (conflict
            // between multiple cachegrinded processes?), give up now.
            VG_(umsg)
            ("error: can't open cache learning output file '%s'\n",
             "learning.dat");
            VG_(umsg)
            ("       ... so learning results will be missing.\n");
            return;
         }
      }
      //if(start_learning > access_counter || end_learning < learning_counter)
      //   return;
      for (ULong i = 0; i < POLICIES; i++)
      {
         cachesim_setref_is_miss = policies[i].is_miss_func;
         if (cachesim_ref_is_miss(&policies[i].D1, a, size))
         {
            if (cachesim_ref_is_miss(&policies[i].LL, a, size))
            {
               result[i] = 0; // Miss in LL
            }
            else
            {
               result[i] = 0; // Hit in LL
            }
         }
         else
         {
            result[i] = 1; // Hit in D1
         }
      }
      learning_counter++;
      long long difAddress = a - lastAddress;
      lastAddress = (a);
      int resultBits = 1*result[0] + 2*result[1] + 4*result[2] + 8*result[3];
      UWord block1 = a >> policies[0].D1.line_size_bits;
      UInt set1 = block1 & policies[0].D1.sets_min_1;
      VG_(fprintf)      
      (fp, "%ld %d %d %lld\n", difAddress, set1 , resultBits, a);
      return;
   }
   // Avoid processing in case of ADAPTIVE or single policy
   if (current_cache_replacement_policy == FIXED_WINDOW ||
          current_cache_replacement_policy == SLIDING_WINDOW ||
          current_cache_replacement_policy == ONLINE)

      {
         // First copy status of best cache to result one, otherwise no miss will be counted on it
         if (current_cache_replacement_policy == SLIDING_WINDOW)
         {
            // Select the policy that is performing better at the moment and copy its cache to the result
            cachesim_setref_is_miss = policies[index_selected_policy].is_miss_func;
            policies[index_selected_policy].uses++;
            if (prev_index_selected_policy != index_selected_policy)
            {
               //NOT NEEDED TO COPY, this simulates a real use of the approach
               //copy_caches(&policies[index_selected_policy].I1, &I1);
               //copy_caches(&policies[index_selected_policy].D1, &D1);
               //copy_caches(&policies[index_selected_policy].LL, &LL);
               prev_index_selected_policy = index_selected_policy;
            }
         }
         for (ULong i = 0; i < POLICIES; i++)
         {
            cachesim_setref_is_miss = policies[i].is_miss_func;
            if (cachesim_ref_is_miss(&policies[i].D1, a, size))
            {
               store_misses_history(&policies[i], 1, False);
               if (cachesim_ref_is_miss(&policies[i].LL, a, size))
               {
                  store_misses_history(&policies[i], 1, True);
               }
               else
               {
                  store_misses_history(&policies[i], 0, True);
               }
            }
            else
            {
               store_misses_history(&policies[i], 0, False);
            }
            if ((policies[i].historic_misses_D1 < min_misses) && current_cache_replacement_policy != ONLINE)
            {
               // Sliding window uses the historic values of the misses
               min_misses = policies[i].historic_misses_D1;
               index_min_misses_policy = i;
            }
            if ((policies[i].online_misses_D1 < min_misses) && current_cache_replacement_policy == ONLINE)
            {
               // Online uses just a counter of misses
               min_misses = policies[i].online_misses_D1;
               index_min_misses_policy = i;
            }
         }
         /*PROCESS THE DIFFERENT APPROACHES*/
         if (current_cache_replacement_policy == FIXED_WINDOW)
         {
            // Check if the window was already processed
            if (blocks >= window_counter)
            {
               /*Copy the best policy cache to all other caches*/
               for (ULong i = 0; i < POLICIES; i++)
               {
                  if (i != index_min_misses_policy)
                  {
                     copy_caches(&policies[index_min_misses_policy].I1, &policies[i].I1);
                     copy_caches(&policies[index_min_misses_policy].D1, &policies[i].D1);
                     copy_caches(&policies[index_min_misses_policy].LL, &policies[i].LL);
                  }
                  else
                  {
                     // Control how many times the policy was used
                     policies[i].uses++;
                     // Updates the counter of the result
                     (*m1) += policies[i].misses_D1;
                     (*mL) += policies[i].misses_DL;
                     (*missesI1) += policies[i].misses_I1;
                     (*missesIL) += policies[i].misses_IL;
                  }
                  // Clean last window to start a new one
                  VG_(free)
                  (policies[i].array_misses_D1);
                  VG_(free)
                  (policies[i].array_misses_LL);
                  policies[i].array_misses_D1 = (int *)VG_(calloc)("array_misses_D1", window_counter, sizeof(int));
                  policies[i].array_misses_LL = (int *)VG_(calloc)("array_misses_LL", window_counter, sizeof(int));
                  policies[i].historic_misses_D1 = 0;
                  policies[i].historic_misses_LL = 0;
                  policies[i].misses_DL = 0;
                  policies[i].misses_IL = 0;
                  policies[i].misses_D1 = 0;
                  policies[i].misses_I1 = 0;
               }
               // Restart processing
               blocks = 0;
               data_blocks++;
            }
            return;
         }
         if (current_cache_replacement_policy == ONLINE)
         {
            // Apply the threshold before switching
            if (policies[index_min_misses_policy].online_misses_D1 < policies[index_selected_policy].online_misses_D1 - switching_threshold_parameter)
            {
               index_selected_policy = index_min_misses_policy;
            }
            // Select the policy that is performing better at the moment and copy its cache to the result
            cachesim_setref_is_miss = policies[index_selected_policy].is_miss_func;
            policies[index_selected_policy].uses++;
         }
         if (current_cache_replacement_policy == SLIDING_WINDOW)
         {
            // Apply the threshold before switching
            if (policies[index_min_misses_policy].historic_misses_D1 < policies[index_selected_policy].historic_misses_D1 - switching_threshold_parameter)
            {
               index_selected_policy = index_min_misses_policy;
            }
            // Select the policy that is performing better at the moment and copy its cache to the result
            cachesim_setref_is_miss = policies[index_selected_policy].is_miss_func;
            policies[index_selected_policy].uses++;
         }
      }
      if (current_cache_replacement_policy == ADAPTATIVE || current_cache_replacement_policy == NAIVE)
      {
         // Use the policy selected in CODE
         if (current_cache_replacement_policy == NAIVE)
         {
            // TODO: adapt the number of policies to a parameter
            current_adaptative_cache_replacement_policy = VG_(random)(NULL) % 4;
         }

         switch (current_adaptative_cache_replacement_policy)
         {
         case LRU_POLICY:
            cachesim_setref_is_miss = &cachesim_setref_is_miss_lru;
            policies[0].uses++;
            break;
         case LIP_POLICY:
            cachesim_setref_is_miss = &cachesim_setref_is_miss_lip;
            break;
         case RANDOM_POLICY:
            policies[3].uses++;
            cachesim_setref_is_miss = &cachesim_setref_is_miss_random;
            break;
         case FIFO_POLICY:
            policies[1].uses++;
            cachesim_setref_is_miss = &cachesim_setref_is_miss_fifo;
            break;
         case BIP_POLICY:
            policies[2].uses++;
            cachesim_setref_is_miss = &cachesim_setref_is_miss_bip;
            break;
         case DIP_POLICY:
            cachesim_setref_is_miss = &cachesim_setref_is_miss_dip;
            break;
         default:
            cachesim_setref_is_miss = &cachesim_setref_is_miss_lru;
            policies[0].uses++;
            break;
         }
      }
      data_blocks++;
      // Execution for traditional policies and result cache in case of selection of best policy
      if (cachesim_ref_is_miss(&D1, a, size))
      {
         (*m1)++;
         if (cachesim_ref_is_miss(&LL, a, size))
            (*mL)++;
      }
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
      if (  current_cache_replacement_policy != FIXED_WINDOW   &&
            current_cache_replacement_policy != SLIDING_WINDOW &&
            current_cache_replacement_policy != ONLINE         &&
            current_cache_replacement_policy != CACHE_DUELING  &&
            current_cache_replacement_policy != ADAPTATIVE     &&
            current_cache_replacement_policy != NAIVE  )
   {
      VG_(fprintf)
      (fp, "BIP Throttle parameter %f\n", bip_throttle_parameter);
      return;
   }
   if (current_cache_replacement_policy == FIXED_WINDOW)
   {
      VG_(fprintf)
      (fp, "\n...Fixed-window approach...\n");
      VG_(fprintf)
      (fp, "\n...Block size: %llu...\n", window_counter);
   }
   if (current_cache_replacement_policy == SLIDING_WINDOW)
      VG_(fprintf)
      (fp, "\n...Sliding-window approach...\n");
   if (current_cache_replacement_policy == ONLINE)
      VG_(fprintf)
      (fp, "\n...Online approach...\n");
   if (current_cache_replacement_policy == ADAPTATIVE)
      VG_(fprintf)
      (fp, "\n...HIL approach...\n");
   if (current_cache_replacement_policy == NAIVE)
      VG_(fprintf)
      (fp, "\n...Naive approach...\n");
   if (current_cache_replacement_policy == CACHE_DUELING)
      VG_(fprintf)
      (fp, "\n...Cache dueling approach...\n");
   VG_(fprintf)
   (fp, "BIP Throttle parameter %f\n", bip_throttle_parameter);
   VG_(fprintf)
      (fp,
       "\n\nSummary policy used times [Total uses %llu]:\n"
       " %s %llu - %f \n"
       " %s %llu - %f \n"
       " %s %llu - %f \n"
       " %s %llu - %f \n \n",
       data_blocks,
       policies[0].name, policies[0].uses, (float)policies[0].uses * 100 / (float)data_blocks,
       policies[1].name, policies[1].uses, (float)policies[1].uses * 100 / (float)data_blocks,
       policies[2].name, policies[2].uses, (float)policies[2].uses * 100 / (float)data_blocks,
       policies[3].name, policies[3].uses, (float)policies[3].uses * 100 / (float)data_blocks); 
   if (current_cache_replacement_policy == SLIDING_WINDOW ||
       current_cache_replacement_policy == ONLINE         ||
       current_cache_replacement_policy == CACHE_DUELING)
   {
      VG_(fprintf)
      (fp,
       "\n\nSummary misses with DL [Total accesses %llu]:\n"
       " %s %llu - %f \n"
       " %s %llu - %f \n"
       " %s %llu - %f \n"
       " %s %llu - %f \n \n",
       access_counter,
       policies[0].name, policies[0].misses_DL, (float)policies[0].misses_DL * 100 / (float)access_counter,
       policies[1].name, policies[1].misses_DL, (float)policies[1].misses_DL * 100 / (float)access_counter,
       policies[2].name, policies[2].misses_DL, (float)policies[2].misses_DL * 100 / (float)access_counter,
       policies[3].name, policies[3].misses_DL, (float)policies[3].misses_DL * 100 / (float)access_counter);
      VG_(fprintf)
      (fp,
       "\n\nSummary misses considering only D1 [Total accesses %llu]:\n"
       " %s %llu - %f \n"
       " %s %llu - %f \n"
       " %s %llu - %f \n"
       " %s %llu - %f \n \n",
       access_counter,
       policies[0].name, policies[0].misses_D1, (float)policies[0].misses_D1 * 100 / (float)access_counter,
       policies[1].name, policies[1].misses_D1, (float)policies[1].misses_D1 * 100 / (float)access_counter,
       policies[2].name, policies[2].misses_D1, (float)policies[2].misses_D1 * 100 / (float)access_counter,
       policies[3].name, policies[3].misses_D1, (float)policies[3].misses_D1 * 100 / (float)access_counter);
  

      }

} 
/*--------------------------------------------------------------------*/
/*--- end                                                 cg_sim.c ---*/
/*--------------------------------------------------------------------*/
