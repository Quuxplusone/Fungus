
#include <stdio.h>
#include <string.h>
#include "fungal.h"

static const char *nazg(int ALU, int A, int B);
 static const char *reg(int R);

const char *disasm(unsigned int inst)
{
    static char buffer[50];
    static const char *Mask[4] = { "", ".x", ".y", ".s" };
    int G = (inst >> 17) & 01;
    int M = (inst >> 15) & 03;
    unsigned int OP = (inst >> 12) & 07;
    unsigned int X = (inst >> 9) & 07;
    if (G == 0) {
        unsigned int L = (inst >> 0) & 0777;
        const char *G0op[] = {0, 0, 0, "SZ", "SNZ", "DZ", "DNZ"};
        switch (OP) {
            case 0:
                if (32 < L && L <= 126)
                  sprintf(buffer, "TRP\t'%c'", L);
                else
                  sprintf(buffer, "TRP\t%03o", L);
                break;
            case 1:
                sprintf(buffer, "LI%s\t%s, %03o", Mask[M], reg(X), L);
                if (!strcmp(buffer, "LI\t$DPC, 777"))
                  strcpy(buffer, "GOW");
                else if (!strcmp(buffer, "LI\t$DPC, 001"))
                  strcpy(buffer, "GOE");
                break;
            case 2:
                sprintf(buffer, "LV%s\t%s, %03o", Mask[M], reg(X), L);
                if (!strcmp(buffer, "LV\t$DPC, 777"))
                  strcpy(buffer, "GONW");
                else if (!strcmp(buffer, "LV\t$DPC, 001"))
                  strcpy(buffer, "GOSE");
                break;
            case 3: case 4:
            case 5: case 6:
                if (L == 0)
                  sprintf(buffer, "%s%s\t%s", G0op[OP], Mask[M], reg(X));
                else
                  sprintf(buffer, "%s%s\t%s, %03o", G0op[OP], Mask[M], reg(X), L);
                if (!strcmp(buffer, "DZ.y\t$0"))
                  strcpy(buffer, "GOS");
                else if (!strcmp(buffer, "DNZ.y\t$0"))
                  strcpy(buffer, "GON");
                else if (!strcmp(buffer, "DZ.x\t$0"))
                  strcpy(buffer, "GOE");
                else if (!strcmp(buffer, "DNZ.x\t$0"))
                  strcpy(buffer, "GOW");
                else if (!strcmp(buffer, "DZ\t$0"))
                  strcpy(buffer, "GOSE");
                else if (!strcmp(buffer, "DNZ\t$0"))
                  strcpy(buffer, "GONW");
                break;
            case 7: sprintf(buffer, "RET"); break;
        }
    } else if (G == 1) {
        unsigned int ALU = (inst >> 6) & 07;
        unsigned int A = (inst >> 3) & 07;
        unsigned int B = (inst >> 0) & 07;
        if (OP == 0) {
            switch (ALU) {
                case 0: if (A == 0 || B == 0)
                          sprintf(buffer, "MR%s\t%s, %s", Mask[M], reg(X), reg(B));
                        else if (A == B)
                          sprintf(buffer, "SHL%s\t%s, %s", Mask[M], reg(X), reg(B));
                        else
                          sprintf(buffer, "ADD%s\t%s, %s, %s", Mask[M], reg(X), reg(A), reg(B));
                        break;
                case 1: if (A == 0)
                          sprintf(buffer, "NEG%s\t%s, %s", Mask[M], reg(X), reg(B));
                        else
                          sprintf(buffer, "SUB%s\t%s, %s, %s", Mask[M], reg(X), reg(A), reg(B));
                        break;
                case 2: sprintf(buffer, "AND%s\t%s, %s, %s", Mask[M], reg(X), reg(A), reg(B)); break;
                case 3: sprintf(buffer, "OR%s\t%s, %s, %s",  Mask[M], reg(X), reg(A), reg(B)); break;
                case 4: sprintf(buffer, "XOR%s\t%s, %s, %s", Mask[M], reg(X), reg(A), reg(B)); break;
                case 5: goto print_word;
                case 6: goto print_word;
                case 7:
                    switch (B) {
                        case 0: sprintf(buffer, "NOT%s\t%s, %s", Mask[M], reg(X), reg(A)); break;
                        case 1: sprintf(buffer, "SHR%s\t%s, %s", Mask[M], reg(X), reg(A)); break;
                        case 2: sprintf(buffer, "INV%s\t%s, %s", Mask[M], reg(X), reg(A)); break;
                        case 3: sprintf(buffer, "DEV%s\t%s, %s", Mask[M], reg(X), reg(A)); break;
                        case 4:
                            if ((M == 2) && (X == A)) /* INC.y adds zero */
                              strcpy(buffer, "NOP");
                            else
                              sprintf(buffer, "INC%s\t%s, %s", Mask[M], reg(X), reg(A));
                            break;
                        case 5:
                            if ((M == 2) && (X == A)) /* DEC.y subtracts zero */
                              strcpy(buffer, "NOP");
                            else
                              sprintf(buffer, "DEC%s\t%s, %s", Mask[M], reg(X), reg(A));
                            break;
                        case 6: goto print_word;
                        case 7: goto print_word;
                    }
                    break;
            }
        } else if (OP == 7) {
            unsigned int msr = (inst & 077);
            switch (ALU) {
                /* undefined by the official spec: LMR, SMR */
                case 0: sprintf(buffer, "LMR%s\t%s, #%02o", Mask[M], reg(X), msr); break;
                case 1: sprintf(buffer, "SMR%s\t%s, #%02o", Mask[M], reg(X), msr); break;
                default: goto print_word;
            }
        } else {
            const char *opc[] = {0, "LW", "LX", "LY", "SW", "SX", "SY"};
            const char *opd = nazg(ALU, A, B);
            if (opd == NULL)
              goto print_word;
            sprintf(buffer, "%s%s\t%s, [%s]", opc[OP], Mask[M], reg(X), opd);
        }
    }
    return buffer;

  print_word:
    sprintf(buffer, ".WORD\t%03o %03o", (inst>>9)&0777, inst&0777);
    return buffer;
}



