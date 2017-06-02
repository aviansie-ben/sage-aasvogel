#include <core/gdb_stub.h>
#include <core/klog.h>
#include <io/serial.h>
#include <lock/spinlock.h>
#include <string.h>
#include <core/crash.h>
#include <printf.h>
#include <cpu/idt.h>
#include <assert.h>

#ifdef GDB_STUB_ENABLED

#define EFLAGS_TF (1u << 8)

#define PACKET_BUF_SIZE 400

#define NUM_REGS 16
#define REG_DATA_SIZE (NUM_REGS * 8)

static const char* stop_reason_replies[] = {
    "S02", // interrupted by gdb
    "S05", // breakpoint
    "S06", // aborted
    "S0B", // page fault
};

static serial_port* gdb_port;

static char packet_buf[PACKET_BUF_SIZE];
static size_t packet_buf_len;
static bool packet_ready;

static bool packet_sending;
static uint8 packet_sending_checksum;

static bool stepping;
static bool suspended;
static int stop_reason;

static int gdb_send_msg(const char* c)
{
    return serial_send_spin(gdb_port, c, strlen(c));
}

static int gdb_send_char(char c)
{
    return serial_send_spin(gdb_port, &c, 1);
}

static int gdb_receive_char(char* c)
{
    return serial_receive_spin(gdb_port, c, 1);
}

static int char_from_hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else
        return -1;
}

static char hex_to_char(uint8 d)
{
    if (d <= 9)
        return (char)('0' + d);
    else
        return (char)('a' + d - 10);
}

static int gdb_parse_checksum(const char* c, uint8* checksum)
{
    int cd1 = char_from_hex(c[0]);
    int cd2 = char_from_hex(c[1]);

    if (cd1 >= 0 && cd2 >= 0)
    {
        *checksum = (uint8)(cd1 * 16 + cd2);
        return E_SUCCESS;
    }
    else
    {
        return E_INVALID;
    }
}

static char* gdb_format_checksum(char* c, uint8 checksum)
{
    c[0] = hex_to_char((uint8)(checksum >> 4));
    c[1] = hex_to_char(checksum & 0xf);
    c[2] = '\0';

    return c;
}

static uint8 gdb_calculate_checksum(const char* packet)
{
    uint8 s = 0;

    while (*packet != '\0')
        s = (uint8)(s + *(packet++));

    return s;
}

static int gdb_start_send_packet(void)
{
    int error;

    assert(!packet_sending);

    if ((error = gdb_send_msg("$")) != E_SUCCESS)
        return error;

    packet_sending = true;
    packet_sending_checksum = 0;

    return E_SUCCESS;
}

static int gdb_send_packet_fragment(const char* fragment)
{
    int error;

    assert(packet_sending);

    packet_sending_checksum = (uint8)(packet_sending_checksum + gdb_calculate_checksum(fragment));

    if ((error = gdb_send_msg(fragment)) != E_SUCCESS)
    {
        packet_sending = false;
        return error;
    }

    return E_SUCCESS;
}

static int gdb_send_packet_char(void* ctx, char c)
{
    int error;

    assert(packet_sending);

    packet_sending_checksum = (uint8)(packet_sending_checksum + c);

    if ((error = gdb_send_char(c)) != E_SUCCESS)
    {
        packet_sending = false;
        return error;
    }

    return E_SUCCESS;
}

static int gdb_send_packet_fragment_printf(const char* format, ...)
{
    va_list vararg;
    int r;

    va_start(vararg, format);
    r = gprintf(format, 0xffffffffu, NULL, gdb_send_packet_char, vararg);
    va_end(vararg);

    return r;
}

static int gdb_end_send_packet(void)
{
    char checksum[3];
    int error;

    assert(packet_sending);

    packet_sending = false;

    if ((error = gdb_send_msg("#")) != E_SUCCESS)
        return error;

    if ((error = gdb_send_msg(gdb_format_checksum(checksum, packet_sending_checksum))) != E_SUCCESS)
        return error;

    return E_SUCCESS;
}

static int gdb_send_packet(const char* packet)
{
    int error;

    if ((error = gdb_start_send_packet()) != E_SUCCESS)
        return error;

    if ((error = gdb_send_packet_fragment(packet)) != E_SUCCESS)
        return error;

    if ((error = gdb_end_send_packet()) != E_SUCCESS)
        return error;

    return E_SUCCESS;
}

