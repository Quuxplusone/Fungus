
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fungal.h"

const char *fungasm_error;
static unsigned int error(const char *fmt, ...);
static void *Nerror(const char *fmt, ...);
 static void verror(const char *fmt, va_list ap);

enum { OP_NONE=-1,
       OP_INC=4, OP_INV=2, OP_DEC=5, OP_DEV=3, OP_NOT=0, OP_SHR=1,
       OP_PLUS=0, OP_MINUS=1, OP_AND=2, OP_OR=3, OP_XOR=4 };

static const char *get_char_literal(const char *ip, unsigned int *lval);

static unsigned int get_G0op(const char *text, const char *ip, unsigned int ret);
static unsigned int arithmetic_op(const char *text, const char *ip, unsigned int ret);
 static unsigned int eval_scalar_nomath(const char *text, int *len);
static unsigned int eval_register(const char *text, int *len);
static unsigned int eval_mem(const char *text, int *len);

static int maskingmode;
static unsigned int apply_masking_mode(unsigned int inst);


/*
 * Assemble a plain Fungus instruction. This is totally according to the
 * hardware spec; all the assembler magic should have been resolved by
 * now. There will be no PC-relative magic ("."), no macros, no comments,
 * and no assembler directives (".FILL"). There will be EQU symbols, but
 * every EQU symbol will have its One True Value accessible via true_value().
 * Any use of an unlisted symbol is an error.
 */
