// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include "inc/mmu.h"
#include "inc/types.h"
#include "kdebug.h"
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/consoleColors.h>
#include <kern/hidden.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

int show(int argc, char **argv, struct Trapframe *tf) {
	cprintf(BLUE("-----") " " RED("TEAM") " " GREEN("98") " " YELLOW("JOS") " " MAGENTA("!")" " BLUE("------") "\n");
	return 0;
}

int exec_hidden_cases(int argc, char **argv, struct Trapframe *tf) {
	hidden_test_cases();
	return 0;
}

// LAB 1: add your command to here...
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "hidden", "Run hidden test cases", exec_hidden_cases},
	{ "backtrace", "Backtrace the stack", mon_backtrace},
	{ "show", "fancy art on console", show},
	{"clear", "clear terminal screen",clear},
	{"memmap","display physical page address mappings", memmap},
	{"setpermission", "change permissions at addresses",setPerm},
	{"memdump","isplay memory contents in 16-byte chunks within the specified address range", memdump},
};

int clear(int argc, char **argv, struct Trapframe *tf) {
	cprintf("\x1b[2J\x1b[H"); // Found on the internet, QEMU supports this, fun.
	return 0;
}


int get_permission_flag(const char *perm_str) {
    if (strcmp(perm_str, "PTE_U") == 0) return PTE_U;
    if (strcmp(perm_str, "PTE_W") == 0) return PTE_W;
    if (strcmp(perm_str, "PTE_P") == 0) return PTE_P;
    if (strcmp(perm_str, "PTE_AVAIL") == 0) return PTE_AVAIL;
    return -1; // Invalid permission
}

int setPerm(int argc, char ** argv, struct Trapframe *tf){
	if (argc < 3){
		cprintf("%s: Usage: setperm <va> <perm>\n", __func__);
		return 1;
	}

    uintptr_t virtualAddy = strtol(argv[1], NULL, 16);
    int perm = get_permission_flag(argv[2]); // Looks mwuah
	cprintf("%s: %d\n", __func__, perm);
	pte_t *pte = pgdir_walk(kern_pgdir, (void *)virtualAddy, 0);
    if (!pte || !(*pte & PTE_P)) {
        cprintf("%s: Virtual address %08x not mapped\n", virtualAddy);
        return 1;
    }

	*pte = (*pte & ~PTE_SYSCALL) | perm | PTE_P;
	invlpg((void *)virtualAddy);
	cprintf("%s: Updated permissions for %p to %x\n", __func__, virtualAddy, perm);

	return 0;
}

int memmap(int argc, char **argv, struct Trapframe *tf){
	// Convert tf to ptr
	if (argc < 3){
		cprintf("%s: Usage: showmapping <start_va> <end_va>\n", __func__);
		return 1;
	}
	uintptr_t first = strtol(argv[1], NULL, 0);
	uintptr_t second = strtol(argv[2], NULL, 0);

	for(uintptr_t virtualAddy = ROUNDDOWN(first, PGSIZE); virtualAddy <= second; virtualAddy += PGSIZE){
		pte_t * pte = pgdir_walk(kern_pgdir, (void *) virtualAddy, 0);
        if (!pte || !(*pte & PTE_P)) {
            cprintf("%s: VA: %08x -> Not mapped\n", __func__, virtualAddy);
        } else {
            cprintf("%s: VA: %08x -> PA: %08x | Permissions: %c%c%c\n",
                __func__,
				virtualAddy,
                PTE_ADDR(*pte),
                (*pte & PTE_W) ? 'W' : '-',
                (*pte & PTE_U) ? 'U' : '-',
                (*pte & PTE_P) ? 'P' : '-');
        }
	}
	return 0;
}

int memdump(int argc, char **argv, struct Trapframe *tf) {
    if (argc < 4) {
        cprintf("%s: Usage: memdump <start_va> <end_va> 'PA|VA'\n", __func__);
        return 1;
    }

    uintptr_t first = strtol(argv[1], NULL, 0);
    uintptr_t second = strtol(argv[2], NULL, 0);
    bool isPhysical = (strcmp(argv[3], "PA") == 0);

    if (first >= second) {
        cprintf("%s: Invalid! start_va > end_va\n", __func__);
        return 1;
    }

    for (uintptr_t virtualAddy = first; virtualAddy <= second; virtualAddy++) {
        if (virtualAddy % 16 == 0) {
            cprintf("\n%s: %08x: ", __func__, virtualAddy);
        }

        unsigned char *memContent; // NEded reset

        if (!isPhysical) {
			// virtual addy
            pte_t *pte = pgdir_walk(kern_pgdir, (void *)virtualAddy, 0);
            if (pte && (*pte & PTE_P)) {
                memContent = (unsigned char *)KADDR(*pte);
            } else {
                cprintf("?? ");
                continue;
            }
        } else {
			// phys addy edition
            memContent = (unsigned char *)KADDR(virtualAddy);
        }
        cprintf("%02x ", *memContent);
    }
    cprintf("\n");
    return 0;
}
/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// LAB 1: Your code here.
    // HINT 1: use read_ebp().
    // HINT 2: print the current ebp on the first line (not current_ebp[0])
	cprintf("Stack backtrace:\n");
	uint32_t *frame = (uint32_t *) read_ebp();
	struct Eipdebuginfo info;
	while (frame) {
		uint32_t eip = frame[1];
		cprintf("ebp %x eip %x args ", (uint32_t) frame, eip);
		for(int i = 0; i < 5; i++){
			cprintf("%08x ", frame[2 + i]);
		}
		debuginfo_eip(eip, &info);		
		cprintf("\n\t%s:%d: %.*s+%d\n", 
		info.eip_file, 
		info.eip_line, 
		info.eip_fn_namelen,
		info.eip_fn_name,
		eip - info.eip_fn_addr
		);
		frame = (uint32_t *)frame[0];
	}
	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");
	cprintf("****** Now supporting clear! ******\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("98-469> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
