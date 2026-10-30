    .syntax unified
    .thumb
    .section .entry,"ax"
    .global _entry
_entry:
    b.w voice_btn       @ +0x00
    b.w voice_other     @ +0x04
    b.w c274_wrap       @ +0x08
    b.w latch_off       @ +0x0C
    b.w row_read        @ +0x10
    b.w boot_b2c4       @ +0x14
    b.w scan_wrap       @ +0x18
    b.w mix_tail        @ +0x1C
    b.w blink_wrap      @ +0x20
    b.w rhythm_setup    @ +0x24
    b.w pwm_hook        @ +0x28
    b.w touch_hook      @ +0x2C
    b.w r27_step_wrap   @ +0x30  2.60: rhythm tick, once per stock step (0x60007E58)
    b.w r27_row         @ +0x34  2.60: rhythm row player call (0x60007E98)
    b.w r27_bass        @ +0x38  2.60: rhythm bass call (0x60007FFE)
    b.w midi_hook       @ +0x3C  2.73: MIDI note-on / note-off builders (0x600096CA / 0x600096F8), then mod 1
    b.w strum_wrap      @ +0x40  2.73: strum-plate notes (bl 0x6000BD8C at 0x6000C090)
    b.w bat_led         @ +0x44  2.78: battery check LED writes (bl 0x6000F518 at 0x6000825C / 0x6000828C / 0x600082F2)
    b.w bat_row         @ +0x48  2.78: battery check voice-row redraw (bl 0x60003A5C at 0x6000829C)
    b.w panel_shown     @ +0x4C  2.79: end of the startup LED animation / setting blink (0x600047AA / 0x60004F70)
    b.w ma_bass_auto    @ +0x50  2.81: auto bass MIDI also in the third Manual/Auto state (bl 0x6000D8D0 at 0x60008EE6 / 8F2A / 8F7E / 8FF4 / 0x60009114)
    b.w ma_hold_on      @ +0x54  2.81: the held Manual chord's MIDI bass note-on (bl 0x600096A0 at 0x60008BAE)
    b.w ma_hold_off     @ +0x58  2.81: its note-off in 0x60008B0C (bl 0x600096D4 at 0x60008C02)
    b.w clk_gate        @ +0x5C  2.82: MIDI clock / Start / Stop only in the chosen presets (default P4 / P5; MIDI task, 0x6000EC70)
    b.w ma_chord_off    @ +0x60  2.84: the stock Manual branch's rhythm-chord note-offs without its Auto test (bl 0x60008E2C at 0x6000CFCC)
    b.w led_prio        @ +0x64  2.85: the LED task created at priority 3 instead of 1 (strd r4,r3,[sp] at 0x6000F390)
    b.w bass_note       @ +0x68  2.86: bass pitch calls in the rhythm tick (bl 0x60008E74 at 0x60007BD6 / 7C1E / 7C46 / 7E0E)
    b.w plate_on        @ +0x6C  2.86: strum plate note-on in the plate scanner (bl 0x600186D8 at 0x6000A2E4 / 0x6000A47E)
    b.w plate_off       @ +0x70  2.86dev2: strum plate release in the plate scanner (bl 0x60018708 at 0x6000A506 / 0x6000A52E)
    b.w chord_dec       @ +0x74  2.86dev8: the chord decoder (bl 0x6000B468 at 0x6000CCFE / 0x6000D750): the m6 combination
    b.w kb_noteon       @ +0x78  issue #2: keyboard sample note-ons (bl 0x600186D8 at 0x6000B760 / B9BA / CBD2 / D284 / D71E)
    b.w kb_mon          @ +0x7C  issue #2: keyboard MIDI note-ons (bl 0x600096A0 at 0x6000B9DE / 0x6000BA04)
    b.w kb_moff         @ +0x80  issue #2: keyboard MIDI note-offs (bl 0x600096D4 at 0x6000BB6A / BB8C / BC6E / BC94)

    .text
    .thumb_func
    .global blink_wrap
