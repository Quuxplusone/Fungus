
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getline.h"
#include "fungal.h"
#include "fungelf.h"
#include "asmtypes.h"

#define TABSTOP 8
#define NELEM(a) ((int)(sizeof a / sizeof *a))

static int assemble_file(FILE *in, FILE *out);
 static struct Section *new_Section(struct Section *next);
 static void kill_Section(struct Section *sp);
 static void add_row(struct Section *sp, const struct Row *r);
 static struct Row *split_strings(const char *line);
  static int detab_selection(char *bp, const char *text, int len, int realpos);

 static void resolve_grid(struct Section *sp);
 static void extract_directives(struct Section *sp);
  static int add_macro(struct MacroDef **mp, const char *text, int textlen,
                        const char *replacement);
  static void replace_dot(char **ptext, int x, int y);
 static struct EquDef *new_Equ(const char *text, int len,
         const char *replacement, struct EquDef *next);
  static struct EquDef *find_equ(const char *text, int len);
 static void kill_Equ(struct EquDef *ep);

 static void decide_global(struct Section *sp);

 static void resolve_macros(struct Section *sp, char **text);
  static struct MacroDef *find_macro(struct MacroDef *mlist, const char *text, int len);
  static char *find_identifier(const char *text, int *len);

 static void trim_section(struct Section *sp);
 static unsigned int makebgfill(const struct Section *sp);
 static void kill_all_globals(void);

unsigned int true_value(const char *equname, int len);

void fatal_error(const struct Row *r, int pos, const char *msg, ...);
void warning(const struct Row *r, int pos, const char *msg, ...);
 static int print_Row(FILE *fp, const struct Row *r, int arrow);
static void dohelp(int man);


int main(int argc, const char *argv[])
{
    FILE *in, *out;

    if (argc != 2 && argc != 3)
      dohelp(1);

    in = fopen(argv[1], "r");
    if (in == NULL)
      dohelp(0);

    if (argc == 3) {
        out = fopen(argv[2], "wb");
        if (out == NULL)
          dohelp(0);
    } else {
        char buffer[100];
        char *p = strrchr(argv[1], '.');
        if (p == NULL) p = strchr(argv[1], '\0');
        if (p - argv[1] > 90)
          fatal_error(NULL, -1, "The provided filename was too long.");
        sprintf(buffer, "%.*s.elf", p - argv[1], argv[1]);
        out = fopen(buffer, "wb");
        if (out == NULL)
          fatal_error(NULL, -1, "File \"%s\" could not be opened.", buffer);
    }

    if (assemble_file(in, out) != 0) {
        puts("An error was encountered while assembling the file.");
    }
    fclose(out);
    return 0;
}


static struct EquDef *global_equs = NULL;
static struct Section *secthead = NULL;
static struct MacroDef *global_macros = NULL;
static unsigned int global_entry = -1u;
static unsigned int global_fillvalue = -1u;
static unsigned int global_flags = 0u;


