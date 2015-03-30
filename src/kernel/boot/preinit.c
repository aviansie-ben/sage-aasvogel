/*
 * This file contains pre-initialization code. This code is mapped into the
 * .setup section and is generally executed BEFORE the kernel has been properly
 * mapped at 0xC0000000. Thus, the use of ANY functions or variables outside of
 * this file SHOULD NOT BE ASSUMED TO WORK PROPERLY!
 */

#include "preinit.h"

bool _preinit_no_pae __section__(".setup_data") = false;
bool _preinit_serial_enable __section__(".setup_data") = false;

void _preinit_parse_cmdline(multiboot_info* mb_info)
{
    const char* cmdline;
    
    // If the boot loader didn't provide a cmdline, just use the defaults...
    if ((mb_info->flags & MB_FLAG_CMDLINE) == 0) return;
    
    cmdline = (const char*) mb_info->cmdline_addr;
    
    _preinit_serial_enable = _preinit_cmdline_option_set(cmdline, _preinit_cmdline_serial_debug);
    _preinit_no_pae = _preinit_cmdline_option_set(cmdline, _preinit_cmdline_no_pae);
}

bool _preinit_cmdline_option_set(const char* cmdline, const char* option)
{
    int matched = 0; // Number of characters of the option that have been matched
    bool skip = true; // We skip the first part, since it is the location of the kernel binary
    
    while (*cmdline != '\0')
    {
        if (!skip)
        {
            // When not skipping characters, compare them to the next expected character
            if (*cmdline == option[matched])
                matched++;
            else
                skip = true;
        }
        
        // Stop skipping characters and reset the matched count if we reach a space
        if (*cmdline == ' ')
        {
            skip = false;
            matched = 0;
        }
        
        cmdline++;
        
        // Check if we've found the requested option.
        if (option[matched] == '\0' && (*cmdline == ' ' || *cmdline == '\0'))
        {
            return true;
        }
    }
    
    // We didn't find the requested option.
    return false;
}

void _preinit_error(const char* message)
{
    // Pointer into the standard location for writing characters to the console
    uint8* video = (uint8*) 0xB8000;
    size_t i;
    
    // Each character is printed individually, and the console color is set for each
    // printed character.
    for (i = 0; message[i] != '\0'; i++)
    {
        video[i * 2] = (uint8) message[i];
        video[(i * 2) + 1] = PREINIT_ERROR_COLOR;
    }
    
    // If possible, write the error message out to the serial port as well
    _preinit_write_serial(message);
    _preinit_write_serial(_preinit_serial_stop_msg);
    
    // We have crashed, so hang the CPU
    hang();
}
