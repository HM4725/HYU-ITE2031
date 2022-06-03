#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINELENGTH 1000
#define MAXLABELSIZE  6
#define MAXINT16 32767
#define MININT16 (-32768)
#define MAXINT32 2147483647
#define MININT32 (-2147483648)

// Types ///////////////////////////////////////////
typedef int word_t;
typedef short half_t;
enum instType {RTYPE, ITYPE, JTYPE, OTYPE};

struct symbol{
  char name[MAXLABELSIZE+2];
  word_t word;
  struct symbol *next;
};

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
typedef union instruction {
  rType r;
  iType i;
  jType j;
  oType o;
  word_t  x32;
} instruction;

// Globals ///////////////////////////////////////////
static struct symbol *entry;
static struct symbol notfound = {
  "404", 0, 0
};
static struct isa {
  char *name;
  int opcode;
  enum instType format;
} isa[] = {
  {"add", 0, RTYPE},
  {"nor", 1, RTYPE},
  {"lw",  2, ITYPE},
  {"sw",  3, ITYPE},
  {"beq", 4, ITYPE},
  {"jalr", 5, JTYPE},
  {"halt", 6, OTYPE},
  {"noop", 7, OTYPE},
};
static int pc;

// Errors ///////////////////////////////////////////
#define ER_WRONGUSAGE   0
#define ER_OPENFILE     1
#define ER_LINEOVFL     2
#define ER_UNRECOGNIZE  3
#define ER_WRONGREG     4
#define ER_INSUFFICIENT 5
#define ER_WORDOVFL     6
#define ER_OFFSETOVFL   7
#define ER_LABELINVALID 8
#define ER_DUPLICATE    9
#define ER_UNDEFINED    10

char* errorMsg[] = {
  [ER_WRONGUSAGE]   "usage: assemble <assembly-code-file> <machine-code-file>",
  [ER_OPENFILE]     "error in opening file",
  [ER_LINEOVFL]     "line too long",
  [ER_UNRECOGNIZE]  "unrecognized opcode",
  [ER_WRONGREG]     "register number must be 0 to 7",
  [ER_INSUFFICIENT] "insufficient instruction",
  [ER_WORDOVFL]     "bounds are -2147483648 to 2147483647 in 32bits",
  [ER_OFFSETOVFL]   "offsetFields that don't fit in 16bits",
  [ER_LABELINVALID] "valid labels contain a maximum of 6 characters and can consist of letters and numbers (but must start with a letter)",
  [ER_DUPLICATE]    "duplicate labels",
  [ER_UNDEFINED]    "use of undefined label",
};

// Functions ///////////////////////////////////////////
instruction translate(int, char*, char*, char*, char*);
void addLabel(char*, word_t);
struct symbol*  readLabel(const char*);
#ifdef _DEBUG
void    checkLabels();
#else
#define checkLabels() {}
#endif
void    freeSymbols();
int     readAndParse(FILE*, char*, char*, char*, char*, char*);
int     isNumber(const char*);
void    raiseError(int, const char*);

///////////////////////////////////////////////////////////
//                      main start                       //
///////////////////////////////////////////////////////////
int main(int argc, char *argv[]){
  char *inFileString, *outFileString;
  FILE *inFilePtr, *outFilePtr;
  char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH], arg1[MAXLINELENGTH], arg2[MAXLINELENGTH];
  instruction inst;

  if(argc != 3){
    raiseError(ER_WRONGUSAGE, argv[0]);
  }

  inFileString = argv[1];
  outFileString = argv[2];
  inFilePtr = fopen(inFileString, "r");
  if(inFilePtr == NULL){
    raiseError(ER_OPENFILE, inFileString);
  }
  outFilePtr = fopen(outFileString, "w");
  if(outFilePtr == NULL) {
    raiseError(ER_OPENFILE, outFileString);
  }

  // 1. First pass: calculate the address for every symbolic label
  pc = 0;
  while(readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)){
    if(opcode[0] == '\0')
      continue;
    if(strlen(label) > 0){
      addLabel(label, pc);
    }
    pc++;
  }
  rewind(inFilePtr);

  checkLabels();
  // 2. Second pass: generate a machine-language instruction (in decimal)
  pc = 0;
  while(readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)){
    if(opcode[0] == '\0')
      continue;
    inst = translate(pc, opcode, arg0, arg1, arg2);
#ifdef _DEBUG
    fprintf(outFilePtr, "(address %d): %d (hex 0x%x)\n", pc, inst.x32, inst.x32);
#else
    fprintf(outFilePtr, "%d\n", inst.x32);
#endif
    pc++;
  }
  freeSymbols();

  exit(0);
}
///////////////////////////////////////////////////////////
//                      main end                         //
///////////////////////////////////////////////////////////


