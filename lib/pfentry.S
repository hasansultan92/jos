#include <inc/mmu.h>
#include <inc/memlayout.h>

// Page fault upcall entrypoint.

// This is where we ask the kernel to redirect us to whenever we cause
// a page fault in user space (see the call to sys_set_pgfault_handler
// in pgfault.c).
//
// When a page fault actually occurs, the kernel switches our ESP to
// point to the user exception stack if we're not already on the user
// exception stack, and then it pushes a UTrapframe onto our user
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_eax
//	...
//	utf_regs.reg_esi
//	utf_regs.reg_edi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
//
// If this is a recursive fault, the kernel will reserve for us a
// blank word above the trap-time esp for scratch work when we unwind
// the recursive call.
//
// We then have call up to the appropriate page fault handler in C
// code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
	// Now the C page fault handler has returned and you must return
	// to the trap time state.
	// Push trap-time %eip onto the trap-time stack.
	//
	// Explanation:
	//   We must prepare the trap-time stack for our eventual return to
	//   re-execute the instruction that faulted.
	//   Unfortunately, we can't return directly from the exception stack:
	//   We can't call 'jmp', since that requires that we load the address
	//   into a register, and all registers must have their trap-time
	//   values after the return.
	//   We can't call 'ret' from the exception stack either, since if we
	//   did, %esp would have the wrong value.
	//   So instead, we push the trap-time %eip onto the *trap-time* stack!
	//   Below we'll switch to that stack and call 'ret', which will
	//   restore %eip to its pre-fault value.
	//
	//   In the case of a recursive fault on the exception stack,
	//   note that the word we're pushing now will fit in the
	//   blank word that the kernel reserved for us.
	//
	// Throughout the remaining code, think carefully about what
	// registers are available for intermediate calculations.  You
	// may find that you have to rearrange your code in non-obvious
	// ways as registers become unavailable as scratch space.
	//
	// LAB 4: Your code here.

	// LLMPROMPT: What does popal do? Which registers (if any) should we avoid using due to this operation?

	// Carefully choose which registers to use for temporary storage.
	// %ebp and %edi (and other general-purpose registers) should NOT be used for temporary 
	// storage before the popal instruction because:
	// 1. popal restores ALL general-purpose registers (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
	//    from the UTrapframe
	// 2. Any values we store in these registers will be overwritten when popal executes
	// 3. Instead, we use %eax and %ebx for any operations we need to perform before popal,
	//    knowing they'll be properly restored later

	// ** OLD **
	// mov 0x40(%esp), %ebp // Get trap-time eip from UTF
	// mov 0x48(%esp), %edi	// Get trap-time esp from UTF

							// Push the trap-time %eip onto the *trap-time* stack!
	// sub $4, %ebp 		// reserve space 
	// mov %eax, (%edi)		// store trap-time eip -> trap-time stack

	// ** NEW **
	mov 48(%esp), %eax		// Get trap-time esp from UTF
	
							// Get trap-time eip from UTF
	sub $4, %eax			// reserve space 
	mov 40(%esp), %ebx		// trap-time eip
	mov %ebx, (%eax)		// store trap-time eip -> trap-time stack

	mov %eax, 48(%esp)		// update saved trap-time esp in UTF



	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.

	add $8, %esp 			// mov esp to 8
	popal 					// pop all registers

	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.

	add $4, %esp			// Skip utf_fault_va & utf_err
	popf					// Restore eflags

	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.

	// ** OLD **
	// $4, %esp			// move stack pointer by 4
	// mov (%esp), %esp
	// lea -4(%esp), %esp

	// ** NEW **
	pop %esp 			// restore trap-time esp

	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.

	ret 					// return eip value
