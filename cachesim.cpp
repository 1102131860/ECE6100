#include "cachesim.hpp"
#include <vector>
#include <deque>        // a double-linked list (FIFO)
#include <cmath>        // for log2 operation
#include <cstdio>       // for printf debug

using namespace std;

typedef struct block {
    uint64_t tag;       // the front [s, B] segmentation of address
    uint64_t address;   // the block address
    bool isDirty;       // signed this block data has been written or not
} block_t;

typedef struct victim_cache {
    uint64_t num_block;     // the number of blocks it can hold, must be 0, 1 or 2, zero disables the victim cache
    deque<block_t> entries;   // the entries hold the evicted blocks from cache l1
} victim_cache_t;

typedef struct cache {
    bool disabled;
    bool enable_ER;
    replacement_policy_t repo;
    write_strat_t wrst;
    uint64_t total_size;    // a total size of 1 << C (2^C) bytes, l2's is strictly larger than l1's total_size
    uint64_t block_size;    // block size of 1 << B (2^B) bytes, l2's equal to the l1's block_size
    uint64_t set_size;      // each set contains 1 << s (2^s) blocks, l2's should be larger than or equal to l1's
    uint64_t num_block;     // the number of blocks 1 << C - B (2^(C-B)) it can hold
    uint64_t num_set;       // the number of sets 1 << C - B - S (2^(C - B - S)) it can hold
    vector<deque<block_t>> sets; // the set hold the cache l2 blocks
} cache_t;

// Local variables (global scale in cachesim.cpp)
static cache_t l1, l2;
static victim_cache_t vc;

uint64_t get_tag(uint64_t address, uint64_t block_size, uint64_t num_set) {
    return address / (block_size * num_set);    // take out the tag (remove the byte offset and index)
}

uint64_t get_index(uint64_t address, uint64_t block_size, uint64_t num_set) {
    return (address / block_size) % num_set;   // take out the index from tag 
}

void cache_setup(cache_t *ca, cache_config_t config) {
    ca->disabled = config.disabled;
    ca->enable_ER = config.enable_ER;
    ca->repo = config.replace_policy;
    ca->wrst = config.write_strat;
    ca->total_size = 1ULL << config.c;                      // 2^C
    ca->block_size = 1ULL << config.b;                      // 2^B
    ca->set_size = 1ULL << config.s;                        // 2^S
    ca->num_block = 1ULL << (config.c - config.b);          // 2^(C - B)
    ca->num_set = 1ULL << (config.c - config.b - config.s); // 2^(C - B - S)
    ca->sets.resize(ca->num_set);
}

void victim_cache_setup(victim_cache_t *vc, uint64_t entries) {
    vc->num_block = entries;
}

bool cache_insert(cache_t *ca, char rw, uint64_t address, block_t *evicted_block) {
    uint64_t index = get_index(address, ca->block_size, ca->num_set);
    uint64_t tag = get_tag(address, ca->block_size, ca->num_set);
    bool isDirty = (rw == WRITE && ca->wrst == WRITE_STRAT_WBWA);     // WBWA needs to mark the written data as dirty
    block_t inserted_block = {tag, address, isDirty};
    bool isEvicted = false;

    if (ca->sets[index].size() >= ca->set_size) {       // set is full
        if (ca->repo != REPLACEMENT_POLICY_RANDOM) {    // LRU-MIP, LRU-LIP, FIFO
            *evicted_block = ca->sets[index].front();
            ca->sets[index].pop_front();
        } else {                                        // RANDOM
            uint64_t evicted_index = (uint64_t)(evict_random() % ca->sets[index].size());
            auto evicted_block_it = ca->sets[index].begin() + evicted_index;
            *evicted_block = *evicted_block_it;
            ca->sets[index].erase(evicted_block_it);
        }
        isEvicted = true;
    }

    if (ca->repo == REPLACEMENT_POLICY_LIP)
        ca->sets[index].push_front(inserted_block);     // LRU-LIP
    else
        ca->sets[index].push_back(inserted_block);      // LRU-MIP or FIFO or RANDOM
    return isEvicted;
}

