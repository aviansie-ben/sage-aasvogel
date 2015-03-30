/*
 * This file contains pre-initialization code. This code is mapped into the
 * .setup section and is generally executed BEFORE the kernel has been properly
 * mapped at 0xC0000000. Thus, the use of ANY functions or variables outside of
 * this file SHOULD NOT BE ASSUMED TO WORK PROPERLY!
 */

#include "preinit.h"

bool _preinit_pae_enabled __section__(".setup_data") = false;

// All entries need to have the present bit to prevent page faults, and the PDEs
// and PTEs need to have the writable bit.
static uint32 _preinit_pdpte_flags __section__(".setup_data") = 0b01;
static uint32 _preinit_pde_flags __section__(".setup_data")   = 0b10000011;

static uint64 _preinit_pdpt[4] __attribute__((section(".setup_pagedir"), aligned(0x20)));
static _preinit_page_struct _preinit_pd __attribute__((section(".setup_pagedir"), aligned(0x1000)));

void* _preinit_setup_paging(bool pae_supported)
{
    if (pae_supported && !_preinit_no_pae) return _preinit_pae_setup_paging();
    else return _preinit_legacy_setup_paging();
}

void* _preinit_legacy_setup_paging(void)
{
    uint32 i;
    
    _preinit_write_serial(_preinit_serial_page_legacy_msg);
    _preinit_pae_enabled = false;
    
    // We will use 4MiB pages to map physical addresses 0x00000000-0x3FFFFFFF to
    // virual addresses 0x00000000-0x3FFFFFFF and 0xC0000000-0xFFFFFFFF.
    for (i = 0; i < 0x100; i++)
    {
        // We map the 4MiB entry into the page directory at two locations: one for
        // lower-half access (for boot code) and one for higher-half access.
        _preinit_pd.legacy[0x000 + i] = _preinit_pd.legacy[0x300 + i] = (i << 22) | _preinit_pde_flags;
    }
    
    return &_preinit_pd;
}

void* _preinit_pae_setup_paging(void)
{
    uint64 pdpte;
    uint32 i;
    
    _preinit_write_serial(_preinit_serial_page_pae_msg);
    _preinit_pae_enabled = true;
    
    // Get the PDPT entry value to use
    pdpte = (((uint32)&_preinit_pd) & 0xFFFFF000) | _preinit_pdpte_flags;
    
    // We will be mapping the kernel in at both 0x00000000 and 0xC0000000, so we can
    // use the same page directory for both locations.
    _preinit_pdpt[0] = _preinit_pdpt[3] = pdpte;
    
    // We will use 2MiB pages to map physical addresses 0x00000000-0x3FFFFFFF to
    // virual addresses 0x00000000-0x3FFFFFFF and 0xC0000000-0xFFFFFFFF.
    for (i = 0; i < 0x200; i++)
    {
        // Map the entry for this 2MiB page into the page directory.
        _preinit_pd.pae[i] = (i << 21) | _preinit_pde_flags;
    }
    
    return &_preinit_pdpt;
}
