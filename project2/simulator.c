/* LC-2K Instruction-level simulator */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINELENGTH 1000
#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 /* JALR will not implemented for this project */
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

typedef struct IFIDStruct {
	int instr;
	int pcPlus1;
} IFIDType;

typedef struct IDEXStruct {
	int instr;
	int pcPlus1;
	int readRegA;
	int readRegB;
	int offset;
} IDEXType;

typedef struct EXMEMStruct {
	int instr;
	int branchTarget;
	int aluResult;
	int readRegB;
} EXMEMType;

typedef struct MEMWBStruct {
	int instr;
	int writeData;
} MEMWBType;

typedef struct WBENDStruct {
	int instr;
	int writeData;
} WBENDType;

typedef struct stateStruct {
	int pc;
	int instrMem[NUMMEMORY];
	int dataMem[NUMMEMORY];
	int reg[NUMREGS];
	int numMemory;
	IFIDType IFID;
	IDEXType IDEX;
	EXMEMType EXMEM;
	MEMWBType MEMWB;
	WBENDType WBEND;
	int cycles; /* number of cycles run so far */
} stateType;

////

// Sign Extend
#define convertNum(v) ({                    \
  const short __unext_v16 = (v);            \
  const int   __ext_v32 = (int)__unext_v16; \
  __ext_v32;})

// Error handling
#define ER_WRONGUSAGE     0
#define ER_OPENFILE       1
#define ER_WRONGADDRESS   2
#define ER_OUTOFBOUNDMEM  3

char* errorMsg[] = {
  [ER_WRONGUSAGE]     "usage: simulate <machine-code file>",
  [ER_OPENFILE]       "error in opening file",
  [ER_WRONGADDRESS]   "error in reading address",
  [ER_OUTOFBOUNDMEM]  "memory address out of bound",
};

#ifdef _DEBUG
#define raiseError(code, data)                          \
  do {                                                  \
    fprintf(stderr, "[ERROR] [%s:%d] %s -> %d\n",       \
            __FILE__, __LINE__, errorMsg[code], data);  \
    exit(1);                                            \
  } while(0)
#else
#define raiseError(code, data)              \
  do {                                      \
    fprintf(stderr, "[ERROR] %s -> %d\n",   \
            (errorMsg[code]), (data));      \
    exit(1);                                \
  } while(0);
#endif
#define raiseErrorMsg(code, msg)            \
  do {                                      \
    fprintf(stderr, "[ERROR] %s -> %s\n",   \
            (errorMsg[code]), (msg));       \
    exit(1);                                \
  } while(0);

// Function declarations
void run(stateType*);
void printState(stateType*);
int field0(int);
int field1(int);
int field2(int);
int opcode(int);
void printInstruction(int);

///////////////////////////////////////////////////////////
//                      main start                       //
///////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  char line[MAXLINELENGTH] = {0,};
  FILE *filePtr;
  int instCount;
  stateType state;
  int mem;

  if (argc != 2)
    raiseErrorMsg(ER_WRONGUSAGE, argv[0]);

  filePtr = fopen(argv[1], "r");
  if (filePtr == NULL)
    raiseErrorMsg(ER_OPENFILE, argv[1]);

  /* read in the entire machine-code file into memory */
  for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; state.numMemory++) {
    if (sscanf(line, "%d", &mem) != 1)
      raiseError(ER_WRONGADDRESS, state.numMemory);
    state.instrMem[state.numMemory] = mem;
    state.dataMem[state.numMemory] = mem;
    printf("memory[%d]=%d\n", state.numMemory, mem);
  }
  printf("%d memory words\n", state.numMemory);
  printf("\tinstruction memory:\n");
  for(int i = 0; i < state.numMemory; i++){
    printf("\t\tinstrMem[ %d ] ", i);
    printInstruction(state.instrMem[i]);
  }

  run(&state);

  return(0);
}
///////////////////////////////////////////////////////////
//                      main end                         //
///////////////////////////////////////////////////////////