static const char *nazg(int ALU, int A, int B)
{
    static char buffer[50];
    switch (ALU) {
        case 0:
            if (B==0)
              strcpy(buffer, reg(A));
            else if (A==0)
              strcpy(buffer, reg(B));
            else
              sprintf(buffer, "%s+%s", reg(A), reg(B));
            break;
        case 1: sprintf(buffer, "%s-%s", reg(A), reg(B)); break;
        case 2: sprintf(buffer, "%s&%s", reg(A), reg(B)); break;
        case 3: sprintf(buffer, "%s|%s", reg(A), reg(B)); break;
        case 4: sprintf(buffer, "%s^%s", reg(A), reg(B)); break;
        case 5: return NULL;
        case 6: return NULL;
        case 7:
            switch (B) {
                case 0: sprintf(buffer, "~%s", reg(A)); break;
                case 1: sprintf(buffer, ">>%s", reg(A)); break;
                case 2: sprintf(buffer, "++%s", reg(A)); break;
                case 3: sprintf(buffer, "--%s", reg(A)); break;
                case 4: sprintf(buffer, "+%s", reg(A)); break;
                case 5: sprintf(buffer, "-%s", reg(A)); break;
                case 6: return NULL;
                case 7: return NULL;
            }
            break;
    }
    return buffer;
}

static const char *reg(int R)
{
    static char buffer[4][10];
    static int timer = 0;
    char *bp = buffer[timer];
    if (++timer == 4)
      timer = 0;
    switch (R) {
        case 1: strcpy(bp, "$PC"); break;
        case 2: strcpy(bp, "$DPC"); break;
        default: sprintf(bp, "$%d", R); break;
    }
    return bp;
}

