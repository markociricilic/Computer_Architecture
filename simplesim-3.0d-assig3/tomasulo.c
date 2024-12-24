
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         10

#define RESERV_INT_SIZE    4
#define RESERV_FP_SIZE     2
#define FU_INT_SIZE        2
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     4
#define FU_FP_LATENCY      9

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */

/* ECE552 Assignment 3 - BEGIN CODE */
void print_instruction(instruction_t* instruction) {
  printf("index: %d, op: %d, dispatch: %d, issue: %d, execute: %d, cdb: %d\n", instruction->index, (int)instruction->op, instruction->tom_dispatch_cycle, instruction->tom_issue_cycle, instruction->tom_execute_cycle, instruction->tom_cdb_cycle);
}
/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {

  // check if instructions fetched >= number of instructions simulated and check if IFQ is empty
  if (fetch_index >= sim_insn && instr_queue_size == 0) {
    int i;
    // Check INT Reservation Stations
    for (i = 0; i < RESERV_INT_SIZE; i++) {
      if (reservINT[i]) {
        // implies that entry is not empty, therefore there are instructions remaining in the pipeline
        return false;
      }
    }
    // Check FP Reservation Stations
    for (i = 0; i < RESERV_FP_SIZE; i++) {
      if (reservFP[i]) {
        // implies that entry is not empty, therefore there are instructions remaining in the pipeline
        return false;
      }
    }
    // Check INT Functional Units
    for (i = 0; i < FU_INT_SIZE; i++) {
      if (fuINT[i]) {
        // implies that entry is not empty, therefore there are instructions remaining in the pipeline
        return false;
      }
    }
    // Check FP Functional Units
    for (i = 0; i < FU_FP_SIZE; i++) {
      if (fuFP[i]) {
        // implies that entry is not empty, therefore there are instructions remaining in the pipeline
        return false;
      }
    }
    // If the code gets to this point, all arrays are empty and therefore the simulation is done.
    return true;
  }
  // If the code gets to this point, fails initial check, therefore simulation is not done.
  return false; //ECE552: you can change this as needed; we've added this so the code provided to you compiles
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

  // clear CDB, clear map_table, clear RS and FU entries, clear dependencies in RS
  // Check if any instruction is broadcasting
  if (commonDataBus != NULL) {
    // Check reservation stations for any instructions waiting on result, resolve RAW hazards
    int i;
    int j;

    // Check for dependencies in INT RS
    for (i = 0; i < RESERV_INT_SIZE; i++) {
      for (j = 0; j < 3; j++) {
        if (reservINT[i] != NULL) {
          if (reservINT[i]->Q[j] == commonDataBus) {
            // If instruction in RS depends on data from CDB, clear Q[j]
            reservINT[i]->Q[j] = NULL;
          }
        }
      }
      if (reservINT[i] == commonDataBus) {
        // If RS entry is the instruction on the CDB, flush it
        reservINT[i] = NULL;
      }
    }

    // Check for dependencies in FP RS
    for (i = 0; i < RESERV_FP_SIZE; i++) {
      for (j = 0; j < 3; j++) {
        if (reservFP[i] != NULL) {
          if (reservFP[i]->Q[j] == commonDataBus) {
            // If instruction in RS depends on data from CDB, clear Q[j]
            reservFP[i]->Q[j] = NULL;
          }
        }
      }
      if (reservFP[i] == commonDataBus) {
        // If RS entry is the instruction on the CDB, flush it
        reservFP[i] = NULL;
      }
    }

    // Clear INT FUs
    for (i = 0; i < FU_INT_SIZE; i++) {
      if (fuINT[i] == commonDataBus) {
        // If FU entry is the instruction on the CDB, flush it
        fuINT[i] = NULL;
      }
    }

    // Clear FP FUs
    for (i = 0; i < FU_FP_SIZE; i++) {
      if (fuFP[i] == commonDataBus) {
        // If FU entry is the instruction on the CDB, flush it
        fuFP[i] = NULL;
      }
    }

    // Clear map table
    for (i = 0; i < MD_TOTAL_REGS; i++) {
      if (map_table[i] == commonDataBus) {
        // If map table entry is the instruction on the CDB, flush it
        map_table[i] = NULL;
      }
    }
  }

  // Clear CDB
  commonDataBus = NULL;
}