static void gdb_write_reg(char* buf, uint32 val)
{
    snprintf(
        buf,
        9,
        "%02x%02x%02x%02x",
        (val & 0xff),
        ((val >> 8) & 0xff),
        ((val >> 16) & 0xff),
        ((val >> 24) & 0xff)
    );
}

static uint32 gdb_read_reg(const char* p, jmp_buf env)
{
    char buf[9];
    char* endptr;
    uint32 val;

    if (strlen(p) < 8)
        longjmp(env, 1);

    strncpy(buf, p, 8);
    buf[8] = '\0';

    val = strtoul(buf, &endptr, 16);

    if (endptr != &buf[8])
        longjmp(env, 1);

    return ((val & 0x000000ffu) << 24) |
        ((val & 0x0000ff00u) << 8) |
        ((val & 0x00ff0000u) >> 8) |
        ((val & 0xff000000u) >> 24);
}

static void gdb_handle_halt_reason(const char* p, regs32* r)
{
    gdb_send_packet(stop_reason_replies[stop_reason]);
}

static void gdb_handle_reg_read(const char* p, regs32* r)
{
    char packet[REG_DATA_SIZE + 1];

    gdb_write_reg(&packet[  0], r->eax   );
    gdb_write_reg(&packet[  8], r->ecx   );
    gdb_write_reg(&packet[ 16], r->edx   );
    gdb_write_reg(&packet[ 24], r->ebx   );
    gdb_write_reg(&packet[ 32], r->esp   );
    gdb_write_reg(&packet[ 40], r->ebp   );
    gdb_write_reg(&packet[ 48], r->esi   );
    gdb_write_reg(&packet[ 56], r->edi   );
    gdb_write_reg(&packet[ 64], r->eip   );
    gdb_write_reg(&packet[ 72], r->eflags);
    gdb_write_reg(&packet[ 80], r->cs    );
    gdb_write_reg(&packet[ 88], r->ss    );
    gdb_write_reg(&packet[ 96], r->ds    );
    gdb_write_reg(&packet[104], r->es    );
    gdb_write_reg(&packet[112], r->fs    );
    gdb_write_reg(&packet[120], r->gs    );

    gdb_send_packet(packet);
}

static void gdb_handle_reg_write(const char* p, regs32* r)
{
    jmp_buf env;

    if (!setjmp(env))
    {
        p++;

        r->eax    = gdb_read_reg(&p[  0], env);
        r->ecx    = gdb_read_reg(&p[  8], env);
        r->edx    = gdb_read_reg(&p[ 16], env);
        r->ebx    = gdb_read_reg(&p[ 24], env);
        r->esp    = gdb_read_reg(&p[ 32], env);
        r->ebp    = gdb_read_reg(&p[ 40], env);
        r->esi    = gdb_read_reg(&p[ 48], env);
        r->edi    = gdb_read_reg(&p[ 56], env);
        r->eip    = gdb_read_reg(&p[ 64], env);
        r->eflags = gdb_read_reg(&p[ 72], env);
        r->cs     = gdb_read_reg(&p[ 80], env);
        r->ss     = gdb_read_reg(&p[ 88], env);
        r->ds     = gdb_read_reg(&p[ 96], env);
        r->es     = gdb_read_reg(&p[104], env);
        r->fs     = gdb_read_reg(&p[112], env);
        r->gs     = gdb_read_reg(&p[120], env);

        gdb_send_packet("OK");
    }
    else
    {
        gdb_send_packet("E00");
    }
}

static void gdb_parse_addr(const char* p, uint32* addr, const char** remainder)
{
    *addr = strtoul(p, (char**) remainder, 16);
}

static bool gdb_parse_addr_len(const char* p, uint32* addr, uint32* len, const char** remainder)
{
    gdb_parse_addr(p, addr, remainder);

    if (**remainder != ',')
        return false;

    (*remainder)++;
    *len = strtoul(*remainder, (char**) remainder, 16);

    if (**remainder != '\0' && **remainder != ':')
        return false;

    return true;
}

