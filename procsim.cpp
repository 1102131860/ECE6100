#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <deque>

using namespace std;

#include "procsim.hpp"

//
// TODO: Define any useful data structures and functions here
//

typedef enum {
    ALU_FU = 0,
    MUL_FU,
    LSU_FU
} FU_t;

// Reservation Station
typedef struct {
    FU_t fu;
    size_t destPreg;
    size_t src1Preg;
    size_t src2Preg;

    size_t fu_no;                   // refer to FUs[fu_no]
    size_t fire_stage;              // 0: hasn't fire; 1: fire; 2: 1-stage complete; 3: 2-stage complete; 4: 3-stage completes ...
    opcode_t opcode;                // judge LOAD or STORE
    cache_lentency_t dcache_hit;    // determine the stall cycles of LSU
    uint64_t dyn_instruction_count; // determine program order
} RS_t;

// ROB entry
typedef struct {
    size_t destAreg;
    size_t prevPreg;
    bool ready;

    uint64_t pc;                    // record inst's pc
    opcode_t opcode;                // check the BRANCH opcode
    bool mispredict;                // record inst's mispredict field
    bool br_taken;                  // record inst's br_taken
    uint64_t dyn_instruction_count; // determine or used to compare which inst are ready
} ROB_t;

// Register
typedef struct {
    bool ready;
    bool free;
} reg_t;

// DispatchQueue
// don't specified the size, but every time fill fetch f instructions
static size_t fetch_width;
static deque<inst_t> DispQ;

// RAT
// size: NUM REGS
// maps an architectural register to the most recent preg allocated
// initialized to point to the architectural registers
static size_t RAT_size;
static vector<size_t> RAT;

// RF (AReg and Preg)
// NUM REGS: A; searching a free Preg from index NUM REGS; resize: -p : (p + NUM REGS)
static size_t RF_size;
static vector<reg_t> RF;

// SchedulingQueue
// resize: -p : (s x (a + m + l))
static size_t dispatch_width;
static size_t ScheQ_capacity;
static deque<RS_t> ScheQ;

// FUs
// resize -a, -m, -l, first a is ALU, second m is MUL and third l is LSU
static size_t alu_mul_divide_index;
static size_t mul_lsu_divide_index;
static size_t unified_size;
static vector<bool> FUs;

// Result Buses
// size is same with FU's unified_size
static vector<size_t> CDB;

// ROB Queue
// size: (p + 32)
static size_t ROBQ_capacity;
static deque<ROB_t> ROBQ;

// Sum of DispQ, ScheQ, ROBQ and number of retured instructions for each cycles
static uint64_t dispq_size_sum = 0;
static uint64_t scheq_size_sum = 0;
static uint64_t robq_size_sum = 0;
static uint64_t retire_inst_sum = 0;

// Helper functions

// Check free Preg is avaiable
// If available, return first free Preg's index
// else return the size of RF (RF_size)
size_t free_Preg_available() {
    for (size_t i = RAT_size; i < RF_size; i++) {
        if (RF[i].free) {
            return i;
        }
    }
    return RF_size;
}

// Check free FU is available
// If avaiable, return first free FU's index
// else return the size of FUs (unified_size)
size_t free_FU_available(FU_t fu) {
    size_t start_index = 0;
    size_t end_index = 0;

    if (fu == ALU_FU) {
        start_index = 0;
        end_index = alu_mul_divide_index;
    } else if (fu == MUL_FU) {
        start_index = alu_mul_divide_index;
        end_index = mul_lsu_divide_index;
    } else {
        start_index = mul_lsu_divide_index;
        end_index = unified_size;
    }

    for (size_t i = start_index; i < end_index; i++) {
        if (FUs[i]) {
            return i;
        }
    }
    return unified_size;
}



