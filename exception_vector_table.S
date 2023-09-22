// every entry is 128 bytes
#define VECTOR_ENTRY(handler) b handler \
.balign 128

.global init_exception_vector_table
init_exception_vector_table:
  ldr x0, =exception_table
  msr VBAR_EL1, x0
  ret

.align 11 // Table is 2048 bits in total
exception_table:
  // Current EL with SP0
  VECTOR_ENTRY(exception_handler) // Synchronized
  VECTOR_ENTRY(exception_handler) // IRQ/vIRQ
  VECTOR_ENTRY(exception_handler) // FIQ/vFIQ
  VECTOR_ENTRY(exception_handler) // // SError/vSError

  // Currentl EL with SPx
//  VECTOR_ENTRY(exception_handler) // Synchronized
//  VECTOR_ENTRY(exception_handler) // IRQ/vIRQ
//  VECTOR_ENTRY(exception_handler) // FIQ/vFIQ
//  VECTOR_ENTRY(exception_handler) // // SError/vSError

  // Lower EL using AArch64
//  VECTOR_ENTRY(exception_handler) // Synchronized
//  VECTOR_ENTRY(exception_handler) // IRQ/vIRQ
//  VECTOR_ENTRY(exception_handler) // FIQ/vFIQ
//  VECTOR_ENTRY(exception_handler) // // SError/vSError

  // Lower EL using AArch32
//  VECTOR_ENTRY(exception_handler) // Synchronized
//  VECTOR_ENTRY(exception_handler) // IRQ/vIRQ
//  VECTOR_ENTRY(exception_handler) // FIQ/vFIQ
//  VECTOR_ENTRY(exception_handler) // // SError/vSError
