
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fungelf.h"

#define ELFCLASS32   1
#define ELFDATA2MSB  2
#define EV_CURRENT   1   /* ELF version number? */
#define ET_EXEC      2
#define EM_NONE      0
#define SIZEOF_SHDR  40  /* we don't bother with sections */
#define PT_LOAD      1

static struct ELFHeader default_EHdr = {
    { 0x7f, 'E', 'L', 'F', ELFCLASS32, ELFDATA2MSB, EV_CURRENT, 0,
      0,     0,   0,   0,  0,          0,           0,     0 },
    ET_EXEC,
    EM_NONE,      /* safer than making up a value for Fungus */
    EV_CURRENT,   /* ELF version again? */
    0,            /* e_entry, to be filled in later */
    sizeof (struct ELFHeader), /* e_phoff */
    0,            /* e_shoff, unused since e_shnum==0 */
    0,            /* e_flags, either zero or fillvalue|01000000 */
    sizeof (struct ELFHeader),
    sizeof (struct PHdr),
    0,            /* e_phnum, to be filled in later */
    SIZEOF_SHDR,
    0,            /* e_shnum, zero */
    0,            /* e_shstrndx, zero */
};

static struct PHdr default_text_PHdr = {
    PT_LOAD,
    0,         /* p_offset, to be filled in later */
    0,         /* p_vaddr, to be filled in later */
    0,         /* p_paddr */
    0,         /* p_filesz, int, to be filled in later */
    0,         /* p_memsz, vector, product equals p_filesz */
    0,         /* p_flags */
    0,         /* p_align, zero */
};

static struct PHdr default_bss_PHdr = {
    PT_LOAD,
    0,         /* p_offset, to be filled in later */
    0,         /* p_vaddr, to be filled in later */
    0,         /* p_paddr */
    0,         /* p_filesz, int, zero */
    0,         /* p_memsz, vector, to be filled in later */
    0,         /* p_flags */
    0,         /* p_align, zero */
};

enum { FUNG_TEXT, FUNG_BSS };

/* The internal representation of an ELF section. */
struct SavedSection {
    int mode;
    unsigned int origin, size;
    unsigned int flags;
    unsigned int *data; /* points to (w*h) unsigned ints */
    struct SavedSection *next;
};


static void flip_psects(void);
static int write_ehdr(const struct ELFHeader *eh, FILE *fp);
static int write_phdr(const struct PHdr *ph, FILE *fp);
 static void write_int(unsigned char *bp, unsigned int val, int len);

static unsigned int saved_e_entry = -1u;
static unsigned int saved_fillvalue = -1u;
static unsigned int saved_e_phnum = 0;
static struct SavedSection *saved_psects = NULL;

void FungELF_entrypoint(unsigned int e_entry)
{
    saved_e_entry = e_entry;
}

void FungELF_fillvalue(unsigned int value)
{
    saved_fillvalue = value;
}

void FungELF_addtext(unsigned int origin, unsigned int size, unsigned int flags, unsigned int *data)
{
    struct SavedSection *p = malloc(sizeof *p);
    int len = ((size >> 9) & 0777) * (size & 0777);
    assert(p != NULL);
    p->mode = FUNG_TEXT;
    p->origin = origin;
    p->size = size;
    p->flags = flags;
    p->data = malloc(len * sizeof *p->data);
    assert(p->data != NULL);
    memcpy(p->data, data, len * sizeof *p->data);
    p->next = saved_psects;
    saved_psects = p;
    saved_e_phnum += 1;
}

void FungELF_addbss(unsigned int origin, unsigned int size, unsigned int flags)
{
    struct SavedSection *p = malloc(sizeof *p);
    assert(p != NULL);
    p->mode = FUNG_BSS;
    p->origin = origin;
    p->size = size;
    p->flags = flags;
    p->data = NULL;
    p->next = saved_psects;
    saved_psects = p;
    saved_e_phnum += 1;
}

static void flip_psects(void)
{
    struct SavedSection *p = NULL;
    while (saved_psects != NULL) {
        struct SavedSection *q = saved_psects->next;
        saved_psects->next = p;
        p = saved_psects;
        saved_psects = q;
    }
    saved_psects = p;
}