unsigned int fungasm(const char *text)
{
    const char *ip;
    int len, mlen;
    unsigned int lval, rval, dummy;

    /* Clear the error message buffer. */
    fungasm_error = NULL;

    assert(text != NULL);
    if (text[0] == '.')
      return error("Directive cannot be assembled!");

    /* These predefined macros don't accept masking modes. */
    if (!strcmp(text, "GON"))  return 0260000;   /* DNZ.Y $0 */
    if (!strcmp(text, "GOS"))  return 0250000;   /* DZ.Y $0 */
    if (!strcmp(text, "GOW"))  return 0012777;   /* LI $PC, -1 */
    if (!strcmp(text, "GOE"))  return 0012001;   /* LI $PC, 1 */
    if (!strcmp(text, "GONW")) return 0022777;   /* LV $PC, -1 */
    if (!strcmp(text, "GOSE")) return 0022001;   /* LV $PC, 1 */
    if (!strcmp(text, "GOB"))  return 0402102;   /* SUB $PC, $0, $PC */
    if (!strcmp(text, "NOP"))  return 0607774;   /* INC.Y $7, $7 */
    if (!strcmp(text, "IRET")) return 0470176;   /* SMR $0, #076 */

    ip = find_token(text, &len, NULL);
    if (ip == NULL) return error("No text in cell");
    if (!isalpha(*ip)) return error("Instruction must begin with an identifier");

    mlen = len;
    if (ip[len] == '.') {
        maskingmode = tolower(ip[len+1]);
        if (maskingmode != '\0' && !strchr("vxys", maskingmode))
          return error("Unrecognized masking mode suffix on opcode mnemonic");
        mlen = len+2;
        if (isalnum(ip[mlen]) || ip[mlen] == '_')
          return error("Unrecognized masking mode suffix on opcode mnemonic");
    } else maskingmode = -1;

    if (len == 2) {
        if (!strncmp(ip, "JR", 2)) {
            lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            return apply_masking_mode(0401000 | lval);
        } else if (strchr("SL", *ip) && strchr("WXY", ip[1])) {
            /* same handling for all of LW, LX, LY, SW, SX, SY */
            lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            lval = 0400000 | (lval << 9);
            lval |= ((ip[0]=='S')*3 + (ip[1]-'V')) << 12;
            ip += mlen + len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_mem(ip, &len);
            if (rval == -1u) return -1u;
            return apply_masking_mode(lval | rval);
        } else if (!strncmp(ip, "OR", 2)) {
            return arithmetic_op(text, ip+mlen, 0400300);
        } else if (!strncmp(ip, "LV", 2)) {
            lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            lval = 020000 | (lval << 9);
            ip += mlen + len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_scalar(ip, &len);
            if (rval == -1u) return -1u;
            if ((rval & IS_VECTOR) && (maskingmode == 'y')) {
                /* LV.Y $4, (123,456) will set $4.y to 456,
                 * because that's the friendly thing to do.
                 * It will not behave the same as LV.Y $4, 0456123
                 * even though (123,456) == 0456123. */
                rval >>= 9;
            }
            return apply_masking_mode(lval | (rval & 0777));
        } else if (!strncmp(ip, "LI", 2)) {
            lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            lval = 010000 | (lval << 9);
            ip += mlen + len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_scalar(ip, &len);
            if (rval == -1u) return -1u;
            return apply_masking_mode(lval | (rval & 0777));
        } else if (!strncmp(ip, "SZ", 2)) {
            return get_G0op(text, ip+mlen, 030000);
        } else if (!strncmp(ip, "DZ", 2)) {
            return get_G0op(text, ip+mlen, 050000);
        } else if (!strncmp(ip, "MR", 2)) {
            /* "MR $X, $A" is an alias for "ADD $X, $0, $A". */
            int len;
            unsigned int lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            ip += mlen + len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_register(ip, &len);
            if (rval == -1u) return -1u;
            assert(rval <= 7);
            ip += len;
            if (*ip++ != '\0')
              return error("Expected only two operands in \"%s\"", text);
            return apply_masking_mode(0400000 | (lval << 9) | rval);
        }
    } else if (len == 3) {
        if (!strncmp(ip, "RET", 3)) {
            if (find_token(ip+mlen, &len, &dummy) != NULL)
              return error("RET instruction takes no operands");
            return apply_masking_mode(070000);
        } else if (!strncmp(ip, "TRP", 3)) {
            lval = eval_scalar(ip+mlen, &len);
            if (lval == -1u)
              return error("TRP instruction takes one operand");
            return apply_masking_mode(lval & 0777);
        } else if (!strncmp(ip, "ADD", 3)) {
            return arithmetic_op(text, ip+mlen, 0400000);
        } else if (!strncmp(ip, "SUB", 3)) {
            return arithmetic_op(text, ip+mlen, 0400100);
        } else if (!strncmp(ip, "AND", 3)) {
            return arithmetic_op(text, ip+mlen, 0400200);
        } else if (!strncmp(ip, "XOR", 3)) {
            return arithmetic_op(text, ip+mlen, 0400400);
        } else if (!strncmp(ip, "NOT", 3)) {
            return arithmetic_op(text, ip+mlen, 0400700);
        } else if (!strncmp(ip, "SHR", 3)) {
            return arithmetic_op(text, ip+mlen, 0400701);
        } else if (!strncmp(ip, "SHL", 3)) {
            /* "SHL $X, $A" is an alias for "ADD $X, $A, $A". */
            int len;
            unsigned int lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            ip += mlen + len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_register(ip, &len);
            assert(rval <= 7);
            if (rval == -1u) return -1u;
            if (ip[len] != '\0')
              return error("Expected only two operands in \"%s\"", text);
            return apply_masking_mode(0400000 | (lval << 9) |
                                      (rval << 3) | rval);
        } else if (!strncmp(ip, "INV", 3)) {
            return arithmetic_op(text, ip+mlen, 0400702);
        } else if (!strncmp(ip, "DEV", 3)) {
            return arithmetic_op(text, ip+mlen, 0400703);
        } else if (!strncmp(ip, "INC", 3)) {
            return arithmetic_op(text, ip+mlen, 0400704);
        } else if (!strncmp(ip, "DEC", 3)) {
            return arithmetic_op(text, ip+mlen, 0400705);
        } else if (!strncmp(ip, "SNZ", 3)) {
            return get_G0op(text, ip+mlen, 040000);
        } else if (!strncmp(ip, "DNZ", 3)) {
            return get_G0op(text, ip+mlen, 060000);
        } else if (!strncmp(ip, "LMR", 3)) {
            lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            ip += mlen+len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_scalar(ip, &len);
            if (rval == -1u) return -1u;
            if (rval & IS_VECTOR)
              return error("Type error: Second operand to LMR cannot be a vector");
            return apply_masking_mode(0470000 | (lval << 9) | (rval & 077));
        } else if (!strncmp(ip, "SMR", 3)) {
            lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            ip += mlen+len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_scalar(ip, &len);
            if (rval == -1u) return -1u;
            if (rval & IS_VECTOR)
              return error("Type error: Second operand to SMR cannot be a vector");
            return apply_masking_mode(0470100 | (lval << 9) | (rval & 077));
        } else if (!strncmp(ip, "NEG", 3)) {
            /* "NEG $X, $A" is an alias for "SUB $X, $0, $A". */
            int len;
            unsigned int lval = eval_register(ip+mlen, &len);
            if (lval == -1u) return -1u;
            assert(lval <= 7);
            ip += mlen + len;
            if (*ip++ != ',')
              return error("Expected second operand in \"%s\"", text);
            rval = eval_register(ip, &len);
            if (rval == -1u) return -1u;
            assert(lval <= 7);
            if (ip[len] != '\0')
              return error("Expected only two operands in \"%s\"", text);
            return apply_masking_mode(0400100 | (lval << 9) | rval);
        }
    } else if (len == 4) {
        if (!strncmp(ip, "WORD", 4)) {
            lval = eval_scalar(ip+mlen, &len);
            if (lval == -1u || find_token(ip+mlen+len, &len, &dummy))
              return error("WORD pseudo-op expects one operand");
            return lval & 0777777;
        }
    }

    return error("Unknown opcode mnemonic \"%.*s\"", mlen, ip);
}