// Initialize State
void __initState(stateType *statePtr, const stateType *prototype)
{
  statePtr->pc = 0;
  memcpy(statePtr->instrMem, prototype->instrMem, sizeof(int)*NUMMEMORY);
  memcpy(statePtr->dataMem, prototype->dataMem, sizeof(int)*NUMMEMORY);
  statePtr->numMemory = prototype->numMemory;
  memset(statePtr->reg, 0, sizeof(int)*NUMREGS);
  statePtr->IFID.instr = NOOPINSTRUCTION;
  statePtr->IDEX.instr = NOOPINSTRUCTION;
  statePtr->EXMEM.instr = NOOPINSTRUCTION;
  statePtr->WBEND.instr = NOOPINSTRUCTION;
}

// 5-stages Pipeline
void fetch(stateType *newStatePtr, const stateType *statePtr)
{
  if(statePtr->pc < 0 || statePtr->pc >= NUMMEMORY)
    raiseError(ER_OUTOFBOUNDMEM, statePtr->pc);
  newStatePtr->IFID.instr = statePtr->instrMem[statePtr->pc];
  newStatePtr->IFID.pcPlus1 = statePtr->pc + 1;
  newStatePtr->pc = statePtr->pc + 1;
}

void decode(stateType *newStatePtr, const stateType *statePtr)
{
  int instr = statePtr->IFID.instr;
  int regA = field0(instr);
  int regB = field1(instr);

  newStatePtr->IDEX.pcPlus1 = statePtr->IFID.pcPlus1;
  newStatePtr->IDEX.instr = instr;
  newStatePtr->IDEX.readRegA = regA == 0 ?
    0 : statePtr->reg[field0(instr)];
  newStatePtr->IDEX.readRegB = regB == 0 ?
    0 : statePtr->reg[field1(instr)];
  newStatePtr->IDEX.offset = convertNum(field2(instr));
}

void execute(stateType *newStatePtr, const stateType *statePtr)
{
  int instr = statePtr->IDEX.instr;

  newStatePtr->EXMEM.instr = instr;
  newStatePtr->EXMEM.branchTarget = \
    statePtr->IDEX.pcPlus1 + statePtr->IDEX.offset;
  newStatePtr->EXMEM.readRegB = statePtr->IDEX.readRegB;

  switch(opcode(instr)){
    case ADD:
      newStatePtr->EXMEM.aluResult = \
        statePtr->IDEX.readRegA + statePtr->IDEX.readRegB;
      break;
    case NOR:
      newStatePtr->EXMEM.aluResult = \
        ~(statePtr->IDEX.readRegA | statePtr->IDEX.readRegB);
      break;
    case LW:
    case SW:
      newStatePtr->EXMEM.aluResult = \
        statePtr->IDEX.readRegA + statePtr->IDEX.offset;
      break;
    case BEQ:
      newStatePtr->EXMEM.aluResult = \
        statePtr->IDEX.readRegA - statePtr->IDEX.readRegB;
      break;
    default:
      break;
  }
}

void memory(stateType *newStatePtr, const stateType *statePtr)
{
  int instr = statePtr->EXMEM.instr;
  int aluResult = statePtr->EXMEM.aluResult;

  newStatePtr->MEMWB.instr = instr;

  switch(opcode(instr)){
    case ADD:
    case NOR:
      newStatePtr->MEMWB.writeData = aluResult;
      break;
    case LW:
      if(aluResult < 0 || aluResult >= NUMMEMORY)
        raiseError(ER_OUTOFBOUNDMEM, aluResult);
      newStatePtr->MEMWB.writeData = statePtr->dataMem[aluResult];
      break;
    case SW:
      if(aluResult < 0 || aluResult >= NUMMEMORY)
        raiseError(ER_OUTOFBOUNDMEM, aluResult);
      newStatePtr->dataMem[aluResult] = statePtr->EXMEM.readRegB;
      break;
    case BEQ:
      if(statePtr->EXMEM.aluResult == 0){
        newStatePtr->pc = statePtr->EXMEM.branchTarget;
        newStatePtr->IFID.instr = NOOPINSTRUCTION;
        newStatePtr->IDEX.instr = NOOPINSTRUCTION;
        newStatePtr->EXMEM.instr = NOOPINSTRUCTION;
      }
    default:
      break;
  }
}

