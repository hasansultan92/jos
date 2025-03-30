// implement fork from user space

#include "inc/assert.h"
#include "inc/env.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/stdio.h"
#include "inc/types.h"
#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
extern void _pgfault_upcall(void);

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

    // // Check if fault is a write and page is COW
    // cprintf("\n==== PAGE FAULT ====\n");
    // cprintf("Fault VA: %08x\n", addr);
    // cprintf("Fault err: %08x\n", err);
    // cprintf("UVPT entry: %08x\n", uvpt[PTX(addr)]);
    // cprintf("UVPD entry: %08x\n", uvpd[PDX(addr)]);
    
    if ((uintptr_t)addr >= UTOP) {
        panic("pgfault: fault VA %08x above UTOP", addr);
    }
    
    if (!(err & FEC_WR)) {
        panic("pgfault: not a write (error code %08x)", err);
    }
    if (!(uvpt[PTX(addr)] & PTE_COW)) {
        panic("pgfault: not COW (PTE %08x)", uvpt[PTX(addr)]);
    }

    // Allocate new page at temporary location
    if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0){
        panic("pgfault: sys_page_alloc: %e", r); // Propogated error
	}

    // Copy old page to new page
    memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);

    // Map new page at old address
    if ((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P|PTE_U|PTE_W)) < 0){
        panic("sys_page_map: %e", r);
	}

    // Unmap temporary page
    if ((r = sys_page_unmap(0, PFTEMP)) < 0){
        panic("pgfault: sys_page_unmap: %e", r);  // Propogated error
	}

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?) "Because the child might spawn another fork which will also need to be copied over"
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
    void *addr = (void*)(pn * PGSIZE);
    // Check if page is present and either writable or COW
    if (!(uvpt[pn] & PTE_P)){
        panic("duppage: page not present");
	}
    
    if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW) {
        // Map COW in child
        if ((r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P)) < 0){
			cprintf("mapping failed\n");
            return r;
		}
        // Remap COW in parent
        if ((r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P)) < 0){
			cprintf("mapping failed 2nd\n");
            return r;
		}
    } else {
        // Map read-only pages directly
		cprintf("I am in read-only pages here\n");
        if ((r = sys_page_map(0, addr, envid, addr, PTE_U|PTE_P)) < 0) {
            return r;}
    }
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t envid;
	uintptr_t addr;
	int r;
	envid = sys_exofork();
	if (envid < 0){
		panic("Error with fork creation");
	}
	if (envid == 0){
		// Should be the child process
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

    for (uintptr_t addr = 0; addr < UTOP; addr += PGSIZE) {
        if ((uvpd[PDX(addr)] & (PTE_P)) && 
            (uvpt[PTX(addr)] & (PTE_P)) &&
            (addr != (UXSTACKTOP - PGSIZE))) {  // Skip exception stack
                if (duppage(envid, PTX(addr)) < 0) {
                panic("error");
            }
        }
    }

	// Allocate new exception stack
	if ((r = sys_page_alloc(envid, (void*)(UXSTACKTOP-PGSIZE), PTE_U|PTE_P|PTE_W)) < 0){
		panic("fork: sys_page_alloc for exception stack: %e", r);
    }
    // Set child's page fault upcall
    if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0){
        panic("fork: sys_env_set_pgfault_upcall: %e", r);
    }

    // Mark child runnable
    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0) {
        panic("fork: sys_env_set_status: %e", r);
    }
    return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
