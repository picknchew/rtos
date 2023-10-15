.macro set_reg_to_task_context, reg
  // set sp to current task descriptor
  ldr \reg, =current_task // current_task is a pointer to a pointer of the current task
  ldr \reg, [\reg] // dereference
  add \reg, \reg, #8 // offset to context within task descriptor
.endm

// x0 is pointer to trapframe struct
// store values currently in the registers in the
// trapframe.
.macro kern_enter
  // temporarily store x3
  str x3, [sp, #-16]!

  set_reg_to_task_context x3

  // save x0, x1 to context
  stp x0, x1, [x3], #16

  // restore x3 and use x0 to keep track of offset
  // within context instead
  mov x0, x3
  ldr x3, [sp], #16

  // save rest of pointers (x2-x30)
  stp	x2, x3, [x0], #16
  stp	x4, x5, [x0], #16
  stp	x6, x7, [x0], #16
  stp	x8, x9, [x0], #16
  stp	x10, x11, [x0], #16
  stp	x12, x13, [x0], #16
  stp	x14, x15, [x0], #16
  stp	x16, x17, [x0], #16
  stp	x18, x19, [x0], #16
  stp	x20, x21, [x0], #16
  stp	x22, x23, [x0], #16
  stp	x24, x25, [x0], #16
  stp	x26, x27, [x0], #16
  stp x28, x29, [x0], #16
  str	x30, [x0], #8

  // store sp register
  mrs x1, SP_EL0
  str x1, [x0], #8

  // store lr register
  mrs x1, ELR_EL1
  str x1, [x0], #8

  // store pstate
  mrs x1, SPSR_EL1
  str x1, [x0]

  // reset kernel stack
  ldr x1, =stackend
  mov sp, x1
  mov x1, x0
.endm

// restore registers and return to user mode (EL0)
.global kern_exit
kern_exit:
  set_reg_to_task_context x0

  // start from the end of the struct (restore backwards)
  add x0, x0, #272 // total size of context

  // restore pstate
  ldr x1, [x0, #-8]!
  msr SPSR_EL1, x1

  // restore lr register
  ldr x1, [x0, #-8]!
  msr ELR_EL1, x1

  // restore sp register
  ldr x1, [x0, #-8]!
  msr SP_EL0, x1

  // restore x0-x30 from context
  ldr	x30, [x0, #-8]!
  ldp x28, x29, [x0, #-16]!
  ldp	x26, x27, [x0, #-16]!
  ldp	x24, x25, [x0, #-16]!
  ldp	x22, x23, [x0, #-16]!
  ldp	x20, x21, [x0, #-16]!
  ldp	x18, x19, [x0, #-16]!
  ldp	x16, x17, [x0, #-16]!
  ldp	x14, x15, [x0, #-16]!
  ldp	x12, x13, [x0, #-16]!
  ldp	x10, x11, [x0, #-16]!
  ldp	x8, x9, [x0, #-16]!
  ldp	x6, x7, [x0, #-16]!
  ldp	x4, x5, [x0, #-16]!
  ldp	x2, x3, [x0, #-16]!
  ldp x0, x1, [x0, #-16]

  eret

.global init_exception_vector_table
init_exception_vector_table:
  ldr x0, =exception_table
  msr VBAR_EL1, x0
  ret

.macro exception_handler
  kern_enter
  mrs x0, ESR_EL1 // ESR_EL1 as argument to handler
  b handle_exception // handle_exception calls kern_exit
  .balign 128
.endm

.macro irq_handler
  kern_enter
  b handle_irq // handle_irq calls kern_exit
  .balign 128
.endm

.align 11 // Table is 2048 bytes in total
exception_table:
  // Current EL with SP0
  exception_handler
  exception_handler
  exception_handler
  exception_handler

  exception_handler
  exception_handler
  exception_handler
  exception_handler

  exception_handler
  irq_handler
  exception_handler
  exception_handler

  exception_handler
  exception_handler
  exception_handler
  exception_handler
