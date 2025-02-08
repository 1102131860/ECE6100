#include "cachesim.hpp"
#include <vector>
#include <deque>        // a double-linked list (FIFO)

using namespace std;

typedef struct block
{
    uint64_t tag;       // the front [s, B] segmentation of address
    bool isDirty;       // signed this block data has been written or not
} block_t;

typedef struct victim_cache
{
    uint64_t num_block;     // the number of blocks it can hold, must be 0, 1 or 2, zero disables the victim cache
    deque<block_t> entries;   // the entries hold the evicted blocks from cache l1
} victim_cache_t;

typedef struct cache
{
    bool disabled;
    bool enable_ER;
    replacement_policy_t repo;
    write_strat_t wrst;
    uint64_t total_size;    // a total size of 1 << C (2^C) bytes, l2's is strictly larger than l1's total_size
    uint64_t block_size;    // block size of 1 << B (2^B) bytes, l2's equal to the l1's block_size
    uint64_t associativity; // each set contains 1 << s (2^s) blocks, l2's should be larger than or equal to l1's
    uint64_t num_block;     // the number of blocks 1 << C - B (2^(C-B)) it can hold
    uint64_t num_set;       // the number of sets 1 << C - B - S (2^(C - B - S)) it can hold
    vector<deque<block_t>> sets; // the set hold the cache l2 blocks
} cache_t;

// Local variables (global scale in cachesim.cpp)
static cache_t l1, l2;
static victim_cache_t vc;

uint64_t extract_tag(uint64_t address, uint64_t block_size, uint64_t num_set)
{
    return address / (block_size * num_set);    // take out the tag (remove the byte offset and index)
}

uint64_t extract_index(uint64_t address, uint64_t block_size, uint64_t num_set)
{
    return (address / block_size) % num_set;   // take out the index from tag 
}

void cache_setup(cache_t *ca, cache_config_t config)
{
    ca->disabled = config.disabled;
    ca->enable_ER = config.enable_ER;
    ca->repo = config.replace_policy;
    ca->wrst = config.write_strat;
    ca->total_size = 1ULL << config.c;
    ca->block_size = 1ULL << config.b;
    ca->associativity = 1ULL << config.s;
    ca->num_block = 1ULL << (config.c - config.b);
    ca->num_set = 1ULL << (config.c - config.b - config.s);
    ca->sets.resize(ca->num_set);
    // ensure there are at most num_set sets in cache
    // ensure there are at most associativity tags in each set
}

void victim_cache_setup(victim_cache_t *vc, uint64_t entries)
{
    vc->num_block = entries;
    // ensure the block should not exceed the num_block (fully-associative)
}

/*-------------DO NOT CHANGE THIS BLOCK OF CODE-------------*/
//Pseudo-Random Number Generator for RANDOM Replacement policy
static unsigned long int evict_random_next = 1;
int evict_random(void)
{
    evict_random_next = evict_random_next * 1103515243 + 12345;
    return (unsigned int)(evict_random_next / 65536) % 32768;
}

void evict_srand(unsigned int seed)
{
    evict_random_next = seed;
}
/*----------------------------------------------------------*/

/**
 * Subroutine for initializing the cache simulator. You many add and initialize any global or heap
 * variables as needed.
 * TODO: You're responsible for completing this routine
 */

void sim_setup(sim_config_t *config) {
    cache_setup(&l1, config->l1_config);
    victim_cache_setup(&vc, config->victim_cache_entries);
    cache_setup(&l2, config->l2_config);
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 * TODO: You're responsible for completing this routine
 */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {

}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * TODO: You're responsible for completing this routine
 */
void sim_finish(sim_stats_t *stats) {

}