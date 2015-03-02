
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "fungelf.h"

static int bef2elf(FILE *in, FILE *out);
static void dohelp(int man);

int main(int argc, const char *argv[])
{
    FILE *in, *out;

    if (argc != 3)
      dohelp(1);

    in = fopen(argv[1], "r");
    if (in == NULL)
      dohelp(0);

    out = fopen(argv[2], "wb");
    if (out == NULL)
      dohelp(0);

    if (bef2elf(in, out) != 0)
      puts("An error was encountered while processing the file.");
    fclose(in);
    fclose(out);
    return 0;
}


#define BW 80
#define BH 25

static unsigned int ubuffer[BW*BH];


static int bef2elf(FILE *in, FILE *out)
{
    int i, j;
    int rc = 0;

    for (i=0; i < BW*BH; ++i)
      ubuffer[i] = ' ';

    for (i=0; i < 25; ++i) {
        char buffer[BW+3];
        if (fgets(buffer, sizeof buffer, in) == NULL)
          break;
        for (j=0; buffer[j] != '\0'; ++j) {
            if (32 <= buffer[j])
              ubuffer[j*BW+i] = buffer[j];
            else break;
        }
    }
    FungELF_entrypoint(0);
    FungELF_fillvalue(' ');
    FungELF_addtext(000000, (BH<<9)|BW, 0, ubuffer);
    rc |= FungELF_writefile(out);
    FungELF_done();
    return rc;
}

static void dohelp(int man)
{
    puts("Usage: bef2elf input.bf output.elf");
    if (man) {
        puts("");
        puts("  program.bf should be an ASCII file, at most 80 columns by");
        puts("25 rows, containing no control characters except at the ends");
        puts("of lines. bef2elf will strip out any control characters");
        puts("(ASCII 31 or less).");
        puts("  bef2elf's output is a binary FungELF file. It consists of");
        puts("one text section, size (25,80), starting at (0,0), and containing");
        puts("the ASCII characters from program.bf. Ragged and missing lines");
        puts("are padded to (25,80) with spaces (ASCII 32). The global fill");
        puts("value is set to ASCII 32, and the program entry point is set");
        puts("to (0,0).");
        puts("  bef2elf's output will not do anything sensible by itself, because");
        puts("it fills all of memory with trap instructions. Running output.elf");
        puts("in simfunge should produce a simulator error about an infinite");
        puts("loop in TRP 32.");
    }
    exit(EXIT_FAILURE);
}

