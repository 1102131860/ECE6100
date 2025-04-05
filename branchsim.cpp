#include "branchsim.hpp"

static uint64_t H, P, ghr;
static Counter_t* counter_table = NULL;

uint64_t gselect_get_index(uint64_t pc) {
    // PC[2+P−1:2], P bits wide
    uint64_t row = (pc >> 2) & ((1UL << P) - 1);

    // selects the appropriate counter in that row using the GHR[H−1:0]
    return (row << H) | ghr;
}

uint64_t gsplit_get_index(uint64_t address) {
    // uses bits [2+H−1:2] of the branch address as PC, H bits wide
    uint64_t pc_bits = (address >> 2) & ((1UL << H) - 1);

    // PC_HASH = PC[H−P−1:0], (H-P) bits wide
    uint64_t pc_hash = pc_bits & ((1UL << (H - P)) - 1);

    // HIST_HASH = GHR[H−P−1:0], (H-P) bits wide
    uint64_t hist_hash = ghr & ((1UL << (H - P)) - 1);

    // SHARE_HASH = XOR(PC,GHR)[H−1:H−P], P bits wide
    uint64_t share_hash = ((pc_bits ^ ghr) >> (H - P)) & ((1UL << P) - 1);

    // SHARE_HASH|HIST_HASH|PC_HASH, (2*H - P) bits wide
    return (share_hash << (2 * (H - P))) | (hist_hash << (H - P)) | pc_hash;
}

void gselect_init_predictor(branchsim_conf_t *sim_conf) {
    H = sim_conf->H;    // 2 ^ H counters in a row
    P = sim_conf->P;    // 2 ^ P rows
    ghr = 0;            // initialize GHR to 0

    // create Counter table of 2^PH counters and init to weakly not taken
    size_t size = (1UL << (H + P));
    counter_table = (Counter_t*)malloc(size * sizeof(Counter_t));
    // each smith counter is 2-bits wide and initialized to 0b01
    for (size_t i = 0; i < size; i++) {
        Counter_init(&counter_table[i], 2);
    }

#ifdef DEBUG
    printf("GSelect: Creating a Counter table of %" PRIu64 " 2-bit saturating counters\n", size);
#endif
}

bool gselect_predict(branch_t *br) {
#ifdef DEBUG
    printf("\tGSelect: Predicting... \n"); // PROVIDED
#endif

    uint64_t index = gselect_get_index(br->ip);
    bool isTaken = Counter_isTaken(&counter_table[index]);

#ifdef DEBUG
    uint64_t row = (br->ip >> 2) & ((1UL << P) - 1);  // pth row
    printf("\t\tHistory: 0x%" PRIx64 ", Counter index: 0x%" PRIx64 ", Prediction: %d\n", ghr, row, (int) isTaken);
#endif
    return isTaken;
}

void gselect_update_predictor(branch_t *br) {
#ifdef DEBUG
    printf("\tGSelect: Updating based on actual behavior: %d\n", (int) br->is_taken); // PROVIDED
    uint64_t old_ghr = ghr; // before ghr is assigned
#endif

    bool isTaken = br->is_taken;
    uint64_t index = gselect_get_index(br->ip);
    ghr = (ghr << 1 | isTaken) & ((1UL << H) - 1);  // update GHR
    Counter_update(&counter_table[index], isTaken); // update Smith counters

#ifdef DEBUG
    uint64_t row = (br->ip >> 2) & ((1UL << P) - 1);  // pth row
    uint64_t new_counter_value = Counter_get(&counter_table[index]);
    printf("\t\tHistory: 0x%" PRIx64 ", Counter index: 0x%" PRIx64 ", New Counter value: 0x%" PRIx64 ", New History: 0x%" PRIx64 "\n", old_ghr, row, new_counter_value, ghr);
#endif
}

void gselect_cleanup_predictor() {
    if (counter_table) {
        free(counter_table);
        counter_table = NULL;
    }
}

void gsplit_init_predictor(branchsim_conf_t *sim_conf) {
    H = sim_conf->H;
    P = sim_conf->P;
    ghr = 0;

    // create Counter table of 2^(2*H-P) counters and init to weakly not taken
    size_t size = (1UL << (2*H - P));
    counter_table = (Counter_t*)malloc(size * sizeof(Counter_t));
    // each smith counter is 2-bits wide and initialized to 0b01
    for (size_t i = 0; i < size; i++) {
        Counter_init(&counter_table[i], 2);
    }

#ifdef DEBUG
    printf("GSplit: Creating a Counter table of %" PRIu64 " 2-bit saturating counters\n", size);
#endif
}

bool gsplit_predict(branch_t *br) {
#ifdef DEBUG
    printf("\tGSplit: Predicting... \n"); // PROVIDED
#endif
    
    uint64_t index = gsplit_get_index(br->ip);
    bool isTaken = Counter_isTaken(&counter_table[index]);

#ifdef DEBUG
    printf("\t\tHistory: 0x%" PRIx64 ", Counter index: 0x%" PRIx64 ", Prediction: %d\n", ghr, index, (int) isTaken); // TODO: FIX ME
#endif
    return isTaken;
}

void gsplit_update_predictor(branch_t *br) {
#ifdef DEBUG
    printf("\tGSplit: Updating based on actual behavior: %d\n", (int) br->is_taken); // PROVIDED
    uint64_t old_ghr = ghr; // before ghr is assigned
#endif

    bool isTaken = br->is_taken;
    uint64_t index = gsplit_get_index(br->ip);
    ghr = (ghr << 1 | isTaken) & ((1UL << H) - 1);  // update GHR
    Counter_update(&counter_table[index], isTaken); // update Smith counters

#ifdef DEBUG
    uint64_t new_counter_value = Counter_get(&counter_table[index]);
    printf("\t\tHistory: 0x%" PRIx64 ", Counter index: 0x%" PRIx64 ", New Counter value: 0x%" PRIx64 ", New History: 0x%" PRIx64 "\n", old_ghr, index, new_counter_value, ghr);
#endif
}

void gsplit_cleanup_predictor() {
    if (counter_table) {
        free(counter_table);
        counter_table = NULL;
    }
}

/**
 *  Function to update the branchsim stats per prediction
 *
 *  @param prediction The prediction returned from the predictor's predict function
 *  @param br Pointer to the branch_t that is being predicted -- contains actual behavior
 *  @param sim_stats Pointer to the simulation statistics -- update in this function
*/
void branchsim_update_stats(bool prediction, branch_t *br, branchsim_stats_t *sim_stats) {
    sim_stats->num_branch_instructions++;
    if (prediction == br->is_taken) {
        sim_stats->num_branches_correctly_predicted++;
    } else {
        sim_stats->num_branches_mispredicted++;
    }
}

/**
 *  Function to cleanup branchsim statistic computations such as prediction rate, etc.
 *
 *  @param sim_stats Pointer to the simulation statistics -- update in this function
*/
void branchsim_finish_stats(branchsim_stats_t *sim_stats) {
    sim_stats->fraction_branch_instructions = (double) sim_stats->num_branch_instructions / sim_stats->total_instructions;
    sim_stats->prediction_accuracy = (double) sim_stats->num_branches_correctly_predicted / sim_stats->num_branch_instructions;
}