/* Return the code for one of SZ, SNZ, DZ, DNZ. */
static unsigned int get_G0op(const char *text, const char *ip, unsigned int ret)
{
    int len;
    unsigned int lval = eval_register(ip, &len);
    if (lval == -1u) return -1u;
    assert(lval <= 7);
    ret |= (lval << 9);
    ip += len;
    if (*ip == ',') {
         lval = eval_scalar(ip, &len);
         if (lval == -1u) return -1u;
         if (lval & IS_VECTOR)
           return error("Type error: L field cannot be a vector");
         ret |= (lval & 0777);
         ip += len;
    }
    if (*ip != '\0')
      return error("Extra text at end of \"%s\"", text);
    return apply_masking_mode(ret);
}


/* Return the X, OP, A, B fields of one of the Fungus arithmetic instructions,
 * given a pointer to the first one (i.e., having read the "ADD.X" part).
 */
static unsigned int arithmetic_op(const char *text, const char *ip, unsigned int ret)
{
    int len;
    unsigned int lval = eval_register(ip, &len);
    if (lval == -1u) return -1u;
    ret |= (lval << 9);
    ip += len;
    if (*ip++ != ',')
      return error("Expected second operand in \"%s\"", text);
    lval = eval_register(ip, &len);
    if (lval == -1u) return -1u;
    assert(lval <= 7);
    ret |= (lval << 3);
    ip += len;
    if ((ret & 0700) != 0700) {
        if (*ip++ != ',')
          return error("Expected third operand in \"%s\"", text);
        lval = eval_register(ip, &len);
        if (lval == -1u) return -1u;
        assert(lval <= 7);
        ret |= lval;
        ip += len;
        if (*ip++ != '\0')
          return error("Expected only three operands in \"%s\"", text);
    } else {
        if (*ip++ != '\0')
          return error("Expected only two operands in \"%s\"", text);
    }
    return apply_masking_mode(ret);
}


static const char *get_char_literal(const char *ip, unsigned int *lval)
{
    if (*ip == '\0')
      return Nerror("Unterminated character constant");
    *lval = *ip;
    if (*ip++ == '\\') {
        switch (*ip++) {
            case '\0': return Nerror("Unterminated character constant");
            case '\\': *lval = '\\'; break;
            case '\'': *lval = '\''; break;
            case 'b': *lval = '\b'; break;
            case 'n': *lval = '\n'; break;
            case 'r': *lval = '\r'; break;
            case 't': *lval = '\t'; break;
            case 'v': *lval = '\v'; break;
            case '0': *lval = '\0'; break;
            default:
                return Nerror("Unrecognized escape in character constant");
        }
    }
    return ip;
}