static void gdb_handle_mem_read(const char* p, regs32* r)
{
    uint32 addr;
    uint32 len;

    if (!gdb_parse_addr_len(p + 1, &addr, &len, &p) || *p != '\0')
    {
        gdb_send_packet("E00");
        return;
    }

    if (len > PACKET_BUF_SIZE / 2)
        len = PACKET_BUF_SIZE / 2;

    char response_buf[PACKET_BUF_SIZE + 1];
    uint8 mem[PACKET_BUF_SIZE / 2];

    if (memcpy_safe(mem, (void*)addr, len) != NULL)
    {
        for (size_t i = 0; i < len; i++)
            snprintf(&response_buf[i * 2], 3, "%02x", mem[i]);
    }
    else
    {
        gdb_send_packet("E01");
        return;
    }

    gdb_send_packet(response_buf);
}

static bool gdb_parse_hex_mem(const char* p, uint32 len, uint8* mem)
{
    int d1, d2;

    while (len-- != 0)
    {
        if ((d1 = char_from_hex(*(p++))) < 0)
            return false;

        if ((d2 = char_from_hex(*(p++))) < 0)
            return false;

        *(mem++) = (uint8)((d1 << 4) | d2);
    }

    return *p == '\0';
}

static void gdb_handle_mem_write(const char* p, regs32* r)
{
    uint32 addr;
    uint32 len;

    if (!gdb_parse_addr_len(p + 1, &addr, &len, &p) || *p != ':')
    {
        gdb_send_packet("E00");
        return;
    }

    uint8 mem[PACKET_BUF_SIZE / 2];

    if (!gdb_parse_hex_mem(p + 1, len, mem))
    {
        gdb_send_packet("E00");
        return;
    }

    bool wp = kmem_disable_write_protect();

    if (memcpy_safe((void*)addr, mem, len) != NULL)
        gdb_send_packet("OK");
    else
        gdb_send_packet("E01");

    if (wp)
        kmem_enable_write_protect();
}

static void gdb_handle_continue(const char* p, regs32* r)
{
    uint32 addr;

    if (*(p + 1) != '\0')
    {
        gdb_parse_addr(p + 1, &addr, &p);

        if (*p != '\0')
        {
            gdb_send_packet("E00");
            return;
        }

        r->eip = addr;
    }

    suspended = false;
}

static void gdb_handle_step(const char* p, regs32* r)
{
    uint32 addr;

    if (*(p + 1) != '\0')
    {
        gdb_parse_addr(p + 1, &addr, &p);

        if (*p != '\0')
        {
            gdb_send_packet("E00");
            return;
        }

        r->eip = addr;
    }

    r->eflags |= EFLAGS_TF;

    suspended = false;
    stepping = true;
}

static void gdb_handle_query_packet(const char* p, regs32* r)
{
    int error;

    if (strncmp(p, "qSupported", strlen("qSupported")) == 0)
    {
        if ((error = gdb_start_send_packet()) != E_SUCCESS)
            return;

        if ((error = -gdb_send_packet_fragment_printf("PacketSize=%u", (uint32)(PACKET_BUF_SIZE - 1))) > 0)
            return;

        if ((error = gdb_end_send_packet()) != E_SUCCESS)
            return;
    }
    else
    {
        gdb_send_packet("");
    }
}

static void gdb_handle_packet(const char* p, regs32* r)
{
    packet_ready = false;

    if (!suspended)
    {
        suspended = true;
        stop_reason = STOPPED_INTERRUPTED;
    }

    switch (p[0])
    {
        case '?':
            gdb_handle_halt_reason(p, r);
            break;
        case 'g':
            gdb_handle_reg_read(p, r);
            break;
        case 'G':
            gdb_handle_reg_write(p, r);
            break;
        case 'm':
            gdb_handle_mem_read(p, r);
            break;
        case 'M':
            gdb_handle_mem_write(p, r);
            break;
        case 'c':
            gdb_handle_continue(p, r);
            break;
        case 's':
            gdb_handle_step(p, r);
            break;
        case 'q':
        case 'Q':
            gdb_handle_query_packet(p, r);
            break;
        default:
            gdb_send_packet("");
    }
}

