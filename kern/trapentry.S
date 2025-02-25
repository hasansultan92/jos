/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

// HINT 1 : TRAPHANDLER_NOEC(t_divide, T_DIVIDE);
//          Do something like this if there is no error code for the trap
// HINT 2 : TRAPHANDLER(t_dblflt, T_DBLFLT);
//          Do something like this if the trap includes an error code..
// HINT 3 : READ Intel's manual to check if the trap includes an error code
//          or not...


// Define trap handlers w/out error codes
TRAPHANDLER_NOEC(t_divide, T_DIVIDE);		// divide error
TRAPHANDLER_NOEC(t_debug, T_DEBUG);			// debug exception
TRAPHANDLER_NOEC(t_nmi, T_NMI);				// NMI interupt
TRAPHANDLER_NOEC(t_bkrpt, T_BRKPT);			// breakpoint
TRAPHANDLER_NOEC(t_oflow, T_OFLOW);			// overflow
TRAPHANDLER_NOEC(t_bound, T_BOUND);			// bound range exceeded
TRAPHANDLER_NOEC(t_illop, T_ILLOP);			// invalid opcode
TRAPHANDLER_NOEC(t_device, T_DEVICE);		// device not available
// TRAPHANDLER_NOEC(t_coproc, T_COPROC);		// coprocessor segment overrun
TRAPHANDLER_NOEC(t_simderr, T_SIMDERR);		// (Intel reserved. Do not use.) 

// Define trap handlers for exceptions 
TRAPHANDLER(t_dblflt, T_DBLFLT);			// double fault
TRAPHANDLER(t_tss, T_TSS);					// invalid TSS
TRAPHANDLER(t_segnp, T_SEGNP);				// segment not present
TRAPHANDLER(t_stack, T_STACK);				// stack-segment fault
TRAPHANDLER(t_gpflt, T_GPFLT);				// general protection
TRAPHANDLER(t_pgflt, T_PGFLT);				// page fault
// TRAPHANDLER(t_alignt, T_ALIGNT);			// alignment check

// Hardware IRQ numbers
TRAPHANDLER_NOEC(t_irq0, IRQ_OFFSET + 0)	// timer
TRAPHANDLER_NOEC(t_irq1, IRQ_OFFSET + 1)	// keyboard
TRAPHANDLER_NOEC(t_irq2, IRQ_OFFSET + 2)	// cascade 
TRAPHANDLER_NOEC(t_irq3, IRQ_OFFSET + 3)	// COM2
TRAPHANDLER_NOEC(t_irq4, IRQ_OFFSET + 4)	// COM1
TRAPHANDLER_NOEC(t_irq5, IRQ_OFFSET + 5)	// LPT2
TRAPHANDLER_NOEC(t_irq6, IRQ_OFFSET + 6)	// Floppy disk
TRAPHANDLER_NOEC(t_irq7, IRQ_OFFSET + 7)	// LPT1

/*
 * Lab 3: Your code here for _alltraps
 */


.globl _alltraps
_alltraps:
    # Save registers
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    # Load kernel data segment (in case user mode was running)
    movl $GD_KD, %eax
    movw %ax, %ds
    movw %ax, %es

    # Pass pointer to trapframe to C handler
    pushl %esp
    call trap

    # Restore segment registers
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    addl $8, %esp   # Remove error code & trap number

    iret            # Return from interrupt