/* Scan one token from the input stream. "." counts as a token. If the token
 * can be evaluated, then put its value in 'value' (if provided). Evaluable
 * tokens are integer literals, character constants, and EQU symbols. Vectors
 * scan as "(" integer "," integer ")", and cannot be evaluated as a single
 * token. Arithmetic also doesn't happen here.
 */
char *find_token(const char *text, int *len, unsigned int *value)
{
    const char *ret;
    const char *ip = text;
    int minus = 0;

    if (value != NULL)
      *value = -1u;
    while (isspace(*ip)) ++ip;
    if (*ip == '$' || *ip == '#') {
        ++ip;
        if (!isalnum(*ip) && *ip != '_')
          return Nerror("Encountered '%c' alone in an instruction", ip[-1]);
    }

    if (isalpha(*ip) || *ip == '_') {
        ret = ip;
        while (isalnum(*ip) || *ip == '_')
          ++ip;
        if (value != NULL) {
            /* This might be an EQU symbol. Can we resolve it? */
            *value = true_value(ret, ip-ret);
        }
    } else if (*ip == '0' && ip[1] == 'x') {
        ret = ip;
    scan_hex_number:
        ip += 2;  /* skip the "0x" */
        while (isxdigit(*ip))
          ++ip;
        if (isalpha(*ip) || *ip == '_')
          return Nerror("Hex number contains extra characters");
        if (value != NULL) {
            sscanf(ret+2, "%x", value);
            if (minus) *value = 0777777-(*value);
        }
    } else if (*ip == '-') {
        ret = ip;
        if (ip[1] == '-')
          ++ip;
        /* Scan '-1' as '-' '1' if value==NULL, or '-1' if value!=NULL. */
        else if (value != NULL && *ip == '0' && ip[1] == 'x') {
            minus = 1;
            goto scan_hex_number;
        } else if (value != NULL && isdigit(*ip)) {
            minus = 1;
            goto scan_number;
        }
        ++ip;
    } else if (isdigit(*ip)) {
        ret = ip;
    scan_number:
        while (isdigit(*ip))
          ++ip;
        if (*ip == 'd' || *ip == 'D') {
            if (value != NULL)
              sscanf(ret, "%u", value);
            ++ip;
        } else {
            if (value != NULL)
              sscanf(ret, "%o", value);
        }
        if (minus && (value != NULL))
          *value = 0777777-(*value);
        if (isalpha(*ip) || *ip == '_') {
            return Nerror("%s number contains extra characters",
                isalpha(ip[-1])? "Decimal": "Octal");
        }
    } else {
        switch (*ip) {
            case '\0':
                return NULL;
            case '[': case ']': case '*': case '&': case '|': case '^':
            case '(': case ')': case ',': case '.': case '~':
                ret = ip++;
                break;
            case '+':  /* match '++' and '+' */
                ret = ip++;
                if (*ip == '+') ++ip;
                break;
            case '>':  /* match '>>' */
                ret = ip;
                if (ip[1] != '>')
                  return Nerror("Unrecognized token beginning with '>'");
                break;
            case '\'': { /* match a character constant */
                unsigned int dummy;
                ret = ip++;
                ip = get_char_literal(ip, &dummy);
                if (ip == NULL)
                  return NULL;  /* message was set by get_char_literal() */
                if (value != NULL)
                  *value = dummy;
                if (*ip == '\'') {
                    ip += 1;
                    break;
                }
                ip = get_char_literal(ip, &dummy);
                if (ip == NULL)
                  return NULL;  /* message was set by get_char_literal() */
                if (value != NULL)
                  *value = IS_VECTOR | (*value << 9) | dummy;
                if (*ip++ != '\'')
                  return Nerror("Unterminated character constant");
                break;
            }
            default:
                return Nerror("Unrecognized token beginning with '%c'", *ip);
        }
    }
    assert(ip > ret);
    assert(ret >= text);
    *len = ip - ret;
    return (char *)ret;  /* casting away const */
}


/* Scan a register number, as "$3" in "ADD.X $3, $4, $5". Register numbers are
 * never the result of arithmetic, so they are always single tokens. */
