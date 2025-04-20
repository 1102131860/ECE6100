#include "MOESIF_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOESIF_protocol::MOESIF_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
    // Initialize lines to not have the data yet!
    this->state = MOESIF_CACHE_I;
}

MOESIF_protocol::~MOESIF_protocol ()
{    
}

void MOESIF_protocol::dump (void)
{
#ifdef DEBUG
    // MOESIF_CACHE_I = 1, MOESIF_CACHE_S, MOESIF_CACHE_E, MOESIF_CACHE_O, MOESIF_CACHE_M, MOESIF_CACHE_F,
    // MOESIF_CACHE_IS, MOESIF_CACHE_IM, MOESIF_CACHE_SM, MOESIF_CACHE_OM, MOESIF_CACHE_FM
    const char *block_states[12] = {"X", "I", "S", "E", "O", "M", "F", "IS", "IM", "SM", "OM", "FM"};
    fprintf (stdout, "MOESIF_protocol - state: %s\n", block_states[state]);
#endif
}

void MOESIF_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
    case MOESIF_CACHE_I:  do_cache_I (request); break;
    case MOESIF_CACHE_S:  do_cache_S (request); break;
    case MOESIF_CACHE_E:  do_cache_E (request); break;
    case MOESIF_CACHE_O:  do_cache_O (request); break;
    case MOESIF_CACHE_M:  do_cache_M (request); break;
    case MOESIF_CACHE_F:  do_cache_F (request); break;
    case MOESIF_CACHE_IS: do_cache_IS (request); break;
    case MOESIF_CACHE_IM: do_cache_IM (request); break;
    case MOESIF_CACHE_SM: do_cache_SM (request); break;
    case MOESIF_CACHE_OM: do_cache_OM (request); break;
    case MOESIF_CACHE_FM: do_cache_FM (request); break;
    default:
        fatal_error ("Invalid Cache State for MOESIF Protocol\n");
    }
}

void MOESIF_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MOESIF_CACHE_I:  do_snoop_I (request); break;
    case MOESIF_CACHE_S:  do_snoop_S (request); break;
    case MOESIF_CACHE_E:  do_snoop_E (request); break;
    case MOESIF_CACHE_O:  do_snoop_O (request); break;
    case MOESIF_CACHE_M:  do_snoop_M (request); break;
    case MOESIF_CACHE_F:  do_snoop_F (request); break;
    case MOESIF_CACHE_IS: do_snoop_IS (request); break;
    case MOESIF_CACHE_IM: do_snoop_IM (request); break;
    case MOESIF_CACHE_SM: do_snoop_SM (request); break;
    case MOESIF_CACHE_OM: do_snoop_OM (request); break;
    case MOESIF_CACHE_FM: do_snoop_FM (request); break;
    default:
    	fatal_error ("Invalid Cache State for MOESIF Protocol\n");
    }
}

