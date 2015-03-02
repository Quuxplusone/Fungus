
#include <stdio.h>
#include <stdlib.h>
#include "fungus.h"
#include "uint18.h"

#include "fungal.h" /* for debugging purposes */

#define MSR_HCON    040
#define MSR_HCAND   041
#define MSR_HCOR    042
#define MSR_OSEC    043
#define MSR_ISTACK  050
#define MSR_DISTK   051
#define MSR_IRET    052
#define MSR_TICKS   070

int DebugPrint = 0;

enum MaskingModes currentMode;
uint18 register_file[8];
uint18 & PC = register_file[1];
uint18 & DeltaPC = register_file[2];
uint18 & TDeltaPC = register_file[6];
uint18 & TPC = register_file[7];


static uint18 HCON, HCAND, HCOR;  /* hardware context MSRs */
static uint18 OSEC;       /* operating-system security MSR */
static uint18 ISTACK;     /* async interrupt stack pointer */
static uint18 DISTK;      /* async interrupt stack delta */
uint18 memory[0777+1][0777+1];

unsigned int cycle;  /* hardware cycle counter */


extern "C" void run(void);
 extern "C" void step(void);
  static void SetMaskingMode(int M);
  static void group0(unsigned int inst);
  static void group1(unsigned int inst);
   static uint18 readMSR(int R);
   static void writeMSR(int R, uint18 value);
  static uint18 nazg(int ALU, int A, int B);
   static uint18 nazgUnary(int B, int A);
  static void inspect(unsigned int X);
static void (*exceptionHandler)(unsigned int inst);
static void UndefinedException(void);
static void InfiniteLoopException(void);
static void AssignToZeroException(void);
static void async_interrupt(unsigned int where);
static void async_iret(void);

extern "C" void initMachine(void)
{
    HCON = uint18(0);
    HCAND = uint18(0777,0777);
    HCOR = uint18(0,0);
    OSEC = uint18(0);
    ISTACK = uint18(0,0);
    DISTK = uint18(0,3);
    cycle = 0;
}

extern "C" void initRAM(const char *rambuffer, int width, int height)
{
    initmem(rambuffer, 0, 0, width, height);
}

extern "C" void initmem(const char *buffer, int sx, int sy, int width, int height)
{
    int i, j;
    int k = 0;
    for (j = sy; j < sy+height; ++j) {
        for (i = sx; i < sx+width; ++i) {
            memory[i][j].setx(buffer[k++]);
            memory[i][j].sety(0);
        }
    }
}

extern "C" void initROMw(const unsigned int *rombuffer, int width, int height)
{
    initmemw(rombuffer, 01000 - height, 0, width, height);
}

extern "C" void initmemw(const unsigned int *buffer, int sx, int sy, int width, int height)
{
    int i, j;
    int k = 0;
    for (j = sy; j < sy+height; ++j)
      for (i = sx; i < sx+width; ++i)
        memory[i][j] = uint18(buffer[k++]);
}


extern "C" void run(void)
{
    while (1)
      step();
}

extern "C" void step(void)
{
    currentMode = MaskVector;
    PC += DeltaPC;  /* increment the PC */
    if (HCON) PC = (PC & HCAND) | HCOR;

    unsigned int inst = (unsigned int)memory[PC.getx()][PC.gety()];
    int G = (inst >> 17) & 01;
    int M = (inst >> 15) & 03;
    int OP = (inst >> 9) & 07;
    SetMaskingMode(M);

    if (DebugPrint >= 3 || (DebugPrint >= 2 && inst < 01000)) {
        printf("PC=(%03o,%03o) DPC=(%03o,%03o) I=%03o:%03o   %s\n",
            PC.getx(), PC.gety(), DeltaPC.getx(), DeltaPC.gety(),
            (inst>>9)&0777, (inst & 0777), disasm(inst));
    }

    int OSEC_mask = (G << 3) | OP;
    if (HCON && ((unsigned int)OSEC & (1 << OSEC_mask)))
      async_interrupt(0777);
    else
      (G ? group1 : group0)(inst);
}

static void SetMaskingMode(int M)
{
    enum MaskingModes lookup[] = {
        MaskVector, MaskX, MaskY, MaskScalar
    };
    currentMode = lookup[M];
    return;
}

static void hconfy(uint18 & x)
{
    if (HCON) {
        enum MaskingModes saved = currentMode;
        currentMode = MaskVector;
        x &= HCAND;
        x |= HCOR;
        currentMode = saved;
    }
}