static int assemble_file(FILE *in, FILE *out)
{
    char *line = NULL;
    struct Section *sp, **psp;
    struct EquDef *ep;

    /* Built-in register names. */
    global_equs = new_Equ("PC", 2, "1", global_equs);
    global_equs = new_Equ("DPC", 3, "2", global_equs);
    global_equs = new_Equ("TPC", 3, "7", global_equs);
    global_equs = new_Equ("TDPC", 4, "6", global_equs);

    /* "simfunge" machine-specific register names. */
    global_equs = new_Equ("INPUT", 2, "000", global_equs);
    global_equs = new_Equ("OUTPUT", 2, "001", global_equs);
    global_equs = new_Equ("PRGMEXIT", 2, "002", global_equs);

    /* Built-in machine-specific register names. */
    global_equs = new_Equ("HCON", 2, "040", global_equs);
    global_equs = new_Equ("HCAND", 2, "041", global_equs);
    global_equs = new_Equ("HCOR", 2, "042", global_equs);
    global_equs = new_Equ("OSEC", 2, "043", global_equs);
    global_equs = new_Equ("ISTACK", 2, "050", global_equs);
    global_equs = new_Equ("DISTK", 2, "051", global_equs);
    global_equs = new_Equ("IRET", 2, "052", global_equs);
    global_equs = new_Equ("TICKS", 2, "070", global_equs);

    secthead = sp = new_Section(NULL);

    while (fgetline_notrim(&line, in) != NULL) {
        struct Row *r;
        assert(line != NULL);
        r = split_strings(line);
        if (r == NULL) {
            /* An empty line. This section is done! */
            if (sp->rowslen != 0) {
                sp->next = new_Section(NULL);
                sp = sp->next;
            }
        } else {
            add_row(sp, r);
        }
        free(line);
        line = NULL;
    }

    /* Now everything is in the form of Strings and Sections. */
    free(line);
    fclose(in);

    /* Do all the processing that definitely won't require
     * resolving any EQU or MACRO directives. */

puts("Resolving grid...");
puts("Extracting directives, macros, and equs...");
    for (sp = secthead; sp != NULL; sp = sp->next) {
        /* Remove the leftover "empty" section from the end of the section list. */
        if (sp->next != NULL && sp->next->rowslen == 0) {
            assert(sp->next->next == NULL);
            free(sp->next);
            sp->next = NULL;
        } else {
            assert(sp->rowslen > 0);
            assert(sp->rows[0].len > 0);
        }
        mark_comments(sp);
        resolve_grid(sp);
        extract_directives(sp);
        decide_global(sp);
    }

puts("Resolving dots in .EQU expansions...");
    for (ep = global_equs; ep != NULL; ep = ep->next) {
        if ((ep->sp == NULL) || ep->sp->global) {
            /* Can't replace_dot() in a global section; it has no location.
             * Let the assembler's tokenizer choke on any dots, later. */
            ep->sp = NULL;
        } else if (ep->sp->org_x == -1u) {
            fatal_error(&ep->sp->rows[0], ep->sp->rows[0].s[0].start,
                "Non-global section contains no .ORG directive");
        } else {
            int x, y;
            assert(ep->s != NULL);
            x = (ep->sp->org_x + ep->s->x);
            y = (ep->sp->org_y + ep->s->y);
            replace_dot(&ep->replacement, x, y);
        }
    }

    /* Now replace all macros. Macros are literal text replacement. */
puts("Resolving macros in sections...");
    for (sp = secthead; sp != NULL; sp = sp->next) {
        int i, j;
        for (i=0; i < sp->rowslen; ++i) {
            for (j=0; j < sp->rows[i].len; ++j) {
                struct String *s = &sp->rows[i].s[j];
                assert(s != NULL);
                if (s->comment) continue;
                if (s->text[0] != '.') {
                    assert(s->y < sp->size_y);
                    assert(s->x < sp->size_x);
                }
                if (!strncmp(s->text, ".MACRO", 6) && isspace(s->text[6]))
                    /* do not resolve macros inside themselves! */ ;
                else
                    resolve_macros(sp, &s->text);
                if (!strncmp(s->text, ".ENTRY", 6) && isspace(s->text[6])) {
                    int len;
                    char *entry_expr;
printf("  Found .ENTRY at (%d,%d) in section\n", s->x, s->y);
                    if (global_entry != -1u)
                      fatal_error(&sp->rows[i], s->start, "Only one .ENTRY directive allowed in program");
                    entry_expr = malloc(strlen(s->text+7)+1);
                    assert(entry_expr != NULL);
                    strcpy(entry_expr, s->text+7);
                    if (!sp->global) {
                        assert(sp->org_x != -1u);
                        replace_dot(&entry_expr,
                                sp->org_x + s->x, sp->org_y + s->y);
                    }
                    global_entry = eval_scalar(entry_expr, &len);
                    if (global_entry == -1u)
                      fatal_error(&sp->rows[i], s->start+7, "Expected scalar value in .ENTRY directive");
                    if (entry_expr[len] != '\0')
                      fatal_error(&sp->rows[i], s->start+7, "Extra text after scalar in .ENTRY directive");
                    global_entry &= ~IS_VECTOR;
                    free(entry_expr);
                }
            }
        }
    }

    if (global_entry == -1u)
      warning(NULL, -1, "Program contains no .ENTRY directive");

puts("Deleting global sections...");
    for (psp = &secthead; (*psp) != NULL; ) {
        struct Section *sp = *psp;
        assert((sp->org_x == -1u) == (sp->org_y == -1u));
        if ((*psp)->global) {
            /* No need to keep global sections around. */
            assert(sp->org_x == -1u);
            sp = *psp;
            *psp = (*psp)->next;
            kill_Section(sp);
            free(sp);
        } else {
            /* Non-global sections had better have an ORG directive. */
            assert(sp->rowslen > 0);
            if (sp->org_x == -1u) {
                fatal_error(&sp->rows[0], -1,
                    "Non-global section contains no .ORG directive");
            }
            psp = &(*psp)->next;
        }
    }

puts("Resolving macros in EQUs...");
    for (ep = global_equs; ep != NULL; ep = ep->next) {
        if (ep->value == -1u) {
            resolve_macros(NULL, &ep->replacement);
            /* Any dots have already been expanded by extract_directives(). */
        }
    }

puts("Trimming sections and marking BSS...");
    for (sp = secthead; sp != NULL; sp = sp->next)
      trim_section(sp);

puts("Dumping ELF binary...");
    if (global_entry != -1u)
      FungELF_entrypoint(global_entry);
    if (global_fillvalue != -1u)
      FungELF_fillvalue(global_fillvalue);

    /* Now parse and assemble each individual cell. */
    for (sp = secthead; sp != NULL; sp = sp->next) {
        int i, j;
        unsigned int e_origin = (sp->org_y << 9) | sp->org_x;
        unsigned int e_size = (sp->size_y << 9) | sp->size_x;
        unsigned int e_flags;

        assert(sp->size_x >= 0);
        assert(sp->size_y >= 0);
        assert(!sp->size_x == !sp->size_y);

        if (sp->global) {
            /* it's been deleted by trim_section() */
            sp->global = 0;
            continue;
        } else if (sp->is_bss) {
            assert(sp->fillvalue != -1u);
            e_flags = PF_FUNG_FILLVALUE | sp->fillvalue;
            FungELF_addbss(e_origin, e_size, e_flags);
            continue;
        }

        sp->assembled = malloc(sp->size_x * sp->size_y * sizeof *sp->assembled);
        assert(sp->assembled != NULL);

        if (sp->fillvalue != -1u) {
            assert((sp->fillvalue & ~0777777) == 0);
            for (i=0; i < sp->size_x * sp->size_y; ++i)
              sp->assembled[i] = sp->fillvalue;
        } else {
            memset(sp->assembled, 0xFF,
                sp->size_x * sp->size_y * sizeof *sp->assembled);
        }

        for (i=0; i < sp->size_y; ++i) {
            assert(i < sp->rowslen);
            for (j=0; j < sp->rows[i].len; ++j) {
                struct String *s = &sp->rows[i].s[j];
                unsigned int code;
                assert(s != NULL);
                if (s->comment) continue;
                if (s->x >= sp->size_x) break;
                assert(s->y == i);
                if (s->text[0] == '.') continue;
                replace_dot(&s->text, sp->org_x+s->x, sp->org_y+s->y);
                code = fungasm(s->text);
                if (code & ~0777777)
                  fatal_error(&sp->rows[i], s->start, fungasm_error);
                sp->assembled[(s->y * sp->size_x) + s->x] = code;
            }
        }

        if (sp->fillvalue == -1u)
          e_flags = makebgfill(sp);
        else
          e_flags = PF_FUNG_FILLVALUE | sp->fillvalue;
        e_flags |= sp->flags;

        FungELF_addtext(e_origin, e_size, e_flags, sp->assembled);
    }

    FungELF_writefile(out);
    FungELF_done();

    kill_all_globals();
    return 0;
}


