// https://stackoverflow.com/posts/55452533
// get the address of address of current_task descriptor
.text
.global current_task
.current_task_addr_addr:
    .word current_task

.macro set_sp_to_task_context
  // set sp to current task descriptor
  ldr sp, #current_task_addr_addr
  ldr sp, [sp] // dereference
  add sp, #8 // offset to context within task descriptor
.endm

// x0 is pointer to trapframe struct
// store values currently in the registers in the
// trapframe.
.macro kern_enter
  set_sp_to_task_context

  // save x0-x30 to the struct
  stp x0, x1, [sp], #16
  stp	x2, x3, [sp], #16
  stp	x4, x5, [sp], #16
  stp	x6, x7, [sp], #16
  stp	x8, x9, [sp], #16
  stp	x10, x11, [sp], #16
  stp	x12, x13, [sp], #16
  stp	x14, x15, [sp], #16
  stp	x16, x17, [sp], #16
  stp	x18, x19, [sp], #16
  stp	x20, x21, [sp], #16
  stp	x22, x23, [sp], #16
  stp	x24, x25, [sp], #16
  stp	x26, x27, [sp], #16
  stp x28, x29, [sp], #16
  str	x30, [sp], #8

  // store sp register
  mrs x0, SP_EL0
  str x0, [sp], #8

  // store lr register
  mrs x0, ELR_EL1
  str x0, [sp], #8

  // store pstate
  mrs x0, SPSR_EL1
  str x0, [sp]

  // reset kernel stack
  ldr x0, =stackend
  mov sp, x0
.endm

// task struct is passed from x0
.macro kern_exit
  set_sp_to_task_context

  // restore x0-x30 from context
  ldp x0, x1, [sp], #16
  ldp	x2, x3, [sp], #16
  ldp	x4, x5, [sp], #16
  ldp	x6, x7, [sp], #16
  ldp	x8, x9, [sp], #16
  ldp	x10, x11, [sp], #16
  ldp	x12, x13, [sp], #16
  ldp	x14, x15, [sp], #16
  ldp	x16, x17, [sp], #16
  ldp	x18, x19, [sp], #16
  ldp	x20, x21, [sp], #16
  ldp	x22, x23, [sp], #16
  ldp	x24, x25, [sp], #16
  ldp	x26, x27, [sp], #16
  ldp x28, x29, [sp], #16
  ldr	x30, [sp], #8

  // restore sp register
  ldr x0, [sp], #8
  msr x0, SP_EL0

  // restore lr register
  mrs x0, ELR_EL1
  str x0, [sp], #8

  // restore pstate
  mrs x0, SPSR_EL1
  str x0, [sp]

  // restore x0, since overwrote it
  ldr x0, [sp]
  
  // return to EL0, run task
  eret
.endm

.global init_exception_vector_table
init_exception_vector_table:
  ldr x0, =exception_table
  msr VBAR_EL1, x0
  ret

.align 11 // Table is 2048 bits in total
exception_table:
  // Current EL with SP0
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128

  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128

  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128

  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
  b handle_exception
  .balign 128
