
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fungelf.h"

#define ELFCLASS32   1
#define ELFDATA2LSB  1
#define ELFDATA2MSB  2
#define ET_EXEC      2
#define PT_LOAD      1

static cbns_t CBnewsection = NULL;
static cbgc_t CBgetcell = NULL;
static cbsc_t CBsetcell = NULL;
static cbse_t CBsetentry = NULL;

static int read_ehdr(struct ELFHeader *eh, FILE *fp);
static int read_phdr(struct PHdr *eh, FILE *fp);
 static void read_int(const unsigned char *bp, void *pval, int len);
static void load_BSS_section(struct PHdr *phdr, unsigned int gfill);
static int load_text_section(struct PHdr *phdr, FILE *fp, unsigned int gfill);


/* Load either an ELF file or a plain ASCII file. */
int FungELF_load(FILE *fp, cbns_t cbns, cbgc_t cbgc, cbsc_t cbsc, cbse_t cbse)
{
    int rc = FungELF_loadelf(fp, cbns, cbgc, cbsc, cbse);
    if (rc < 0) {
	rewind(fp);
	rc = FungELF_loadascii(fp, cbns, cbgc, cbsc, cbse);
    }
    return rc;
}


const char *FungELF_strerror(int rc)
{
    switch (rc) {
	case -1: return "File too short";
	case -2: return "Wrong file layout";
	case -3: return "Error reading ELF headers";
	case -4: return "Error reading text section";
    }
    if (rc >= 0) return "No error";
    if (rc < 0) return "Unknown error";
}


/* Get ASCII data from a text file, and write it into the 'memory' array
 * starting at (0,0). Do not treat tabs specially; do not pad with spaces.
 * Treat \n, \r, and \r\n as line separators; never write \r or \n into
 * memory. Do not treat \f specially; this is not Trefunge.
 */
int FungELF_loadascii(FILE *fp, cbns_t cbns, cbgc_t cbgc, cbsc_t cbsc, cbse_t cbse)
{
    int x=0, y=0;

    CBnewsection = cbns;
    CBgetcell = cbgc;
    CBsetcell = cbsc;
    CBsetentry = cbse;

    while (1) {
	int k = getc(fp);
	if (k == EOF) break;
	if (k == '\r') {
	    y += 1;
	    if (y == 512)
	      break;
	    x = 0;
	    k = getc(fp);
	    if (k == '\n')
	      continue;
	} else if (k == '\n') {
	    y += 1;
	    if (y == 512)
	      break;
	    x = 0;
	    continue;
	}
	assert(0 <= x);
	assert(0 <= y && y < 512);
	if (x < 512) {
	    CBsetcell(x, y, k);
	    ++x;
	}
    }

    return 1;
}

/* Get the data from a FungELF file, and write it into the 'memory' array.
 */
int FungELF_loadelf(FILE *fp, cbns_t cbns, cbgc_t cbgc, cbsc_t cbsc, cbse_t cbse)
{
    struct ELFHeader ehdr;
    unsigned int gfill = -1u;
    int i;

    CBnewsection = cbns;
    CBgetcell = cbgc;
    CBsetcell = cbsc;
    CBsetentry = cbse;

    if (read_ehdr(&ehdr, fp) != sizeof ehdr)
      return -1;
    if (ehdr.e_ident[0] != 0x7F) return -2;
    if (ehdr.e_ident[1] != 'E') return -2;
    if (ehdr.e_ident[2] != 'L') return -2;
    if (ehdr.e_ident[3] != 'F') return -2;
    if (ehdr.e_ident[4] != ELFCLASS32) return -2;
    if (ehdr.e_ident[5] != ELFDATA2MSB &&
        ehdr.e_ident[5] != ELFDATA2LSB) return -2;
    if (ehdr.e_type != ET_EXEC) return -2;
    if (ehdr.e_ehsize < 42) return -2;
    if (ehdr.e_phentsize < 28) return -2;

    /* Deal with a global fill value. */
    if (ehdr.e_flags & PF_FUNG_FILLVALUE) {
        int i, j;
        gfill = ehdr.e_flags & 0777777;
        if (CBsetcell != NULL)
          for (i=0; i <= 0777; ++i)
            for (j=0; j <= 0777; ++j)
              CBsetcell(i, j, gfill);
    }

    /* Read the sections. */
    for (i=0; i < ehdr.e_phnum; ++i) {
        struct PHdr phdr;
        if (fseek(fp, ehdr.e_phoff + i*ehdr.e_phentsize, SEEK_SET) != 0)
          return -3;
        if (read_phdr(&phdr, fp) != ehdr.e_phentsize)
          return -3;
        if (phdr.p_type != PT_LOAD) return -3;
        if (phdr.p_filesz == 0) {
            /* it's a BSS section */
            load_BSS_section(&phdr, gfill);
        } else {
            if (load_text_section(&phdr, fp, gfill) != 0)
              return -4;
        }
    }

    /* Set PC to the entry point, and DeltaPC to zero.
     * The first instruction of the program had better
     * be something that affects PC or DeltaPC, or the
     * program will immediately go into an infinite loop
     * and get killed by the simulator. */
    if (CBsetentry != NULL)
      CBsetentry(ehdr.e_entry & 0777, (ehdr.e_entry >> 9) & 0777);
    return ehdr.e_phnum;
}

