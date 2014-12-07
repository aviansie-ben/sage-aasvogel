/*
 * This file contains pre-initialization code. This code is mapped into the
 * .setup section and is generally executed BEFORE the kernel has been properly
 * mapped at 0xC0000000. Thus, the use of ANY functions or variables outside of
 * this file SHOULD NOT BE ASSUMED TO WORK PROPERLY!
 */

#include "preinit.hpp"

char _preinit_serial_stop_msg[] __section__(".setup_data") = "\r\nKernel pre-initialization failed! Hanging...\r\n";
char _preinit_serial_init_msg[] __section__(".setup_data") = "Serial port initialized!\r\nPre-initialization in progress...\r\n";
char _preinit_serial_page_legacy_msg[] __section__(".setup_data") = "Setting up pre-initialization paging without PAE support...\r\n";
char _preinit_serial_page_pae_msg[] __section__(".setup_data") = "Setting up pre-initialization paging with PAE support...\r\n";

char _preinit_cmdline_serial_debug[] __section__(".setup_data") = "preinit_serial";
char _preinit_cmdline_no_pae[] __section__(".setup_data") = "no_pae";