void writeback(stateType *newStatePtr, const stateType *statePtr)
{
  int instr = statePtr->MEMWB.instr;
  int writeData = statePtr->MEMWB.writeData;
  int destReg;

  newStatePtr->WBEND.instr = instr;
  newStatePtr->WBEND.writeData = writeData;

  switch(opcode(instr)){
    case ADD:
    case NOR:
      destReg = (instr & 0x7);
      newStatePtr->reg[destReg] = writeData;
      break;
    case LW:
      destReg = field1(instr);
      newStatePtr->reg[destReg] = writeData;
      break;
    default:
      break;
  }
}

// Main Run Method
void run(stateType *prototype)
{
  stateType state = {0,};
  stateType newState = {0,};

  __initState(&state, prototype);

  while (1) {
    printState(&state);

	/* check for halt */
	if (opcode(state.MEMWB.instr) == HALT) {
		printf("machine halted\n");
		printf("total of %d cycles executed\n", state.cycles);
		exit(0);
	}

	newState = state;
	newState.cycles++;

	/* --------------------- IF stage --------------------- */
    fetch(&newState, &state);

	/* --------------------- ID stage --------------------- */
    decode(&newState, &state);

	/* --------------------- EX stage --------------------- */
    execute(&newState, &state);

	/* --------------------- MEM stage --------------------- */
    memory(&newState, &state);

	/* --------------------- WB stage --------------------- */
    writeback(&newState, &state);

	state = newState; /* this is the last statement before end of the loop.
			It marks the end of the cycle and updates the
			current state with the values calculated in this
			cycle */
  }
}

// Print state helper
void
printState(stateType *statePtr)
{
    int i;
    printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
    printf("\tpc %d\n", statePtr->pc);

    printf("\tdata memory:\n");
	for (i=0; i<statePtr->numMemory; i++) {
	    printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
	}
    printf("\tregisters:\n");
	for (i=0; i<NUMREGS; i++) {
	    printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
	}
    printf("\tIFID:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IFID.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
    printf("\tIDEX:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IDEX.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
	printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
	printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
	printf("\t\toffset %d\n", statePtr->IDEX.offset);
    printf("\tEXMEM:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->EXMEM.instr);
	printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
	printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
	printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
    printf("\tMEMWB:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->MEMWB.instr);
	printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
    printf("\tWBEND:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->WBEND.instr);
	printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
}

int
field0(int instruction)
{
	return( (instruction>>19) & 0x7);
}

int
field1(int instruction)
{
	return( (instruction>>16) & 0x7);
}

int
field2(int instruction)
{
	return(instruction & 0xFFFF);
}

int
opcode(int instruction)
{
	return(instruction>>22);
}

void
printInstruction(int instr)
{
	
	char opcodeString[10];
	
	if (opcode(instr) == ADD) {
		strcpy(opcodeString, "add");
	} else if (opcode(instr) == NOR) {
		strcpy(opcodeString, "nor");
	} else if (opcode(instr) == LW) {
		strcpy(opcodeString, "lw");
	} else if (opcode(instr) == SW) {
		strcpy(opcodeString, "sw");
	} else if (opcode(instr) == BEQ) {
		strcpy(opcodeString, "beq");
	} else if (opcode(instr) == JALR) {
		strcpy(opcodeString, "jalr");
	} else if (opcode(instr) == HALT) {
		strcpy(opcodeString, "halt");
	} else if (opcode(instr) == NOOP) {
		strcpy(opcodeString, "noop");
	} else {
		strcpy(opcodeString, "data");
    }
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
		field2(instr));
}