static void async_interrupt(unsigned int where)
{
    unsigned int ix = ISTACK.getx();
    unsigned int iy = ISTACK.gety();
    unsigned int dx = DISTK.getx();
    unsigned int dy = DISTK.gety();
    ix += dx;
    iy += dy;
    ISTACK = uint18(iy, ix);
    HCON.sety(0);
    memory[ix-1 & 0777][iy-1 & 0777] = register_file[1];
    memory[ix+0 & 0777][iy-1 & 0777] = register_file[2];
    memory[ix-1 & 0777][iy+0 & 0777] = register_file[3];
    memory[ix+0 & 0777][iy+0 & 0777] = register_file[4];
    memory[ix+1 & 0777][iy+0 & 0777] = register_file[5];
    memory[ix+0 & 0777][iy+1 & 0777] = register_file[6];
    memory[ix+1 & 0777][iy+1 & 0777] = register_file[7];
    TPC = PC; TDeltaPC = DeltaPC;
    PC = uint18(0777, where);
    DeltaPC = uint18(-1,0);
    cycle += 12;
}

static void async_iret(void)
{
    unsigned int ix = ISTACK.getx();
    unsigned int iy = ISTACK.gety();
    unsigned int dx = DISTK.getx();
    unsigned int dy = DISTK.gety();
    uint18 m;
    m = uint18(ix-1,iy-1); hconfy(m); register_file[1] = memory[m.getx()][m.gety()];
    m = uint18(ix+0,iy-1); hconfy(m); register_file[2] = memory[m.getx()][m.gety()];
    m = uint18(ix-1,iy+0); hconfy(m); register_file[3] = memory[m.getx()][m.gety()];
    m = uint18(ix+0,iy+0); hconfy(m); register_file[4] = memory[m.getx()][m.gety()];
    m = uint18(ix+1,iy+0); hconfy(m); register_file[5] = memory[m.getx()][m.gety()];
    m = uint18(ix+0,iy+1); hconfy(m); register_file[6] = memory[m.getx()][m.gety()];
    m = uint18(ix+1,iy+1); hconfy(m); register_file[7] = memory[m.getx()][m.gety()];
    ix -= dx;
    iy -= dy;
    ISTACK = uint18(iy, ix);
    HCON.sety(1);
    cycle += 8;
}


static void group0(unsigned int inst)
{
    unsigned int OP = (inst >> 12) & 07;
    unsigned int X = (inst >> 9) & 07;
    unsigned int L = (inst >> 0) & 0777;
    switch (OP) {
        case 0: {  /* TRP, 0XX 000 XXX LLLLLLLLL */
            if (PC == uint18(L,-1))
              InfiniteLoopException();
            TPC = PC; TDeltaPC = DeltaPC;
            HCON.sety(0);
            PC = uint18(L,0);
            DeltaPC = uint18(0,-1);
            cycle += 8;
            break;
        }
        case 1: {  /* LI, 0mm 001 xxx LLLLLLLLL */
            if (X == 0 && L != 0 && currentMode != MaskY)
              AssignToZeroException();
            register_file[X].setm(uint18(L,0));
            if (X == 1)
              hconfy(PC);
            cycle += 4;
            break;
        }
        case 2: {  /* LV, 0mm 010 xxx LLLLLLLLL */
            if (X == 0 && L != 0)
              AssignToZeroException();
            register_file[X].setm(uint18(L,L));
            if (X == 1)
              hconfy(PC);
            cycle += 4;
            break;
        }
        case 3: {  /* SZ, 0mm 011 xxx LLLLLLLLL */
            /* SZ and friends are incorrectly documented as
             * "0mm 011 XXX XXX aaa XXX" in parts of the
             * original paper, but using the "X" field instead
             * of the "A" field makes much more sense. */
            int cc = 0;
            if (currentMode != MaskY && register_file[X].getx())
              cc = 1;
            if (currentMode != MaskX && register_file[X].gety())
              cc = 1;
            if (!cc) {
                currentMode = MaskVector;
                PC += DeltaPC;
                hconfy(PC);
                cycle += 7;
            } else {
                cycle += 6;
            }
            break;
        }
        case 4: {  /* SNZ, 0mm 100 xxx LLLLLLLLL */
            int cc = 0;
            if (currentMode != MaskY && register_file[X].getx())
              cc = 1;
            if (currentMode != MaskX && register_file[X].gety())
              cc = 1;
            if (cc) {
                currentMode = MaskVector;
                PC += DeltaPC;
                hconfy(PC);
                cycle += 7;
            } else {
                cycle += 6;
            }
            break;
        }
        case 5: {  /* DZ, 0mm 101 xxx LLLLLLLLL */
            DeltaPC = uint18(0);
            DeltaPC.setm(-1,-1);
            cycle += 8;
            if (!register_file[X]) {
                DeltaPC.setm(1,1);
                cycle += 1;
            }
            break;
        }
        case 6: {  /* DNZ, 0mm 110 xxx LLLLLLLLL */
            DeltaPC = uint18(0);
            DeltaPC.setm(-1,-1);
            cycle += 8;
            if (register_file[X]) {
                DeltaPC.setm(1,1);
                cycle += 1;
            }
            break;
        }
        case 7: {  /* RET, 0XX 111 XXX XXXXXXXXX */
            /* RET is incorrectly documented as "0XX 001 XX..."
             * in the original paper, but it's clearly intended
             * to fill this otherwise unused instruction space. */
            PC = TPC;
            DeltaPC = TDeltaPC;
            HCON.sety(1);
            cycle += 5;
            inspect(1);
            inspect(2);
            break;
        }
    }
}

