
#ifndef H_FUNGUS
 #define H_FUNGUS

#ifdef __cplusplus
 #include "uint18.h"
 extern uint18 register_file[8];
 extern uint18 memory[0777+1][0777+1];
 extern "C" {
#endif

extern unsigned int cycle;  /* hardware cycle counter */
extern int DebugPrint;  /* bool: print each instruction as it's executed? */

void initMachine(void);
void initmem(const char *buffer, int sx, int sy, int width, int height);
void initmemw(const unsigned int *rombuffer, int sx, int sy, int width, int height);
void initRAM(const char *rambuffer, int width, int height);
void initROMw(const unsigned int *rombuffer, int width, int height);

void run(void);
void step(void);

void onException(void (*handler)(unsigned int inst));
void onReadMSR(unsigned int msr, unsigned int (*handler)(void));
void onWriteMSR(unsigned int msr, void (*handler)(int, unsigned int));


/* Provided for the benefit of C callers. */
void setmem(unsigned int x, unsigned int y, unsigned int value);
unsigned int readmem(unsigned int x, unsigned int y);
void setReg(unsigned int which, unsigned int value);
unsigned int readReg(unsigned int which);

#ifdef __cplusplus
 }
#endif

#endif
