#include <printf.h>
#include <string.h>
#include <core/crash.h>

static int gprintf_str(const char* buf, size_t min_len, size_t max_len, bool zero_pad, bool left_justify, size_t* nc, size_t n, void* a, gprintf_write_char write)
{
    int err;
    uint32 len = strlen(buf);
    
    if (max_len == 0) max_len = 0xffffffffu;
    
    while (!left_justify && len < min_len)
    {
        len++;
        
        if ((*nc)++ < n && max_len != 0)
        {
            err = write(a, (zero_pad) ? '0' : ' ');
            if (err != E_SUCCESS) return err;
            
            max_len--;
        }
    }
    
    while (*buf != '\0')
    {
        if ((*nc)++ < n && max_len != 0)
        {
            err = write(a, *buf);
            if (err != E_SUCCESS) return err;
            
            max_len--;
        }
        
        buf++;
    }
    
    while (left_justify && len < min_len)
    {
        len++;
        
        if ((*nc)++ < n && max_len != 0)
        {
            err = write(a, (zero_pad) ? '0' : ' ');
            if (err != E_SUCCESS) return err;
            
            max_len--;
        }
    }
    
    return 0;
}

static int gprintf_arg(const char** format, char* buf, size_t* nc, size_t n, void* a, gprintf_write_char write, va_list* vararg)
{
    const char* s;
    int i;
    unsigned int u;
    long long l;
    
    size_t min_length = 0;
    size_t precision = 0;
    bool zero_pad = false;
    bool left_justify = false;
    
    if ((*format)[0] == '-')
    {
        *format += 1;
        left_justify = true;
    }
    
    if ((*format)[0] == '0')
    {
        *format += 1;
        zero_pad = true;
    }
    
    while ((*format)[0] >= '0' && (*format)[0] <= '9')
    {
        min_length = (min_length * 10) + (uint32)((*format)[0] - '0');
        *format += 1;
    }
    
    if ((*format)[0] == '.')
    {
        *format += 1;
        while ((*format)[0] >= '0' && (*format)[0] <= '9')
        {
            precision = (precision * 10) + (uint32)((*format)[0] - '0');
            *format += 1;
        }
    }
    
    if ((*format)[0] == 's')
    {
        *format += 1;
        
        s = va_arg(*vararg, const char*);
        return gprintf_str(s, min_length, precision, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'd')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 10);
        return gprintf_str(buf, min_length, precision, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'u')
    {
        *format += 1;
        
        u = va_arg(*vararg, unsigned int);
        itoa_l(u, buf, 10);
        return gprintf_str(buf, min_length, precision, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'x' || (*format)[0] == 'X')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 16);
        return gprintf_str(buf, min_length, precision, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'd')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 10);
        return gprintf_str(buf, min_length, precision, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'l' && ((*format)[1] == 'x' || (*format)[1] == 'X'))
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 16);
        return gprintf_str(buf, min_length, precision, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == '%')
    {
        *format += 1;
        
        if ((*nc)++ < n)
            return write(a, '%');
        else
            return E_SUCCESS;
    }
    else
    {
        return E_INVALID;
    }
}

int gprintf(const char* format, size_t n, void* a, gprintf_write_char write, va_list vararg)
{
    int err;
    size_t nc;
    char ch;
    char buf[256];
    
    nc = 0;
    while ((ch = *(format++)) != '\0')
    {
        if (ch == '%')
        {
            err = gprintf_arg(&format, buf, &nc, n, a, write, &vararg);
            if (err != E_SUCCESS) return -err;
        }
        else
        {
            if (nc++ < n)
            {
                err = write(a, ch);
                if (err != E_SUCCESS) return -err;
            }
        }
    }
    
    return (int)nc;
}

static int sprintf_write(void* a, char c)
{
    char** s = (char**) a;
    
    *((*s)++) = c;
    return E_SUCCESS;
}

int snprintf(char* s, size_t n, const char* format, ...)
{
    va_list vararg;
    int r;
    
    va_start(vararg, format);
    r = vsnprintf(s, n, format, vararg);
    va_end(vararg);
    
    return r;
}

int vsnprintf(char* s, size_t n, const char* format, va_list vararg)
{
    int r;
    
    r = gprintf(format, n - 1, &s, sprintf_write, vararg);
    if (r >= 0) *s = '\0';
    
    return r;
}
