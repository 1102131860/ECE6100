#include "MOSI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOSI_protocol::MOSI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
    // Initialize lines to not have the data yet!
    this->state = MOSI_CACHE_I;
}

MOSI_protocol::~MOSI_protocol ()
{    
}

void MOSI_protocol::dump (void)
{
#ifdef DEBUG
    // MOSI_CACHE_I = 1, MOSI_CACHE_S, MOSI_CACHE_O, MOSI_CACHE_M, MOSI_CACHE_IS,
    // MOSI_CACHE_IM, MOSI_CACHE_SM, MOSI_CACHE_OM
    const char *block_states[9] = {"X", "I", "S", "O", "M", "IS", "IM", "SM", "OM"};
    fprintf (stdout, "MOSI_protocol - state: %s\n", block_states[state]);
#endif
}

void MOSI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
    case MOSI_CACHE_I:  do_cache_I (request); break;
    case MOSI_CACHE_S:  do_cache_S (request); break;
    case MOSI_CACHE_O:  do_cache_O (request); break;
    case MOSI_CACHE_M:  do_cache_M (request); break;
    case MOSI_CACHE_IS: do_cache_IS (request); break;
    case MOSI_CACHE_IM: do_cache_IM (request); break;
    case MOSI_CACHE_SM: do_cache_SM (request); break;
    case MOSI_CACHE_OM: do_cache_OM (request); break;
    default:
        fatal_error ("Invalid Cache State for MOSI Protocol\n");
    }
}

void MOSI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MOSI_CACHE_I:  do_snoop_I (request); break;
    case MOSI_CACHE_S:  do_snoop_S (request); break;
    case MOSI_CACHE_O:  do_snoop_O (request); break;
    case MOSI_CACHE_M:  do_snoop_M (request); break;
    case MOSI_CACHE_IS: do_snoop_IS (request); break;
    case MOSI_CACHE_IM: do_snoop_IM (request); break;
    case MOSI_CACHE_SM: do_snoop_SM (request); break;
    case MOSI_CACHE_OM: do_snoop_OM (request); break;
    default:
    	fatal_error ("Invalid Cache State for MOSI Protocol\n");
    }
}

inline void MOSI_protocol::do_cache_I (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
        send_GETS(request->addr);
        state = MOSI_CACHE_IS;
        Sim->cache_misses++;
        break;
    case STORE:
        send_GETM(request->addr);
        state = MOSI_CACHE_IM;
        Sim->cache_misses++;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_S (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
        // send data back to the processor to finish the request
        send_DATA_to_proc(request->addr);
        // Load will not change the state as well
        // We hold the data in cache, cache doesn't miss
        break;
    case STORE:
        // Line up the GETM in the Bus' queue
        send_GETM(request->addr);
        // change state to SM
        state = MOSI_CACHE_SM;
        // Write miss happens
        Sim->cache_misses++;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_O (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
        send_DATA_to_proc(request->addr);
        // load will not change the state
        // We hold the data in cahce, cache doesn't miss
        break;
    case STORE:
        send_GETM(request->addr);
        // change state to OM
        state = MOSI_CACHE_OM;
        // Write miss happens
        Sim->cache_misses++;
        break;
    default:
    request->print_msg (my_table->moduleID, "ERROR");
    fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_M (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        /* The M state means we have the data and we can modify it. Therefore any request
        * from the processor (read or write) can be immediately satisfied.
        */
        send_DATA_to_proc(request->addr);
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: M state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_IS (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error("Should only have one outstanding request per processor!");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: IS state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_IM (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error("Should only have one outstanding request per processor!");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: IM state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_SM (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error("Should only have one outstanding request per processor!");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: SM state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_OM (Mreq* request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error("Should only have one outstanding request per processor!");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: OM state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_I (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
    case DATA:
        // do nothing
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_S (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        // still stay in the shared state
        break;
    case GETM:
        // modified by other cache and set to invalid (not $_to_$ transfer)
        state = MOSI_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_O (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        send_DATA_on_bus(request->addr, request->src_mid);
        // increase $-to-$ transfers
        Sim->cache_to_cache_transfers++;
        // Still stay in state S
        break;
    case GETM:
        send_DATA_on_bus(request->addr, request->src_mid);
        // increase $-to-$ transfers
        Sim->cache_to_cache_transfers++;
        // change state to invliad
        state = MOSI_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: O state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_M (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        // send data to other cache via bus
        send_DATA_on_bus(request->addr,request->src_mid);
        Sim->cache_to_cache_transfers++;
        // degrades to Owned
        state = MOSI_CACHE_O;
        break;
    case GETM:
        // send the modified datat to other cache via bus
        send_DATA_on_bus(request->addr,request->src_mid);
        Sim->cache_to_cache_transfers++;
        state = MOSI_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: M state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_IS (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
        // Ignore our GETS or GETM on the bus and wait for DATA to show up
        break;
    case DATA:
        // Data is received we can send the DATA to the processor
        // and finish the transition to S
        send_DATA_to_proc(request->addr);
        // default we assume the data is only from memory, then change to E state
        state = MOSI_CACHE_S;
        if (get_shared_line())
        {
            // Do nothing
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: IS state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_IM (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
        // Ignore our GETS or GETM on the bus and wait for DATA to show up
        break;
    case DATA:
        // Data is received we can send the DATA to the processor
        // and finish the transition to M
        send_DATA_to_proc(request->addr);
        state = MOSI_CACHE_M;
        if (get_shared_line())
        {
            // Nothing to do
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: IM state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_SM (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
        // Ignore our GETS or GETM on the bus and wait for DATA to show up
        break;
    case DATA:
        // Data is received we can send the DATA to the processor
        //and finish the transition to M
        send_DATA_to_proc(request->addr);
        state = MOSI_CACHE_M;
        if (get_shared_line())
        {
            // Nothing to do
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: SM state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_OM (Mreq* request)
{
    switch (request->msg) {
    case GETS:
        // Send data to other caches
        send_DATA_on_bus(request->addr, request->src_mid);
        Sim->cache_to_cache_transfers++;
        break;
    case GETM:
        // Send data to other caches
        send_DATA_on_bus(request->addr, request->src_mid);
        Sim->cache_to_cache_transfers++;
        // Change state to IM
        state = MOSI_CACHE_IM;
        break;
    case DATA:
        // Data is received we can send the DATA to the processor
        // and finish the transition to M
        send_DATA_to_proc(request->addr);
        state = MOSI_CACHE_M;
        if (get_shared_line())
        {
            // Nothing to do
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: OM state shouldn't see this message\n");
    }
}
