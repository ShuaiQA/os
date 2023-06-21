        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid + 1 * 4096) // stack from top to bootom
        la sp, stack0         # sp = &stack0
        li a0, 1024*4         # a0 = 4096
        csrr a1, mhartid      # a1 = 0    // 以mhartid = 0为例
        addi a1, a1, 1        # a1 = a1 + 1 = 1
        mul a0, a0, a1        # a0 = a0 * a1 = 1 * 4096 = 4096 
        add sp, sp, a0        # sp = &stack0 + 4096
        # jump to start() in start.c
        call start
spin:
        j spin
