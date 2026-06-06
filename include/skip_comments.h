#ifndef SKIP_COMMENTS_H
#define SKIP_COMMENTS_H

#include <stdio.h>

/*
 * Shared helper — skips whitespace and lines beginning with '#'.
 * Leaves the file pointer at the start of the next real token.
 * Include this header in any .c file that reads the input format.
 */
static inline void skip_comments(FILE* f)
{
    int c;
    while (1) {
        /* skip whitespace */
        while ((c = fgetc(f)) != EOF &&
               (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
            ;
        if (c == EOF) return;
        if (c == '#') {
            /* skip rest of comment line */
            while ((c = fgetc(f)) != EOF && c != '\n')
                ;
        } else {
            ungetc(c, f);
            return;
        }
    }
}

#endif
