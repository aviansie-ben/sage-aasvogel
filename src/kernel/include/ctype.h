#ifndef CTYPE_H
#define CTYPE_H

#define _CTYPE_U 0x01 // Uppercase
#define _CTYPE_L 0x02 // Lowecase
#define _CTYPE_D 0x04 // Digit
#define _CTYPE_C 0x08 // Control
#define _CTYPE_P 0x10 // Punctuation
#define _CTYPE_S 0x20 // Whitespace
#define _CTYPE_X 0x40 // Hex digit

extern const unsigned char _ctype[];

#define __ctype_mask(c) (_ctype[(int)(unsigned char)(c)])

#define isalnum(c)  ((__ctype_mask(c) & (_CTYPE_U | _CTYPE_L | _CTYPE_D)) != 0)
#define isalpha(c)  ((__ctype_mask(c) & (_CTYPE_U | _CTYPE_L)) != 0)
#define iscntrl(c)  ((__ctype_mask(c) & (_CTYPE_C)) != 0)
#define isdigit(c)  ((__ctype_mask(c) & (_CTYPE_D)) != 0)
#define isgraph(c)  ((__ctype_mask(c) & (_CTYPE_U | _CTYPE_L | _CTYPE_D | _CTYPE_P)) != 0)
#define islower(c)  ((__ctype_mask(c) & (_CTYPE_L)) != 0)
#define isprint(c)  ((__ctype_mask(c) & (_CTYPE_U | _CTYPE_L | _CTYPE_D | _CTYPE_P | _CTYPE_S)) != 0)
#define ispunct(c)  ((__ctype_mask(c) & (_CTYPE_P)) != 0)
#define isspace(c)  ((__ctype_mask(c) & (_CTYPE_S)) != 0)
#define isupper(c)  ((__ctype_mask(c) & (_CTYPE_U)) != 0)
#define isxdigit(c) ((__ctype_mask(c) & (_CTYPE_X)) != 0)

static inline int tolower(int c)
{
    return isupper(c) ? (c - ('A' - 'a')) : c;
}

static inline int toupper(int c)
{
    return islower(c) ? (c + ('A' - 'a')) : c;
}

#endif