bool cache_access(cache_t *ca, char rw, uint64_t address) {
    uint64_t index = get_index(address, ca->block_size, ca->num_set);
    uint64_t tag = get_tag(address, ca->block_size, ca->num_set);

    for (auto it = ca->sets[index].begin(); it != ca->sets[index].end(); ++it) {
        if (it->tag == tag) {
            // LRU-MIP and LRU-LIP update priority
            if (ca->repo == REPLACEMENT_POLICY_MIP || ca->repo == REPLACEMENT_POLICY_LIP) {
                if (rw == WRITE && ca->wrst == WRITE_STRAT_WBWA)
                    it->isDirty = true;                 // make the written block dirty
                block_t hit_block = *it;
                ca->sets[index].erase(it);
                ca->sets[index].push_back(hit_block);   // last block is MRU
            }
            // FIFO and RANDOM do nothing
            return true;
        }
    }
    return false;
}

bool victim_cache_insert(victim_cache_t *vc, block_t inserted_block, block_t *evicted_block) {
    bool isEvicted = false;
    if (vc->entries.size() >= vc->num_block) {
        // the victim cache is full
        *evicted_block = vc->entries.front();
        vc->entries.pop_front();                // LRU
        isEvicted = true;
    }
    vc->entries.push_back(inserted_block);      // MIP
    return isEvicted;
}

