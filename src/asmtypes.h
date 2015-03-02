
#ifndef H_ASMTYPES
 #define H_ASMTYPES


/* Macros are basically like C preprocessor macros: literal text replacement.
 * We have one global macro list, plus one local list for each section. */

struct MacroDef {
    char *text;
    char *replacement;
    struct MacroDef *next;
    int unusable;  /* marks mutually recursive or unresolvable EQUs */
};



/* Equ directives are kind of like constants. An equ is evaluated in its
 * original context, and then its value is plugged in wherever it's used.
 * But since we could have .EQU A .+B (with B an unresolved equ), we might
 * have to wait before evaluating it fully. In the given case, we'd
 * immediately expand .+B to (105,027)+B, and store that string in the
 * "replacement" field. */

struct EquDef {
    char *text;
    struct Section *sp;
    struct String *s;
    /* replacement: local macros expanded, "." replaced by "(x,y)" */
    char *replacement;
    /* value: the actual uint18 value of this symbol */
    unsigned int value;
    struct EquDef *next;
    int unusable;  /* marks mutually recursive or unresolvable EQUs */
};



/*
 * Strings and Rows are the basic units used by the parser. Everything
 * has to be broken down into a set of Strings first; then those Strings
 * are fitted into a grid; then we start processing directives and such.
 */

struct String {
    int start;    /* column position at which it started */
    int len;      /* strlen(text) */
    char *text;   /* dynamically allocated */
    int x, y;     /* offset from the upper left of the section, in cells */
    int comment;  /* this string is part of a comment */
};

struct Row {
    int len;
    struct String *s;  /* dynamically allocated array */
};

struct Section {
    struct Row *rows;  /* dynamically allocated array */
    int rowslen, rowscap;
    struct MacroDef *local_macros;
    int global;        /* this section is global, and about to get deleted */
    struct Section *next;

    /* This is stuff related to helpful warnings,
     * not critical to the assembly process. */
    int contains_offset_org_directive;

    /* This is stuff directly related to the ELF output. */
    unsigned int fillvalue;
    unsigned int org_x, org_y;
    int size_x, size_y;
    unsigned int flags;
    int is_bss;
    unsigned int *assembled;
};

/* Defined in asmmain.c */
void fatal_error(const struct Row *r, int pos, const char *msg, ...);
void warning(const struct Row *r, int pos, const char *msg, ...);

/* Defined in asmcmnt.c */
void mark_comments(struct Section *sp);

#endif
