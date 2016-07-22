#include <string.h>
#include <typedef.h>
#include <assert.h>

#include <setjmp.h>
#include <memory/page.h>
#include <lock/spinlock.h>

static const char* itoa_values = "0123456789ABCDEF";
static const char* errcode_names[] = {
    "E_SUCCESS",
    "E_NOT_FOUND",
    "E_NOT_SUPPORTED",
    "E_IO_ERROR",
    "E_INVALID",
    "E_BUSY",
    "E_NOT_DIR",
    "E_IS_DIR",
    "E_NAME_TOO_LONG",
    "E_LOOP",
    "E_NO_SPACE",
    "E_NO_MEMORY",
    "E_ALREADY_EXISTS"
};

const char* errcode_to_str(int errcode)
{
    if (errcode >= 0 && (unsigned int)errcode < (sizeof(errcode_names) / sizeof(*errcode_names)))
        return errcode_names[errcode];
    else
        return "E_UNKNOWN";
}

void strcpy(char* dest, const char* src)
{
    while (*src != '\0')
        *(dest++) = *(src++);
    
    *dest = '\0';
}

void strncpy(char* dest, const char* src, size_t n)
{
    while (*src != '\0' && n != 0)
    {
        *(dest++) = *(src++);
        n--;
    }
    
    while (n != 0)
    {
        *(dest++) = '\0';
        n--;
    }
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
    return strncmp(s1, s2, 0xffffffffu);
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    while (n != 0 && *s1 != '\0' && *s2 != '\0')
    {
        if (*s1 < *s2)
            return -1;
        else if (*s1 > *s2)
            return 1;
        
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0)
        return 0;
    else if (*s1 == '\0' && *s2 != '\0')
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

static unsigned long long __strtounum(const char** str, unsigned long long max, unsigned int base, bool* error)
{
    unsigned long long acc = 0;
    
    unsigned long long cutoff = max / base;
    unsigned int cutlim = (unsigned int)(max % base);
    
    if (**str == '\0')
    {
        if (error != NULL) *error = true;
        return 0;
    }
    
    while (**str != '\0')
    {
        unsigned int val;
        
        if (**str >= '0' && **str <= '9')
            val = (unsigned int)(**str - '0');
        else if (**str >= 'A' && **str <= 'Z')
            val = (unsigned int)(**str - 'A' + 10);
        else if (**str >= 'a' && **str <= 'z')
            val = (unsigned int)(**str - 'a' + 10);
        else
            break;
        
        if (val >= base)
            break;
        
        if (acc > cutoff || (acc == cutoff && val > cutlim))
        {
            if (error != NULL) *error = true;
            return max;
        }
        
        acc = (acc * base) + val;
        (*str)++;
    }
    
    return acc;
}

long long strtonum(const char* str, long long minval, long long maxval, bool* error)
{
    long long acc;
    bool neg = false;
    bool error2;
    
    if (*str == '-')
    {
        neg = true;
        str++;
    }
    
    acc = (long long) __strtounum(&str, LLONG_MAX, 10, &error2);
    if (neg) acc = -acc;
    
    if (error2 || *str != '\0')
    {
        if (error != NULL) *error = true;
        return acc;
    }
    else if (acc > maxval)
    {
        if (error != NULL) *error = true;
        return maxval;
    }
    else if (acc < minval)
    {
        if (error != NULL) *error = true;
        return minval;
    }
    
    if (error != NULL) *error = false;
    return acc;
    
}

unsigned long strtoul(const char* src, char** endptr, int base)
{
    unsigned long acc;
    
    acc = (unsigned long) __strtounum(&src, ULONG_MAX, (unsigned int) base, NULL);
    
    if (endptr != NULL) *endptr = (char*) src;
    return acc;
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

int memcmp(const void* p1, const void* p2, size_t size)
{
    const uint8* p1_b = p1;
    const uint8* p2_b = p2;
    
    while (size != 0 && *p1_b == *p2_b)
    {
        size--;
        p1_b++;
        p2_b++;
    }
    
    return size == 0 ? 0 : (*p1_b - *p2_b);
}

void* memcpy_safe(void* dest, const void* src, size_t size)
{
    jmp_buf env;
    void* result;
    
    // We cannot allow interrupts while a temporary page fault handler is set
    uint32 eflags = eflags_save();
    asm volatile ("cli");
    
    if (!setjmp(env))
    {
        kmem_page_set_temp_fault_handler(env, NULL, NULL);
        result = memcpy(dest, src, size);
        kmem_page_clear_temp_fault_handler();
    }
    else
    {
        result = NULL;
    }
    
    eflags_load(eflags);
    
    return result;
}