/* Free the global lists. */
static void kill_all_globals(void)
{
    while (secthead != NULL) {
        struct Section *sp = secthead->next;
        kill_Section(secthead);
        free(secthead);
        secthead = sp;
    }
    while (global_equs != NULL) {
        struct EquDef *ep = global_equs->next;
        kill_Equ(global_equs);
        free(global_equs);
        global_equs = ep;
    }
    while (global_macros != NULL) {
        struct MacroDef *mp = global_macros->next;
        free(global_macros->text);
        free(global_macros->replacement);
        free(global_macros);
        global_macros = mp;
    }
}


static struct Section *new_Section(struct Section *next)
{
    struct Section *sp = malloc(sizeof *sp);
    assert(sp != NULL);
    sp->rows = NULL;
    sp->rowslen = sp->rowscap = 0;
    sp->local_macros = NULL;
    sp->global = 0;
    sp->next = next;

    sp->contains_offset_org_directive = 0;

    sp->fillvalue = -1u;
    sp->org_x = sp->org_y = -1u;
    sp->size_x = sp->size_y = 0;
    sp->flags = 0;
    sp->is_bss = 0;
    sp->assembled = NULL;
    return sp;
}

static void kill_Section(struct Section *sp)
{
    /* Do not free(sp); just delete all its members. */
    int i, j;
    assert(sp != NULL);
    while (sp->local_macros != NULL) {
        struct MacroDef *mp = sp->local_macros->next;
        free(sp->local_macros->text);
        free(sp->local_macros->replacement);
        free(sp->local_macros);
        sp->local_macros = mp;
    }
    for (i=0; i < sp->rowslen; ++i) {
        struct Row *r = &sp->rows[i];
        for (j=0; j < r->len; ++j) {
            free(r->s[j].text);
        }
        free(r->s);
    }
    free(sp->rows);
    if (sp->assembled != NULL)
      free(sp->assembled);
}


static void add_row(struct Section *sp, const struct Row *r)
{
    if (sp->rowslen >= sp->rowscap) {
        sp->rowscap = sp->rowslen + 5;
        sp->rows = realloc(sp->rows, sp->rowscap * sizeof *sp->rows);
        assert(sp->rows != NULL);
    }
    assert(sp->rowslen < sp->rowscap);
    sp->rows[sp->rowslen++] = *r;
}

static int add_macro(struct MacroDef **mlist, const char *text, int textlen,
                      const char *replacement)
{
    struct MacroDef *mp = find_macro(*mlist, text, textlen);
    if (mp != NULL)
      return -1;
    mp = malloc(sizeof *mp);
    assert(mp != NULL);
    mp->text = malloc(textlen+1);
    assert(mp->text != NULL);
    memcpy(mp->text, text, textlen);
    mp->text[textlen] = '\0';
    mp->replacement = malloc(strlen(replacement)+1);
    assert(mp->replacement != NULL);
    strcpy(mp->replacement, replacement);
    mp->unusable = 0;
    mp->next = *mlist;
    *mlist = mp;
    return 0;
}



/*
 * Given an input line, return an array of Strings. A String is a
 * sequence of characters, terminated by two or more spaces.
 */
static struct Row *split_strings(const char *line)
{
    static struct Row ret;
    int ridx, rlen = 0;
    int in_string;
    int spaces_until_end;
    int i, start;
    int realpos, realstart;

    assert(line != NULL);

    /* Do the splitting in two largely identical passes.
     * First, get the number of strings on this line. */
    in_string = 0;
    realpos = 0;
    spaces_until_end = 0;
    for (i=0; /* forever */; ++i) {
        if (line[i] == 0x1A) {
            if (rlen > 0 || in_string) {
                fatal_error(NULL, -1, "DOS end-of-file ^Z encountered"
                            " in the middle of a line");
            }
            break;
        } else if (strchr("\r\n", line[i]) != NULL) {
            spaces_until_end = 0;
            if (!in_string) break;
        } else if (line[i] == '\t') {
            int added_spaces = TABSTOP - (realpos % TABSTOP);
            spaces_until_end -= added_spaces;
            realpos += added_spaces;
        } else {
            realpos += 1;
            if (line[i] == ' ')
              spaces_until_end -= 1;
            else
              spaces_until_end = 2;
        }
        if (in_string && (spaces_until_end <= 0)) {
            /* We just finished a string. */
            rlen += 1;
            in_string = 0;
        } else if (!in_string && (spaces_until_end > 0)) {
            /* We just got the start of a new string. */
            in_string = 1;
        }
        if (line[i] == '\0')
          break;
    }

    /* No strings on this line? */
    if (rlen == 0)
      return NULL;

    ret.len = rlen;
    ret.s = malloc(rlen * sizeof *ret.s);
    assert(ret.s != NULL);

