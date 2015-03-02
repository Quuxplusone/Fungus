
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "asmtypes.h"


void mark_comments(struct Section *sp);
 static void pull_out_delimiters(struct Row *r, struct String *s);
 static int look_for_terminator(struct Row *r, struct String *s);
 static void flow_downward(struct Row *r, struct Row *rr, struct String *head);


/*
 * This function deletes any complete comments embedded in its string
 * argument. For example, it will turn "foo{bar}baz" into "foobaz".
 *   If the string argument contains the beginning of a comment, but
 * not its end, then we report an error; multi-line comments are
 * required to start in their own Strings (i.e., separated from the
 * preceding instruction by at least two spaces).
 *   If the string contains the end of a comment, but not its beginning,
 * then we were in a comment to begin with, and thus shouldn't have
 * called this function.
 */
static void pull_out_delimiters(struct Row *r, struct String *s)
{
    int i;
    for (i=0; s->text[i] != '\0'; ++i) {
        if (i == 0 && s->text[i] == ' ') {
            memmove(s->text, s->text+1, 1+strlen(s->text+1));
            i = -1;
            continue;
        } else if (s->text[i] == '\'') {
            /* Skip over character literals. */
            int quote = i;
            do {
                ++i;
                /* The order of these tests matters! */
                if (s->text[i] == '\'') break;
                if (s->text[i] == '\\') ++i;
                if (s->text[i] == '\0')
                  fatal_error(r, s->start + quote,
                     "Unterminated character literal");
            } while (1);
            continue;
        } else if (s->text[i] == '{') {
            int comment = i;
            do {
                ++i;
                if (s->text[i] == '\0') {
                    if (comment == 0) return;
                    fatal_error(r, s->start + comment,
                       "Unterminated comment not preceded by 2 spaces");
                }
            } while (s->text[i] != '}');
            /* Remove the comment. */
            memmove(&s->text[comment], &s->text[i+1], 1+strlen(&s->text[i+1]));
            i = comment-1;
            continue;
        } else if (!strncmp(&s->text[i], "(*", 2)) {
            int comment = i;
            do {
                ++i;
                if (s->text[i] == '\0') {
                    if (comment == 0) return;
                    fatal_error(r, s->start + comment,
                        "Unterminated comment not preceded by 2 spaces");
                }
            } while (strncmp(&s->text[i], "*)", 2) != 0);
            /* Remove the comment. */
            memmove(&s->text[comment], &s->text[i+2], 1+strlen(&s->text[i+2]));
            i = comment-1;
            continue;
        }
    }
    return;
}

static int look_for_terminator(struct Row *r, struct String *s)
{
    int i;
    char *end;
    assert(s->comment != 0);
    end = (s->comment & 1)? strchr(s->text, '}'): strstr(s->text, "*)");
    if (end == NULL) return 0;
    if (end[(s->comment & 1)? 1: 2] != '\0') {
        fatal_error(r, s->start + (end - s->text),
            "Comment terminator not followed by 2 spaces");
    }
    /* Okay, the user is trying to end this comment. Can he do it? */
    for (i=0; i < r->len; ++i) {
        struct String *ss = &r->s[i];
        if (ss == s) continue;
        if (ss->comment == s->comment)
          fatal_error(r, s->start, "Cannot close comment continued elsewhere");
    }
    return 1;
}

static void flow_downward(struct Row *r, struct Row *rr, struct String *head)
{
    int j;
    int continued = 0;
    for (j=0; j < rr->len; ++j) {
        struct String *s = &rr->s[j];
        if (s->start + s->len <= head->start) continue;
        if (s->start >= head->start + head->len) continue;
        /* Okay, it abuts, so it should continue this comment. */
        if (s->comment != 0 && s->comment != head->comment)
          fatal_error(rr, s->start, "Comment collision!");
        s->comment = head->comment;
        continued = 1;
    }
    if (!continued)
      fatal_error(r, head->start, "Unterminated comment");
}


void mark_comments(struct Section *sp)
{
    int i, j;
    int last_tag = 0;

    for (i=0; i < sp->rowslen; ++i) {
        struct Row *r = &sp->rows[i];
        for (j=0; j < r->len; ++j) {
            struct String *s = &r->s[j];

            /* Is this already a comment? Look for its end. */
            if (s->comment != 0) {
                int done = look_for_terminator(r, s);
                if (done)
                  continue;
            } else {
                /* This is not yet a comment. Try to make it one. */
                pull_out_delimiters(r, s);
                if (s->text[0] == '\0') {
                    /* Remove the empty string left behind by a deleted comment. */
                    free(s->text);
                    r->len -= 1;
                    memmove(&r->s[j], &r->s[j+1], (r->len - j) * sizeof *s);
                    j -= 1;
                    continue;
                } else if (s->text[0] == '{') {
                    /* Give {it} an odd tag. */
                    last_tag = (last_tag+1) | 1;
                    s->comment = last_tag;
                } else if (!strncmp(s->text, "(*", 2)) {
                    /* Give (*it*) an even tag. */
                    last_tag = (last_tag+2) & ~1;
                    s->comment = last_tag;
                } else {
                    continue;
                }
            }

            /* Is there a string abutting this comment from below? */
            assert(s->comment != 0);
            if (i+1 >= sp->rowslen)
              fatal_error(r, s->start, "Unterminated comment");
            flow_downward(r, &sp->rows[i+1], s);
        }
    }
}