inline void MOESIF_protocol::do_cache_F (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
        send_DATA_to_proc(request->addr);
        // stay in F
        break;
    case STORE:
        send_GETM(request->addr);
        state = MOESIF_CACHE_FM;
        // Write miss happens
        Sim->cache_misses++;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: F state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_cache_I (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
        send_GETS(request->addr);
        state = MOESIF_CACHE_IS;
        Sim->cache_misses++;
        break;
    case STORE:
        send_GETM(request->addr);
        state = MOESIF_CACHE_IM;
        Sim->cache_misses++;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_cache_S (Mreq *request)
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
        state = MOESIF_CACHE_SM;
        // Write miss happens
        Sim->cache_misses++;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_cache_E (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
        // send data back to the processor to finish the request
        send_DATA_to_proc(request->addr);
        // Load will not change the state
        // Hold the data in cache, cache doesn't miss
        break;
    case STORE:
        send_DATA_to_proc(request->addr);
        // Since you are the only one hold the data, you don't need to tell others
        state = MOESIF_CACHE_M;
        // so you need increase silent_upgrades
        Sim->silent_upgrades++;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: E state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_cache_O (Mreq *request)
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
        state = MOESIF_CACHE_OM;
        // Write miss happens
        Sim->cache_misses++;
        break;
    default:
    request->print_msg (my_table->moduleID, "ERROR");
    fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_cache_M (Mreq *request)
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

inline void MOESIF_protocol::do_cache_IS (Mreq *request)
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

inline void MOESIF_protocol::do_cache_IM (Mreq *request)
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

inline void MOESIF_protocol::do_cache_SM (Mreq *request)
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

inline void MOESIF_protocol::do_cache_OM (Mreq *request)
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

inline void MOESIF_protocol::do_cache_FM (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error("Should only have one outstanding request per processor!");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: FM state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_F (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        // Here should send data on bus, as it's exculsive
        send_DATA_on_bus(request->addr, request->src_mid);
        Sim->cache_to_cache_transfers++;
        // still stay in F
        break;
    case GETM:
        send_DATA_on_bus(request->addr, request->src_mid);
        // increase $-to-$ transfers
        Sim->cache_to_cache_transfers++;
        // change state to invliad
        state = MOESIF_CACHE_I;
        break;    
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: F state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_I (Mreq *request)
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

inline void MOESIF_protocol::do_snoop_S (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        // still stay in the shared state
        break;
    case GETM:
        // modified by other cache and set to invalid (not $_to_$ transfer)
        // set_shared_line();
        state = MOESIF_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_E (Mreq *request)
{
    switch(request->msg) {
    case GETS:
        // Here should send data on bus, as it's exculsive
        send_DATA_on_bus(request->addr, request->src_mid);
        Sim->cache_to_cache_transfers++;
        // degrades to F (you are no long the only one hold the data)
        state = MOESIF_CACHE_F;
        break;
    case GETM:
        // Modified by other cache and set to invalid
        send_DATA_on_bus(request->addr, request->src_mid);
        Sim->cache_to_cache_transfers++;
        // degreades to I
        state = MOESIF_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: E state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_O (Mreq *request)
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
        state = MOESIF_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: O state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_M (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        // send data to other cache via bus
        send_DATA_on_bus(request->addr,request->src_mid);
        Sim->cache_to_cache_transfers++;
        // degrades to Owned
        state = MOESIF_CACHE_O;
        break;
    case GETM:
        // send the modified datat to other cache via bus
        send_DATA_on_bus(request->addr,request->src_mid);
        Sim->cache_to_cache_transfers++;
        state = MOESIF_CACHE_I;
        break;
    case DATA:
        fatal_error ("Should not see data for this line!  I have the line!");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: M state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_IS (Mreq *request)
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
        state = MOESIF_CACHE_E;
        if (get_shared_line())
        {
            // If the data come from other cache, then change to S state
            state = MOESIF_CACHE_S;
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: IS state shouldn't see this message\n");
    }
}

inline void MOESIF_protocol::do_snoop_IM (Mreq *request)
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
        state = MOESIF_CACHE_M;
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

inline void MOESIF_protocol::do_snoop_SM (Mreq *request)
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
        state = MOESIF_CACHE_M;
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

inline void MOESIF_protocol::do_snoop_OM (Mreq *request)
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
        state = MOESIF_CACHE_IM;
        break;
    case DATA:
        // Data is received we can send the DATA to the processor
        // and finish the transition to M
        send_DATA_to_proc(request->addr);
        state = MOESIF_CACHE_M;
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

inline void MOESIF_protocol::do_snoop_FM (Mreq *request)
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
        state = MOESIF_CACHE_IM;
        break;
    case DATA:
        // Data is received we can send the DATA to the processor
        // and finish the transition to M
        send_DATA_to_proc(request->addr);
        state = MOESIF_CACHE_M;
        if (get_shared_line())
        {
            // Nothing to do
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: FM state shouldn't see this message\n");
    }
} 