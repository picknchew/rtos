// x0 is pointer to trapframe struct
// store values currently in the registers in the
// trapframe.
trapframe_save:
  // need to change this to push to stack
  // since x0 will get replaced.
  stp	x0, x1, [x8], #16
  stp	x2, x3, [x8], #16
  stp	x4, x5, [x8], #16
  stp	x6, x7, [x8], #16
  stp	x8, x9, [x8], #16
  stp	x10, x11, [x8], #16
  stp	x12, x13, [x8], #16
  stp	x14, x15, [x8], #16
  stp	x16, x17, [x8], #16
  stp	x18, x19, [x8], #16
  stp	x20, x21, [x8], #16
  stp	x22, x23, [x8], #16
  stp	x24, x25, [x8], #16
  stp	x26, x27, [x8], #16
  stp   x28, x29, [x8], #16
  stp	x30, x31, [x8]
  ret
