
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fungelf.h"
#include "ImageFmtc.h"

#define steq(x,y) (!strcmp(x,y))

static void process_one(const char *inname);
static void do_error(const char *fmat, ...);
static void do_help(int man);


static const char *OutputName = NULL;
static unsigned char (*Im)[3];

int main(int argc, char *argv[])
{
    int i, j;

    for (i=1; i < argc; i++)
    {
        if (argv[i][0] != '-') break;
        if (argv[i][1] == '\0') break;

        if (steq(argv[i]+1, "-")) { ++i; break; }
        else if (steq(argv[i]+1, "?")) do_help(0);
        else if (steq(argv[i]+1, "-help")) do_help(0);
        else if (steq(argv[i]+1, "-man")) do_help(1);
        else if (steq(argv[i], "-o")) {
            if (i >= argc-1) {
                do_error("Need output filename with option -o\n");
            }
            OutputName = argv[++i];
        } else {
            for (j=1; argv[i][j]; ++j) {
                switch (argv[i][j]) {
                    default:
                        do_error("Unrecognized option(s) %s\n", argv[i]);
                }
            }
        }
    }

    if (i == argc) do_error("No files to process\n");

    Im = malloc(512*512 * sizeof *Im);
    if (Im == NULL) do_error("Out of memory for PPM image");
    memset(Im, 0xFF, 512*512*3); /* white */
    
    for (; i < argc; i++)
    {
        const char *outname;
        char buffer[100];

        if (OutputName != NULL) {
	    process_one(argv[i]);
        } else {
            char *p = argv[i];
            p = strrchr(p, '/');
            if (p == NULL) p = argv[i]; else ++p;
            p = strrchr(p, '\\');
            if (p == NULL) p = argv[i]; else ++p;
            sprintf(buffer, "%.*s", (int)((sizeof buffer)-5), p);
            p = strrchr(buffer, '.');
            if (p == NULL) p = strchr(buffer, '\0');
            strcpy(p, ".ppm");
            outname = buffer;
	    process_one(argv[i]);
	    if (WritePPM6(outname, Im, 512, 512) != 0)
		do_error("Error writing PPM output file \"%s\"\n", outname);
	    memset(Im, 0xFF, 512*512*3); /* white */
        }
    }

    if (OutputName != NULL) {
	if (WritePPM6(OutputName, Im, 512, 512) != 0)
	    do_error("Error writing PPM output file \"%s\"\n", OutputName);
    }
    free(Im);
    return 0;
}


static int cbsc(int x, int y, unsigned int value)
{
    char buffer[7];
    unsigned char v0, v1, v2;
    sprintf(buffer, "%06o", value & 0777777);
    v0 = (buffer[0]-'0') << 4 | (buffer[1]-'0');
    v1 = (buffer[2]-'0') << 4 | (buffer[3]-'0');
    v2 = (buffer[4]-'0') << 4 | (buffer[5]-'0');
    Im[y*512+x][0] = v0;
    Im[y*512+x][1] = v1;
    Im[y*512+x][2] = v2;
    return 0;
}


static void process_one(const char *inname)
{
    FILE *in;
    int rc;

    if (steq(inname, "-"))
      in = stdin;
    else if (NULL == (in = fopen(inname, "rb")))
      do_error("Couldn't open input file \"%s\"\n", inname);

    if (in == stdin) {
	/* Can't use FungELF_load() since we can't rewind stdin.
	 * Assume the text stream will be ASCII, not ELF. */
	rc = FungELF_loadascii(in, NULL, NULL, cbsc, NULL);
    } else {
	rc = FungELF_load(in, NULL, NULL, cbsc, NULL);
    }
    
    if (rc < 0) {
        printf("%s in file \"%s\"\n", FungELF_strerror(rc), inname);
        exit(EXIT_FAILURE);
    } else {
        printf("Loaded %d program sections.\n", rc);
    }

    return;
}


static void do_error(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    printf("Error: ");
    vprintf(msg, ap);
    putchar('\n');
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void do_help(int man)
{
    puts("Usage: elf2ppm [-o out.ppm] image.elf [image2.elf ...]");
    if (man) {
        puts("");
        puts("  image.elf should be a binary file in ELF format, as");
        puts("produced by the fungasm assembler. The elf2ppm program will");
        puts("convert the ELF file to a PPM (Portable Pixelmap) image,");
        puts("512 pixels on a side, graphically representing the memory");
        puts("layout of the binary image.");
        puts("  Cells that are part of a section in the ELF file");
        puts("will be colored according to their octal value; for example,");
        puts("a cell whose initial octal value is 777000 will be colored");
        puts("with the RGB hex value (#77,#70,#00), a brownish shade.");
        puts("  Cells that are not part of the ELF image will be colored");
        puts("with the RGB hex value (#FF,#FF,#FF), or white, a color");
        puts("which is easily distinguished from any octal value.");
	puts("  Input files that are not in ELF format will be read");
	puts("as plain ASCII files; therefore, elf2ppm kernel.elf hello.b93");
	puts("will show the kernel and a \"hello world\" program together");
	puts("in the same memory image.");
    }
    exit(EXIT_FAILURE);
}