bool victim_cache_access(victim_cache_t *vc, cache_t *l1, char rw, uint64_t address) {
    uint64_t index = get_index(address, l1->block_size, l1->num_set);
    uint64_t tag = get_tag(address, l1->block_size, l1->num_set);

    for (auto it = vc->entries.begin(); it != vc->entries.end(); ++it) {
        if (it->tag == tag) {
            block_t vc_victim_block = *it;
            vc->entries.erase(it);
            // swap
            if (!l1->sets[index].empty()) { 
                block_t l1_victim_block = l1->sets[index].front();
                l1->sets[index].pop_front();
                vc->entries.push_back(l1_victim_block);
            }
            // no matter l1's set is empty or not
            l1->sets[index].push_back(vc_victim_block);
            return true;
        }
    }
    return false;
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
    bool hit_l1, hit_vc, hit_l2;

    if (rw == READ)
        stats->reads++;
    else if (rw == WRITE)
        stats->writes++;

    // ----- Step 1. Try accessing L1 cache -----
    stats->accesses_l1++;
    hit_l1 = cache_access(&l1, rw, addr);
    if (hit_l1) {
        stats->hits_l1++;
        return;
    }
    stats->misses_l1++;

    // ----- Step 2. On L1 miss, check victim cache if enabled -----
    if (vc.num_block > 0) {
        hit_vc = victim_cache_access(&vc, &l1, rw, addr);
        if (hit_vc) {
            stats->hits_victim_cache++;
            return;
        }
        stats->misses_victim_cache++;
    }

    // ----- Step 3. L1 and victim miss – access L2 (always a read first) -----
    if (!l2.disabled) {
        stats->reads_l2++;
        hit_l2 = cache_access(&l2, READ, addr);
        if (hit_l2) {
            stats->read_hits_l2++;
        } else {
            stats->read_misses_l2++;
            // Read-allocate the block into L2.
            block_t dummy;
            cache_insert(&l2, READ, addr, &dummy);

            // Early Restart enabled: use the word offset.
            uint64_t word_offset = l2.block_size / WORD_SIZE;
            if (l2.enable_ER) {
                uint64_t block_offset = addr % l2.block_size;       // byte offset within block
                word_offset = block_offset / WORD_SIZE;             // which word (0-indexed)
            }
            double miss_penalty = DRAM_AT + (DRAM_AT_PER_WORD * word_offset);
            stats->cumulative_l2_mp += miss_penalty;
        }
    }

    // ----- Step 4. Now fetch the block into L1 (from L2 or memory) -----
    bool l1_evicted, vc_evicted;
    block_t evicted_block_l1, evicted_block_vc;
    
    l1_evicted = cache_insert(&l1, rw, addr, &evicted_block_l1);
    if (l1_evicted && evicted_block_l1.isDirty) {
        stats->write_backs_l1_or_victim_cache++;
        // victim cache is enabled
        if (vc.num_block > 0) {
            vc_evicted = victim_cache_insert(&vc, evicted_block_l1, &evicted_block_vc);
            if (vc_evicted && evicted_block_vc.isDirty && !l2.disabled) {
                stats->writes_l2++;
                cache_access(&l2, WRITE, evicted_block_vc.address);
                // If l2_write_hit is false, then L2 missed and no block is allocated
            }
        } // l2 cache is enabled
        else if (!l2.disabled) {
            stats->writes_l2++; 
            cache_access(&l2, WRITE, evicted_block_l1.address);
            // If l2_write_hit is false, then L2 missed and no block is allocated
        }
        // If L2 is disabled, the write goes directly to main memory.
    }
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * TODO: You're responsible for completing this routine
 */
void sim_finish(sim_stats_t *stats) {
    // Use <cmath> for log2 computation.
    double s_l1 = (l1.set_size > 0) ? log2(l1.set_size) : 0;
    double HT_L1 = L1_HIT_TIME_CONST + (s_l1 * L1_HIT_TIME_PER_S);
    double s_l2 = (l2.set_size > 0) ? log2(l2.set_size) : 0;
    double HT_L2 = L2_HIT_TIME_CONST + (s_l2 * L2_HIT_TIME_PER_S);

    // Compute ratios.
    stats->hit_ratio_l1 = (stats->accesses_l1 > 0) ? ((double)stats->hits_l1 / stats->accesses_l1) : 0;
    stats->miss_ratio_l1 = (stats->accesses_l1 > 0) ? ((double)stats->misses_l1 / stats->accesses_l1) : 0;
    
    stats->hit_ratio_victim_cache = (stats->misses_l1 > 0) ? ((double)stats->hits_victim_cache / stats->misses_l1) : 0;
    stats->miss_ratio_victim_cache = (stats->misses_l1 > 0) ? ((double)stats->misses_victim_cache / stats->misses_l1) : 0;
    
    stats->read_hit_ratio_l2 = (stats->reads_l2 > 0) ? ((double)stats->read_hits_l2 / stats->reads_l2) : 0;
    stats->read_miss_ratio_l2 = (stats->reads_l2 > 0) ? ((double)stats->read_misses_l2 / stats->reads_l2) : 0;
    
    stats->averaged_miss_penalty_l2 = (stats->read_misses_l2 > 0) ? (stats->cumulative_l2_mp / stats->read_misses_l2) : 0;
    
    // Compute L2 Average Access Time (AAT).
    double AAT_L2 = HT_L2 + (stats->read_miss_ratio_l2 * stats->averaged_miss_penalty_l2);
    stats->avg_access_time_l2 = AAT_L2;
    
    // Compute L1 Average Access Time.
    // For L1, the miss penalty is that of an access that misses both L1 and victim cache,
    // which we approximate here by the L2 AAT.
    double MR_VC = (vc.num_block > 0 && stats->misses_l1 > 0) ? ((double)stats->misses_victim_cache / stats->misses_l1) : 1.0;
    stats->avg_access_time_l1 = HT_L1 + (stats->miss_ratio_l1 * MR_VC * AAT_L2);

    // Print out final statistics.
    printf("Overall Reads: %lu\n", stats->reads);
    printf("Overall Writes: %lu\n", stats->writes);
    printf("L1 Accesses: %lu\n", stats->accesses_l1);
    printf("L1 Hits: %lu\n", stats->hits_l1);
    printf("L1 Misses: %lu\n", stats->misses_l1);
    printf("L1 Hit Ratio: %.4f\n", stats->hit_ratio_l1);
    printf("L1 Miss Ratio: %.4f\n", stats->miss_ratio_l1);
    printf("Victim Cache Hits: %lu\n", stats->hits_victim_cache);
    printf("Victim Cache Misses: %lu\n", stats->misses_victim_cache);
    printf("Victim Cache Hit Ratio: %.4f\n", stats->hit_ratio_victim_cache);
    printf("Victim Cache Miss Ratio: %.4f\n", stats->miss_ratio_victim_cache);
    printf("Write-Backs (L1/Victim): %lu\n", stats->write_backs_l1_or_victim_cache);
    printf("L2 Reads: %lu\n", stats->reads_l2);
    printf("L2 Writes: %lu\n", stats->writes_l2);
    printf("L2 Read Hits: %lu\n", stats->read_hits_l2);
    printf("L2 Read Misses: %lu\n", stats->read_misses_l2);
    printf("L2 Read Hit Ratio: %.4f\n", stats->read_hit_ratio_l2);
    printf("L2 Read Miss Ratio: %.4f\n", stats->read_miss_ratio_l2);
    printf("Averaged L2 Miss Penalty: %.2f ns\n", stats->averaged_miss_penalty_l2);
    printf("L1 Average Access Time: %.2f ns\n", stats->avg_access_time_l1);
    printf("L2 Average Access Time: %.2f ns\n", stats->avg_access_time_l2);
}