#include <string.hpp>
#include <typedef.hpp>

int strcmp(const char* s1, const char* s2)
{
    for (; *s1 != '\0' && *s2 != '\0'; s1++, s2++)
    {
        if (*s1 < *s2) return -1;
        else if (*s1 > *s2) return 1;
    }
    
    if (*s1 != '\0') return 1;
    else if (*s2 != '\0') return -1;
    else return 0;
}