    /* Now, fill in the array elements. */
    in_string = 0;
    realpos = 0;
    spaces_until_end = 0;
    ridx = 0;
    for (i=0; /* forever */; ++i) {
        if (strchr("\r\n", line[i]) != NULL) {
            spaces_until_end = 0;
        } else if (line[i] == '\t') {
            int added_spaces = TABSTOP - (realpos % TABSTOP);
            spaces_until_end -= added_spaces;
            realpos += added_spaces;
        } else {
            realpos += 1;
            if (line[i] == ' ')
              spaces_until_end -= 1;
            else
              spaces_until_end = 2;
        }
        if (in_string && (spaces_until_end <= 0)) {
            /* We just finished a string. */
            int len;
            assert(ridx < rlen);
            ret.s[ridx].start = realstart;
            ret.s[ridx].text = malloc(realpos - realstart + 2);
            assert(ret.s[ridx].text != NULL);
            len = detab_selection(ret.s[ridx].text, line+start, i-start, realstart);
            ret.s[ridx].len = len;
            ret.s[ridx].comment = 0;
            ret.s[ridx].x = 0;
            ret.s[ridx].y = 0;
            ridx += 1;
            if (line[i] == '\0')
              break;
            in_string = 0;
        } else if (!in_string && (spaces_until_end > 0)) {
            /* We just got the start of a new string. */
            start = i;
            realstart = realpos;
            in_string = 1;
        }
        if (line[i] == '\0')
          break;
    }

    assert(ret.len > 0);
    return &ret;
}


/*
 * Take the first 'len' characters of 'src', and copy them into 'dst',
 * expanding any tabs as you go. 'realpos' will tell us how close we are
 * to the next tabstop, so we insert the right number of spaces.
 * Null-terminate the resulting string. If it has whitespace at the
 * beginning or the end, trim that whitespace.
 */
static int detab_selection(char *dst, const char *src, int len, int realpos)
{
    int si, di=0;
    for (si=0; si < len; ++si) {
        if (src[si] == '\t') {
            int added_spaces = TABSTOP - (realpos % TABSTOP);
            realpos += added_spaces;
            if (di != 0) {
                while (added_spaces--)
                  dst[di++] = ' ';
            }
        } else {
            if (!(src[si]==' ' && di==0))
              dst[di++] = src[si];
            realpos += 1;
        }
    }
    while (di > 0 && isspace(dst[di-1]))
      --di;
    dst[di] = '\0';
    return di;
}


#if 0
static void mark_comments(struct Section *sp)
{
    int i, j;
    for (i=0; i < sp->rowslen; ++i) {
        struct Row *r = &sp->rows[i];
        for (j=0; j < r->len; ++j) {
            struct String *s = &r->s[j];
            struct Row *rr;
            int jj;
            const char *end;

            /* Is this already a comment? Look for its end. */
        look_for_terminator:
            if (s->comment == 1) {
                const char *end = strchr(s->text, '}');
                if (end == NULL) goto flow_downward;
                if (end != s->text + s->len-1) {
                    fatal_error(r, s->start + (end - s->text),
                        "Comment terminator not followed by 2 spaces");
                }
                continue;
            } else if (s->comment == 2) {
                const char *end = strstr(s->text, "*)");
                if (end == NULL) goto flow_downward;
                if (end != s->text + s->len-2) {
                    fatal_error(r, s->start + (end+1 - s->text),
                        "Comment terminator not followed by 2 spaces");
                }
                continue;
            }

            /* This is not yet a comment. Try to make it one. */
            assert(s->comment == 0);
            end = strstr(s->text, "(*");
            if (end != NULL && end != s->text) {
                fatal_error(r, s->start + (end - s->text),
                    "Comment delimiter not preceded by 2 spaces");
            }
            if (end != s->text) {
                /* Our searching for '{' in the middle of strings is slightly
                 * broken, because "'{'" is valid while "'m'{foo}" is invalid.
                 * But a little error-checking is better than none. */
                end = strchr(s->text, '{');
                if (end != NULL && end != s->text && end[-1] != '\'') {
                    fatal_error(r, s->start + (end - s->text),
                        "Comment delimiter not preceded by 2 spaces");
                }
            }
            if (end != NULL) {
                /* This string does begin a comment. Go look for its end. */
                s->comment = 1 + (s->text[0] == '(');
                goto look_for_terminator;
            } else {
                /* This string is not part of any comment. */
                continue;
            }


            /* Is there a string abutting this one from below? */
        flow_downward:
            if (i+1 >= sp->rowslen)
              fatal_error(r, s->start, "Unterminated comment");
            rr = &sp->rows[i+1];
            for (jj=0; jj < rr->len; ++jj) {
                if (rr->s[jj].start+rr->s[jj].len <= s->start) continue;
                if (rr->s[jj].start >= s->start+s->len)
                  fatal_error(r, s->start, "Unterminated comment");
                break;
            }
            if (jj == rr->len)
              fatal_error(r, s->start, "Unterminated comment");
            /* Found a string under this one! But are there more? */
            if (jj+1 < rr->len && rr->s[jj+1].start < s->start + s->len) {
                fatal_error(rr, rr->s[jj+1].start,
                    "Competing comment continuations");
            }
            /* Okay, we'll call this abutting string a comment, too. */
            rr->s[jj].comment = s->comment;
        }
    }
}
#endif