static void load_BSS_section(struct PHdr *phdr, unsigned int gfill)
{
    int fillvalue = phdr->p_flags & 0777777;
    int nevermyfill = ((phdr->p_flags & PF_FUNG_NEVERMYFILL) != 0);
    int i, j;
    int startx = phdr->p_vaddr & 0777;
    int starty = (phdr->p_vaddr >> 9) & 0777;
    int w = phdr->p_memsz & 0777;
    int h = (phdr->p_memsz >> 9) & 0777;

    if (CBnewsection != NULL)
      CBnewsection(startx, starty, w, h, 0);

    if (nevermyfill || !(phdr->p_flags & PF_FUNG_FILLVALUE)) {
        /* This BSS section is a no-op; it never fills any cells. */
        return;
    }

    if (CBsetcell != NULL) {
        for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
                  int cx = (startx+i) & 0777;
                  int cy = (starty+j) & 0777;
                  CBsetcell(cx, cy, fillvalue);
            }
        }
    }
}

static int load_text_section(struct PHdr *phdr, FILE *fp, unsigned int gfill)
{
    unsigned int myfill = (phdr->p_flags & PF_FUNG_FILLVALUE) ?
                            (phdr->p_flags & 0777777) : -1u;
    int nevermyfill = ((phdr->p_flags & PF_FUNG_NEVERMYFILL) != 0);
    int i, j;
    int startx = phdr->p_vaddr & 0777;
    int starty = (phdr->p_vaddr >> 9) & 0777;
    int w = phdr->p_memsz & 0777;
    int h = (phdr->p_memsz >> 9) & 0777;
    static unsigned char buffer[512*3];

    if (CBnewsection != NULL)
      CBnewsection(startx, starty, w, h, 1);

    if (fseek(fp, phdr->p_offset, SEEK_SET) != 0)
      return -1;

    for (j=0; j < h; ++j) {
        if ((int)fread(buffer, 1, 3*w, fp) != 3*w)
          return -1;
        if (CBsetcell != NULL) {
            for (i=0; i < w; ++i) {
                unsigned int cx = (startx+i) & 0777;
                unsigned int cy = (starty+j) & 0777;
                unsigned int WoRd = buffer[3*i] & 3;
                WoRd = (WoRd << 8) | buffer[3*i+1];
                WoRd = (WoRd << 8) | buffer[3*i+2];
                if (!(nevermyfill && WoRd == myfill))
                  CBsetcell(cx, cy, WoRd);
            }
        }
    }

    return 0;
}



static int endianness;

/* We need our own struct-writing (and struct-reading) routines
 * to make sure that the endianness of the host isn't a problem. */
static int read_ehdr(struct ELFHeader *eh, FILE *fp)
{
    unsigned char buffer[sizeof *eh];
    int rc = fread(buffer, 1, sizeof buffer, fp);
    if (rc != sizeof buffer) return rc;
    memcpy(eh->e_ident, buffer, 16);
    endianness = eh->e_ident[5];
    assert(sizeof buffer == 52);
    read_int(buffer+16, &eh->e_type, 2);
    read_int(buffer+18, &eh->e_machine, 2);
    read_int(buffer+20, &eh->e_version, 4);
    read_int(buffer+24, &eh->e_entry, 4);
    read_int(buffer+28, &eh->e_phoff, 4);
    read_int(buffer+32, &eh->e_shoff, 4);
    read_int(buffer+36, &eh->e_flags, 4);
    read_int(buffer+40, &eh->e_ehsize, 2);
    read_int(buffer+42, &eh->e_phentsize, 2);
    read_int(buffer+44, &eh->e_phnum, 2);
    read_int(buffer+46, &eh->e_shentsize, 2);
    read_int(buffer+48, &eh->e_shnum, 2);
    read_int(buffer+50, &eh->e_shstrndx, 2);
    return rc;
}

static int read_phdr(struct PHdr *ph, FILE *fp)
{
    unsigned char buffer[sizeof *ph];
    int rc = fread(buffer, 1, sizeof buffer, fp);
    if (rc != sizeof buffer) return rc;
    assert(sizeof buffer == 32);

    read_int(buffer+0, &ph->p_type, 4);
    read_int(buffer+4, &ph->p_offset, 4);
    read_int(buffer+8, &ph->p_vaddr, 4);
    read_int(buffer+12, &ph->p_paddr, 4);
    read_int(buffer+16, &ph->p_filesz, 4);
    read_int(buffer+20, &ph->p_memsz, 4);
    read_int(buffer+24, &ph->p_flags, 4);
    read_int(buffer+28, &ph->p_align, 4);
    return rc;
}

/* Reads big-endian or little-endian, depending on 'endianness'. */
static void read_int(const unsigned char *bp, void *pval, int len)
{
    unsigned int val = 0;
    int oldlen = len;
    if (endianness == ELFDATA2MSB) {
        while (len > 0) {
            val <<= 8;
            val |= *bp++;
            --len;
        }
    } else {
        while (len > 0) {
            val <<= 8;
            val |= bp[--len];
        }
    }
    if (oldlen == 2)
      *(unsigned short *)pval = val;
    else
      *(unsigned int *)pval = val;
}
