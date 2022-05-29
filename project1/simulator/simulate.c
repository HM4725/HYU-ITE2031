/* LC-2K Instruction-level simulator */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NUMMEMORY 65536 /* maximum number of words in memory */
#define NUMREGS 8 /* number of machine registers */
#define MAXLINELENGTH 1000

// Virtualization datatypes
#define signExtend(v) ({                    \
  const short __unext_v16 = (v);            \
  const int   __ext_v32 = (int)__unext_v16; \
  __ext_v32;})

typedef unsigned int word_t;
enum instType {RTYPE, ITYPE, JTYPE, OTYPE};

typedef struct {
  word_t destReg:  3;
  word_t unused2: 13;
  word_t regB   :  3;
  word_t regA   :  3;
  word_t opcode :  3;
  word_t unused :  7;
} rType;
typedef struct {
  word_t offset : 16;
  word_t regB   :  3;
  word_t regA   :  3;
  word_t opcode :  3;
  word_t unused :  7;
} iType;
typedef struct {
  word_t unused2: 16;
  word_t regB   :  3;
  word_t regA   :  3;
  word_t opcode :  3;
  word_t unused :  7;
} jType;
typedef struct {
  word_t unused2: 22;
  word_t opcode :  3;
  word_t unused :  7;
} oType;
typedef union {
  rType r;
  iType i;
  jType j;
  oType o;
  word_t  x32;
} instruction;

#define OP_ADD   0
#define OP_NOR   1
#define OP_LW    2
#define OP_SW    3
#define OP_BEQ   4
#define OP_JALR  5
#define OP_HALT  6
#define OP_NOOP  7

static struct isa {
  char *name;
  int opcode;
  enum instType format;
} isa[] = {
  {"add",  OP_ADD,  RTYPE},
  {"nor",  OP_NOR,  RTYPE},
  {"lw",   OP_LW,   ITYPE},
  {"sw",   OP_SW,   ITYPE},
  {"beq",  OP_BEQ,  ITYPE},
  {"jalr", OP_JALR, JTYPE},
  {"halt", OP_HALT, OTYPE},
  {"noop", OP_NOOP, OTYPE},
};

typedef struct stateStruct {
  int pc;
  int mem[NUMMEMORY];
  int reg[NUMREGS];
  int numMemory;
  struct {
    int opcode;
    enum instType format;
  } cunit;
} stateType;

// Error handling
#define ER_WRONGUSAGE     0
#define ER_OPENFILE       1
#define ER_WRONGADDRESS   2
#define ER_OUTOFBOUNDMEM  3
#define ER_OUTOFBOUNDREG  5
#define ER_UNRECOGNIZE    6
#define ER_WRITEREG0      7

char* errorMsg[] = {
  [ER_WRONGUSAGE]     "usage: simulate <machine-code file>",
  [ER_OPENFILE]       "error in opening file",
  [ER_WRONGADDRESS]   "error in reading address",
  [ER_OUTOFBOUNDMEM]  "memory address out of bound",
  [ER_OUTOFBOUNDREG]  "register number out of bound",
  [ER_UNRECOGNIZE]    "unrecognized opcode",
  [ER_WRITEREG0]      "illegal write to register 0",
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
int  run(stateType *);
void printState(stateType *);

///////////////////////////////////////////////////////////
//                      main start                       //
///////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  char line[MAXLINELENGTH] = {0,};
  stateType state = {0,};
  FILE *filePtr;
  int instCount;

  if (argc != 2)
    raiseErrorMsg(ER_WRONGUSAGE, argv[0]);

  filePtr = fopen(argv[1], "r");
  if (filePtr == NULL)
    raiseErrorMsg(ER_OPENFILE, argv[1]);

  /* read in the entire machine-code file into memory */
  for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; state.numMemory++) {
    if (sscanf(line, "%d", state.mem+state.numMemory) != 1)
      raiseError(ER_WRONGADDRESS, state.numMemory);
    printf("memory[%d]=%d\n", state.numMemory, state.mem[state.numMemory]);
  }

  instCount = run(&state);

  printf("total of %d instructions executed\n", instCount);
  printf("final state of machine:");
  printState(&state);

  return(0);
}
///////////////////////////////////////////////////////////
//                      main end                         //
///////////////////////////////////////////////////////////

// Register&Memory R/W
static word_t __readReg(stateType *statePtr, word_t reg)
{
  if(reg > NUMREGS)
    raiseError(ER_OUTOFBOUNDREG, reg);
  return statePtr->reg[reg]; 
}

static void __writeReg(stateType *statePtr, word_t reg, word_t data)
{
  if(reg == 0)
    raiseError(ER_WRITEREG0, statePtr->pc);
  statePtr->reg[reg] = data;
}

static word_t __readMem(stateType *statePtr, word_t addr)
{
  if(addr >= statePtr->numMemory)
    raiseError(ER_OUTOFBOUNDMEM, addr);
  return statePtr->mem[addr];
}