static void resolve_grid(struct Section *sp)
{
    int i, j;
    short *is_x;
    int width_in_columns = 0;

    /* Find the width of the page, in (input) columns */
    for (i=0; i < sp->rowslen; ++i) {
        struct Row *r = &sp->rows[i];
        for (j = r->len-1; j >= 0; --j) {
            struct String *s = &r->s[j];
            if (s->start < width_in_columns) break;
            if (s->comment) continue;
            width_in_columns = s->start;
            break;
        }
    }

    is_x = calloc(width_in_columns+1, sizeof *is_x);
    assert(is_x != NULL);

    /* Figure out which columns correspond to Funge-space x values */
    for (i=0; i < sp->rowslen; ++i) {
        struct Row *r = &sp->rows[i];
        for (j = 0; j < r->len; ++j) {
            struct String *s = &r->s[j];
            int k;
            if (s->comment) continue;
            is_x[s->start] = 1;
            for (k = 1; k <= 3; ++k) {
                if (s->start >= k && is_x[s->start - k])
                  fatal_error(r, s->start, "Instruction not on grid");
                if (s->start+k <= width_in_columns && is_x[s->start + k])
                  fatal_error(r, s->start, "Instruction not on grid");
            }
        }
    }

    /* Assign the first x "1", the second x "2",... */
    for (i=0, j=1; i <= width_in_columns; ++i) {
        if (is_x[i] != 0)
          is_x[i] = j++;
    }

    /* Assign x and y coordinates to all the
     * non-comment Strings in the grid.
     */
    sp->size_x = 0;
    sp->size_y = 0;
    for (i=0; i < sp->rowslen; ++i) {
        struct Row *r = &sp->rows[i];
        for (j = 0; j < r->len; ++j) {
            struct String *s = &r->s[j];
            if (s->comment) continue;
            assert(s->start <= width_in_columns);
            s->x = is_x[s->start] - 1;
            s->y = i;
            assert(s->x >= 0);
            assert(s->x >= 0);
            if (s->text[0] != '.' || !strncmp(s->text, ".WO", 3)) {
                /* Non-directives get sizes. */
                if (s->x >= sp->size_x) sp->size_x = s->x+1;
                sp->size_y = s->y+1;
            }
        }
    }

    free(is_x);
}


static void extract_directives(struct Section *sp)
{
    int i, j;
    for (i=0; i < sp->rowslen; ++i) {
        for (j=0; j < sp->rows[i].len; ++j) {
            struct String *s = &sp->rows[i].s[j];
            if (s->comment) continue;
            if (s->text[0] != '.') continue;
            if (!strncmp(s->text, ".MACRO", 6) && isspace(s->text[6])) {
                int len;
                const char *ip = find_identifier(s->text+6, &len);
                const char *replacement;
                if (ip == NULL) {
                    fatal_error(&sp->rows[i], s->start,
                        ".MACRO must be followed by an identifier");
                }
                replacement = ip+len;
                if (*replacement != '\0' && *replacement != ' ') {
                    fatal_error(&sp->rows[i], s->start + (replacement - s->text),
                        "Missing whitespace in macro definition");
                }
                while (*replacement == ' ')
                  ++replacement;
                if (add_macro(&sp->local_macros, ip, len, replacement) != 0) {
                    fatal_error(&sp->rows[i], s->start + (ip - s->text),
                        "Redefinition of macro %.*s", len, ip);
                }
            } else if (!strncmp(s->text, ".EQU", 4) && isspace(s->text[4])) {
                int len;
                const char *ip = find_identifier(s->text+4, &len);
                const char *replacement;
                struct EquDef *ep;
                if (ip == NULL) {
                    fatal_error(&sp->rows[i], s->start,
                        ".EQU must be followed by an identifier");
                }
                replacement = ip+len;
                if (*replacement != '\0' && *replacement != ' ') {
                    fatal_error(&sp->rows[i], s->start + (replacement - s->text),
                        "Missing whitespace in .EQU directive");
                }
                while (*replacement == ' ')
                  ++replacement;
                ep = new_Equ(ip, len, replacement, global_equs);
                /* do not replace_dot() yet, because we don't know the .ORG */
                ep->sp = sp;
                ep->s = s;
                if (find_equ(ep->text, len) != NULL) {
                    fatal_error(&sp->rows[i], s->start + (ip - s->text),
                        "Redefinition of symbol %.*s", len, ip);
                } else {
                    global_equs = ep;
                }
            } else if (!strncmp(s->text, ".ENTRY", 6) && isspace(s->text[6])) {
                /* do nothing; it will be handled later, because we can't
                 * parse it until we know all the .EQU symbols' values */
            } else if (!strncmp(s->text, ".ORG", 4) && isspace(s->text[4])) {
                int len;
                unsigned int value = eval_scalar(s->text+5, &len);
                unsigned int ox = value & 0777;
                unsigned int oy = (value >> 9) & 0777;
                /* An .ORG directive refers to its own position. Notice that
                 * this can cause funny behaviors when the only thing north
                 * of an .ORG directive is comments, and the section contains
                 * a .SIZE directive; the user might expect the section to
                 * be a rectangle with the northwest corner at .ORG, but
                 * in fact the northwest corner will be at the comment! */
                ox = (ox - s->x) & 0777;
                oy = (oy - s->y) & 0777;
                if (value == -1u)
                  fatal_error(&sp->rows[i], s->start+5, "Expected vector after .ORG");
                if (s->text[5+len] != '\0')
                  fatal_error(&sp->rows[i], s->start+5+len, "Extra text after .ORG");
                if (sp->org_x != -1u) {
                    assert(sp->org_y != -1u);
                    if (sp->org_x != ox || sp->org_y != oy)
                      fatal_error(&sp->rows[i], s->start,
                          "Inconsistent .ORG directives within the same section");
                } else {
                    if (s->x != 0 || s->y != 0)
                      sp->contains_offset_org_directive = 1;
                    sp->org_x = ox;
                    sp->org_y = oy;
                }
            } else if (!strncmp(s->text, ".FLAGS", 6) && isspace(s->text[6])) {

                /* I haven't implemented .FLAGS yet;
                 * I'm not sure what its syntax should be. */
                warning(&sp->rows[i], s->start, ".FLAGS directive is not implemented yet");

            } else if (!strncmp(s->text, ".FILL", 5) && isspace(s->text[5])) {
                int len;
                unsigned int value = eval_scalar(s->text+6, &len);
                if (value == -1u)
                  fatal_error(&sp->rows[i], s->start+6, "Expected integer after .FILL");
                if (s->text[6+len] != '\0')
                  fatal_error(&sp->rows[i], s->start+6+len, "Extra text after .FILL");
                if (sp->fillvalue == -1u)
                  sp->fillvalue = value & 0777777;
                else if (sp->fillvalue != (value & 0777777))
                  fatal_error(&sp->rows[i], s->start,
                      "Inconsistent .FILL directives within the same section");
            } else if (!strncmp(s->text, ".SIZE", 5) && isspace(s->text[5])) {
                int len;
                unsigned int value = eval_scalar(s->text+6, &len);
                if (value == -1u)
                  fatal_error(&sp->rows[i], s->start+6, "Expected vector after .SIZE");
                if (s->text[6+len] != '\0')
                  fatal_error(&sp->rows[i], s->start+6+len, "Extra text after .SIZE");
                if (sp->size_x != 0) {
                    if ((sp->size_x != (int)(value & 0777)) ||
                        (sp->size_y != (int)((value >> 9) & 0777)))
                      fatal_error(&sp->rows[i], s->start,
                          "Inconsistent .SIZE directives within the same section"
                          " (previous size was %03o,%03o)", sp->size_x, sp->size_y);
                } else {
                    sp->size_x = (value & 0777);
                    sp->size_y = ((value >> 9) & 0777);
                    if (sp->size_x == 0 || sp->size_y == 0)
                      fatal_error(&sp->rows[i], s->start+6,
                          "Explicitly specified .SIZE cannot be zero");
                }
            } else {
                fatal_error(&sp->rows[i], s->start, "Unrecognized directive");
            }
        }
    }
}

