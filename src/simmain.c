
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fungus.h"
#include "fungelf.h"

/* Callbacks for FungELF_load() */
static unsigned int cbgc(int x, int y);
static int cbsc(int x, int y, unsigned int value);
static int cbse(int x, int y);


jmp_buf exceptionCaught;
static void holler(unsigned int inst);
static unsigned int readChar(void);
static void writeChar(int curmode, unsigned int value);
static void programExit(int curmode, unsigned int value);
static void dohelp(int man);


int main(int argc, char **argv)
{
    FILE *bffp = NULL, *kernfp = NULL;
    int i, rc;

    if (argc < 2) dohelp(1);

    if (argc > 2 && !strncmp(argv[1], "-d", 2)) {
        /* Print debugging information during the run. */
        DebugPrint = isdigit(argv[1][2])? (argv[1][2] - '0') : 1;
        --argc;
        ++argv;
    }

    kernfp = fopen(argv[1], "rb");
    if (kernfp == NULL) dohelp(0);

    if (argc == 3) {
        bffp = fopen(argv[2], "r");
        if (bffp == NULL) dohelp(0);
    }


    /* Read a ROM image. If we don't have a ROM image, then
     * the first ASCII instruction in RAM will trap into empty
     * memory, the PC will go into an infinite loop, and the
     * simulator will detect the loop and exit noisily.
     *
     * FungELF_load() reads the bits straight into memory.
     */
    for (i=1; i < argc; ++i) {
	const char *filename = argv[i];
	FILE *infp = fopen(filename, "rb");
	if (infp == NULL) {
	    printf("Couldn't read from file \"%s\"\n", filename);
	    dohelp(0);
	}
	rc = FungELF_load(infp, NULL, cbgc, cbsc, cbse);
	fclose(infp);
	if (rc < 0) {
	    printf("%s in file \"%s\"\n", FungELF_strerror(rc), filename);
	    exit(EXIT_FAILURE);
	} else {
	    if (DebugPrint)
		printf("Loaded %d program sections.\n", rc);
	}
    }


    /* Set up the virtual machine callbacks. */
    onException(holler);
    onReadMSR(0, readChar);
    onWriteMSR(1, writeChar);
    onWriteMSR(2, programExit);
    initMachine();

    /* The PC is initialized by the ELF loader,
     * when it loads the kernel image. */

    switch (setjmp(exceptionCaught)) {
        case 0: /* Set up and run. */
            run();
            break; /* NOT REACHED */
        case 42: /* Caught an invalid instruction. */
            printf("The simulator caught an invalid instruction. Quitting...\n");
            break;
        case 77: /* Caught an infinite loop. */
            printf("The simulator is hung in TRP. Quitting...\n");
            break;
        /* Simulations should normally exit through the programExit() callback. */
    }
    return 0;
}


static unsigned int cbgc(int x, int y)
{
    return readmem(x, y);
}

static int cbsc(int x, int y, unsigned int value)
{
    setmem(x, y, value);
    return 0;
}

static int cbse(int x, int y)
{
    setReg(1, ((y << 9) | x));
    setReg(2, 0);
    return 0;
}


static void holler(unsigned int inst)
{
    if ((inst & 0470000) == 0) { /* TRP, 0XX 000 XXX LLLLLLLLL */
        printf("Simulator reports: PC in infinite loop at (000,%03o)\n",
                (inst & 0777));
        longjmp(exceptionCaught, 77);
    } else {
        unsigned int pc = readReg(1);
        printf("Exception: undefined instruction %03o:%03o at (%03o,%03o)\n",
                (inst >> 9) & 0777, (inst >> 0) & 0777,
                (pc >> 9) & 0777, (pc >> 0) & 0777);
        longjmp(exceptionCaught, 42);
    }
}


/* Callback for reads from MSR 00. */
static unsigned int readChar(void)
{
    return (getchar() & 0777777u);
}

/* Callback for writes to MSR 01. */
static void writeChar(int curmode, unsigned int value)
{
    if (curmode == 0 || curmode == 2) {  /* MaskVector, MaskY */
        int ch = (value >> 9) & 0xFF;
        if (ch != 10 && !isprint(ch)) {
            printf("writeChar() called with char %dd, which isn't printable\n", ch);
            exit(0);
        }
        putchar(ch);
    }
    if (curmode != 2) {  /* anything but MaskY */
        int ch = value & 0xFF;
        if (ch != 10 && !isprint(ch)) {
            printf("writeChar() called with char %dd, which isn't printable\n", ch);
            exit(0);
        }
        putchar(ch);
    }
}

/* Callback for writes to MSR 02. */
static void programExit(int curmode, unsigned int value)
{
    unsigned int negsign = -1u;
    int sv = value;
    (void)curmode;  /* unused */
    if (value & 0400000u)  /* sign-extend the return code */
      sv |= (negsign & ~0777777u);
    printf("Program exited with %d.\n", sv);
    exit(sv);
}


static void dohelp(int man)
{
    puts("Usage: simfunge kernel.elf [program.bf]");
    if (man) {
        puts("");
        puts("  kernel.elf should be a binary file in ELF format, as");
        puts("produced by the fungasm assembler. It will be loaded first.");
        puts("Think of kernel.elf as a \"kernel\" for the system --- it");
        puts("could be a Befunge interpreter, for example. It will typically");
        puts("occupy \"kernel space\" at the bottom of RAM, although you");
        puts("can put it anywhere you like; the loader simply follows what's");
        puts("specified in the ELF file.");
        puts("  program.bf should be an ASCII file, at most 80 columns by");
        puts("25 rows. It will be loaded into the top of memory. This is for");
        puts("the convenience of users who already have a \"kernel.elf\"");
        puts("that interprets Befunge. If you don't have such a kernel, then");
        puts("this option probably won't help you.");
    }
    exit(EXIT_FAILURE);
}

