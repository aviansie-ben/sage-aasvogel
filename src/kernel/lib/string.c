#include <string.h>
#include <typedef.h>
#include <assert.h>

static const char* itoa_values = "0123456789ABCDEF";

void strcpy(char* dest, const char* src)
{
    while (*src != '\0')
        *(dest++) = *(src++);
    
    *dest = '\0';
}

void strcat(char* dest, const char* src)
{
    while (*dest != '\0')
        dest++;
    
    while (*src != '\0')
        *(dest++) = *(src++);
    
    *dest = '\0';
}

static void reverse(char* s, size_t len)
{
    uint32 i = 0, j = len - 1;
    char t;
    
    while (i < j)
    {
        t = s[i];
        s[i] = s[j];
        s[j] = t;
        
        i++;
        j--;
    }
}

char* itoa(int val, char* s, unsigned int base)
{
    bool neg = false;
    int r;
    size_t i = 0;
    
    assert(base <= 16);
    
    if (val == 0)
    {
        s[0] = '0';
        s[1] = '\0';
        return s;
    }
    
    if (base == 10 && val < 0)
    {
        val = -val;
        neg = true;
    }
    
    while (val != 0)
    {
        r = (int)((unsigned int)val % base);
        val = (int)((unsigned int)val / base);
        
        s[i++] = (r < 0) ? itoa_values[(int)base + r] : itoa_values[r];
    }
    
    if (neg)
        s[i++] = '-';
    
    s[i] = '\0';
    reverse(s, i);
    
    return s;
}
