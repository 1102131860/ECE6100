#include "cachesim.hpp"
#include <vector>
#include <deque>        // a double-linked list (FIFO)

using namespace std;

typedef struct block {
    uint64_t tag;       // different caches have different sizes of tag
    uint64_t address;   // the block address
    bool isValid;       // the initial block in the cache should be false;
    bool isDirty;       // signed this block data has been written or not
} block_t;

typedef struct victim_cache {
    uint64_t block_size;    // same the l1 and l2's block size, 1 << B (2^B) bytes
    uint64_t num_block;     // the number of blocks it can hold, must be 0, 1 or 2, zero disables the victim cache
    uint64_t num_set;       // should always be 1, as it is full associative cache
    deque<block_t> entries; // the entries hold the evicted blocks from cache l1
} victim_cache_t;

typedef struct cache {
    bool disabled;
    bool enable_ER;
    replacement_policy_t repo;
    write_strat_t wrst;
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
    return address >> (__builtin_ctz(block_size) + __builtin_ctz(num_set));
}

uint64_t get_index(uint64_t address, uint64_t block_size, uint64_t num_set) {
    return (address >> __builtin_ctz(block_size)) & (num_set - 1); 
}

uint64_t get_miss_penalty(uint64_t address, uint64_t block_size, bool enable_ER) {
    uint64_t word_offset = block_size >> __builtin_ctz(WORD_SIZE);
    if (enable_ER) {                                            // Early Restart enabled: use the word offset
        uint64_t block_offset = address & (block_size - 1);     // byte offset within block
        word_offset = block_offset >> __builtin_ctz(WORD_SIZE); // which word (0-indexed)
    }
    return DRAM_AT + (DRAM_AT_PER_WORD * word_offset);
}

void cache_setup(cache_t *ca, cache_config_t config) {
    ca->disabled = config.disabled;
    ca->enable_ER = config.enable_ER;
    ca->repo = config.replace_policy;
    ca->wrst = config.write_strat;
    ca->block_size = 1ULL << config.b;                      // 2^B
    ca->set_size = 1ULL << config.s;                        // 2^S
    ca->num_block = 1ULL << (config.c - config.b);          // 2^(C - B)
    ca->num_set = 1ULL << (config.c - config.b - config.s); // 2^(C - B - S)
    ca->sets.resize(ca->num_set);

    uint64_t init_addr = 0x0000000000000000;
    uint64_t init_tag = get_tag(init_addr, ca->block_size, ca->num_set);
    bool isValid = false;
    bool isDirty = false;
    for (auto& set : ca->sets)
        set = deque<block_t>(ca->set_size, block_t{init_tag, init_addr, isValid, isDirty});
}

void victim_cache_setup(victim_cache_t *vc, uint64_t entries, uint64_t b) {
    vc->block_size = 1ULL << b;                             // 2^b
    vc->num_block = entries;                                // number of blocks
    vc->num_set = 1;                                        // fully associative cache has only one set
    
    uint64_t init_addr = 0x0000000000000000;
    uint64_t init_tag = get_tag(init_addr, vc->block_size, vc->num_set);
    bool isValid = false;
    bool isDirty = false;
    for (uint64_t i = 0; i < entries; i++) 
        vc->entries.push_back(block_t{init_tag, init_addr, isValid, isDirty});
}

bool cache_insert(cache_t *ca, char rw, uint64_t address, block_t *evicted_block) {
    // when disabled, don't actually insert
    if (ca->disabled) 
        return false;
    
    bool isEvicted = false;
    uint64_t index = get_index(address, ca->block_size, ca->num_set);
    if (ca->sets[index].size() >= ca->set_size) {
        // firstly need to pop out all the invalid blocks
        bool all_valid = true;
        for (auto it = ca->sets[index].begin(); it != ca->sets[index].end(); ++it) {
            if (!it->isValid) {
                *evicted_block = *it;
                ca->sets[index].erase(it);
                all_valid = false;
                break;
            }
        }
        if (all_valid) {
            if (ca->repo != REPLACEMENT_POLICY_RANDOM) {    // MIP, LIP, FIFO
                *evicted_block = ca->sets[index].front();
                ca->sets[index].pop_front();                // LRU
            } else {                                        // RANDOM
                uint64_t mod = ca->sets[index].size() > 1 ? ca->sets[index].size() - 1 : 1;
                uint64_t evicted_index = (uint64_t)(evict_random() % mod);
                auto evicted_block_it = ca->sets[index].begin() + evicted_index;
                *evicted_block = *evicted_block_it;
                ca->sets[index].erase(evicted_block_it);    // random position
            }    
        }
        isEvicted = true;
    }

    block_t inserted_block;
    inserted_block.tag = get_tag(address, ca->block_size, ca->num_set);
    inserted_block.address = address;
    inserted_block.isValid = true;
    inserted_block.isDirty = (rw == WRITE && ca->wrst == WRITE_STRAT_WBWA); // WBWA needs to mark the written data as dirty
    if (ca->repo == REPLACEMENT_POLICY_LIP)
        ca->sets[index].push_front(inserted_block);     // LIP
    else
        ca->sets[index].push_back(inserted_block);      // MIP or FIFO or RANDOM
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
                ca->sets[index].push_back(hit_block);   // update recent access
            }
            // FIFO and RANDOM do nothing
            return true;
        }
    }
    return false;
}