static void gdb_handle_char(char c)
{
    if (c == '\x03')
    {
        strcpy(packet_buf, "$?");
        packet_buf_len = strlen("$?");
        packet_ready = true;
        return;
    }

    if (packet_buf_len == 0 && c != '$') return;

    if (packet_buf_len == PACKET_BUF_SIZE - 1)
    {
        packet_buf_len = 0;
        gdb_send_msg("-");

        return;
    }

    packet_buf[packet_buf_len++] = c;

    if (packet_buf_len >= 4 && packet_buf[packet_buf_len - 3] == '#')
    {
        packet_buf[packet_buf_len] = '\0';
        packet_buf[packet_buf_len - 3] = '\0';

        uint8 checksum;

        if (gdb_parse_checksum(&packet_buf[packet_buf_len - 2], &checksum) == E_SUCCESS && checksum == gdb_calculate_checksum(&packet_buf[1]))
        {
            packet_ready = true;
            packet_buf_len = 0;
            gdb_send_msg("+");
        }
        else
        {
            packet_buf_len = 0;
            gdb_send_msg("-");
        }
    }
}

static void gdb_loop(regs32* r)
{
    char c;

    while (suspended)
    {
        while (!packet_ready)
        {
            if (gdb_receive_char(&c) == E_SUCCESS)
                gdb_handle_char(c);
        }

        gdb_handle_packet(&packet_buf[1], r);
    }
}

static void gdb_serial_sink(regs32* r, serial_port* p, char c)
{
    gdb_handle_char(c);

    if (packet_ready)
        gdb_handle_packet(&packet_buf[1], r);

    if (suspended)
        gdb_loop(r);
}

static void gdb_breakpoint_handler(regs32* r)
{
    if (!suspended)
    {
        suspended = true;
        stop_reason = STOPPED_BREAKPOINT;
    }

    gdb_send_packet(stop_reason_replies[stop_reason]);
    gdb_loop(r);
}

static void gdb_debug_handler(regs32* r)
{
    if (stepping)
    {
        r->eflags &= ~EFLAGS_TF;
        stepping = false;
    }

    if (!suspended)
    {
        suspended = true;
        stop_reason = STOPPED_BREAKPOINT;
    }

    gdb_send_packet(stop_reason_replies[stop_reason]);

    gdb_loop(r);
}

void gdb_stub_init(const boot_param* param)
{
    int gdb_port_num = cmdline_get_int(param, "kernel_gdb_serial", 0, NUM_SERIAL_PORTS - 1, -1);
    bool gdb_break = cmdline_get_bool(param, "kernel_gdb_break");

    if (gdb_port_num != -1)
    {
        gdb_port = &serial_ports[gdb_port_num];

        if (!gdb_port->io_port)
        {
            gdb_port = NULL;
            klog(KLOG_LEVEL_ERR, "gdb_stub: cannot listen on disconnected serial port %d\n", gdb_port_num);
            return;
        }

        kmem_page_preinit(param);

        serial_set_sink(gdb_port, gdb_serial_sink);

        idt_register_isr_handler(3, gdb_breakpoint_handler);
        idt_register_isr_handler(1, gdb_debug_handler);

        klog(KLOG_LEVEL_DEBUG, "gdb_stub: listening on serial port %d\n", gdb_port_num);

        if (gdb_break)
            asm volatile ("int $3");
    }
}

bool gdb_stub_is_active(void)
{
    return gdb_port != NULL;
}

void gdb_stub_break(int reason)
{
    if (gdb_port != NULL)
    {
        uint32 eflags = eflags_save();
        asm volatile ("cli");

        suspended = true;
        stop_reason = reason;

        asm volatile ("int $3");

        eflags_load(eflags);
    }
}

void gdb_stub_break_interrupt(regs32* r, int reason)
{
    if (gdb_port != NULL)
    {
        suspended = true;
        stop_reason = reason;

        gdb_send_packet(stop_reason_replies[reason]);
        gdb_loop(r);
    }
}

#else

void gdb_stub_init(const boot_param* param)
{
    if (cmdline_get_int(param, "kernel_gdb_serial", 0, NUM_SERIAL_PORTS - 1, -1) != -1)
        klog(KLOG_LEVEL_WARN, "gdb_stub: requested, but support not compiled\n");
}

bool gdb_stub_is_active(void)
{
    return false;
}

void gdb_stub_break(int reason)
{
}

void gdb_stub_break_interrupt(regs32* r, int reason)
{
}

#endif