@ Called with bl from 0x6000EA9E instead of "bl 0x60004EF0" (stock power-on blink routine: r0-r3 + 2 stack args).
@ Calls it with the same arguments, then blink_after.
blink_wrap:
    push    {r0-r6, lr}                 @ 32 bytes; caller's stack args now at [sp,#32] / [sp,#36]
    ldr     r4, [sp, #32]
    ldr     r5, [sp, #36]
    ldm     sp, {r0-r3}
    sub     sp, #8
    str     r4, [sp]
    str     r5, [sp, #4]
    ldr     r6, =0x60004EF1
    blx     r6
    add     sp, #8
    bl      blink_after
    add     sp, #16
    pop     {r4-r6, pc}
    .ltorg

    .text
    .thumb_func
    .global scan_wrap
@ Called with bl from 0x6000EB50 instead of "bl 0x60009B20" (strumplate scanner, args r0-r3 + one stack arg).
@ Filters r1 (DEA1 byte) through vib_tick, then tail-calls the scanner with sp and lr exactly as the caller set them.
scan_wrap:
    push    {r0-r4, lr}
    mov     r0, r1
    bl      vib_tick
    mov     ip, r0
    pop     {r0-r4, lr}
    mov     r1, ip
    ldr     ip, =0x60009B21
    bx      ip
    .ltorg

    .text
    .thumb_func
    .global row_read
@ Called with bl from 0x6000C2F4, replacing "ldr r4,[r1]; ldr r2,[r2]" in the key-matrix scan.
@ Returns r4 = column bits (| injected press), r2 = column mask. Keeps r0-r3, ip.
row_read:
    ldr     r4, [r1]
    ldr     r2, [r2]
    push    {r0-r3, ip, lr}
    ldr     r0, [sp, #(24 + 0x1c)]     @ scan row index (caller's [sp,#0x1c])
    mov     r1, r4
    bl      row_filter                  @ adds restore presses; hides Up/Down while the depth menu is up
    mov     r4, r0
    pop     {r0-r3, ip, pc}

    .text
    .thumb_func
    .global pwm_hook
@ Reached with b.w from 0x600166D0 (first 4 bytes of QTMR_SetupPwm, "cmp r2,#0x64; bhi 0x60016756", replaced).
@ Same args as the stock function (r0 base, r1 channel, r2 duty, r3 polarity, s0 frequency, one stack arg); sp/lr untouched.
@ For QTMR1 ch 0-3, while our analog vibrato state is live and modulating, marks the channel "programmed by the stock
@ code" (an_t.wr[ch], ANB+44) so the next an_apply takes the new values as its base. Then continues the stock function
@ at 0x600166D4. Every path of the stock function from there either writes both preload registers or never returns.
pwm_hook:
    cmp     r2, #0x64
    bhi     1f
    ldr     ip, =0x401DC000
    cmp     r0, ip
    bne     2f
    cmp     r1, #3
    bhi     2f
    push    {r4, r5}
    ldr     r4, =0x20002B60
    ldr     r5, [r4]
    ldr     ip, =0x414E3232
    cmp     r5, ip
    bne     3f
    ldrb    r5, [r4, #40]
    cbz     r5, 3f
    adds    r4, r4, r1
    movs    r5, #1
    strb    r5, [r4, #44]
3:  pop     {r4, r5}
2:  ldr     ip, =0x600166D5
    bx      ip
1:  movs    r0, #1
    bx      lr
    .ltorg

    .text
    .thumb_func
    .global r27_step_wrap
@ 2.60: called with bl from 0x60007E58 in the rhythm tick (0x60007C8C), replacing "ldr r4, =0x2000DE42; ldrb r3, [r4]"
@ (4 bytes), once per stock step just before the 19 row calls and the bass call. Runs r27_step (27 mode: works out this
@ step's OM-27 frame), then does the two replaced instructions. r0-r3 / ip are preserved too (not live there).
r27_step_wrap:
    push    {r0-r3, ip, lr}
    bl      r27_step
    pop     {r0-r3, ip, lr}
    ldr     r4, =0x2000DE42
    ldrb    r3, [r4]
    bx      lr
    .ltorg

    .text
    .thumb_func
    .global strum_wrap
@ 2.73: called with bl from 0x6000C090 instead of "bl 0x6000BD8C" (the stock strum-plate notes from the chord: r0 root,
@ r1 3rd-type, r2 5th-type pitch class, r3 = the plate array 0x20006AD8, 3 stack args). strum_shift moves r0-r2 for jazz
@ mode, then the stock function is tail-called with sp and lr exactly as the caller set them.
strum_wrap:
    push    {r0-r4, lr}
    mov     r0, sp
    bl      strum_shift
    pop     {r0-r4, lr}
    ldr     ip, =0x6000BD8D
    bx      ip
    .ltorg

    .text
    .thumb_func
    .global clk_gate
@ 2.82: called with bl from 0x6000EC70 in the stock MIDI task (0x6000EC10), replacing "ldrb r3,[r7]; cmp r3,#4" (r7 =
@ 0x2000DE6E, the internal preset, which mod 1 forces to 3). The stock "beq 0x6000EC88" right after skips the Stop / Start /
@ Clock sends when Z is set, as stock preset 4 did. Z is set unless clk_ok (mod2.c: the settings block's clock presets,
@ default P4 / P5) says the user's preset (settings byte 0x20000280) sends them. Only r3 and the flags change (r3 is
@ reloaded on both paths before any use); pop does not change the flags.
clk_gate:
    push    {r0-r3, ip, lr}
    bl      clk_ok
    cmp     r0, #0
    pop     {r0-r3, ip, lr}
    beq     1f
    ldrb    r3, [r7]                    @ the two replaced stock instructions
    cmp     r3, #4
    bx      lr
1:  cmp     r3, r3                      @ Z = 1: no realtime messages
    bx      lr
    .ltorg

@ 2.85: called with bl from main (0x6000F390) instead of "strd r4, r3, [sp]", the stack arguments of xTaskCreate
@ (0x60011564) for the LED task: [sp] = priority (stock r4 = 1), [sp+4] = handle (r3 = 0). The priority is 3 (= the touch
@ task; control is 2, MIDI 4): the stock scheduler has no time slicing, so at priority 1 the LED task got no time at all
@ while the audio interrupt and the control task kept the processor busy (a long organ chord: LEDs frozen for seconds,
@ 2.85dev measurement). The task sleeps on its 1 ms semaphore and on the I2C transfer, so it takes very little time.
@ r0-r4 and the flags are unchanged (main reuses r4 = 1 right after the call); ip is free here (AAPCS scratch).
    .thumb_func
    .global led_prio
led_prio:
    mov.w   ip, #3
    str.w   ip, [sp]
    str     r3, [sp, #4]
    bx      lr

@ issue #2: the keyboard notes (r1) through kb_note (+ the latched Transpose, CF_KTR), then the stock routine with r0, r2, r3,
@ the stack argument and lr exactly as the caller set them.
    .macro KBNOTE name, target
    .text
    .thumb_func
    .global \name
\name:
    push    {r0, r2, r3, lr}
    mov     r0, r1
    bl      kb_note
    mov     r1, r0
    pop     {r0, r2, r3, lr}
    ldr     ip, =\target
    bx      ip
    .ltorg
    .endm
    KBNOTE kb_noteon, 0x600186D9
    KBNOTE kb_mon, 0x600096A1
    KBNOTE kb_moff, 0x600096D5