bool victim_cache_insert(victim_cache_t *vc, block_t l1_evicted_block, block_t *evicted_block) {
    // when disabled, don't actually insert
    if (vc->num_block == 0) 
        return false;
    
    bool isEvicted = false;
    if (vc->entries.size() >= vc->num_block) {
        *evicted_block = vc->entries.front();
        vc->entries.pop_front();                // LRU
        isEvicted = true;
    }

    // Re-tag l1 evicted block to insert it into victim cache
    l1_evicted_block.tag = get_tag(l1_evicted_block.address, vc->block_size, vc->num_set);
    vc->entries.push_back(l1_evicted_block);      // MIP
    return isEvicted;
}

bool victim_cache_access(victim_cache_t *vc, cache_t *l1, char rw, uint64_t address) {
    uint64_t tag = get_tag(address, vc->block_size, vc->num_set);

    for (auto it = vc->entries.begin(); it != vc->entries.end(); ++it) {
        if (it->tag == tag) {
            // literal swap
            block_t vc_victim_block = *it;
            vc->entries.erase(it);                          // it won't exist in victim cache

            if (vc_victim_block.isDirty)                    // If this access is write, inserted block should mark as dirty
                rw = WRITE;                                 // or the vc_victim_block is already is dirty, also mark as dirty
            
            block_t l1_victim_block;
            cache_insert(l1, rw, vc_victim_block.address, &l1_victim_block);
            block_t dummy;
            victim_cache_insert(vc, l1_victim_block, &dummy);

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

void sim_setup(sim_config_t *config) {
    cache_setup(&l1, config->l1_config);
    victim_cache_setup(&vc, config->victim_cache_entries, config->l1_config.b);
    cache_setup(&l2, config->l2_config);
}

void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    bool hit_l1, hit_vc, hit_l2;
    bool l1_evicted, vc_evicted;
    hit_l1 = hit_vc = hit_l2 = l1_evicted = vc_evicted = false;
    block_t evicted_block_l1, evicted_block_vc, evicted_block_l2;

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

    // ----- Step 2. On L1 miss, access victim cache -----
    hit_vc = victim_cache_access(&vc, &l1, rw, addr);
    if (hit_vc) {
        stats->hits_victim_cache++;
        return;
    }
    stats->misses_victim_cache++;

    // ----- Step 3. L1 and victim miss, access L2 (always a read first) -----
    stats->reads_l2++;
    hit_l2 = cache_access(&l2, READ, addr);
    if (hit_l2) {
        stats->read_hits_l2++;
    } else {
        stats->read_misses_l2++;
        // Sum up the L2 miss penalty.
        stats->cumulative_l2_mp += get_miss_penalty(addr, l2.block_size, l2.enable_ER);
        // Read miss and insert the missed block
        cache_insert(&l2, READ, addr, &evicted_block_l2);
    }

    // ----- Step 4. Fetch the block into L1 and victiom cache -----
    l1_evicted = cache_insert(&l1, rw, addr, &evicted_block_l1);
    if (l1_evicted && evicted_block_l1.isValid) {
        vc_evicted = victim_cache_insert(&vc, evicted_block_l1, &evicted_block_vc);
        if ((vc_evicted && evicted_block_vc.isDirty) || (vc.num_block == 0 && evicted_block_l1.isDirty)) {
            // write back from L1 or victim_cache
            stats->write_backs_l1_or_victim_cache++;
            // write through wirte no allication to L2
            stats->writes_l2++;
            // Write Access L2
            if (vc.num_block == 0) 
                cache_access(&l2, WRITE, evicted_block_l1.address);
            else
                cache_access(&l2, WRITE, evicted_block_vc.address);
        }
    }
}

void sim_finish(sim_stats_t *stats) {
    double s_l1 = __builtin_ctz(l1.set_size);                                               
    double HT_L1 = L1_HIT_TIME_CONST + (s_l1 * L1_HIT_TIME_PER_S);
    double s_l2 = __builtin_ctz(l2.set_size);                                               // log2
    double HT_L2 = (!l2.disabled) ? (L2_HIT_TIME_CONST + (s_l2 * L2_HIT_TIME_PER_S)) : 0;   // no searching and hit time

    // Compute ratios.
    stats->hit_ratio_l1 = (double)stats->hits_l1 / stats->accesses_l1;
    stats->miss_ratio_l1 = (double)stats->misses_l1 / stats->accesses_l1;
    
    stats->hit_ratio_victim_cache = (double)stats->hits_victim_cache / stats->misses_l1;
    stats->miss_ratio_victim_cache = (double)stats->misses_victim_cache / stats->misses_l1;
    
    stats->read_hit_ratio_l2 = (double)stats->read_hits_l2 / stats->reads_l2;
    stats->read_miss_ratio_l2 = (double)stats->read_misses_l2 / stats->reads_l2;
    
    stats->averaged_miss_penalty_l2 = (double)stats->cumulative_l2_mp / stats->read_misses_l2;
    
    // Compute L2 Average Access Time (L2 AAT).
    stats->avg_access_time_l2 = HT_L2 + (stats->read_miss_ratio_l2 * stats->averaged_miss_penalty_l2);
    
    // Compute L1 Average Access Time (L1 AAT).
    // For L1, the miss penalty is an access that misses both L1 and victim cache, which approximate here by the L2 AAT.
    stats->avg_access_time_l1 = HT_L1 + (stats->miss_ratio_l1 * stats->miss_ratio_victim_cache * stats->avg_access_time_l2);
}