// The helper functions in this#ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)
#ifdef DEBUG
static void print_operand(int8_t rx) {
    if (rx < 0) {
        printf("(none)"); //  PROVIDED
    } else {
        printf("R%" PRId8, rx); //  PROVIDED
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction(const inst_t *inst) {
    if (!inst) {
        return;
    }
    static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    printf("opcode=%s, dest=", opcode_names[inst->opcode]); // PROVIDED
    print_operand(inst->dest); // PROVIDED
    printf(", src1="); // PROVIDED
    print_operand(inst->src1); // PROVIDED
    printf(", src2="); // PROVIDED
    print_operand(inst->src2); // PROVIDED
    printf(", dyncount=%lu", inst->dyn_instruction_count); //  PROVIDED
}

// This will print the state of the ROB where instructions are identified by their dyn_instruction_count
static void print_rob(void) {
    size_t printed_idx = 0;
    printf("\tAllocated Entries in ROB: %lu\n", ROBQ.size()); // TODO: Fix Me
    for (auto rob = ROBQ.begin(); rob != ROBQ.end(); ++rob) { // TODO: Fix Me
        if (printed_idx == 0) {
            printf("    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", rob->dyn_instruction_count, rob->ready, rob->mispredict); // TODO: Fix Me
        } else if (!(printed_idx & 0x3)) {
            printf("\n    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", rob->dyn_instruction_count, rob->ready, rob->mispredict); // TODO: Fix Me
        } else {
            printf(", { dyncount=%05" PRIu64 " completed: %d, mispredict: %d }", rob->dyn_instruction_count, rob->ready, rob->mispredict); // TODO: Fix Me
        }
        printed_idx++;
    }
    if (!printed_idx) {
        printf("    (ROB empty)"); //  PROVIDED
    }
    printf("\n"); //  PROVIDED
}

// This will print out the state of the RAT
static void print_rat(void) {
    for (uint64_t regno = 0; regno < NUM_REGS; regno++) {
        if (regno == 0) {
            printf("    { R%02" PRIu64 ": P%03" PRIu64 " }", regno, RAT[regno]); // TODO: fix me
        } else if (!(regno & 0x3)) {
            printf("\n    { R%02" PRIu64 ": P%03" PRIu64 " }", regno, RAT[regno]); //  TODO: fix me
        } else {
            printf(", { R%02" PRIu64 ": P%03" PRIu64 " }", regno, RAT[regno]); //  TODO: fix me
        }
    }
    printf("\n"); //  PROVIDED
}

// This will print out the state of the register file, where P0-P31 are architectural registers 
// and P32 is the first PREG 
static void print_prf(void) {
    for (uint64_t regno = 0; regno < RF_size; regno++) { // TODO: fix me
        if (regno == 0) {
            printf("    { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, RF[regno].ready, RF[regno].free); // TODO: fix me
        } else if (!(regno & 0x3)) {
            printf("\n    { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, RF[regno].ready, RF[regno].free); // TODO: fix me
        } else {
            printf(", { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, RF[regno].ready, RF[regno].free); // TODO: fix me
        }
    }
    printf("\n"); //  PROVIDED
}
#endif


// Optional helper function which retires instructions in ROB in program
// order. (In a real system, the destination register value from the ROB would be written to the
// architectural registers, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring a mispredicting branch, this function will set
// *retired_mispredict_out = true and will not retire any more instructions. 
// Note that in this case, the mispredict must be counted as one of the retired instructions.
// During mispredict, reset the register files and RAT to initial values.
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_mispredict_out) {
    // TODO: fill me in
    uint64_t retirement_count = 0;
    // Because earse happens, cannot directly ++rob
    for (auto rob = ROBQ.begin(); rob != ROBQ.end(); /*++rob*/) {
        // ROB hasn't complete, skip
        if (!rob->ready) {
            ++rob;
            continue;
        }

        // rob is ready/retire (complete execution)
        retirement_count++;
        // increment instructions_retired
        stats->instructions_retired++;

        // No mispredict occurs
        if (rob->opcode != OPCODE_BRANCH || !rob->mispredict) {
            // retring an instruction will commit their dest reg to the ARegs
            // since we aren't modeling data, don't need actually model this behavior
            // when the prev preg is a Preg (not a Areg), free it
            if (rob->prevPreg >= RAT_size) {
                RF[rob->prevPreg].free = true;
            }

            // delete the rob from ROBQ
            ROBQ.erase(rob);
        } // mispredict occurs
        else {
            // call procsim_driver_update_predictor function to update branch predictor
            procsim_driver_update_predictor(rob->pc, !rob->br_taken, rob->dyn_instruction_count);
            // set retired_mispredict_out to true
            *retired_mispredict_out = true;
            // reset the register files and RAT to initial values
            // RAT is initialized to point to the architectural registers
            // All architecture registers are ready but not free (never free)
            for (size_t i = 0; i < RAT_size; i++) {
                RAT[i] = i;
                RF[i].ready = true;
                RF[i].free = false;
            }
            // All phyiscal registers are free but bot ready
            for (size_t i = RAT_size; i < RF_size; i++) {
                RF[i].ready = false;
                RF[i].free = true;
            }

            // delete the rob from ROBQ
            ROBQ.erase(rob);
            // stop retiring any more instructions
            break;
        }
    }

#ifdef DEBUG
    printf("Stage State Update: \n"); //  PROVIDED
#endif
    return retirement_count; // TODO: Fix Me
}

// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the register file. This function 
// should remove an instruction from the scheduling queue when it has completed.
static void stage_exec(procsim_stats_t *stats) {
    // TODO: fill me in
    // Because earse happens, cannot directly ++rs
    for (auto rs = ScheQ.begin(); rs != ScheQ.end(); /*++rs*/) {
        // RS hasn't been fired, don't execute RS
        if (rs->fire_stage == 0) {
            ++rs;
            continue;
        }

        // pipeline advances and now first stage is free
        rs->fire_stage++;
        // after 1 cycle, all piplined FUs (only MUL_FU) can be free
        if (rs->fu == MUL_FU) {
            FUs[rs->fu_no] = true;
        }

        // check compeletion
        // the initial fire_stage starts from 1, so the true complete stage needs to add 1
        size_t complete_stage = 2;
        // ALU
        if (rs->fu == ALU_FU) {
            complete_stage = ALU_STAGES + 1;
        } // MUL
        else if (rs->fu == MUL_FU) {
            complete_stage = MUL_STAGES + 1;
        } // LSU (STORE)
        else if (rs->opcode == OPCODE_STORE) {
            complete_stage = LSU_STAGES + 1;
        } // LSU (LOAD) and L1 hit
        else if (rs->dcache_hit == CACHE_LATENCY_L1_HIT) {
            complete_stage = L1_HIT_CYCLES + 1;
        } // LSU (LOAD) and L2 hit
        else if (rs->dcache_hit == CACHE_LATENCY_L2_HIT) {
            complete_stage = L2_HIT_CYCLES + 1;
        } // LSU (LOAD) and L2 miss
        else if (rs->dcache_hit == CACHE_LATENCY_L2_MISS) {
            complete_stage = L2_MISS_CYCLES + 1;
        }

        // not complete
        if (rs->fire_stage < complete_stage) {
            // move iterator to next
            ++rs;
            continue;
        }

        // at completion of instruction I
        // the non-piplined FU now can free
        if (rs->fu != MUL_FU) {
            FUs[rs->fu_no] = true;
        }

        // only when destPreg is valid, then update CDB and RF
        if (rs->destPreg != RF_size) {
            // broadcast the results
            CDB[rs->fu_no] = rs->destPreg;
            // mark dest preg as ready (but not free)
            RF[rs->destPreg].ready = true;
        }

        // the ROB entries correpsonding to these instructions are also marked as ready
        for (auto rob = ROBQ.begin(); rob != ROBQ.end(); ++rob) {
            if (rob->dyn_instruction_count == rs->dyn_instruction_count) {
                rob->ready = true;
            }
        }

        // delete the completed instruction I
        ScheQ.erase(rs);
    }

#ifdef DEBUG
    printf("Stage Exec: \n"); //  PROVIDED
#endif

    // Progress ALUs
#ifdef DEBUG
    printf("\tProgressing ALU units\n");  // PROVIDED
#endif

    // Progress MULs
#ifdef DEBUG
    printf("\tProgressing MUL units\n");  // PROVIDED
#endif

    // Progress LSU loads / stores
#ifdef DEBUG
    printf("\tProgressing LSU units\n");  // PROVIDED
#endif

    // Apply Result Busses
#ifdef DEBUG
    printf("\tProcessing Result Buses\n"); // PROVIDED
#endif

}

// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions that have their source pregs
// marked as ready. Note that when multiple instructions are ready to fire
// in a given cycle, they must be fired in program order. 
// Also, load and store instructions must be fired according to the 
// memory disambiguation algorithm described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats) {
    // TODO: fill me in

    // use a copy vector for sort dyncount
    // then iterate in that sorted vector, will it make sense?

    bool no_fire = true;
    bool enable_fire_store = true;
    for (auto rs = ScheQ.begin(); rs != ScheQ.end(); ++rs) {
        // RS has fired, don't fire RS again
        if (rs->fire_stage != 0) {
            continue;
        }

        // one of source pregs doesn't wake up, don't fire RS
        if ((rs->src1Preg != RF_size && !RF[rs->src1Preg].ready) || // src1Preg doesn't wakeup
            (rs->src2Preg != RF_size && !RF[rs->src2Preg].ready)) { // src2Preg doesn't wakeup
            continue;
        }

        // two source pregs both wake up now
        size_t fu_index = free_FU_available(rs->fu);
        // no free FU unit, don't fire RS
        if (fu_index == unified_size) {
            continue;
        }

        // free FU are now avilable for alu, mul, and lsu
        if (rs->fu == LSU_FU) {
            // check Memory Dismbiguation
            bool violate_mem_disam = false;
            if (rs->opcode == OPCODE_LOAD) {
                // check whether there is a preceding store (RS can be fire at the stage of exec)
                // use inst_t.dyn_instruction_count to determine preceding or not
                for (auto it = ScheQ.begin(); it != ScheQ.end(); ++it) {
                    // a Load instruction can never be fired before ALL Store instructions that precedes it in program order complete
                    if (it->dyn_instruction_count < rs->dyn_instruction_count && it->opcode == OPCODE_STORE) {
                        violate_mem_disam = true;
                        break;
                    }
                }
            } // OPCODE_STORE
            else {
                if (enable_fire_store) {
                    // check whether there is a preceding load or store (FU = lsu) before this store
                    for (auto it = ScheQ.begin(); it != ScheQ.end(); ++it) {
                        // a Store instruction can never be fired before ALL Load and Store instructions that precede it in program order complete
                        if (it->dyn_instruction_count < rs->dyn_instruction_count && it->fu == LSU_FU) {
                            violate_mem_disam = true;
                            break;
                        }
                    }
                    // when this store inst can fire, mark enable_fire_store as false
                    enable_fire_store = false;
                } // only 1 store can be fired per cycle
                else {
                    violate_mem_disam = true;
                }
            }

            // violate the memory disambiguation algorithm
            if (violate_mem_disam) {
                // don't fire RS
                continue;
            }
        }

        // now all srcPregs wakeup, FU avilable and all Load/Store obey Memory disambiguation, reserve the FU
        // fire this RS, set stage to 1
        rs->fire_stage = 1;
        // assign the sepcific FU
        rs->fu_no = fu_index;
        // set the fu as not free
        FUs[fu_index] = false;
        // set no_fire to false
        no_fire = false;
    }

    // increment no_fire_cycles
    if (no_fire) {
        stats->no_fire_cycles++;
    }

#ifdef DEBUG
    printf("Stage Schedule: \n"); //  PROVIDED
#endif
}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB; 
// effectively, dispatch allocates reservation stations and ROB space for 
// each instruction dispatched and stalls if there any are unavailable. 
// Note the scheduling queue has a configurable size and the ROB has p + 32 entries.
// The PDF has details.
// dispatch width is fetch width
static void stage_dispatch(procsim_stats_t *stats) {
    // TODO: fill me in
    size_t NOP_count = 0;
    size_t Not_NOP_count = 0;
    while (true) {
        // 1. nothing in the DispQ, stall
        if (DispQ.empty()) {
            break;
        }
        // fill ScheQ with instructions from the head of DispQ
        inst_t I = DispQ.front();
        // Instruction I now is "dispatched"
        DispQ.pop_front();

        // 2. already dispatched dispatch_width NOPs, stall
        if (NOP_count >= dispatch_width) {
            break;
        }
        // I is a NOP
        if (I.dest == (int8_t)0) {
            NOP_count++;
            // should be dropped
            continue;
        }

        // 3. already dispacthed dispacth_width are not NOPs, stall
        if (Not_NOP_count >= dispatch_width) {
            break;
        }
        // I is not a NOP
        Not_NOP_count++;

        RS_t rs;
        // set a reservation station's function unit specifier
        if (I.opcode == OPCODE_ADD || I.opcode == OPCODE_BRANCH) {
            rs.fu = ALU_FU;
        } else if (I.opcode == OPCODE_MUL) {
            rs.fu = MUL_FU;
        } else { // OPCODE_STORE, OPCODE_LOAD
            rs.fu = LSU_FU;
        }
        // rs hasn't allocate to a specific FU, use unified_size to indicate invalid
        rs.fu_no = unified_size;
        // rs hasn't been fired in the dispatch stage
        rs.fire_stage = 0;
        // set source pregs using the RAT
        rs.src1Preg = (I.src1 == (int8_t)-1) ? RF_size : RAT[I.src1];
        rs.src2Preg = (I.src2 == (int8_t)-1) ? RF_size : RAT[I.src2];
        // record opcode, dcache_hit, and dyn_instruction_count
        rs.opcode = I.opcode;
        rs.dcache_hit = I.dcache_hit;
        rs.dyn_instruction_count = I.dyn_instruction_count;

        ROB_t rob;
        // set the ROB's ready to false
        rob.ready = false;
        // record the ROB's pc, br_taken, opcode, mispredict and dyn_instruction_count
        rob.pc = I.pc;
        rob.opcode = I.opcode;
        rob.mispredict = I.mispredict;
        rob.br_taken = I.br_taken;
        rob.dyn_instruction_count = I.dyn_instruction_count;

        // an instruction has a dest
        if (I.dest != (int8_t)-1) {
            // set the ROB prev preg, dest preg
            rob.destAreg = I.dest;
            rob.prevPreg = RAT[I.dest];

            // search a free Preg
            size_t preg_index = free_Preg_available();
            // 4. No free Preg for a destination, stall
            if (preg_index == RF_size) {
                // increment No_dispatch_cycles_pregs
                stats->no_dispatch_pregs_cycles++;
                break;
            }

            // set the rs dest to the found free preg
            rs.destPreg = preg_index;
            // mark the RF[preg_index] as not ready and not free
            RF[preg_index].ready = false;
            RF[preg_index].free = false;
            // update RAT, after 'rob.prevPreg = RAT[I.dest]'
            RAT[I.dest] = preg_index;
        } // an instruction hasn't a dest
        else {
            // set the ROB prev preg, dest preg as invalid indices
            rob.destAreg = RAT_size;
            rob.prevPreg = RF_size;
            // set destPreg as an invalid index
            rs.destPreg = RF_size;
            // no need to set RF and RAT
        }

        // 5. ROBQ is full, stall
        if (ROBQ.size() >= ROBQ_capacity) {
            // increment No_dispatch_cycles_rob
            stats->rob_no_dispatch_cycles++;
            break;
        }
        // add to the tail of ROBQ
        ROBQ.push_back(rob);

        // 6. ScheQ is full, stall
        if (ScheQ.size() >= ScheQ_capacity) {
            break;
        }
        // add to the tail of ScheQ
        ScheQ.push_back(rs);
    }

#ifdef DEBUG
    printf("Stage Dispatch: \n"); //  PROVIDED
#endif
}

// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. 
// If a NULL pointer is fetched from the procsim_driver_read_inst, and driver_read_status is DRIVER_READ_ICACHE_MISS
// insert a NOP to the dispatch queue
// that NOP should be dropped at the dispatch stage
static void stage_fetch(procsim_stats_t *stats) {
    // TODO: fill me in
    bool end_of_trace = false;
    for (size_t i = 0; i < fetch_width && !end_of_trace; i++) {
        driver_read_status_t driver_read_status_output;
        const inst_t* inst = procsim_driver_read_inst(&driver_read_status_output);

        switch(driver_read_status_output) {
        case DRIVER_READ_OK:
            // push inst into DispQ
            DispQ.push_back(*inst);
            stats->instructions_fetched++;
            break;
        case DRIVER_READ_ICACHE_MISS:
            // push NOP into DispQ
            // Reg0 cannot be a real destination, so set dest as Reg0 to indicate a NOP
            DispQ.push_back(inst_t{});
            DispQ.back().dest = 0;
            // else do not increment fetched instructions for NOPs
            break;
        case DRIVER_READ_MISPRED:
            // don't push NOPs into the dispatch queue
            // should make i-- to maintain the fetch_width?
            // increment instructions_fetched due to a branch misprediction
            stats->instructions_fetched++;
            break;
        case DRIVER_READ_END_OF_TRACE:
            // finish fetching
            end_of_trace = true;
            break;
        default:
            // do nothing
            break;
        }
    }

#ifdef DEBUG
    printf("Stage Fetch: \n"); //  PROVIDED
#endif
}

// Use this function to initialize all your data structures, simulator
// state, and statistics.
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    // TODO: fill me in
    // 1. RAT
    RAT_size = NUM_REGS;
    RAT.resize(RAT_size);
    // RAT is initialized to point to the architectural registers
    for (size_t i = 0; i < RAT_size; i++) {
        RAT[i] = i;
    }

    // 2. RF
    RF_size = sim_conf->num_pregs + RAT_size;
    RF.assign(RF_size, {true, true});
    // All architecture registers are ready but not free (never free)
    for (size_t i = 0; i < RAT_size; i++) {
        RF[i].free = false;
    }
    // All phyiscal registers are free but bot ready
    for (size_t i = RAT_size; i < RF_size; i++) {
        RF[i].ready = false;
    }

    // 3. FU
    alu_mul_divide_index = sim_conf->num_alu_fus;
    mul_lsu_divide_index = alu_mul_divide_index + sim_conf->num_mul_fus;
    unified_size = mul_lsu_divide_index + sim_conf->num_lsu_fus;
    // All function units are ready
    FUs.assign(unified_size, true);

    // 4. CDB
    // iniatially pointing to PReg0
    CDB.assign(unified_size, 0);

    // 5. DispQ
    // Deque, defult is empty
    // No specific max size
    fetch_width = sim_conf->fetch_width;

    // 6. ScheQ
    // Deque, defult is empty
    dispatch_width = sim_conf->dispatch_width;
    ScheQ_capacity = unified_size * sim_conf->num_schedq_entries_per_fu;

    // 7. ROBQ
    // Deque, defult is empty
    ROBQ_capacity = sim_conf->num_rob_entries;

#ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", ScheQ_capacity); // TODO: Fix Me
    printf("Initial RAT state:\n"); //  PROVIDED
    print_rat();
    printf("\n"); //  PROVIDED
#endif
}

// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if a mispredict was retired by assigning true or false to
// *retired_mispredict_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_mispredict_out) {
#ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
#endif

    // stage_state_update() should set *retired_mispredict_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_mispredict_out);

    if (*retired_mispredict_out) {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired mispredict, so notifying driver to fetch correctly!\n", retired_this_cycle); //  PROVIDED
#endif

        // After we retire a misprediction, the other stages don't need to run
        stats->branch_mispredictions++;
    } else {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire mispredict, so proceeding with other pipeline stages.\n", retired_this_cycle); //  PROVIDED
#endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

    uint64_t dispq_size_this_cycle = DispQ.size();
    uint64_t scheq_size_this_cycle = ScheQ.size();
    uint64_t robq_size_this_cycle = ROBQ.size();

#ifdef DEBUG
    printf("End-of-cycle dispatch queue usage: %lu\n", dispq_size_this_cycle); // TODO: Fix Me
    printf("End-of-cycle sched queue usage: %lu\n", scheq_size_this_cycle); // TODO: Fix Me
    printf("End-of-cycle ROB usage: %lu\n", robq_size_this_cycle); // TODO: Fix Me
    printf("End-of-cycle RAT state:\n"); // PROVIDED
    print_rat(); // PROVIDED
    printf("End-of-cycle Physical Register file state:\n"); // PROVIDED
    print_prf(); // PROVIDED
    printf("End-of-cycle ROB state:\n"); // PROVIDED
    print_rob(); // PROVIDED
    printf("================================ End cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
    print_instruction(NULL); // this makes the compiler happy, ignore it
#endif

    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;

    stats->dispq_max_usage = max(stats->dispq_max_usage, dispq_size_this_cycle);
    stats->schedq_max_usage = max(stats->schedq_max_usage, scheq_size_this_cycle);
    stats->rob_max_usage = max(stats->rob_max_usage, robq_size_this_cycle);
    dispq_size_sum += dispq_size_this_cycle;
    scheq_size_sum += scheq_size_this_cycle;
    robq_size_sum += robq_size_this_cycle;
    retire_inst_sum += retired_this_cycle;

    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats) {
    // TODO: fill me in
    stats->dispq_avg_usage = (double) dispq_size_sum / stats->cycles;
    stats->schedq_avg_usage = (double) scheq_size_sum / stats->cycles;
    stats->rob_avg_usage = (double) robq_size_sum / stats->cycles;
    stats->ipc = (double) retire_inst_sum / stats->cycles;
}