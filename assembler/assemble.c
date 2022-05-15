#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINELENGTH 1000
#define NOEXIST 0xF0000000

typedef int word_t;
enum symType {LABEL, DATA};
enum instType {RTYPE, ITYPE, JTYPE, OTYPE};

struct symbol{
  char name[MAXLINELENGTH];
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

static struct symbol *label0;
static struct symbol *data0;
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

instruction translate(int, char*, char*, char*, char*);
struct symbol* addLabel(char*, word_t);
struct symbol* addData(char*, char*);
word_t  readLabel(char*);
word_t  readData(char*);
void    checkLabels();
void    checkData();
void    freeSymbols();
int     readAndParse(FILE *, char *, char *, char *, char *, char *);
int     isNumber(char *);

///////////////////////////////////////////////////////////
//                      main start                       //
///////////////////////////////////////////////////////////
int main(int argc, char *argv[]){
  char *inFileString, *outFileString;
  FILE *inFilePtr, *outFilePtr;
  char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH], arg1[MAXLINELENGTH], arg2[MAXLINELENGTH];
  int pc;
  instruction inst;

  if(argc != 3){
    printf("error: usage: %s <assembly-code-file> <machine-code-file>\n", argv[0]);
    exit(1);
  }

  inFileString = argv[1];
  outFileString = argv[2];
  inFilePtr = fopen(inFileString, "r");
  if(inFilePtr == NULL){
    printf("error in opening %s\n", inFileString);
    exit(1);
  }
  outFilePtr = fopen(outFileString, "w");
  if(outFilePtr == NULL) {
    printf("error in opening %s\n", outFileString);
    exit(1); 
  }

  // 1. First pass: calculate the address for every symbolic label
  pc = 0;
  while(readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)){
    if(strlen(label) > 0){
      addLabel(label, pc);
      if(!strcmp(opcode, ".fill")){
        addData(label, arg0);
      }
    }
    pc++;
  }
  rewind(inFilePtr);

  // 2. Second pass: generate a machine-language instruction (in decimal)
  pc = 0;
  while(readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)){
    if(!strcmp(opcode, ".fill")){
      inst.x32 = readData(label);
    } else {
      inst = translate(pc, opcode, label, arg1, arg2);
    }
    fprintf(outFilePtr, "(address %d): %d (hex 0x%x)\n", pc, inst.x32, inst.x32);
    pc++;
  }
  freeSymbols();
  return (0); 
}
///////////////////////////////////////////////////////////
//                      main end                         //
///////////////////////////////////////////////////////////

struct symbol* __addSymbol(char name[], word_t word, enum symType type){
  struct symbol **entry, **itr;
  struct symbol *sym, *prev;

  entry = type == LABEL ? &label0 : &data0;

  prev = 0;
  itr = entry;
  sym = *itr;
  while(sym != 0){
    prev = sym;
    itr = &sym->next;
    sym = *itr;
  }
  
  *itr = (struct symbol*)malloc(sizeof(struct symbol));
  sym = *itr;
  strncpy(sym->name, name, MAXLINELENGTH);
  sym->word = word;
  sym->next = 0;
  if(prev) prev->next = sym;
  
  return sym;
}
word_t __readSymbol(char name[], enum symType type){
  struct symbol *entry, *itr;

  entry = type == LABEL ? label0 : data0;

  for(itr = entry; itr != 0; itr = itr->next){
    if(!strcmp(name, itr->name))
      return itr->word;
  }
  return NOEXIST;
}
word_t __checkSymbols(enum symType type){
  struct symbol *entry, *itr;

  entry = type == LABEL ? label0 : data0;
  itr = entry;
  while(itr != 0){
    printf("%s: %d\n", itr->name, itr->word);
    itr = itr->next;
  }
}
void __freeSymbols(enum symType type){
  struct symbol *entry, *itr, *nxt;

  entry = type == LABEL ? label0 : data0;
  while(itr != 0){
    nxt = itr->next;
    free(itr);
    itr = nxt;
  }
}

struct symbol* addLabel(char name[], word_t word){
  return __addSymbol(name, word, LABEL);
}
struct symbol* addData(char name[], char data[]){
  word_t word;
  word = isNumber(data) ? atoi(data) : __readSymbol(data, LABEL);
  return word != NOEXIST ? __addSymbol(name, word, DATA) : 0;
}
word_t readLabel(char name[]){
  return __readSymbol(name, LABEL);
}
word_t readData(char name[]){
  return __readSymbol(name, DATA);
}
void checkLabels(){
  __checkSymbols(LABEL);
}
void checkData(){
  __checkSymbols(DATA);
}
void freeSymbols(){
  __freeSymbols(LABEL);
  __freeSymbols(DATA);
}
///////////////////////////////////////////////////////////
instruction translate(int pc, char opcode[], char arg0[], char arg1[], char arg2[]){
  int i, sz, sym, zero = 0;
  instruction inst;

  sz = sizeof(isa)/sizeof(isa[0]);
  for(i = 0; i < sz; i++){
    if(!strcmp(isa[i].name, opcode)){
      switch(isa[i].format){
        case RTYPE:
          inst.r.unused  = zero;
          inst.r.opcode  = isa[i].opcode;
          inst.r.regA    = atoi(arg0);
          inst.r.regB    = atoi(arg1);
          inst.r.unused2 = zero;
          inst.r.destReg = atoi(arg2);
          break;
        case ITYPE:
          inst.i.unused = zero;
          inst.i.opcode = isa[i].opcode;
          inst.i.regA   = atoi(arg0);
          inst.i.regB   = atoi(arg1);
          if(isNumber(arg2)){
            inst.i.offset = atoi(arg2);
          } else if((sym = readLabel(arg2)) != NOEXIST){
            inst.i.offset = strcmp(opcode, "beq") == 0 ?
                              sym - pc - 1 : sym;
          } else {
            printf("The symbol does not exist!\n");
            inst.x32 = 0;
          }
          break;
        case JTYPE:
          inst.j.unused  = zero;
          inst.j.opcode  = isa[i].opcode;
          inst.j.regA    = atoi(arg0);
          inst.j.regB    = atoi(arg1);
          inst.j.unused2 = zero;
          break;
        case OTYPE:
          inst.o.unused  = zero;
          inst.o.opcode  = isa[i].opcode;
          inst.o.unused2 = zero;
          break;
        default: // .fill
          printf("Not supported Instruction");
          inst.x32 = 0;
      }
    }
  }
  return inst;
}
///////////////////////////////////////////////////////////
/*
 * Read and parse a line of the assembly-language file.
 * Fields are returned in label, opcode, arg0, arg1, arg2
 * (these strings must have memory already allocated to them).
 *
 * Return values:
 *   0 if reached end of file
 *   1 if all went well
 *
 * exit(1) if line is too long.
 */
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
    /* line too long */
    printf("error: line too long\n");
    exit(1);
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

int isNumber(char *string){
  /* return 1 if string is a number */
  int i;
  return( (sscanf(string, "%d", &i)) == 1);
}
