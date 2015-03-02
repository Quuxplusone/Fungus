
#ifndef H_FUNGELF
 #define H_FUNGELF

#include <stdio.h>

 #ifdef __cplusplus
  extern "C" {
 #endif

typedef int (*cbns_t)(int sx, int sy, int w, int h, int hastext);
typedef unsigned int (*cbgc_t)(int x, int y);
typedef int (*cbsc_t)(int x, int y, unsigned int value);
typedef int (*cbse_t)(int x, int y);

/* ELF reading functions for simfunge. */
int FungELF_loadelf(FILE *fp, cbns_t cbns, cbgc_t cbgc, cbsc_t cbsc, cbse_t cbse);
int FungELF_loadascii(FILE *fp, cbns_t cbns, cbgc_t cbgc, cbsc_t cbsc, cbse_t cbse);
int FungELF_load(FILE *fp, cbns_t cbns, cbgc_t cbgc, cbsc_t cbsc, cbse_t cbse);

const char *FungELF_strerror(int rc);

/* ELF writing functions for fungasm. */
void FungELF_entrypoint(unsigned int origin);
void FungELF_fillvalue(unsigned int value);
void FungELF_addtext(unsigned int origin, unsigned int size,
        unsigned int flags, unsigned int *data);
void FungELF_addbss(unsigned int origin, unsigned int size,
        unsigned int flags);
int FungELF_writefile(FILE *outfp);

/* Clears the buffers so that a new ELF file can be written. */
void FungELF_done(void);


/* fungasm needs to know these flag values.
 * Suppose a global fill value of G and a section fill value of S.
 * PF_FUNG_FILLVALUE means, "I really do have a fill value." If this flag
 * isn't set, then the low 9 bits of p_flags are just garbage and should be
 * ignored.
 * PF_FUNG_NEVERMYFILL means, "If the ELF file says this memory cell should
 * get S, then don't write anything into this memory cell." In other words,
 * S is a "transparent color".
 */
#define PF_FUNG_FILLVALUE    01000000
#define PF_FUNG_NEVERMYFILL  02000000


/* felfin and felfout share these structures.
 * Nobody else should care about them. */
struct ELFHeader {
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
    unsigned int e_entry;
    unsigned int e_phoff;
    unsigned int e_shoff;
    unsigned int e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
};

struct PHdr {
    unsigned int p_type;    /* relevant: TEXT or BSS */
    unsigned int p_offset;  /* relevant: location in file */
    unsigned int p_vaddr;   /* relevant: uint18 vector */
    unsigned int p_paddr;   /* same as vaddr */
    unsigned int p_filesz;  /* for TEXT, same as p_memsz */
    unsigned int p_memsz;   /* relevant: uint18 vector */
    unsigned int p_flags;   /* low uint18: fill for BSS */
                            /* high: flags for BSS, TEXT */
    unsigned int p_align;
};


 #ifdef __cplusplus
  };
 #endif

#endif