static void group1(unsigned int inst)
{
    unsigned int OP = (inst >> 12) & 07;
    unsigned int X = (inst >> 9) & 07;
    unsigned int ALU = (inst >> 6) & 07;
    unsigned int A = (inst >> 3) & 07;
    unsigned int B = (inst >> 0) & 07;
    switch (OP) {
        case 0: {  /* ALU instructions */
            uint18 old_x = register_file[X];
            register_file[X].setm(nazg(ALU, A, B));
            if (X == 1)
              hconfy(PC);
            if (register_file[X] != old_x)
              inspect(X);
            cycle += 4;
            break;
        }
        case 1: {  /* LW */
            uint18 temp = nazg(ALU, A, B);
            hconfy(temp);
            register_file[X] = memory[temp.getx()][temp.gety()];
            if (X == 1)
              hconfy(PC);
            inspect(X);
            cycle += 5;
            break;
        }
        case 2: {  /* LX */
            uint18 temp = nazg(ALU, A, B);
            hconfy(temp);
            register_file[X].setx(memory[temp.getx()][temp.gety()].getx());
            if (X == 1)
              hconfy(PC);
            inspect(X);
            cycle += 5;
            break;
        }
        case 3: {  /* LY */
            uint18 temp = nazg(ALU, A, B);
            hconfy(temp);
            register_file[X].sety(memory[temp.getx()][temp.gety()].gety());
            if (X == 1)
              hconfy(PC);
            inspect(X);
            cycle += 5;
            break;
        }
        case 4: {  /* SW */
            uint18 temp = nazg(ALU, A, B);
            hconfy(temp);
            memory[temp.getx()][temp.gety()] = register_file[X];
            cycle += 5;
            break;
        }
        case 5: {  /* SX */
            uint18 temp = nazg(ALU, A, B);
            hconfy(temp);
            memory[temp.getx()][temp.gety()].setx(register_file[X].getx());
            cycle += 5;
            break;
        }
        case 6: {  /* SY */
            uint18 temp = nazg(ALU, A, B);
            hconfy(temp);
            memory[temp.getx()][temp.gety()].sety(register_file[X].gety());
            cycle += 5;
            break;
        }
        case 7: {  /* undefined by the official spec: LMR, SMR */
            switch (ALU) {
                case 0: {  /* LMR, 1m 111 xxx 000 aaaaaa */
                    uint18 temp = readMSR(inst & 077);
                    register_file[X].setm(temp);
                    if (X == 1)
                      hconfy(PC);
                    inspect(X);
                    cycle += 4;
                    break;
                }
                case 1:    /* SMR, 1m 111 xxx 001 aaaaaa */
                    writeMSR((inst & 077), register_file[X]);
                    cycle += 4;
                    break;
                default:
                    UndefinedException();
                    cycle += 4;
                    break;
            }
            break;
        }
    }
    if ((X == 0) && register_file[0])
      AssignToZeroException();
}

static uint18 nazg(int ALU, int A, int B)
{
    switch (ALU) {
        case 0:
            return register_file[A] + register_file[B];
        case 1:
            return register_file[A] - register_file[B];
        case 2:
            return register_file[A] & register_file[B];
        case 3:
            return register_file[A] | register_file[B];
        case 4:
            return register_file[A] ^ register_file[B];
        case 5:
            UndefinedException();
            break;
        case 6:
            UndefinedException();
            break;
        case 7:
            return nazgUnary(B, A);
    }
    return uint18(0);  // NOTREACHED
}

