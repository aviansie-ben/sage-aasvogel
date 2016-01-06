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

void strncpy(char* dest, const char* src, size_t n)
{
    while (*src != '\0' && n-- > 0)
        *(dest++) = *(src++);
    
    while (n-- > 0)
        *(dest++) = '\0';
}

void strcat(char* dest, const char* src)
{
    while (*dest != '\0')
        dest++;
    
    while (*src != '\0')
        *(dest++) = *(src++);
    
    *dest = '\0';
}

size_t strlen(const char* str)
{
    size_t sz = 0;
    
    while (*str != '\0')
    {
        sz++;
        str++;
    }
    
    return sz;
}

int strcmp(const char* s1, const char* s2)
{
    while (*s1 != '\0' && *s2 != '\0')
    {
        if (*s1 < *s2)
            return -1;
        else if (*s1 > *s2)
            return 1;
        
        s1++;
        s2++;
    }
    
    if (*s1 == '\0' && *s2 != '\0')
        return 1;
    else if (*s1 != '\0' && *s2 == '\0')
        return -1;
    else
        return 0;
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

long long strtonum(const char* str, long long minval, long long maxval, bool* error)
{
    unsigned long long acc = 0;
    bool neg = false;
    
    if (*str == '-')
    {
        neg = true;
        str++;
    }
    
    unsigned long long cutoff = neg ? -(unsigned long long)LLONG_MIN : LLONG_MAX;
    int cutlim = (int)(cutoff % 10);
    cutoff /= 10;
    
    if (*str == '\0')
    {
        if (error != NULL) *error = true;
        return 0;
    }
    
    while (*str != '\0')
    {
        if (*str < '0' || *str > '9')
        {
            if (error != NULL) *error = true;
            return 0;
        }
        
        int c = *str - '0';
        
        if (acc > cutoff || (acc == cutoff && c > cutlim))
        {
            if (error != NULL) *error = true;
            return neg ? minval : maxval;
        }
        
        acc *= 10;
        acc += (unsigned long long) c;
        
        str++;
    }
    
    long long val = (long long)(neg ? -acc : acc);
    
    if (val < minval)
    {
        if (error != NULL) *error = true;
        return minval;
    }
    else if (val > maxval)
    {
        if (error != NULL) *error = true;
        return maxval;
    }
    else
    {
        if (error != NULL) *error = false;
        return val;
    }
}

void* memmove(void* dest, const void* src, size_t size)
{
    uint8* dest8 = dest;
    const uint8* src8 = src;;
    
    if (src > dest)
    {
        while (size--)
        {
            *dest8++ = *src8++;
        }
    }
    else if (src < dest)
    {
        dest8 += (size - 1);
        src8 += (size - 1);
        
        while (size--)
        {
            *dest8-- = *src8--;
        }
    }
    
    return dest;
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
