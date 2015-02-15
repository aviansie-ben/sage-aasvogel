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
static uint32 _preinit_pde_flags __section__(".setup_data")   = 0b11;
static uint32 _preinit_pte_flags __section__(".setup_data")   = 0b11;

static uint64 _preinit_pdpt[4] __attribute__((section(".setup_pagedir"), aligned(0x20)));
static _preinit_page_struct _preinit_pd __attribute__((section(".setup_pagedir"), aligned(0x1000)));
static _preinit_page_struct _preinit_pt __attribute__((section(".setup_pagedir"), aligned(0x1000)));

void* _preinit_setup_paging(bool pae_supported)
{
    if (pae_supported && !_preinit_no_pae) return _preinit_pae_setup_paging();
    else return _preinit_legacy_setup_paging();
}

void* _preinit_legacy_setup_paging(void)
{
    uint32 pde;
    uint32 i;
    
    _preinit_write_serial(_preinit_serial_page_legacy_msg);
    _preinit_pae_enabled = false;
    
    // Get the PD entry value to use
    pde = (((uint32)&_preinit_pt) & 0xFFFFF000) | _preinit_pde_flags;
    
    // We will be mapping the kernel in at both 0x00000000 and 0xC0000000, so we can
    // use the same page table for both locations.
    _preinit_pd.legacy[0x000] = _preinit_pd.legacy[0x300] = pde;
    
    // We only map the first 512 pages to ensure that we get the same amount of memory
    // mapped as we do when PAE is enabled.
    for (i = 0; i < 512; i++)
    {
        _preinit_pt.legacy[i] = (i * 0x1000) | _preinit_pte_flags;
    }
    
    return &_preinit_pd;
}

void* _preinit_pae_setup_paging(void)
{
    uint64 pdpte, pde;
    uint32 i;
    
    _preinit_write_serial(_preinit_serial_page_pae_msg);
    _preinit_pae_enabled = true;
    
    // Get the PDPT and PD entry values to use
    pdpte = (((uint32)&_preinit_pd) & 0xFFFFF000) | _preinit_pdpte_flags;
    pde = (((uint32)&_preinit_pt) & 0xFFFFF000) | _preinit_pde_flags;
    
    // We will be mapping the kernel in at both 0x00000000 and 0xC0000000, so we can
    // use the same page directory for both locations.
    _preinit_pdpt[0] = _preinit_pdpt[3] = pdpte;
    
    // Map the page table into the page directory.
    _preinit_pd.pae[0] = pde;
    
    // Just go ahead and map as much memory as can be stored in a PAE page table.
    for (i = 0; i < 512; i++)
    {
        _preinit_pt.pae[i] = ((i * 0x1000) & 0xFFFFF000) | _preinit_pte_flags;
    }
    
    return &_preinit_pdpt;
}