static uint18 nazgUnary(int B, int A)
{
    switch (B) {
        case 0:
            return ~register_file[A];
        case 1:
            return register_file[A] >> 1u;
        case 2:
            return register_file[A] + uint18(001,001);
        case 3:
            return register_file[A] - uint18(001,001);
        case 4:
            return register_file[A] + uint18(1);
        case 5:
            return register_file[A] - uint18(1);
        case 6:
        case 7:
            UndefinedException();
            break;
    }
    return uint18(0773, 0440);  /* a suitable magic number */
}


static void inspect(unsigned int X)
{
    if (DebugPrint >= 4) {
        printf(" r%u := (%03o,%03o)\n", X,
            register_file[X].getx(), register_file[X].gety());
    }
}

/************** MSR-handling functions and callback registry. ***************/


static unsigned int (*readMSRhandlers[64])(void);
static void (*writeMSRhandlers[64])(int, unsigned int);

extern "C" void onReadMSR(unsigned int msr, unsigned int (*handler)(void))
{
    readMSRhandlers[msr & 077] = handler;
}

extern "C" void onWriteMSR(unsigned int msr, void (*handler)(int, unsigned int))
{
    writeMSRhandlers[msr & 077] = handler;
}

uint18 readMSR(int reg)
{
    if (readMSRhandlers[reg] != NULL)
      return uint18(readMSRhandlers[reg]());
    switch (reg) {
        case MSR_HCON:
            return HCON;
        case MSR_HCAND:
            return HCAND;
        case MSR_HCOR:
            return HCOR;
        case MSR_OSEC:
            return OSEC;
        case MSR_TICKS:
            return uint18(cycle);
        case MSR_IRET:
            return uint18(0);
        case MSR_ISTACK:
            return ISTACK;
        default:
            return uint18(0);
    }
}

void writeMSR(int reg, uint18 value)
{
    if (writeMSRhandlers[reg] != NULL) {
        writeMSRhandlers[reg](currentMode, (unsigned int)value);
        return;
    }
    switch (reg) {
        case MSR_HCON:
            return; /* HCON is read-only. */
        case MSR_HCAND:
            if (!HCON)
              HCAND.setm(value);
            return; /* HCAND is writeable only in kernel mode. */
        case MSR_HCOR:
            if (!HCON)
              HCOR.setm(value);
            return; /* HCOR is writeable only in kernel mode. */
        case MSR_OSEC:
            OSEC.setm(value);
            return;
        case MSR_TICKS:
            return; /* TICKS is read-only. */
        case MSR_ISTACK:
            ISTACK.setm(value);
            return;
        case MSR_IRET:
            async_iret();
            return;
        default:
            return;
    }
}


/********* Exception-handling functions and callback registry. **************/

extern "C" void onException(void (*ex)(unsigned int))
{
    exceptionHandler = ex;
}

static void UndefinedException(void)
{
    if (exceptionHandler != 0) {
        unsigned int inst = (unsigned int)memory[PC.getx()][PC.gety()];
        exceptionHandler(inst);
    } else {
        /* just plain ignore the unknown instruction, and plow ahead */
    }
}

static void InfiniteLoopException(void)
{
    if (exceptionHandler != 0) {
        unsigned int inst = (unsigned int)memory[PC.getx()][PC.gety()];
        exceptionHandler(inst);
    } else {
        /* kill the looping program */
        exit(EXIT_FAILURE);
    }
}

static void AssignToZeroException(void)
{
    if (exceptionHandler != 0) {
        unsigned int inst = (unsigned int)memory[PC.getx()][PC.gety()];
        exceptionHandler(inst);
    } else {
        /* allow the assignment to zero, and plow ahead */
    }
}


extern "C" void setmem(unsigned int x, unsigned int y, unsigned int value)
{ memory[x&0777][y&0777] = uint18(value); }

extern "C" unsigned int readmem(unsigned int x, unsigned int y)
{ return (unsigned int)memory[x&0777][y&0777]; }

extern "C" void setReg(unsigned int which, unsigned int value)
{ register_file[which&7] = uint18(value); }

extern "C" unsigned int readReg(unsigned int which)
{ return (unsigned int)register_file[which&7]; }

