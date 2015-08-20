#include <printf.h>
#include <string.h>
#include <core/crash.h>

static int gprintf_str(const char* buf, size_t min_len, bool zero_pad, bool left_justify, size_t* nc, size_t n, void* a, gprintf_write_char write)
{
    int err;
    uint32 len = strlen(buf);
    
    while (!left_justify && len < min_len)
    {
        len++;
        
        if ((*nc)++ < n)
        {
            err = write(a, (zero_pad) ? '0' : ' ');
            if (err != E_SUCCESS) return err;
        }
    }
    
    while (*buf != '\0' && *nc < n)
    {
        if ((*nc)++ < n)
        {
            err = write(a, *buf);
            if (err != E_SUCCESS) return err;
        }
        
        buf++;
    }
    
    while (left_justify && len < min_len && *nc < n)
    {
        len++;
        
        if ((*nc)++ < n)
        {
            err = write(a, (zero_pad) ? '0' : ' ');
            if (err != E_SUCCESS) return err;
        }
    }
    
    return 0;
}

static int gprintf_arg(const char** format, char* buf, size_t* nc, size_t n, void* a, gprintf_write_char write, va_list* vararg)
{
    const char* s;
    int i;
    long long l;
    
    size_t min_length = 0;
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
    
    if ((*format)[0] == 's')
    {
        *format += 1;
        
        s = va_arg(*vararg, const char*);
        return gprintf_str(s, min_length, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'd')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 10);
        return gprintf_str(buf, min_length, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'x')
    {
        *format += 1;
        
        i = va_arg(*vararg, int);
        itoa(i, buf, 16);
        return gprintf_str(buf, min_length, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'd')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 10);
        return gprintf_str(buf, min_length, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == 'l' && (*format)[1] == 'x')
    {
        *format += 2;
        
        l = va_arg(*vararg, long long);
        itoa_l(l, buf, 16);
        return gprintf_str(buf, min_length, zero_pad, left_justify, nc, n, a, write);
    }
    else if ((*format)[0] == '%')
    {
        *format += 1;
        *nc += 1;
        
        return write(a, '%');
    }
    else if ((*format)[0] == 'C' && (*format)[1] != '\0')
    {
        i = va_arg(*vararg, int);
        return write(a, '%');
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
    while (nc < n && (ch = *(format++)) != '\0')
    {
        if (ch == '%')
        {
            err = gprintf_arg(&format, buf, &nc, n, a, write, &vararg);
            if (err != E_SUCCESS) return err;
        }
        else
        {
            if (nc++ < n)
            {
                err = write(a, ch);
                if (err != E_SUCCESS) return err;
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