static unsigned int eval_register(const char *text, int *len)
{
    unsigned int lval;
    int ilen;
    const char *ip = find_token(text, &ilen, &lval);
    if (ip == NULL) return error("Expected an integer");
    if (lval == -1u || (lval & IS_VECTOR))
      return error("Expected a register number");
    if (lval & ~07)
      return error("Register number out of range");
    if (ip[0] == '#')
      return error("Type error: integer \"%.*s\" should be a register number", ilen, ip);
    ip += ilen;
    assert(ip >= text);
    *len = (ip - text);
    return lval;
}

/*
 * Returns an integer or vector constant, parsed from the text, and
 * sets 'len' to the number of consumed bytes. Sets IS_VECTOR on the
 * returned value, if it's a vector, because things like addition and
 * negation work differently on vectors than scalars, and we want to
 * use this function recursively.
 */
unsigned int eval_scalar(const char *text, int *len)
{
    int ilen;
    unsigned int lval = eval_scalar_nomath(text, &ilen);
    unsigned int rval;
    const char *ip;
    if (lval == -1u) return -1u;
    ip = text + ilen;

    while (1) {
        while (isspace(*ip)) ++ip;
        if (*ip == '\0') break;
        switch (*ip) {
            case '-':
                rval = eval_scalar_nomath(++ip, &ilen);
                ip += ilen;
                if (rval == -1u) return -1u;
                if (rval & IS_VECTOR) {
                    if (lval & IS_VECTOR) {
                        unsigned int x = (lval & 0777) - (rval & 0777);
                        unsigned int y = ((lval>>9) & 0777) - ((rval>>9) & 0777);
                        lval = IS_VECTOR | ((y & 0777) << 9) | (x & 0777);
                    } else
                      return error("Type error: Cannot subtract vector from integer");
                } else {
                    if (lval & IS_VECTOR)
                      return error("Type error: Cannot subtract integer from vector");
                    else
                      lval = (lval - rval) & 0777777;
                }
                break;
            case '+':
                rval = eval_scalar_nomath(++ip, &ilen);
                ip += ilen;
                if (rval == -1u) return -1u;
                if (rval & IS_VECTOR) {
                    if (lval & IS_VECTOR) {
                        unsigned int x = (lval & 0777) + (rval & 0777);
                        unsigned int y = ((lval>>9) & 0777) + ((rval>>9) & 0777);
                        lval = IS_VECTOR | ((y & 0777) << 9) | (x & 0777);
                    } else
                      return error("Type error: Cannot add vector to integer");
                } else {
                    if (lval & IS_VECTOR)
                      return error("Type error: Cannot add integer to vector");
                    else
                      lval = (lval + rval) & 0777777;
                }
                break;
            default:
                error("Unexpected operator or token after scalar \"%.*s\"",
                    ip-text, ip);
                if (len != NULL)
                  *len = ip - text;
                return lval;
        } /* end switch */
    }

    if (len != NULL)
      *len = ip - text;
    return lval;
}

static unsigned int eval_scalar_nomath(const char *text, int *len)
{
    unsigned int lval, rval;
    const char *ip = text;
    int ilen;

    /* Clear the error message buffer. */
    fungasm_error = NULL;

    ip = find_token(ip, &ilen, &lval);
    if (ip == NULL)
      return error("Expected an integer");
    if (lval == -1u && ilen == 1) {
        switch (*ip) {
            case '(':
                ip += ilen;
                lval = eval_scalar(ip, &ilen);
                if (lval == -1u) return -1u;
                ip += ilen;
                if (*ip == ')') {
                    ilen = 1;
                } else if (*ip == ',') {
                    ip += 1;
                    rval = eval_scalar(ip, &ilen);
                    if (rval == -1u) return -1u;
                    ip += ilen;
                    if (*ip != ')')
                      return error("Unterminated vector expression");
                    ilen = 1;
                    if (lval & IS_VECTOR)
                      return error("Y component of vector \"%.*s\""
                          " cannot be a vector itself", ip-text, text);
                    if (rval & IS_VECTOR)
                      return error("X component of vector \"%.*s\""
                          " cannot be a vector itself", ip-text, text);
                    lval = IS_VECTOR | ((rval & 0777) << 9) | (lval & 0777);
                } else
                  return error("Unterminated parenthesized expression or vector");
                break;
            case '-':
                ip += ilen;
                lval = eval_scalar(ip, &ilen);
                if (lval == -1u) return -1u;
                if (lval & IS_VECTOR) {
                    unsigned int y = (lval & ~IS_VECTOR) >> 9;
                    unsigned int x = lval & 0777;
                    lval = IS_VECTOR | ((-y & 0777) << 9) | (-x & 0777);
                } else
                  lval = -lval & 0777777;
                break;
        }
    }
    if (lval == -1u)
      return error("Expected scalar expression");
    if (ip[0] == '$')
      return error("Type error: register \"%.*s\" should be a scalar", ilen, ip);

    ip += ilen;

    if (ip[0] == '.') {
        if (ip[1] == 'X' || ip[1] == 'x')
          lval = lval & 0777;
        else if (ip[1] == 'Y' || ip[1] == 'y')
          lval = (lval >> 9) & 0777;
        else {
            return error("Unrecognized \".\" suffix after scalar \"%.*s\"",
                ip-text, text);
        }
        ip += 2;
    }

    *len = ip - text;
    return lval;
}


