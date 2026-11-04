@ Mod 1 (MIDI routing, 2026-10: first given a source; before that it was hand-assembled bytes, kept in every image since).
@ Linked at 0x60039000 (fw/build.sh: mod1.bin covers 0x60039000-0x600390E1; mkimg.py writes it there). Its tables are data:
@ the route table 0x60039100 (route[preset][internal channel] = MIDI channel - 1, 0xFF = not sent) and the drum note map
@ 0x60039180 (128 bytes), written by the Custom Firmware Tool's settings (om108_patcher.py ROUTE / DRUMS); the tables are
@ not in this file. Stock addresses come from fw/stock_vNN.h (lib/stock.py). Stock sites (mkimg.py):
@ - the MIDI builders' posts (0x600096CA / 0x600096F8 via mod2's midi_hook, 0x60009728 the CC builder) call mod1_hook;
@ - 0x6000B25E (the MIDI preset set-up 0x6000B204) b.w mod1_preset; 0x6000B22C / 0x6000B270 / 0x6000F2D4 preset bytes.
    .syntax unified
    .thumb
    .text
@ the stock targets (Thumb), defined in mod1_syms.s: a branch to an absolute address must be a link-time symbol

@ r0 = queue, r1 = the message {u8 type, u8 ch, u16 len, u8 *data}: types 1-7 except 4 (CC) go out on the channel the route
@ table gives the internal channel in the user's preset (settings byte 0x20000280, 1-5), or not at all (0xFF); drum notes
@ (internal channel 9, note on / off, 2 bytes) through the drum note map. Then the stock post 0x6000DB28 (its result).
    .thumb_func
    .global mod1_hook
mod1_hook:
    push    {r4, r5, r6, r7, lr}
    sub     sp, #12
    ldrb    r2, [r1]
    subs    r3, r2, #1
    cmp     r3, #6
    bhi     3f
    cmp     r2, #4
    beq     4f
    movw    r4, #:lower16:RA(20000280)
    movt    r4, #:upper16:RA(20000280)
    ldrb    r4, [r4]
    subs    r4, #1
    cmp     r4, #4
    it      hi
    movhi   r4, #0
    ldrb    r5, [r1, #1]
    and     r5, r5, #15
    movw    r6, #0x9100
    movt    r6, #0x6003
    add.w   r6, r6, r4, lsl #4
    ldrb    r6, [r6, r5]
    cmp     r6, #0xFF
    beq     4f
    strb.w  r2, [sp]
    strb.w  r6, [sp, #1]
    ldrh    r7, [r1, #2]
    strh.w  r7, [sp, #2]
    ldr     r3, [r1, #4]
    str     r3, [sp, #4]
    cmp     r5, #9
    bne     2f
    cmp     r2, #2
    bhi     2f
    cmp     r7, #2
    bne     2f
    ldrb    r7, [r3]
    and     r7, r7, #0x7F
    movw    r4, #0x9180
    movt    r4, #0x6003
    ldrb    r7, [r4, r7]
    strb.w  r7, [sp, #8]
    ldrb    r7, [r3, #1]
    strb.w  r7, [sp, #9]
    add     r3, sp, #8
    str     r3, [sp, #4]
2:  mov     r1, sp
3:  bl      stock_post
    b       5f
4:  movs    r0, #1
5:  add     sp, #12
    pop     {r4, r5, r6, r7, pc}

    .org    0xC0, 0xFF
@ Reached with b.w from the power-on MIDI preset keys (0x6000B204, at 0x6000B25E, replacing "ldr r2, =0x2000DE6F; movs r0,
@ #0x1E"; r3 = the key states 0x20006B90): key 0x34 (voice slot 5) not held = the stock path as it was; held = preset 5
@ (r2 = the settings buffer 0x20000280, r6 = 5, then the stock preset store at 0x6000B242).
    .thumb_func
    .global mod1_preset
mod1_preset:
    ldrb.w  r2, [r3, #0x170]
    cbnz    r2, 1f
    movw    r2, #:lower16:RA(2000DE6F)
    movt    r2, #:upper16:RA(2000DE6F)
    movs    r0, #0x1E
    b.w     stock_b262
1:  movw    r2, #:lower16:RA(20000280)
    movt    r2, #:upper16:RA(20000280)
    movs    r6, #5
    b.w     stock_b242
    .balign 4, 0xFF