int FungELF_writefile(FILE *outfp)
{
    struct ELFHeader ehdr = default_EHdr;
    unsigned int file_offset;
    struct SavedSection *p;

    flip_psects();

    ehdr.e_entry = saved_e_entry;
    ehdr.e_phnum = saved_e_phnum;
    if (saved_fillvalue != -1u) {
        ehdr.e_flags =  PF_FUNG_FILLVALUE | (saved_fillvalue & 0777777);
    }
    if (write_ehdr(&ehdr, outfp) != sizeof ehdr)
      return -1;

    /* Where is the first section entry going to start? */
    file_offset = sizeof ehdr;
    file_offset += saved_e_phnum * sizeof (struct PHdr);

    for (p = saved_psects; p != NULL; p = p->next) {
        struct PHdr phdr;
        if (p->mode == FUNG_BSS) {
            phdr = default_bss_PHdr;
            phdr.p_offset = file_offset;
            phdr.p_paddr = phdr.p_vaddr = p->origin;
            phdr.p_memsz = p->size;
            phdr.p_flags = p->flags;
        } else if (p->mode == FUNG_TEXT) {
            phdr = default_text_PHdr;
            phdr.p_offset = file_offset;
            phdr.p_paddr = phdr.p_vaddr = p->origin;
            phdr.p_memsz = p->size;
            phdr.p_filesz = 3*((p->size >> 9) & 0777)*(p->size & 0777);
            phdr.p_flags = p->flags;
        } else return -4;
        file_offset += phdr.p_filesz;
        if (write_phdr(&phdr, outfp) != sizeof phdr)
          return -1;
    }

    /* Okay, the table of contents has been output.
     * Now output the section entries themselves. */
    for (p = saved_psects; p != NULL; p = p->next) {
        if (p->mode == FUNG_BSS) {
            continue;
        } else if (p->mode == FUNG_TEXT) {
            int h = (p->size >> 9) & 0777;
            int w = (p->size >> 0) & 0777;
            int i, j;
            /* Three bytes per uint18; the high 6 bits
             * are ignored, so let's make them zero. */
            for (j=0; j < h; ++j) {
                for (i=0; i < w; ++i) {
                    unsigned int WoRd = p->data[j*w+i];
                    putc((0 << 2) | (WoRd >> 16), outfp);
                    putc(WoRd >> 8, outfp);
                    putc(WoRd >> 0, outfp);
                }
            }
        } else return -4;
    }

    /* All done! */
    return 0;
}

void FungELF_done(void)
{
    while (saved_psects != NULL) {
        struct SavedSection *q = saved_psects->next;
        free(saved_psects->data);
        free(saved_psects);
        saved_psects = q;
    }
    saved_e_entry = -1u;
    saved_e_phnum = 0;
    saved_fillvalue = -1u;
}

/* We need our own struct-writing (and struct-reading) routines
 * to make sure that the endianness of the host isn't a problem. */
static int write_ehdr(const struct ELFHeader *eh, FILE *fp)
{
    unsigned char buffer[sizeof *eh];
    memcpy(buffer, eh->e_ident, 16);
    write_int(buffer+16, eh->e_type, 2);
    write_int(buffer+18, eh->e_machine, 2);
    write_int(buffer+20, eh->e_version, 4);
    write_int(buffer+24, eh->e_entry, 4);
    write_int(buffer+28, eh->e_phoff, 4);
    write_int(buffer+32, eh->e_shoff, 4);
    write_int(buffer+36, eh->e_flags, 4);
    write_int(buffer+40, eh->e_ehsize, 2);
    write_int(buffer+42, eh->e_phentsize, 2);
    write_int(buffer+44, eh->e_phnum, 2);
    write_int(buffer+46, eh->e_shentsize, 2);
    write_int(buffer+48, eh->e_shnum, 2);
    write_int(buffer+50, eh->e_shstrndx, 2);
    return fwrite(buffer, 1, sizeof buffer, fp);
}

static int write_phdr(const struct PHdr *ph, FILE *fp)
{
    unsigned char buffer[sizeof *ph];
    write_int(buffer+0, ph->p_type, 4);
    write_int(buffer+4, ph->p_offset, 4);
    write_int(buffer+8, ph->p_vaddr, 4);
    write_int(buffer+12, ph->p_paddr, 4);
    write_int(buffer+16, ph->p_filesz, 4);
    write_int(buffer+20, ph->p_memsz, 4);
    write_int(buffer+24, ph->p_flags, 4);
    write_int(buffer+28, ph->p_align, 4);
    return fwrite(buffer, 1, sizeof buffer, fp);
}

/* Writes big-endian. */
static void write_int(unsigned char *bp, unsigned int val, int len)
{
    while (len > 0) {
        bp[--len] = val;
        val >>= 8;
    }
}