static unsigned int eval_mem(const char *text, int *len)
{
    const char *ip = text;
    int ilen;
    int op = OP_NONE;
    unsigned int lval, rval;

    while (isspace(*ip)) ++ip;
    if (*ip != '[')
      return error("Expected a memory operand enclosed in brackets []");
    ++ip;

    ip = find_token(ip, &ilen, &lval);
    if (ip == NULL)
      return error("Unterminated memory reference in \"%s\"", text);
    if (lval != -1u) {
        /* then we got a register; maybe there's another one */
        ip += ilen;
        if (lval & ~07)
          return error("Register number out of range in memory reference \"%s\"", text);
        switch (*ip) {
            case ']':
                ip += 1;
                *len = ip-text;
                lval = (OP_PLUS << 6) | (lval << 3) | 0;
                return lval;
            case '+': op = OP_PLUS; break;
            case '-': op = OP_MINUS; break;
            case '&': op = OP_AND; break;
            case '|': op = OP_OR; break;
            case '^': op = OP_XOR; break;
            default:
                return error("Unexpected \"%c\" after register in memory reference", *ip);
        }
        ip = find_token(ip+1, &ilen, &rval);
        if (rval == -1u)
          return error("Expected a register number in memory reference \"%s\"", text);
        if (rval & ~07)
          return error("Register number out of range in memory reference \"%s\"", text);
        if (ip[ilen] != ']')
          return error("Unterminated memory reference in \"%s\"", text);
        /* Pack and return the reference. */
        lval = (op << 6) | (lval << 3) | rval;
        return lval;
    }
    if (*ip == '+') {
        op = (ilen == 1)? OP_INC: OP_INV;
    } else if (*ip == '-') {
        op = (ilen == 1)? OP_DEC: OP_DEV;
    } else if (*ip == '~') {
        op = OP_NOT;
    } else if (*ip == '>') {
        op = OP_SHR;
    } else {
        return error("Expected a register number in memory reference \"%s\"", text);
    }
    /* skip over the operator */
    ip += ilen;
    lval = eval_register(ip, &ilen);
    if (lval == -1u) return -1u;
    assert(lval <= 7);
    if (ip[ilen] != ']')
      return error("Unterminated memory reference in \"%s\"", text);

    ip += ilen+1;
    *len = ip - text;
    lval = (07 << 6) | (lval << 3) | op;
    return lval;
}


static unsigned int apply_masking_mode(unsigned int inst)
{
    switch (maskingmode) {
        case 'V': case 'v': case -1: return inst;
        case 'X': case 'x': return inst | 0100000;
        case 'Y': case 'y': return inst | 0200000;
        case 'S': case 's': return inst | 0300000;
    }
    return error("oops!");
}


static void verror(const char *fmt, va_list ap)
{
    static char fungasm_error_buffer[100];
    if (fungasm_error == NULL) {
        vsprintf(fungasm_error_buffer, fmt, ap);
        fungasm_error = fungasm_error_buffer;
    }
}

static unsigned int error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
    return -1u;
}

static void *Nerror(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
    return NULL;
}