static void decide_global(struct Section *sp)
{
    int i, j;

    /* If 'sp' contains only directives and comments, and no .ORG, then
     * it's a global section. */

    if (sp->org_x != -1u) return;
    if (sp->size_x != 0) return;

    for (i=0; i < sp->rowslen; ++i) {
        for (j=0; j < sp->rows[i].len; ++j) {
            struct String *s = &sp->rows[i].s[j];
            if (s->comment) continue;
            if (s->text[0] != '.') return;
        }
    }
    sp->global = 1;

    /* Okay, copy its local stuff over to the global stuff. */
    while (sp->local_macros != NULL) {
        struct MacroDef *mp = find_macro(global_macros,
            sp->local_macros->text, strlen(sp->local_macros->text));
        if (mp != NULL)
          fatal_error(NULL, -1, "Redefinition of global macro \"%s\"", mp->text);
        mp = sp->local_macros->next;
        sp->local_macros->next = global_macros;
        global_macros = sp->local_macros;
        sp->local_macros = mp;
    }

    if (sp->fillvalue != -1u) {
        if (global_fillvalue != -1u && global_fillvalue != sp->fillvalue)
          fatal_error(NULL, -1, "Inconsistent definitions of global .FILL");
        global_fillvalue = sp->fillvalue;
    }

    global_flags |= sp->flags;
}


/*
 * Replace 'text' with a new string, with all its macros expanded.
 * Macros expand recursively, but we do "poisoning" the same way
 * the C preprocessor does. However, in our case, a recursive macro
 * expansion is actually a fatal error. Or, to put it another way,
 * we do allow recursive macros, but we helpfully detect infinite
 * recursion and error out --- and with our limited macro syntax,
 * all recursion is guaranteed to be infinite!
 *   By the time we get here, all recursive macros and equs will
 * have been marked "unusable", so we don't actually need to check
 * for recursion here.
 */
static void resolve_macros(struct Section *sp, char **text)
{
    char *ip = (*text);
    int len;
    while ((ip = find_identifier(ip, &len)) != NULL) {
        /* If this identifier is a macro, expand it. */
        struct MacroDef *mp = NULL;
        if (sp != NULL)
          mp = find_macro(sp->local_macros, ip, len);
        if (mp == NULL)
          mp = find_macro(global_macros, ip, len);
        if (mp != NULL) {
            int replen;
            if (mp->unusable)
              fatal_error(NULL, -1, "Use of infinitely recursive macro %s", mp->text);
            replen = strlen(mp->replacement);
            if (replen > len) {
                char *newtext = malloc(strlen(*text) + replen-len + 1);
                assert(newtext != NULL);
                strcpy(newtext, *text);
                ip = newtext + (ip - *text);
                free(*text);
                *text = newtext;
            }
            if (replen != len)
              memmove(ip+replen, ip+len, strlen(ip+len)+1);
            memcpy(ip, mp->replacement, replen);
        } else {
            ip += len;
        }
    }
}

static void replace_dot(char **ptext, int x, int y)
{
    char *text = *ptext;
    char *ip = text;
    int len;
    const char *notnow = NULL;

    /* Test whether we need to do this, but the speedy
     * test is overly conservative; we might have
     * ".EQU DOT '.'", which shouldn't be replaced. */
    if (strchr(ip, '.') == NULL)
      return;

    while ((ip = find_token(ip, &len, NULL)) != NULL) {
        if (ip == notnow) {
            notnow = NULL;
        } else if (*ip == ')' || isalnum(*ip)) {
            /* "(A,B).x" or "FOO.x" or "001002.x", but not "ENTRY .+1";
             * distinguish based on whitespace before the dot */
            notnow = ip+len;
        } else if (len == 1 && *ip == '.') {
            /* Replace this dot with (x,y). */
            char *newtext = malloc((ip-text) + 8 + strlen(ip) + 1);
            assert(newtext != NULL);
            if (ip > text)
              memcpy(newtext, text, ip-text);
            sprintf(newtext+(ip-text), "(%03o,%03o)",
                x & 0777, y & 0777);
            strcpy(newtext+(ip-text)+9, ip+1);
            ip = newtext+(ip-text)+9;
            assert(ip[-1] == ')');
            free(text);
            *ptext = text = newtext;
            notnow = ip+1;
        }
        ip += len;
    }
}


