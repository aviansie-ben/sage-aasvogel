#include <core/crash.hpp>
#include <tty.hpp>

using tty::boot_console;

void do_crash(const char* message, const char* file, const char* function, uint32 line)
{
    boot_console.setBase(10);
    
    boot_console << "\nThe Sage Aasvogel kernel has crashed!\nReason: " << message << "\nFile: "
                 << file << "\nLocation: " << function << " at line " << line << "\n";
    
    hang();
}
