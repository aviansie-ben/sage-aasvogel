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

char* itoa_l(long long val, char* s, unsigned int base)
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
        r = (int)((unsigned long long)val % base);
        val = (long long)((unsigned long long)val / base);
        
        s[i++] = (r < 0) ? itoa_values[(int)base + r] : itoa_values[r];
    }
    
    if (neg)
        s[i++] = '-';
    
    s[i] = '\0';
    reverse(s, i);
    
    return s;
}

void* memcpy(void* dest, const void* src, size_t size)
{
    uint8* dest8 = dest;
    const uint8* src8 = src;
    
    while (size--)
    {
        *dest8++ = *src8++;
    }
    
    return dest;
}

void* memset(void* ptr, int val, size_t size)
{
    uint8* ptr8 = ptr;
    
    while (size--)
    {
        *ptr8++ = (uint8) val;
    }
    
    return ptr;
}