/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

  // reminders:
  // instruction can start execute on cycle immediately after receiving source value from cdb
  // B in WB on C9 -> A enters EX on C10
  // prioritize oldest instruction
  // str doesn't use CDB

  int i;
  int j;
  instruction_t *oldest_instruction = NULL;

  // INT Functional Unit
  for (i = 0; i < FU_INT_SIZE; i++) {
    if (fuINT[i] != NULL) {
      if (current_cycle >= fuINT[i]->tom_execute_cycle + FU_INT_LATENCY) {
        // indicates that the execution is done for the instruction
        if (WRITES_CDB(fuINT[i]->op)) {
          // instruction uses CDB
          // check for the oldest instruction that is ready to be broadcast
          if (oldest_instruction == NULL) {
            oldest_instruction = fuINT[i];
          }
          else if (fuINT[i]->index < oldest_instruction->index) {
            oldest_instruction = fuINT[i];
          }
        }
        else {
          // instruction does not use CDB, can clear entries now that execution is complete
          // deallocate current instruction in reservation station
          for (j = 0; j < RESERV_INT_SIZE; j++) {
            if (reservINT[j] == fuINT[i]) {
              reservINT[j] = NULL;
            }
          }
          // assign 0 to cdb cycle and deallocate current instruction in functional unit
          fuINT[i]->tom_cdb_cycle = 0;
          fuINT[i] = NULL;
        }
      }
    }
  }

  // FP Functional Unit
  for (i = 0; i < FU_FP_SIZE; i++) {
    if (fuFP[i] != NULL) {
      if (current_cycle >= fuFP[i]->tom_execute_cycle + FU_FP_LATENCY) {
        // indicates that the execution is done for the instruction
        if (WRITES_CDB(fuFP[i]->op)) {
          // instruction uses CDB
          // check for the oldest instruction that is ready to be broadcast
          if (oldest_instruction == NULL) {
            oldest_instruction = fuFP[i];
          }
          else if (fuFP[i]->index < oldest_instruction->index) {
            oldest_instruction = fuFP[i];
          }
        }
        else {
          // instruction does not use CDB, can clear entries now that execution is complete
          // deallocate current instruction in reservation station
          for (j = 0; j < RESERV_FP_SIZE; j++) {
            if (reservFP[j] == fuFP[i]) {
              reservFP[j] = NULL;
            }
          }
          // assign 0 to cdb cycle and deallocate current instruction in functional unit
          fuFP[i]->tom_cdb_cycle = 0;
          fuFP[i] = NULL;
        }
      }
    }
  }

  // set CDB cycle count
  if (oldest_instruction != NULL) {
    oldest_instruction->tom_cdb_cycle = current_cycle;
  }
  
  // set CDB
  commonDataBus = oldest_instruction;
}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {

  // Check for instructions that have ready registers (all dependencies are resolved)
  // instruction executes in the FU that matches its RS
  for (int i = 0; i < FU_INT_SIZE; i++) {
    instruction_t* oldest_instruction = NULL;

    // iterate over integer RS to identify instructions ready to execute
    for (int j = 0; j < RESERV_INT_SIZE; j++) {
      // instruction is ready if Q[j] is NULL for all inputs (no unresolved dependencies), it is not already in execute stage and it is valid instruction
      if ((reservINT[j] != NULL) && !reservINT[j]->tom_execute_cycle && !reservINT[j]->Q[0] && !reservINT[j]->Q[1] && !reservINT[j]->Q[2]) {
        // if several instructions are ready, prioritize the oldest instruction first
        if (!oldest_instruction || reservINT[j]->index < oldest_instruction->index) {
          oldest_instruction = reservINT[j];  // assign oldest instruction
        }
      }
    }
    // only execute if there is a FU available and oldest instruction is valid
    if (fuINT[i] == NULL) {
      if (oldest_instruction != NULL) {
        fuINT[i] = oldest_instruction;  // assigns oldest instruction a FU
        oldest_instruction->tom_execute_cycle = current_cycle;  // moves oldest ready instruction into execute stage
      }
    }
  }

  // Check for ready floating-point instructions
  for (int i = 0; i < FU_FP_SIZE; i++) {
    instruction_t* oldest_instruction = NULL;

    for (int j = 0; j < RESERV_FP_SIZE; j++) {
      if ((reservFP[j] != NULL) && !reservFP[j]->tom_execute_cycle && !reservFP[j]->Q[0] && !reservFP[j]->Q[1] && !reservFP[j]->Q[2]) {
        if (!oldest_instruction || reservFP[j]->index < oldest_instruction->index) {
          oldest_instruction = reservFP[j];
        }
      }
    }
    if (fuFP[i] == NULL) {
      if (oldest_instruction != NULL) {
        fuFP[i] = oldest_instruction;
        oldest_instruction->tom_execute_cycle = current_cycle;
      }
    }
  }
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

  if (instr_queue_size == 0) {
      return;  // Nothing in IFQ
  }

  instruction_t* instruction = instr_queue[0];
  if (instruction == NULL) {
    return;    // Invalid instruction
  }

  // Conditional and unconditional branches are NOT dispatched to RS and do NOT use any FU
  // Update dispatch cycle to include branch instruction but remove the instructions from occupying any subsequent stages
  if (IS_COND_CTRL(instruction->op) || IS_UNCOND_CTRL(instruction->op)) {
    instr_queue_size--;
    for (int i = 0; i < INSTR_QUEUE_SIZE; i++) {
      instr_queue[i] = instr_queue[i + 1];    // shift the entries 
    }
    instr_queue[INSTR_QUEUE_SIZE - 1] = NULL; // set the last entry to NULL
    return;
  }

  // Check for free RS based on instruction type
  bool dispatched = FALSE;

  // USES_INT_FU covers memory instructions (load/store) also
  if (USES_INT_FU(instruction->op)) {
    for (int i = 0; i < RESERV_INT_SIZE; i++) {
      if (reservINT[i] == NULL) {
        // Assign instruction to integer RS that is free (NULL)
        instruction->tom_issue_cycle = current_cycle;
        reservINT[i] = instruction;   // assign the instruction to that specific RS
        dispatched = TRUE;            // set dispatched flag
        break;
      }
    }
  } else if (USES_FP_FU(instruction->op)) {
    for (int i = 0; i < RESERV_FP_SIZE; i++) {
      if (reservFP[i] == NULL) {
        // Assign instruction to floating-point RS that is free (NULL)
        instruction->tom_issue_cycle = current_cycle;
        reservFP[i] = instruction;
        dispatched = TRUE;
        break;
      }
    }
  }

  // Update dependencies and instruction queue if dispatched
  if (dispatched) {
    instr_queue_size--;   // Remove instruction from issue queue
    for (int i = 0; i < INSTR_QUEUE_SIZE; i++) {
      instr_queue[i] = instr_queue[i + 1];  // shift the entries after the instruction is dispatched
    }
    instr_queue[INSTR_QUEUE_SIZE - 1] = NULL;

    // Stall if there are RAW dependencies (3 input registers)
    for (int i = 0; i < 3; i++) {
      // check if the instruction's input register is accessible and if it is in the map table (it is already being used)
      if (instruction->r_in[i] != DNA && map_table[instruction->r_in[i]] != NULL) {
        instruction->Q[i] = map_table[instruction->r_in[i]];  // if the register is in the map table then set that register as a tag (Qj,Qk)
      }
    }

    // Update map table for output dependencies
    for (int i = 0; i < 2; i++) {
      // check if the instruction's output register is accessible
      if (instruction->r_out[i] != DNA) {
        // add that instruction to the map table and map it to the output register
        map_table[instruction->r_out[i]] = instruction;
      }
    }
  }
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {

  // Check if space in the IFQ and if more instructions to fetch
  if (instr_queue_size >= INSTR_QUEUE_SIZE || fetch_index >= sim_num_insn) {
      return;
  }

  // Skip over TRAP instructions in the trace
  instruction_t* instruction = NULL;
  do {
      fetch_index++;  // no instruction 0 or cycle 0, begins at 1
      instruction = get_instr(trace, fetch_index);
  } while (IS_TRAP(instruction->op));

  // Add fetched instruction to IFQ if queue not full and is valid instruction
  for (int i = 0; i < INSTR_QUEUE_SIZE; i++) {
    if (instr_queue[i] == NULL) {
      instr_queue[i] = instruction;
      instr_queue_size++;
      break;
    }
  }
}

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {
  fetch(trace);

  if (instr_queue_size == 0) {
    return;
  }

  for (int i = 0; i < instr_queue_size; i++) {
    if (instr_queue[i] != NULL) {
      if (instr_queue[i]->tom_dispatch_cycle == 0) {
        instr_queue[i]->tom_dispatch_cycle = current_cycle;
      }
    }
  }
}

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }
  
  int cycle = 1;
  while (true) {
     CDB_To_retire(cycle);
     execute_To_CDB(cycle);
     issue_To_execute(cycle);
     dispatch_To_issue(cycle);
     fetch_To_dispatch(trace, cycle);
     cycle++;

     if (is_simulation_done(sim_num_insn))
        break;
  }
  return cycle;
}