static struct EquDef *new_Equ(const char *text, int len,
        const char *replacement, struct EquDef *next)
{
    struct EquDef *ep = malloc(sizeof *ep);
    assert(ep != NULL);
    ep->text = malloc(len+1);
    assert(ep->text != NULL);
    memcpy(ep->text, text, len);
    ep->text[len] = '\0';
    ep->replacement = malloc(strlen(replacement)+1);
    assert(ep->replacement != NULL);
    strcpy(ep->replacement, replacement);
    ep->sp = NULL;
    ep->s = NULL;
    ep->unusable = 0;
    ep->value = -1u;
    ep->next = next;
    return ep;
}

static void kill_Equ(struct EquDef *ep)
{
    assert(ep != NULL);
    assert(ep->text != NULL);
    assert(ep->replacement != NULL);
    free(ep->text);
    free(ep->replacement);
}

/* Resolve and return the "true value" of an EQU symbol. */
unsigned int true_value(const char *equname, int len)
{
    struct EquDef *ep = find_equ(equname, len);
    if (ep == NULL) return -1u;
    if (ep->unusable) return -1u;
    if (ep->value == -1u) {
        int len;
        ep->unusable = 1;
        ep->value = eval_scalar(ep->replacement, &len);
        if (ep->value != -1u && ep->replacement[len] != '\0') {
            fatal_error(NULL, -1, "Unexpected text after .EQU directive"
                " defining symbol \"%s\": \"%s\"", ep->text,
                ep->replacement+len);
        }
        if (ep->value != -1u)
          ep->unusable = 0;
    }
    return ep->value;
}

static struct EquDef *find_equ(const char *text, int len)
{
    struct EquDef *ep;
    for (ep = global_equs; ep != NULL; ep = ep->next) {
        if (!strncmp(ep->text, text, len) && ep->text[len] == '\0')
          return ep;
    }
    return NULL;
}

static struct MacroDef *find_macro(struct MacroDef *mlist, const char *text, int len)
{
    if (len <= 0) return NULL;
    for ( ; mlist != NULL; mlist = mlist->next) {
        if (strncmp(text, mlist->text, len) == 0)
          if (mlist->text[len] == '\0')
            break;
    }
    return mlist;
}


/* Given "]-3ABCf87-", return a pointer to the 'A', and set 'len' to 6. */
static char *find_identifier(const char *text, int *len)
{
    const char *ret;
    while (1) {
        if (isalpha(*text) || *text == '_') break;
        if (*text == '\0') return NULL;
        if (*text == '.') {
            /* Skip directives and masking-mode suffixes. */
            ++text;
            while (isalnum(*text) || *text == '_')
                ++text;
        } else if (isdigit(*text)) {
            /* Skip numbers like "0xabc" and "9d". */
            ++text;
            while (isalnum(*text) || *text == '_')
                ++text;
        } else {
            /* Skip non-alpha characters one at a time. */
            ++text;
        }
    }
    ret = text;
    while (isalnum(*text) || *text == '_')
      ++text;
    *len = text - ret;
    return (char *)ret;  /* casting away const */
}


static void trim_section(struct Section *sp)
{
    int i, j;
    int minx, miny;
    int maxx, maxy;
    assert(sp != NULL);
    maxx = maxy = -1;
    minx = sp->size_x;
    miny = sp->size_y;
    for (i=0; i < sp->rowslen; ++i) {
        for (j=0; j < sp->rows[i].len; ++j) {
            struct String *s = &sp->rows[i].s[j];
            assert(s != NULL);
            if (s->comment || s->text[0] == '.') continue;
            if (s->y > maxy) maxy = s->y;
            if (s->x > maxx) maxx = s->x;
            if (s->y < miny) miny = s->y;
            if (s->x < minx) minx = s->x;
        }
    }
    if (maxx == -1) {
        assert(maxy == -1);
        assert(minx == sp->size_x);
        assert(miny == sp->size_y);
        if (sp->fillvalue != -1u) {
            sp->is_bss = 1;
            if (sp->contains_offset_org_directive) {
                assert(sp->rowslen > 0);
                warning(&sp->rows[0], -1,
                    "BSS section's .ORG directive is not in the northwest corner.\n"
                    "This may indicate a mistake.");
            }
        } else {
            /* there's nothing to output; delete it */
            if (sp->org_x != -1u) {
                assert(sp->rowslen > 0);
                warning(&sp->rows[0], -1,
                    "Global section contains a useless .ORG directive");
            }
            sp->global = 1;
        }
        return;
    }
    if (maxy < sp->size_y-1)
      sp->size_y = maxy;
    if (maxx < sp->size_x-1)
      sp->size_x = maxx;
    if (miny > 0) {
        assert(miny < sp->rowslen);
        assert(miny < sp->size_y);
        assert(sp->org_y != -1u);
        for (i=0; i < miny; ++i) {
            struct Row *r = &sp->rows[i];
            for (j=0; j < r->len; ++j) {
                free(r->s[j].text);
            }
            free(r->s);
        }
        sp->rowslen -= miny;
        memmove(&sp->rows[0], &sp->rows[miny], sp->rowslen * sizeof *sp->rows);
        sp->size_y -= miny;
        sp->org_y += miny;
        for (i=0; i < sp->rowslen; ++i) {
            struct Row *r = &sp->rows[i];
            for (j=0; j < r->len; ++j)
              r->s[j].y -= miny;
        }
    }
    if (minx > 0) {
        assert(minx < sp->size_x);
        assert(sp->org_x != -1u);
        for (i=0; i < sp->rowslen; ++i) {
            struct Row *r = &sp->rows[i];
            while ((r->len != 0) && (r->s[0].x < minx)) {
                free(r->s[0].text);
                r->len -= 1;
                memmove(&r->s[0], &r->s[1], r->len * sizeof r->s[0]);
            }
            for (j=0; j < r->len; ++j)
              r->s[j].x -= minx;
        }
        sp->size_x -= minx;
        sp->org_x += minx;
    }
    return;
}