static void __writeMem(stateType *statePtr, word_t addr, word_t data)
{
  if(addr >= statePtr->numMemory)
    raiseError(ER_OUTOFBOUNDMEM, addr);
  statePtr->mem[addr] = data;
}

// 5-stages pipeline
typedef struct {
  instruction inst;
} fetchData;

void fetch(stateType *statePtr, fetchData *out)
{
  out->inst.x32 = __readMem(statePtr, statePtr->pc);
  statePtr->pc++;
}

typedef struct {
  word_t rdataA;
  word_t rdataB;
  int    offset; 
  // passed for wb-stage
  word_t destReg;
} decodeData;

int decode(stateType *statePtr, fetchData *in, decodeData *out)
{
  int i, sz;
  instruction *ir;
  int opcode;
  enum instType format;

  ir = &in->inst;
  opcode = ir->o.opcode;
  
  sz = sizeof(isa)/sizeof(isa[0]);
  for(i = 0; i < sz; i++){
    if(isa[i].opcode == opcode){
      statePtr->cunit.opcode = opcode;
      statePtr->cunit.format = format = isa[i].format;
      switch(format){
        case RTYPE:
          out->rdataA = __readReg(statePtr, ir->r.regA);
          out->rdataB = __readReg(statePtr, ir->r.regB);
          out->destReg = ir->r.destReg;
          break;
        case ITYPE:
          out->rdataA = __readReg(statePtr, ir->i.regA);
          out->rdataB = __readReg(statePtr, ir->i.regB);
          out->offset = signExtend(ir->i.offset);
          out->destReg = ir->i.regB;
          break;
        case JTYPE:
          __writeReg(statePtr, ir->j.regB, statePtr->pc);
          out->rdataA = __readReg(statePtr, ir->j.regA);
          break;
        case OTYPE:
          if(opcode == OP_HALT)
            return -1;
          break;
      }
      return 0;
    }
  }
  raiseError(ER_UNRECOGNIZE, opcode);
}

typedef struct {
  word_t address;
  word_t data;
  // passed for wb-stage
  word_t destReg;
} executeData;

void execute(stateType *statePtr, decodeData *in, executeData *out)
{
  out->destReg = in->destReg;
  switch(statePtr->cunit.opcode){
    case OP_ADD:
      out->data = in->rdataA + in->rdataB;
      break;
    case OP_NOR:
      out->data = ~(in->rdataA | in->rdataB);
      break;
    case OP_LW:
      out->address = in->rdataA + in->offset;
      break;
    case OP_SW:
      out->address = in->rdataA + in->offset;
      out->data = in->rdataB;
      break;
    case OP_BEQ:
      if(in->rdataA == in->rdataB)
        statePtr->pc += in->offset;
      break;
    case OP_JALR:
      statePtr->pc = in->rdataA;
      break;
    default:
      break;
  }
}

typedef struct {
  word_t data; 
  // passed for wb-stage
  word_t destReg;
} memoryData;

void memory(stateType *statePtr, executeData *in, memoryData *out)
{
  out->destReg = in->destReg;
  switch(statePtr->cunit.opcode){
    case OP_LW:
      out->data = __readMem(statePtr, in->address);
      break;
    case OP_SW:
      __writeMem(statePtr, in->address, in->data);
      break;
    default:
      out->data = in->data;
      break;
  }
}

void writeback(stateType *statePtr, memoryData *in)
{
  switch(statePtr->cunit.opcode){
    case OP_ADD:
      __writeReg(statePtr, in->destReg, in->data);
      break;
    case OP_NOR:
      __writeReg(statePtr, in->destReg, in->data);
      break;
    case OP_LW:
      __writeReg(statePtr, in->destReg, in->data);
      break;
    default:
      break;
  }
}

int run(stateType *statePtr)
{
  int instCount = 0;
  fetchData fd = {0,};
  decodeData dd = {0,};
  executeData ed = {0,};
  memoryData md = {0,};

  while(1) {
    instCount++;
    printState(statePtr);
    fetch(statePtr, &fd);
    if(decode(statePtr, &fd, &dd) < 0)
      break;
    execute(statePtr, &dd, &ed);
    memory(statePtr, &ed, &md);
    writeback(statePtr, &md);
  }
  printf("machine halted\n");

  return instCount;
}

// Print state helper
void printState(stateType *statePtr)
{
  int i;
  printf("\n@@@\nstate:\n");
  printf("\tpc %d\n", statePtr->pc); printf("\tmemory:\n");
  for (i=0; i<statePtr->numMemory; i++) {
    printf("\t\tmem[ %d ] %d\n", i, statePtr->mem[i]);
  }
  printf("\tregisters:\n");
  for (i=0; i<NUMREGS; i++) {
    printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
  }
  printf("end state\n");
}