// Definitions ///////////////////////////////////////////
void __addSymbol(char name[], word_t word){
  struct symbol **itr;
  struct symbol *sym, *prev;

  prev = 0;
  itr = &entry;
  sym = *itr;
  while(sym != 0){
    prev = sym;
    itr = &sym->next;
    sym = *itr;
  }
  
  *itr = (struct symbol*)malloc(sizeof(struct symbol));
  sym = *itr;
  strncpy(sym->name, name, MAXLABELSIZE+1);
  sym->word = word;
  sym->next = 0;
  if(prev) prev->next = sym;
}
struct symbol* __readSymbol(const char name[]){
  struct symbol *itr;

  for(itr = entry; itr != 0; itr = itr->next){
    if(!strcmp(name, itr->name))
      return itr;
  }
  return &notfound;
}
#ifdef _DEBUG
void __checkSymbols(){
  struct symbol *itr;

  printf("Symbols : Address\n");
  itr = entry;
  while(itr != 0){
    printf("  %6s: %d\n", itr->name, itr->word);
    itr = itr->next;
  }
}
#endif
void __freeSymbols(){
  struct symbol *itr, *nxt;

  itr = entry;
  while(itr != 0){
    nxt = itr->next;
    free(itr);
    itr = nxt;
  }
}
int __isValidLabel(char name[]){
  if(strlen(name) > MAXLABELSIZE)
    return 0;
  if((name[0] >= 'A' && name[0] <= 'Z')
     || (name[0] >= 'a' && name[0] <= 'z'))
    return 1;
  else
    return 0;
}

void addLabel(char name[], word_t word){
  if(__isValidLabel(name) == 0)
    raiseError(ER_LABELINVALID, name);
  if(__readSymbol(name) != &notfound)
    raiseError(ER_DUPLICATE, name);
  __addSymbol(name, word);
}
struct symbol* readLabel(const char name[]){
  return __readSymbol(name);
}
#ifdef _DEBUG
void checkLabels(){
  __checkSymbols();
}
#endif
void freeSymbols(){
  __freeSymbols();
}