static unsigned int makebgfill(const struct Section *sp)
{
    unsigned int goodbgfill = 0x424242;  /* LX.V 2,[4^2] */
    int needs_fill = 0;
    int failed = 0;
    int i;

    assert(sp->assembled != NULL);

    for (i=0; i < sp->size_x * sp->size_y; ++i) {
        if (sp->assembled[i] == -1u) {
            needs_fill = 1;
        } else {
            if (sp->assembled[i] == goodbgfill) {
                goodbgfill = (goodbgfill+1) & 0777777;
                failed = 1;
            }
        }
    }
    if (!needs_fill)
      return 0;

    /* This is mostly paranoia. We never expect this loop to execute. */
    while (failed) {
        failed = 0;
        for (i=0; i < sp->size_x * sp->size_y; ++i) {
            if (sp->assembled[i] == goodbgfill) {
                goodbgfill = (goodbgfill+1) & 0777777;
                failed = 1;
                if (goodbgfill == 0x424242) {
                    assert(sp->rowslen > 0);
                    fatal_error(&sp->rows[0], -1,
                        "Couldn't pick a background value for this section");
                }
            }
        }
    }
      
    for (i=0; i < sp->size_x * sp->size_y; ++i) {
        if (sp->assembled[i] == -1u)
          sp->assembled[i] = goodbgfill;
    }
    return PF_FUNG_NEVERMYFILL | PF_FUNG_FILLVALUE | goodbgfill;
}


#define SCREENW 70
#define FUDGE 2

/* Returns the number of columns to subtract from 'arrow' to make it line up right. */
static int print_Row(FILE *fp, const struct Row *r, int arrow)
{
    int linelen = r->s[r->len-1].start + r->s[r->len-1].len;
    int curpos, i;
    int start=0, end=SCREENW-3;  /* not counting the "..." */

    /* If the whole line won't fit on the screen at once,
     * try to print just the most relevant portion. */

    /* First, get rid of leading whitespace. */
    if (linelen > end) {
        start = r->s[0].start - 1;
        if (start + SCREENW >= linelen) {
            /* The whole line will fit. */
            end = linelen;
        } else {
            /* We must still truncate it at the end. */
            end = start + (SCREENW-3);
        }
    }

    /* Next, try to center the arrow. */
    if (linelen > end && arrow > start+(SCREENW/2)+4) {
        int fudge_to_nice_start = 0;
        int fudge_to_nice_end = 0;
        end = (linelen < arrow + SCREENW/2)? linelen: arrow + SCREENW/2;
        start = end - (SCREENW-6);

        for (i=0; i < r->len; ++i) {
            if (r->s[i].start + r->s[i].len < start) continue;
            fudge_to_nice_start = start - r->s[i].start;
            break;
        }
        if (end != linelen) {
            for ( ; i < r->len; ++i) {
                if (r->s[i].start + r->s[i].len < end) continue;
                fudge_to_nice_end = r->s[i].start + r->s[i].len - end;
                break;
            }
        }

        if (fudge_to_nice_start + fudge_to_nice_end < FUDGE) {
            start -= fudge_to_nice_start;
            end -= fudge_to_nice_end;
        } else {
            if (fudge_to_nice_start < FUDGE &&
                fudge_to_nice_start < fudge_to_nice_end)
              start -= fudge_to_nice_start;
            else if (fudge_to_nice_end < FUDGE)
              end += fudge_to_nice_end;
        }
    }

    if (start > r->s[0].start)
      fprintf(fp, "...");

    curpos = 0;
    for (i=0; i < r->len; ++i) {
        if (r->s[i].start + r->s[i].len < start) {
            curpos = r->s[i].start + r->s[i].len;
            continue;
        }
        if (r->s[i].start >= end)
          break;

        for ( ; curpos < r->s[i].start; ++curpos) {
            if (curpos >= start)
              putc(' ', fp);
        }

        if (curpos + r->s[i].len <= end)
          fputs(r->s[i].text, fp);
        else
          fprintf(fp, "%.*s", end-curpos, r->s[i].text);
        fflush(fp);
        curpos = r->s[i].start + r->s[i].len;
    }

    if (end < linelen)
      fprintf(fp, "...");
    putc('\n', fp);
    return (start > r->s[0].start)? start-3 : start;
}


void fatal_error(const struct Row *r, int pos, const char *msg, ...)
{
    va_list ap;
    int print_arrow = (pos >= 0);
    va_start(ap, msg);
    if (r != NULL)
      pos -= print_Row(stdout, r, pos);
    if (print_arrow)
      printf("%*s\n", pos+1, "^");
    printf("Error: ");
    vprintf(msg, ap);
    putchar('\n');
    va_end(ap);
    exit(EXIT_FAILURE);
}

void warning(const struct Row *r, int pos, const char *msg, ...)
{
    va_list ap;
    int print_arrow = (pos >= 0);
    va_start(ap, msg);
    if (r != NULL)
      pos -= print_Row(stdout, r, pos);
    if (print_arrow)
      printf("%*s\n", pos+1, "^");
    printf("Warning: ");
    vprintf(msg, ap);
    putchar('\n');
    va_end(ap);
    return;
}

static void dohelp(int man)
{
    puts("Usage: fungasm program.asm [output.elf]");
    if (man) {
        puts("");
        puts("  fungasm is the Fungus assembler. It assembles \"program.asm\" into");
        puts("\"program.elf\", or into a filename provided on the command line.");
        puts("For details of the Fungus assembly language, see the HTML manual.");
    }
    exit(EXIT_FAILURE);
}
