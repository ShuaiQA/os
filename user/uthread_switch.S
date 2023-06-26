	.text
	.globl thread_switch
/*
首先应该是保存寄存器,到某个地址处
然后在从某个地址处加载到相关的寄存器中
*/
thread_switch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd a2, 16(a0)
        sd a3, 24(a0)
        sd a4, 32(a0)
        sd a5, 40(a0)
        sd a6, 48(a0)
        sd a7, 56(a0)
        sd s0, 64(a0)
        sd s1, 72(a0)
        sd s2, 80(a0)
        sd s3, 88(a0)
        sd s4, 96(a0)
        sd s5, 104(a0)
        sd s6, 112(a0)
        sd s7, 120(a0)
        sd s8, 128(a0)
        sd s9, 136(a0)
        sd s10, 144(a0)
        sd s11, 152(a0)
        sd a0, 160(a0)
        sd a1, 168(a0)

        ld sp, 8(a1)
        ld ra, 0(a1)
        ld a2, 16(a1)
        ld a3, 24(a1)
        ld a4, 32(a1)
        ld a5, 40(a1)
        ld a6, 48(a1)
        ld a7, 56(a1)
        ld s0, 64(a1)
        ld s1, 72(a1)
        ld s2, 80(a1)
        ld s3, 88(a1)
        ld s4, 96(a1)
        ld s5, 104(a1)
        ld s6, 112(a1)
        ld s7, 120(a1)
        ld s8, 128(a1)
        ld s9, 136(a1)
        ld s10, 144(a1)
        ld s11, 152(a1)
        ld a0, 160(a1)
        ld a1, 168(a1)

	ret    /* return to ra -> pc */
