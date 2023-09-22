.global init_exception_vector_table
init_exception_vector_table:
  ldr x0, =exception_table
  msr VBAR_EL1, x0
  ret

.align 11 // Table is 2048 bits in total
exception_table:
  // Current EL with SP0
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128

  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128

  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128

  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
  b exception_handler
  .balign 128