///////////////////////////////////////////////////////////
int __getReg(const char reg[]){
  if(strlen(reg) != 1)
    goto bad;
  if(reg[0] < '0' || reg[0] > '7')
    goto bad;

  return reg[0] - '0';
bad:
  raiseError(ER_WRONGREG, reg);
  return -1;
}
half_t __getOffset(const int pc, const char opcode[], const char arg[]){
  word_t offset;
  long tmp;
  struct symbol *symbol;

  if(isNumber(arg)){
    tmp = atol(arg);
    if(tmp < MININT32 || tmp > MAXINT32)
      raiseError(ER_WORDOVFL, arg);
    offset = (word_t)tmp;
  } else {
    symbol = readLabel(arg);
    if(symbol != &notfound){
      offset = strcmp(opcode, "beq") == 0 ?
                 symbol->word - pc - 1 : symbol->word;
    } else {
      raiseError(ER_UNDEFINED, arg);
    }
  }
  if(offset < MININT16 || offset > MAXINT16){
    raiseError(ER_OFFSETOVFL, arg);
  }
  return (half_t)offset;
}
word_t __getData(const char arg[]){
  word_t data;
  long tmp;
  struct symbol *symbol;

  if(isNumber(arg)){
    tmp = atol(arg);
    if(tmp < MININT32 || tmp > MAXINT32)
      raiseError(ER_WORDOVFL, arg);
    data = (word_t)tmp;
  } else {
    symbol = readLabel(arg);
    if(symbol != &notfound){
      data = symbol->word;
    } else {
      raiseError(ER_UNDEFINED, arg);
    }
  }
  return data;
}
instruction translate(int pc, char opcode[], char arg0[], char arg1[], char arg2[]){
  int i, sz, zero = 0;
  instruction inst = {0,};

  // Case1) Instructions
  sz = sizeof(isa)/sizeof(isa[0]);
  for(i = 0; i < sz; i++){
    if(!strcmp(isa[i].name, opcode)){
      switch(isa[i].format){
        case RTYPE:
          if(arg0[0] == '\0' || arg1[0] == '\0' || arg2[0] == '\0')
            raiseError(ER_INSUFFICIENT, opcode);
          inst.r.unused  = zero;
          inst.r.opcode  = isa[i].opcode;
          inst.r.regA    = __getReg(arg0);
          inst.r.regB    = __getReg(arg1);
          inst.r.unused2 = zero;
          inst.r.destReg = __getReg(arg2);
          return inst;
        case ITYPE:
          if(arg0[0] == '\0' || arg1[0] == '\0' || arg2[0] == '\0')
            raiseError(ER_INSUFFICIENT, opcode);
          inst.i.unused = zero;
          inst.i.opcode = isa[i].opcode;
          inst.i.regA   = __getReg(arg0);
          inst.i.regB   = __getReg(arg1);
          inst.i.offset = __getOffset(pc, opcode, arg2);
          return inst;
        case JTYPE:
          if(arg0[0] == '\0' || arg1[0] == '\0')
            raiseError(ER_INSUFFICIENT, opcode);
          inst.j.unused  = zero;
          inst.j.opcode  = isa[i].opcode;
          inst.j.regA    = __getReg(arg0);
          inst.j.regB    = __getReg(arg1);
          inst.j.unused2 = zero;
          return inst;
        case OTYPE:
          inst.o.unused  = zero;
          inst.o.opcode  = isa[i].opcode;
          inst.o.unused2 = zero;
          return inst;
      }
    }
  }
  // Case2) Assembler directives
  if(!strcmp(opcode, ".fill")){
    if(arg0[0] == '\0')
      raiseError(ER_INSUFFICIENT, opcode);
    inst.x32 = __getData(arg0);
    return inst;
  }

  raiseError(ER_UNRECOGNIZE, opcode);
  return inst;
}

///////////////////////////////////////////////////////////
int readAndParse(FILE *inFilePtr, char *label, char *opcode, char *arg0, char *arg1, char *arg2){
  char line[MAXLINELENGTH];
  char *ptr = line;
  /* delete prior values */
  label[0] = opcode[0] = arg0[0] = arg1[0] = arg2[0] = '\0';
  /* read the line from the assembly-language file */
  if(fgets(line, MAXLINELENGTH, inFilePtr) == NULL) {
     /* reached end of file */
    return(0);
  }
  /* check for line too long (by looking for a \n) */
  if (strchr(line, '\n') == NULL) {
    raiseError(ER_LINEOVFL, line);
  }
  /* is there a label? */
  ptr = line;
  if (sscanf(ptr, "%[^\t\n\r ]", label)) {
    /* successfully read label; advance pointer over the label */
    ptr += strlen(label);
  }
  /*
   * Parse the rest of the line. Would be nice to have real regular
   * expressions, but scanf will suffice.
   */
  sscanf(ptr, "%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]", opcode, arg0, arg1, arg2);
  return(1);
}

int isNumber(const char *string){
  /* return 1 if string is a number */
  int i;
  return( (sscanf(string, "%d", &i)) == 1);
}

///////////////////////////////////////////////////////////
void raiseError(int code, const char msg[]){
  if(code == ER_WRONGUSAGE || code == ER_OPENFILE)
    fprintf(stderr, "[ERROR] %s %s\n", errorMsg[code], msg);
  else
    fprintf(stderr, "[ERROR] address %d: %s (%s)\n", pc, errorMsg[code], msg);
  exit(1);
}

// End //////////////////////////////////////////////////////
