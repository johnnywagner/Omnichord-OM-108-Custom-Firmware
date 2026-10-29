/* OM-108 mod 2 firmware code (2.85). Built for 0x6003A000: the entry table in entry.s is at a fixed address and the
   stock patch sites (reference/changes_vs_stock_285.md) jump into it. What it does:
   - alternate sub per voice: re-press the selected voice button; sticky per voice and saved;
   - panel memory: saved at power-off (latch_off); put back at power-on before the panel is shown (restore_early, through
     the stock button handler), so the panel comes up with the saved settings; a setting whose button is still held from
     power-on follows when it is let go (restore_tick), and a press of the player's own always wins;
   - power-on keys, applied after the stock factory reset like the stock's own: swing/waltz = 84 mode (Classic rhythm tone),
     country/bossanova = 108 mode, slow rock/funk = 27 mode (each spoken; slow rock/funk's LED blinks in the stock setting
     blink when 27 mode is chosen); yellow VOICE = normal bass, yellow PATTERN = the alternate (pluck) bass (remembered,
     their LEDs blink); both yellows = the stock factory reset, which also erases our records; KEYBOARD = jazz chords for
     this power-on (not remembered, silent, no blink);
   - modulation modes on the Instant OFF ("note off") plate: Note Off (stock: nothing of ours runs) / vibrato auto /
     vibrato manual (finger rock) / tremolo auto / tremolo manual (the floor); chosen in keyboard mode with Gb 7th (tap =
     next + spoken name; hold + Up / Down = step; hold + voice button 1-5 = that mode), shown on the voice LEDs; per-mode
     depth (hold Db minor) and speed (hold Gb minor) menus; keyboard effect (a note held, then Db / Gb minor); analog
     vibrato on the Omni 1/2 oscillators; MIDI pitch bend; 16x Instant OFF chip gain in the effect modes only;
   - 27 mode: the six OM-27 rhythms on the six PATTERN buttons (LATIN = yellow), combined as the OM-27's rhythm chip,
     KEYBOARD locks a held combination; Classic drum sounds; OM-27 chord and bass rhythms;
   - the alternate bass (pulse-width pluck, any rhythm mode) and jazz chords (oscillators, strum plates and MIDI);
   - spoken prompts (mixer) and tremolo gain on the strum-voice bus;
   - the stock battery check sends a battery / power LED update only when it changes (the LED task sends one LED write per
     millisecond, so its constant re-sends delayed every other LED);
   - the third Manual/Auto state (hold Manual/Auto + KEYBOARD: the held Manual chord + the auto bass line, on MIDI too;
     2.83: Manual/Auto is remembered when switched in keyboard mode too);
   - MIDI clock / Start / Stop only in presets P4 / P5 (clk_gate in entry.s);
   - 2.85: the stock LED task runs at priority 3 (led_prio in entry.s; stock 1: with a long chord ringing it got no time
     at all and the LEDs froze for seconds), and row_filter skips its note-key loop when it has nothing to do;
   - 2026-10-17 (the Custom Firmware Tool): the settings block at 0x60047000 (CFG below) switches the modulation modes, the
     Extras and the spoken mode names, sets the clock presets and each voice's sub / alt; Evolve's boost follows Evolve.
     Without a block (or with the defaults) everything is exactly as 2.85.
   History: reference/version_history.md. */
typedef unsigned char u8; typedef signed char s8; typedef unsigned int u32;
#define R8(a)  (*(volatile u8  *)(a))
#define RS8(a) (*(volatile s8  *)(a))
#define R32(a) (*(volatile u32 *)(a))
/* firmware entry points (Thumb) */
#define F_E7DC   ((void(*)(u32))0x6000E7DD)        /* select voice slot */
#define F_C274   ((void(*)(void))0x6000C275)       /* panel/key handler */
#define F_LEDSET ((void(*)(int,int))0x60003CE5)    /* LED flag + output */
#define F_TUNE   ((void(*)(int))0x600186E1)        /* engine master tune (thunk to 0x202000C0) */
#define F_TUNE_A ((void(*)(volatile s8*))0x600085B1) /* tune tables, as stock P+arrow handler */
#define F_TUNE_B ((void(*)(volatile s8*))0x6000873D)
#define KEYBITS  0x600332C0u                         /* key-matrix column bit table (+col*16+8) */
#define F_ERASE  ((int(*)(u32,u32))0x60018721)     /* ITCM sector erase */
#define F_PROG   ((int(*)(u32,u32,void*))0x600186E9)/* ITCM page program */
#define F_B2C4   ((u32(*)(void))0x6000B2C5)          /* stock power-on factory-reset check (yellow VOICE + yellow PATTERN) */
#define DE6B     0x2000DE6Bu                         /* set by it when a factory reset was requested */
#define DE87     0x2000DE87u                         /* stock: keys 0 + 11 held at power-on (no LED animation) */
#define FLEXSPI  0x402A8000u
#define F_DCINV  ((void(*)(u32,int))0x60012C9D)       /* stock D-cache invalidate by address (used by the stock settings writer) */
/* RAM */
#define LED(i)   R8(0x200000F0u+(i))
#define F_3A5C   ((void(*)(u32))0x60003A5D)          /* stock: redraw the voice LED row (arg = power LED) */
#define F_F518   ((void(*)(u32, u32))0x6000F519)     /* stock: LED port write (port, ~bits) */
#define LEDSH    0x20002A54u                          /* stock shadow of LED ports 4 / 5 / 6 */
#define KEY(k)   (0x20006B90u+7u*(k))
#define VT       0x200003ACu              /* voice->layer table */
#define DE88 0x2000DE88u  /* voice bank */
#define DE89 0x2000DE89u  /* voice = bank*5+slot */
#define DE84 0x2000DE84u  /* pattern bank */
#define DE82 0x2000DE82u  /* pattern = bank*5+idx */
#define DE72 0x2000DE72u  /* master tune -6..6 */
#define DE79 0x2000DE79u  /* transpose -6..6 */
#define OCT  0x200009DCu  /* octave 0..2 */
#define OCT2 0x200009DDu
#define KEYS_EN 0x2000DEA2u
#define DE80 0x2000DE80u  /* keyboard mode */
#define PAGEBUF 0x20002AD4u               /* 256 B, used by stock code only at power-on setup */
#define ST      0x20002C54u               /* restore state (inside 2nd stock buffer, upper half) */
#define STATE_SECTOR_OFF 0x00FFE000u
#define STATE_SECTOR     0x60FFE000u
/* ---------------- settings block (the Custom Firmware Tool's choices) ----------------
   64 bytes at CFG (its own 4 KB sector, the last of our code area), written only by the Custom Firmware Tool, read-only
   here: [0..3] CFG_MAGIC, [4] version 1, [5] flags (CF_*), [6] clock / Start / Stop presets (bit 0 = P1 .. bit 4 = P5),
   [8..17] sub per voice, [18..27] alt sub per voice (voice 0 = Omni 1 unused), [28..37] MIDI parts (the tool's own record:
   mod 1's routing table and drum map hold what is used), [60..63] ~CFG_MAGIC. Absent or damaged (blank, a cut write): the
   Suggested Default (every feature on, 2.85's subs and clock presets). The tool writes it last of all. */
#define CFG        0x60047000u
#define CFG_MAGIC  0x31434D4Fu     /* "OMC1" */
#define CF_MOD     0x01u           /* modulation modes (else: always Note Off, Gb 7th / Db / Gb minor stock) */
#define CF_EXTRAS  0x02u           /* 27 mode, the alternate bass, jazz, the third Manual/Auto state */
#define CF_SPOKEN  0x04u           /* spoken rhythm / modulation mode names */
#define CF_DEFAULT 0x3Fu
#define CLK_DEFAULT 0x18u          /* P4 / P5 (2.82) */
static int cfg_ok(void) { return R32(CFG) == CFG_MAGIC && R32(CFG + 60u) == ~CFG_MAGIC && R8(CFG + 4u) == 1u; }
u32 cf(u32 bit) { return (cfg_ok() ? R8(CFG + 5u) : CF_DEFAULT) & bit; }   /* (global: the tests call it) */
u32 clk_ok(void) {                /* clk_gate: send MIDI clock / Start / Stop in the user's preset (an invalid one = P1, as mod 1) */
    u32 p = R8(0x20000280u); if (p < 1u || p > 5u) p = 1u;
    return ((cfg_ok() ? R8(CFG + 6u) : CLK_DEFAULT) >> (p - 1u)) & 1u;
}
/* ---------------- alt sub per voice ----------------
   Voice v (0..9 = bank*5+slot; v+12 is its keyboard-mode copy) has its sub instrument at VT+v*0x40+off.
   Pressing the selected voice again swaps normal <-> alt. The choice lives in the RAM voice table, so it
   stays with each voice when other voices are selected (sticky), and is saved at power-off.
   Omni 1 (voice 0) has an analog sub and is left alone. */
#define NV 10
/* Our settings live in spare bytes of voice-table slot 11 (unreachable from the panel, so never read by the engine; the bytes
   used are zero in flash, but for JAZZ's stock 2), because the stock power-on settings save overwrites the stock page
   buffers after boot_b2c4 / rhythm_setup run. Used: 0x1F VMODE, 0x20 EARLY, 0x2D BLPH, 0x2E BOOTF, 0x2F SPD, 0x34-0x37 PDEP,
   0x38-0x3B PSPD, 0x3C R27, 0x3D BSTY, 0x3E JAZZ, 0x3F MODEV (0x6000E850 writes +0x0B / +0x1B / +0x2B of every slot).
   MODEV: bit 0 = an effect mode is on (0 = Note Off mode), bits 2-5 = the current mode's depth step 0..12; bits 1 / 6 unused
   (kept 0). Saved as bits 1-6 of panel record byte 13.
   BOOTF: power-on events (the prompt to say, the LEDs to blink in the stock setting blink).
   VMODE: the effect mode 1 vibrato auto, 2 vibrato manual, 3 tremolo auto, 4 tremolo manual (anything else loads as 1).
   SPD: the current mode's speed, s8 -6..+6. VMODE / SPD / per-mode values are saved in the settings record (ver 3). */
#define MODEV (VT + 11u*0x40u + 0x3Fu)
#define BOOTF (VT + 11u*0x40u + 0x2Eu)
#define BF_P108  0x01u    /* say "108 mode" once the panel is up */
#define BF_P84   0x02u    /* say "84 mode" */
#define BF_BL11  0x04u    /* blink slow rock/funk LED during the stock power-on blink */
#define BF_P27   0x08u    /* say "27 mode" */
#define BF_YV    0x10u    /* blink yellow VOICE's LEDs in the stock power-on blink (normal bass chosen) */
#define BF_YP    0x20u    /* blink yellow PATTERN's LEDs (alternate bass chosen) */
/* 27 mode on (1) / off (0). Loaded at power-on from settings record byte 13 (0x27 = on), set / cleared by the rhythm-mode
   keys at power-on (rhythm_setup). */
#define R27   (VT + 11u*0x40u + 0x3Cu)
#define IS27() (R8(R27) == 1u && cf(CF_EXTRAS))   /* Extras off: never (the saved choice is kept, see build_set) */
#define REC27 0x27u
/* Bass style, with any rhythm mode: 0 = normal (stock / OM-84 trigger), 2 = the alternate (pluck) bass; saved in settings
   record byte 13 bits 6-7. Chosen at power-on, independent of the mode keys: yellow VOICE = normal, yellow PATTERN =
   alternate (both = the stock factory reset = normal). A saved 1 (the 2.64-2.72 soft pluck) reads as the alternate bass. */
#define BSTY  (VT + 11u*0x40u + 0x3Du)
static u32 bsty_saved(void) { u32 v = R8(BSTY); return (v == 1u || v == 2u) ? 2u : 0u; }
static u32 bsty(void) { return cf(CF_EXTRAS) ? bsty_saved() : 0u; }   /* Extras off: the stock bass */
/* Jazz chords (the user: "a power on only feature that isn't remembered"): KEYBOARD held at power-on. Slot 11's layer-3 bus
   byte (stock 2, never read for slot 11): JAZZ_ON or the stock 2; set by rhythm_setup at every power-on, never saved. */
#define JAZZ     (VT + 11u*0x40u + 0x3Eu)
#define JAZZ_ON  0x4Au
#define JAZZ_OFF 0x02u
static u32 jazz(void) { return R8(JAZZ) == JAZZ_ON; }
/* 2.78: power-on panel restore result (restore_early -> restore_tick): 0x80 | the steps still to do (bits 0-5, step_key),
   0 = restore_early has not run. Spare byte of slot 11's layer 2 (zero in flash, never read by the engine, not written by
   0x6000E850, kept through the stock power-on settings save, which overwrites the page buffers). */
#define EARLY (VT + 11u*0x40u + 0x20u)
/* Runtime state of 27 mode, the alternate bass, jazz, the analog vibrato and the Gb 7th + voice button mode pick (control
   task + rhythm interrupt), in the free bytes 0x20002C84-0x20002CD3 of the stock page buffer (used by stock code only at
   power-on / power-off; magic-checked, zeroed when the magic is wrong) */
#define R7B     0x20002C84u
#define R7MAGIC 0x52323731u
typedef struct {
    u32 magic;
    u8 cur, play, prev, gest;      /* 27 mode: resting selection, what plays now, selection before this press, gesture flags (G_*) */
    u8 held, kbhide, init, first;  /* debounced held buttons (bit 0-5 = LATIN..SWING, bit 6 = KEYBOARD), KEYBOARD hidden, ready, gesture's first button */
    u8 on[7], off[7];              /* debounce counters (matrix scans) */
    u8 c, hits, chord, bcode;      /* rhythm interrupt: OM-27 step counter 0-31, this step's drums / chord stab, last bass code */
    u8 bleft, bkind, rv, bv;       /* bass gate stock steps left, this stock step's bass value kind, row / bass value bytes the stock player reads */
    u8 cg, rst, kbl; s8 sh2;       /* chord gate wanted this stock step; restart requested (a fresh button press); lock blink
                                      running; jazz: the chord root oscillator's shift in semitones (chord_tick) */
    unsigned short kb0;            /* lock blink start (TICKS, low 16 bits) */
    unsigned short py1, py2, ptgt; /* alternate bass pluck (any mode): duty smoothing stages and target, Q16 */
    unsigned short pms;            /* TICKS of the last pluck_tick (low 16 bits) */
    u8 pnew, pinit;                /* a bass note started (rhythm interrupt -> pluck_tick); pluck state ready */
    u8 ign, kinit;                 /* 27 mode: buttons still held from power-on (ignored until let go); ign taken */
    s8 sh0, sh1; u8 vsel, vdn;     /* jazz: the 3rd / 5th chord oscillators' shifts in semitones (chord_tick); mode picked
                                      with a voice button while Gb 7th is held (1-5, 0 none); voice buttons down (hidden from the stock) */
    u8 voff[5], von;               /* voice buttons' release counters; the analog vibrato is on (vib_tick) */
    int vc;                        /* the analog vibrato's offset (1/256 cent), kept for an_write */
    u8 mpre, mfrom, mflag, moff;   /* 2.79 Manual/Auto: its state just before the stock took this press of the button (MA_*);
                                      the state before the third one was turned on; MF_* flags; KEYBOARD release counter */
} r27_t;
_Static_assert(sizeof(r27_t) <= 0x20002CD4u - R7B, "r27_t too big");
_Static_assert(__builtin_offsetof(r27_t, c) == 26 && __builtin_offsetof(r27_t, rst) == 35 && __builtin_offsetof(r27_t, vsel) == 54
               && __builtin_offsetof(r27_t, von) == 61 && __builtin_offsetof(r27_t, vc) == 64, "test offsets");
static r27_t *r27(void);
/* 2.79: Manual/Auto has a third state (the user's find, hold Manual/Auto + KEYBOARD): the stock Manual state (held chord,
   0x200009DA = 0) with the Auto LED flag on as well (0x12: the stock rhythm player reads it - auto bass, chord row - and
   doesn't cut the bass gate), both LEDs lit. Saved as panel record [7] = 2 when it was turned on from Auto, 3 from Manual
   (1 = Manual, 0 = Auto; older versions read 2 / 3 as Auto). The stock Manual/Auto button leaves it to Auto (it tests the
   Manual flag only), as from Manual. */
#define MA_AUTO   0u
#define MA_MANUAL 1u
#define MA_THIRD  2u
static u32 ma_state(void) { return LED(0x11) ? (LED(0x12) ? MA_THIRD : MA_MANUAL) : MA_AUTO; }
static u32 ma_rec(void);
static int r27_ready(void) { const r27_t *S = (const r27_t *)R7B; return IS27() && S->magic == R7MAGIC && S->init; }
#define MODE2()   ((R8(MODEV) & 1u) && cf(CF_MOD))   /* Modulation Modes off: always Note Off (stock); the saved mode is kept */
#define VMODE (VT + 11u*0x40u + 0x1Fu)
#define SPD   (VT + 11u*0x40u + 0x2Fu)
#define NVAR 4u
static u32 rvar(void) { u32 m = R8(VMODE); return (m >= 1u && m <= NVAR) ? m : 1u; }
/* 2.53: each modulation mode keeps its own depth step (PDEP[i]) and speed (PSPD[i], s8), i = mode - 1, in spare bytes of
   slot 11's layer 3 (zero in flash, never read by the engine, not written by 0x6000E850). The engine keeps using MODEV
   bits 2-5 / SPD: they always hold the current mode's values (par_sync copies them back, set_mode loads them). */
#define PDEP (VT + 11u*0x40u + 0x34u)
#define PSPD (VT + 11u*0x40u + 0x38u)
#define DEPTH_MID 6u                     /* centre step: factory default and Up+Down reset */
static int spd(void) { int x = RS8(SPD); return (x < -6 || x > 6) ? 0 : x; }
static u32 depth_idx(void) { u32 d = (R8(MODEV) >> 2) & 15u; return d > 12u ? 0u : d; }
static const u8 SUB_OFF[NV] = {0,    0x11, 0x11, 0x11, 0x11, 0x21, 0x21, 0x11, 0x11, 0x11};
static const u8 DEF_NRM[NV] = {0,    3,    17,   3,    3,    13,   2,    21,   18,   18};   /* the Suggested Default */
static const u8 DEF_ALT[NV] = {0,    2,    1,    7,    19,   2,    11,   20,   15,   2};
/*                          omni1 omni2 harp  celes A.pno guit  FMpno organ vibes banjo
   nrm() = "primary" sub, alt() = "alt" sub: the settings block's (any of the subs 2.85 uses), else DEF_NRM / DEF_ALT.
   instr: 1 evolve_sub, 2 Mellow Synth Pad, 3 pad_sub, 7 churchbell, 11 rezy_sub, 13 pad_sub (copy), 15 vibes_sub_alt,
          17 harp_sub, 18 string_sub, 19 string2_sub, 20 organ_sub_alt, 21 organ_sub
   The same sub and alt is allowed: the re-press then changes nothing you can hear. */
#define SUBS_OK 0x003EA88Eu        /* bit = an instrument a sub may be: 1 2 3 7 11 13 15 17 18 19 20 21 */
static u8 cfg_sub(u32 v, u32 off, const u8 *def) {
    if (!cfg_ok() || v < 1u || v >= NV) return def[v < NV ? v : 0];
    u32 i = R8(CFG + off + v);
    return (i < 32u && ((SUBS_OK >> i) & 1u)) ? (u8)i : def[v];
}
static u8 nrm(u32 v) { return cfg_sub(v, 8u, DEF_NRM); }
static u8 alt(u32 v) { return cfg_sub(v, 18u, DEF_ALT); }
/* Evolve's boost (mod 2.29 on the harp; the user, 2026-10-17: "make sure evolve's volume boost follows it wherever it's
   mapped"): a voice that has Evolve as its sub or alt has a 2nd evolve_sub layer (BOOST_L, a spare layer; FM Piano has
   none) in the flash voice table, a copy of its sub layer (same envelope and bus), written there by the tool. It plays
   sample-for-sample in phase with the sub layer, so its volume adds linearly in the mixer: with Evolve selected its volume
   is 52/127 of the sub layer's (+2.98 dB, 2.29's harp value at 127), else 0 (silent; the layer stays enabled so its
   note-offs are never skipped). A layer that is not such a copy (on + instrument 1) is never touched. */
#define BOOST_NUM 52u
static const u8 BOOST_L[NV] = {0, 2, 2, 2, 2, 1, 0, 2, 2, 2};   /* the boost layer per voice (0 = none) */
static void boost_set(u32 slot, u32 v, u32 ins) {
    if (!BOOST_L[v]) return;
    u32 b = VT + slot*0x40u + BOOST_L[v]*16u;
    if (R8(b) != 1u || R8(b + 1u) != 1u) return;                       /* not a boost layer */
    u32 sv = R8(VT + slot*0x40u + SUB_OFF[v] + 12u);                   /* the sub layer's volume */
    R8(b + 13u) = (u8)(ins == 1u ? (sv * BOOST_NUM + 63u) / 127u : 0u);
}
static void set_sub(u32 v, u8 ins) {
    R8(VT + v*0x40u + SUB_OFF[v]) = ins; boost_set(v, v, ins);
    if (v != 1u) {                        /* keyboard-mode copy; omni 2's (slot 13) is a different voice (OM-84 Keys) */
        R8(VT + (v+12u)*0x40u + SUB_OFF[v]) = ins; boost_set(v + 12u, v, ins);
    }
}
static int alt_on(u32 v) { return v >= 1u && v < NV && R8(VT + v*0x40u + SUB_OFF[v]) == alt(v); }
static u32 alt_mask(void) { u32 m = 0; for (u32 v = 1; v < NV; v++) if (alt_on(v)) m |= 1u << (v - 1u); return m; }
static void apply_mask(u32 m) { for (u32 v = 1; v < NV; v++) set_sub(v, (m >> (v - 1u)) & 1u ? alt(v) : nrm(v)); }
void voice_btn(u32 idx) {               /* replaces bl e7dc at the 5 voice-button sites */
    u32 v = idx >= 12u ? idx - 12u : idx;
    if (v >= 1u && v < NV && v == R8(DE89))   /* the selected voice pressed again: toggle its sub */
        set_sub(v, alt_on(v) ? nrm(v) : alt(v));
    F_E7DC(idx);
}
void voice_other(u32 idx) {             /* bank-button path: subs keep their per-voice setting */
    F_E7DC(idx);
}

#define LEDSAVE 0x20000110u      /* stock LED flags shown when the startup LED animation / setting blink ends (0x16 bytes) */
/* ---------------- state record ---------------- */
/* rec: [0]=0xA5 [1]=ver (1 = mod 2.1-2.14, 2 = mod 2.15) [2]=vbank [3]=voice [4]=pbank [5]=pattern(FF=none)
        [6]=hold LED10 [7]=Manual LED11 (2.79: 2 / 3 = the third Manual/Auto state from Auto / Manual) [8]=LED13 [9]=tune [10]=transpose [11]=octave
        ver 2: [12] bits0-7 = alt sub on for voices 1-8, [13] bit0 = voice 9
        ver 1: [12]/[13] were restore debug bits; [13] bit6 = alt on for voice [3] (2, 4 or 7)   [14]=sum [15]=0 */
static u8 rec_sum(const volatile u8 *r) { u32 s = 0x5A; for (int i = 0; i < 14; i++) s += r[i]; return (u8)s; }
static int rec_valid(const volatile u8 *r) { return r[0] == 0xA5 && (r[1] == 1 || r[1] == 2) && r[15] == 0 && r[14] == rec_sum(r); }
/* settings record ver 3 (layout at build_set). The panel code ignores it. */
static int set_valid(const volatile u8 *r) { return r[0] == 0xA5 && r[1] == 3 && r[15] == 0 && r[14] == rec_sum(r); }
static u32 rec_mask(const volatile u8 *r) {
    if (r[1] == 2) return r[12] | ((r[13] & 1u) << 8);
    if ((r[13] & 0x40) && (r[3] == 2 || r[3] == 4 || r[3] == 7)) return 1u << (r[3] - 1u);
    return 0;
}
static int slot_blank(const volatile u8 *r) { for (int i = 0; i < 16; i++) if (r[i] != 0xFF) return 0; return 1; }
/* returns index of last valid record or -1; *next = first blank slot after last used slot (256 = full) */
static int scan(int *next) {
    int last = -1, used = -1;
    for (int i = 0; i < 256; i++) {
        const volatile u8 *r = (const volatile u8 *)(STATE_SECTOR + 16u*i);
        if (!slot_blank(r)) { used = i; if (rec_valid(r)) last = i; }
    }
    *next = used + 1;
    return last;
}
/* Fill a record from the live panel. panel=0 keeps r[2..6] and r[8] as they are (used in keyboard mode, where the panel
   LEDs show other things); 2.83: r[7] (Manual/Auto) is always live. */
static int scan_set(void) {           /* newest valid settings record or -1 */
    int last = -1;
    for (int i = 0; i < 256; i++) if (set_valid((const volatile u8 *)(STATE_SECTOR + 16u*i))) last = i;
    return last;
}
static void par_sync(void) {             /* the live depth / speed back into the current mode's slot */
    if (!MODE2()) return;
    u32 i = rvar() - 1u; R8(PDEP + i) = (u8)depth_idx(); R8(PSPD + i) = (u8)(s8)spd();
}
/* 2.53 settings record: [2] mode code 1-4, [3] its speed (as 2.52), [4..7] depth per mode, [8..11] speed per mode,
   [12] = PM_MARK (the per-mode bytes are valid). 2.52 reads only [2] / [3]. */
#define PM_MARK 0x5Du
static void build_set(volatile u8 *r) {
    par_sync();
    for (int i = 0; i < 16; i++) r[i] = 0;
    r[0] = 0xA5; r[1] = 3; r[2] = (u8)rvar(); r[3] = R8(PSPD + rvar() - 1u);
    for (u32 i = 0; i < NVAR; i++) { r[4 + i] = R8(PDEP + i); r[8 + i] = R8(PSPD + i); }
    r[12] = PM_MARK; r[13] = (u8)((R8(R27) == 1u ? REC27 : 0u) | (bsty_saved() << 6)); r[14] = rec_sum(r);   /* 2.60: [13] = 27 mode; 2.64: bits 6-7 = bass style (saved as chosen, even with Extras off) */
}
static int set_default(void) {
    par_sync();
    if (rvar() != 1u || R8(R27) == 1u || bsty_saved()) return 0;
    for (u32 i = 0; i < NVAR; i++) if (R8(PDEP + i) != DEPTH_MID || R8(PSPD + i) != 0u) return 0;
    return 1;
}
/* Program one record into slot n (interrupts must be off). */
static void prog_rec(int n, const u8 *rec) {
    volatile u8 *buf = (volatile u8 *)PAGEBUF;
    for (int i = 0; i < 256; i++) buf[i] = 0xFF;
    for (int i = 0; i < 16; i++) buf[(n & 15)*16 + i] = rec[i];
    F_PROG(FLEXSPI, STATE_SECTOR_OFF + (u32)(n >> 4)*256u, (void *)PAGEBUF);
}
/* Append the panel record p (if needp) and the settings record (if needs) at slot next. If they don't fit, erase the
   sector and write both current records again (the settings record only if not at defaults). Interrupts must be off.
   Returns 0 if the erase failed. */
static int append_recs(const u8 *p, int needp, int needs, int next) {
    u8 sr[16]; build_set((volatile u8 *)sr);
    if (!needp && !needs) return 1;
    if (next + needp + needs > 256) {
        if (F_ERASE(FLEXSPI, STATE_SECTOR_OFF) != 0) return 0;
        next = 0; needp = p != 0; needs = !set_default();
    }
    if (needp && p) prog_rec(next++, p);
    if (needs) prog_rec(next, sr);
    return 1;
}
static int set_changed(void) {          /* settings differ from the newest settings record (none: differ from defaults) */
    int ls = scan_set();
    if (ls < 0) return !set_default();
    const volatile u8 *o = (const volatile u8 *)(STATE_SECTOR + 16u*ls);
    u8 r[16]; build_set((volatile u8 *)r);
    for (int i = 2; i <= 13; i++) if (o[i] != r[i]) return 1;
    return 0;
}
static void build(volatile u8 *r, int panel) {
    r[0] = 0xA5; r[1] = 2;
    if (panel) {
        r[2] = R8(DE88); r[3] = R8(DE89); r[4] = R8(DE84);
        int pl = 0; for (int i = 9; i <= 13; i++) if (LED(i)) pl = 1;
        r[5] = pl ? R8(DE82) : 0xFF;
        r[6] = LED(0x10) ? 1 : 0; r[7] = (u8)ma_rec(); r[8] = LED(0x13) ? 1 : 0;     /* 2.79: [7] 2 / 3 = the third state */
    } else r[7] = (u8)ma_rec();          /* 2.83: Manual/Auto (and the third state) saved in keyboard mode too: its flags keep
                                            their meaning there (the stock keyboard mode never touches 0x11 / 0x12) */
    if (r27_ready()) r[5] = (u8)(0x40u | (((const r27_t *)R7B)->cur & 0x3Fu));   /* 2.60: the 27 selection, keyboard mode too */
    r[9] = R8(DE72); r[10] = R8(DE79); r[11] = R8(OCT);
    { u32 m = alt_mask(); r[12] = (u8)m; r[13] = (u8)(((m >> 8) & 1u) | ((R8(MODEV) & 0x7Fu) << 1)); }
    r[15] = 0;
    r[14] = rec_sum(r);
}
#define SNAP_OK 0xC3
/* Called every control tick while the panel is live (not during shutdown), once the restore is done.
   The record saved at power-off is this snapshot, so nothing that happens during shutdown can change it. */
static void snapshot(void) {
    volatile u8 *s = (volatile u8 *)ST;
    int kb = LED(0x15) != 0;              /* keyboard mode */
    if (kb && s[11] != SNAP_OK) return;   /* no normal-mode snapshot yet */
    build(s + 32, !kb);
    s[11] = SNAP_OK;
}
static void save_state(void) {
    volatile u8 *s = (volatile u8 *)ST;
    if (s[0] != 0x5A || s[11] != SNAP_OK) return;   /* restore not finished: keep the previous records */
    u8 rec[16];
    for (int i = 0; i < 16; i++) rec[i] = s[32 + i];
    int next, last = scan(&next), needp = 1;
    if (last >= 0) {
        const volatile u8 *o = (const volatile u8 *)(STATE_SECTOR + 16u*last);
        int same = 1; for (int i = 0; i < 16; i++) if (o[i] != rec[i]) same = 0;
        if (same) needp = 0;
    }
    int needs = set_changed();
    if (!needp && !needs) return;
    __asm volatile ("cpsid i" ::: "memory");
    append_recs(rec, needp, needs, next);
    __asm volatile ("cpsie i" ::: "memory");
}
/* Replaces bl 0x6000B2C4 at 0x6000F284 (power-on setup). Runs the stock check unchanged; if it requested a
   factory reset, erase our panel-state sector too, so the unit comes up with stock defaults (primary subs, Hold
   etc.). The stock code writes its own settings sector right after this, from the same place. */
/* Power-on (no factory reset): MODEV = bits 1-6 of byte 13 of the newest valid panel record (none: Note Off mode, depth
   centre); the effect mode, speeds, depths, 27 mode and the bass from the newest settings record. No power-on key changes
   the plate mode (Gb 7th in keyboard mode does); the rhythm-mode and bass keys are read next, in rhythm_setup. */
static void boot_mode(void) {
    int next, last = scan(&next); (void)next;
    const volatile u8 *o = last >= 0 ? (const volatile u8 *)(STATE_SECTOR + 16u*last) : (const volatile u8 *)0;
    u32 m = (o && o[1] == 2) ? ((o[13] >> 1) & 0x7Fu) : (DEPTH_MID << 2);   /* no record: Note Off, depth centre */
    if (((m >> 2) & 15u) > 12u) m &= 0x43u;
    m &= ~0x42u;                             /* bits 1 and 6 unused since 2.34 */
    {   int ls = scan_set();                 /* plate mode + speed (defaults if none) */
        const volatile u8 *q = ls >= 0 ? (const volatile u8 *)(STATE_SECTOR + 16u*ls) : (const volatile u8 *)0;
        R8(VMODE) = q ? q[2] : 1u;
        R8(VMODE) = (u8)rvar();              /* old or unknown codes load as vibrato auto */
        s8 x = q ? (s8)q[3] : 0; R8(SPD) = (u8)((x >= -6 && x <= 6) ? x : 0);
        for (u32 i = 0; i < NVAR; i++) {     /* 2.53 per-mode values; an older record: every mode starts from the shared ones */
            u32 d = (m >> 2) & 15u; s8 sp = (s8)R8(SPD);
            if (q && q[12] == PM_MARK) { d = q[4 + i]; sp = (s8)q[8 + i]; }
            R8(PDEP + i) = (u8)(d > 12u ? DEPTH_MID : d); R8(PSPD + i) = (u8)((sp >= -6 && sp <= 6) ? sp : 0);
        }
        if (m & 1u) { m = (m & ~0x3Cu) | ((u32)R8(PDEP + rvar() - 1u) << 2); R8(SPD) = R8(PSPD + rvar() - 1u); }
        R8(R27) = (q && (q[13] & 0x3Fu) == REC27) ? 1u : 0u;      /* 2.60 */
        R8(BSTY) = q ? (u8)(q[13] >> 6) : 0u; }                   /* 2.64 (3 = unused: reads as 0) */
    R8(MODEV) = (u8)m;
}
u32 boot_b2c4(void) {
    u32 r = F_B2C4();
    if (R8(DE6B)) {
        R8(0x20000285u) = 1u;            /* factory default battery type = Ni-MH (stock writes 0 = alkaline); saved by the stock write that follows */
        u32 pm; __asm volatile ("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");
        F_ERASE(FLEXSPI, STATE_SECTOR_OFF);
        F_DCINV(STATE_SECTOR, 0x1000);   /* no stale cached copy of the old records */
        __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
        R8(MODEV) = (u8)(DEPTH_MID << 2);   /* factory reset: Note Off mode; every mode's depth and speed at the centre */
        R8(VMODE) = 1u; R8(SPD) = 0u;
        for (u32 i = 0; i < NVAR; i++) { R8(PDEP + i) = DEPTH_MID; R8(PSPD + i) = 0u; }
        R8(BOOTF) = 0u; R8(R27) = 0u; R8(BSTY) = 0u;   /* 2.60: factory reset = 108 mode; 2.64: normal bass */
    } else {
        boot_mode();
    }
    return r;
}
void latch_off(void) {                   /* replaces the two power-latch-off stores */
    save_state();
    R32(0x401B8088u) = 0x8000u;          /* GPIO1 DR_CLEAR bit 15: power latch off (original action) */
    for (;;) { }
}

/* ---------------- panel restore at power-on ----------------
   The saved panel record is put back through the stock button handler (simulated presses: the stock code does every
   selection, its loading and its LEDs, exactly as for the player), only where the panel differs from the record.
   2.78: restore_early does it in the stock power-on set-up (rhythm_setup), before the startup LED animation / setting blink,
   so the panel is shown with the saved settings at once: the stock handler is run back to back with nothing to wait for
   (KEYS_EN set for the press scans only), its LED writes are dropped (the animation / blink draws the LEDs, and its end
   shows the flags it finds in LEDSAVE, which we fill). A step whose button is held from power-on (a power-on key that is
   also a panel button) is left to restore_tick, which presses it once the button has been let go; the player's own
   press of a button of that step's group wins (the step is dropped). Tune, transpose, octave, Start/Sync and the
   alternate subs are set directly (apply_direct / apply_mask).
   ST layout: [0]=phase [1]=step [2]=wait [3]=tries [4..5]=timeout ticks [6]=key being pressed (FF none) [7]=scans left to
              hold it [8]=steps still to do (bit = step) [9]=our last pressed key until it is let go (FF none) [11]=snapshot
              valid (C3) [12..15]=panel buttons held since power-on (bit = key - 0x24) [16..31]=record [32..47]=snapshot */
#define PH_RUN   0x33
#define PH_DONE  0x5A
#define PH_EARLY 0x3E     /* restore_early is running (row_filter: injected presses only) */
#define PRESS_SCANS 2     /* key reads as pressed for 2 matrix scans */
#define GAP_TICKS   45    /* stock release debounce is 30 scans */
#define ARM_SCANS   32    /* restore_early: scans for the stock release debounce (30) */
#define RESTORE_MAX 10000u /* ticks: a power-on button held longer than this leaves its setting as it is */
#define LEDQ_W   0x2000AF8Cu   /* stock LED-write queue: write index (ring 0x2000AB8C, 512 entries, one sent per ms) */
/* Called from the key-matrix scan (row_read): extra column bits that read as pressed for this row. */
u32 inj_bits(u32 row) {
    volatile u8 *s = (volatile u8 *)ST;
    if ((s[0] != PH_RUN && s[0] != PH_EARLY) || s[7] == 0) return 0;
    u32 k = s[6];
    if (k >= 54u || k / 9u != row) return 0;
    return 1u << (R8(KEYBITS + (k % 9u) * 16u + 8u) & 31u);
}
static const u8 slotkey[5] = {0x24, 0x25, 0x26, 0x33, 0x34};
/* returns key to press for this step, or 0 if nothing needed */
static u32 step_key(int step, const volatile u8 *r) {
    switch (step) {
    case 0: if (r[2] <= 1 && R8(DE88) != r[2]) return 0x27; return 0;
    case 1: if (r[3] <= 9 && R8(DE88) == r[3]/5u && R8(DE89) != r[3]) return slotkey[r[3] % 5u]; return 0;
    case 2: if (IS27()) return 0;                     /* 2.60: 27 mode restores its own selection (r27_tick) */
            if (r[5] <= 9 && r[4] <= 1 && R8(DE84) != r[4]) return 0x2D; return 0;
    case 3: if (IS27()) return 0;
            if (r[5] <= 9 && R8(DE84) == r[5]/5u && (R8(DE82) != r[5] || !LED(9 + r[5] % 5u))) return 0x28 + r[5] % 5u; return 0;
    case 4: if (r[7] <= 3 && LED(0x11) != (r[7] ? 1u : 0u)) return 0x2F; return 0;   /* Manual for the third state too */
    case 5: if (r[6] <= 1 && LED(0x10) != r[6]) return 0x30; return 0;
    }
    return 0;
}
static void apply_direct(const volatile u8 *r) {
    s8 t = (s8)r[9], x = (s8)r[10];
    if (t >= -6 && t <= 6) {                 /* same calls as the stock P + arrow handler (0x6000B3B8) */
        RS8(DE72) = t; F_TUNE(t);
        F_TUNE_A((volatile s8 *)DE72); F_TUNE_B((volatile s8 *)DE72);
    }
    if (x >= -6 && x <= 6) RS8(DE79) = x;
    if (r[11] <= 2) { R8(OCT) = r[11]; R8(OCT2) = r[11]; }
    if (r[8] == 1 && LED(0x13) == 0) { F_LEDSET(0x13, 1); F_LEDSET(0x14, 0); }
    else if (r[8] == 0 && LED(0x13) == 1) { F_LEDSET(0x13, 0); F_LEDSET(0x14, 1); }
}
static void third_fix(const volatile u8 *r) {   /* 2.79: Manual/Auto step done: the third state = Manual + the Auto flag */
    if ((r[7] == 2u || r[7] == 3u) && ma_state() == MA_MANUAL && cf(CF_EXTRAS)) F_LEDSET(0x12, 1);   /* Extras off: Manual */
}
static int key_down(u32 k) { return R8(KEY(k)) || R8(KEY(k) + 4u); }   /* raw, or still pressed for the stock */
static u32 step_group(u32 k) {         /* the restore steps a panel button belongs to (bit = step) */
    if ((k >= 0x24u && k <= 0x27u) || k == 0x33u || k == 0x34u) return 0x03u;   /* voice bank, voice */
    if (k >= 0x28u && k <= 0x2Du) return 0x0Cu;                                  /* pattern bank, pattern */
    return k == 0x2Fu ? 0x10u : k == 0x30u ? 0x20u : 0u;                         /* Manual/Auto, Hold */
}
static void restore_early(void) {
    volatile u8 *s = (volatile u8 *)ST;
    R8(EARLY) = 0x80u | 0x3Fu;          /* if anything below stops early, restore_tick does every step */
    if (R8(DE87)) return;               /* 2.79: the stock power-on with keys 0 + 11 held (0x6000B1E8) shows no animation and
                                           never copies LEDSAVE (0x60004028: default flags): restore_tick presses every step */
    int next, last = scan(&next); (void)next;
    if (last < 0) { R8(EARLY) = 0x80u; return; }
    const volatile u8 *r = (const volatile u8 *)(STATE_SECTOR + 16u*last);
    /* The stock start-up keeps the panel's LED flags in LEDSAVE (Hold, Manual/Auto, Start/Sync are flags: the stock button
       code reads them) and shows them when the startup animation / setting blink ends; until then the live flags belong
       to that animation. So: the flags = LEDSAVE while the stock button code runs, LEDSAVE = the result, then the live
       flags, the LED shadows and the LED-write queue exactly as they were (the animation / blink looks as stock). */
    u8 fl[0x16], sh0 = R8(LEDSH), sh1 = R8(LEDSH + 1u), sh2 = R8(LEDSH + 2u);
    for (u32 i = 0; i < 0x16u; i++) { fl[i] = LED(i); LED(i) = R8(LEDSAVE + i); }
    u32 qw = R32(LEDQ_W), en = R8(KEYS_EN);
    apply_direct(r);
    apply_mask(rec_mask(r));            /* per-voice subs; loaded by the voice selection below or at the end */
    s[6] = 0xFFu; s[7] = 0; s[0] = PH_EARLY;
    for (u32 i = 0; i < ARM_SCANS; i++) F_C274();          /* every released button armed (stock release debounce) */
    u32 pend = 0;
    for (u32 st = 0; st < 6u; st++) {
        if ((st == 1u && (pend & 1u)) || (st == 3u && (pend & 4u))) { pend |= 1u << st; continue; }   /* needs its bank first */
        u32 k = step_key((int)st, r);
        if (!k) continue;
        if (key_down(k)) { pend |= 1u << st; continue; }   /* held from power-on: once it is let go (restore_tick) */
        s[6] = (u8)k; s[7] = 1u; R8(KEYS_EN) = 1u;
        F_C274();                                           /* the press: the stock acts on it at once */
        R8(KEYS_EN) = (u8)en; s[7] = 0; s[6] = 0xFFu;
        for (u32 i = 0; i < ARM_SCANS; i++) F_C274();      /* let go (its stock release, so the battery-type keys read 0) */
        if (step_key((int)st, r)) pend |= 1u << st;
    }
    if (!(pend & 0x10u)) third_fix(r);  /* (step 4 pending: restore_tick, once its button is let go) */
    if (rec_mask(r)) F_E7DC(R8(DE89) + (R8(DE80) ? 12u : 0u));   /* a restored alternate sub of a voice that needed no press */
    if (IS27()) {                       /* 27 mode: its saved rhythm on the pattern LEDs (r27_tick takes over at once) */
        static const u8 L[6] = { 0x0Eu, 9, 10, 11, 12, 13 };
        u32 m = (r[5] & 0xC0u) == 0x40u ? (r[5] & 0x3Fu) : 0u;
        if (!m) m = 2u;
        for (u32 i = 0; i < 6u; i++) LED(L[i]) = (u8)((m >> i) & 1u);
        LED(0x0F) = 0;
    }
    for (u32 i = 0; i < 0x16u; i++) { R8(LEDSAVE + i) = LED(i); LED(i) = fl[i]; }   /* shown when the animation / blink ends */
    R8(LEDSH) = sh0; R8(LEDSH + 1u) = sh1; R8(LEDSH + 2u) = sh2;
    R32(LEDQ_W) = qw;                   /* our LED writes dropped: nothing flashes before the startup animation */
    s[0] = 0;
    R8(EARLY) = (u8)(0x80u | pend);
}
/* Restore finished: the selected voice (re)loaded with its saved sub (the stock voice selection call). */
static void restore_end(volatile u8 *s) {
    s[0] = PH_DONE; s[6] = 0xFFu; s[7] = 0;
    F_E7DC(R8(DE89) + (LED(0x15) ? 12u : 0u));
}
static void restore_tick(void) {       /* control task, every tick until done */
    volatile u8 *s = (volatile u8 *)ST;
    u8 ph = s[0];
    if (ph == PH_DONE) return;
    if (ph != PH_RUN) {                   /* first call after boot (buffer is 00.. or FF.. or settings junk) */
        if (!R8(KEYS_EN)) return;          /* wait until the panel accepts key events */
        s[6] = 0xFFu; s[7] = 0; s[9] = 0xFFu; s[11] = 0;
        int next, last = scan(&next);
        if (last < 0) { s[0] = PH_DONE; return; }
        const volatile u8 *r = (const volatile u8 *)(STATE_SECTOR + 16u*last);
        for (int i = 0; i < 16; i++) s[16 + i] = r[i];
        apply_direct(s + 16);              /* again (unchanged unless the stock start-up changed them) */
        apply_mask(rec_mask(s + 16));
        r27()->mfrom = s[16 + 7] == 3u ? (u8)MA_MANUAL : (u8)MA_AUTO;   /* 2.79: a restored third state: where it goes back to */
        u8 e = R8(EARLY);
        s[8] = (e & 0x80u) ? (u8)(e & 0x3Fu) : 0x3Fu;
        u32 h = 0;
        for (u32 k = 0x24u; k <= 0x34u; k++) if (key_down(k)) h |= 1u << (k - 0x24u);
        R32(ST + 12u) = h;
        s[1] = 0; s[2] = 0; s[3] = 0; s[4] = 0; s[5] = 0; s[0] = PH_RUN;
        if (!s[8]) restore_end(s);
        return;
    }
    if (s[7]) { s[7]--; if (!s[7]) s[6] = 0xFFu; }   /* end of a simulated press */
    u32 to = s[4] | (s[5] << 8); to++; s[4] = (u8)to; s[5] = (u8)(to >> 8);
    if (to > RESTORE_MAX) { restore_end(s); return; }     /* safety stop */
    u32 h = R32(ST + 12u);                 /* the player's own presses win; buttons held from power-on get let go */
    for (u32 k = 0x24u; k <= 0x34u; k++) {
        u32 b = 1u << (k - 0x24u), d = (u32)key_down(k);
        if (h & b) { if (!d) h &= ~b; continue; }
        if (k == s[9]) { if (!d && !s[7]) s[9] = 0xFFu; continue; }   /* our own press */
        if (d) s[8] &= (u8)~step_group(k);
    }
    R32(ST + 12u) = h;
    if (s[2]) { s[2]--; return; }
    while (s[1] < 6) {
        u32 st = s[1], bit = 1u << st;
        if (!(s[8] & bit)) { s[1]++; s[3] = 0; continue; }
        u32 k = step_key((int)st, s + 16);
        if (!k || s[3] >= 3) {             /* done, or no effect after 3 presses */
            if (st == 4u && !k) third_fix(s + 16);
            s[8] &= (u8)~bit; s[1]++; s[3] = 0; continue;
        }
        if (key_down(k)) return;           /* still held (from power-on): wait until it is let go */
        s[6] = (u8)k; s[9] = (u8)k; s[7] = PRESS_SCANS; s[3]++; s[2] = GAP_TICKS;
        return;
    }
    restore_end(s);
}
/* ================= runtime: modulation engine, keyboard-mode controls, spoken prompts, power-on blink ================= */
/* The modulation engine (vib_tick). scan_wrap (entry +0x18) replaces "bl 0x60009B20" at 0x6000EB50 (stock strum-plate
   scanner, control loop); r1 = touch-chip status DEA1, bit 6 = Instant OFF plate. Note Off mode: unchanged, nothing
   written. Effect modes, normal play: bit 6 is cleared for the stock code (Instant OFF never fires); the plate reading
   drives the effect (see "the plate in the effect modes" below). Keyboard mode: bit 6 passes through (the stock clap).
   Pitch = engine global sample tune (s16 cents, 0x20007606; re-read by 0x20200ED0 for every sounding melodic channel on
   every audio block). Offset = base + LFO; the base is re-taken whenever someone else changed the tune (P + arrows,
   panel restore); when everything is idle the exact base is written back and the tune is left alone. */
#define TUNE16  (*(volatile short *)0x20007606u)
#define TICKS   R32(0x2000DB84u)
#define SIG14   RS8(0x2000AB86u)
#define DE2C    0x2000DE2Cu      /* stock power-on blink running */
#define DE2F    0x2000DE2Fu      /* stock blink phase */
#define DE66    0x2000DE66u      /* stock: rhythm-tone setup key used (save + blink) */
#define SETBUF  0x20000280u      /* stock settings buffer; byte 4 = rhythm tone 0 Normal (OM-108) / 1 Classic (OM-84) */
#define F_ALLOFF  ((void(*)(void))0x600186C9)            /* all strum channels off (veneer -> 0x202000D8) */
#define F_NOTEON  ((void(*)(u32,u32))0x600186D9)         /* plate note-on (veneer -> 0x20200114) */
#define F_LEDDISP ((void(*)(volatile s8*))0x60003AD1)    /* stock master-tune LED display (pattern row), value -6..+6 */
#define F_LEDDISPT ((void(*)(volatile s8*))0x60003985)   /* stock transpose LED display (voice row), value -6..+6 */
#define F_LEDREST ((void(*)(void))0x60003C15)            /* stock: redraw LEDs from their flags */
#define F_MEMCPY  ((void(*)(void*,const void*,u32))0x600181D7)
#define F_E850    ((void(*)(u32))0x6000E851)              /* stock: layer release-mode byte for all voices (P key uses 1) */
#define DEPTH_STEPS 13
/* 2.48: the user's table (steps 0-12; the menu shows step 0 as -6). Peak cents at a full touch / full rock. 1200 cents = the
   largest the maths allows: w * dc / 32 * env stays below 2^31. Analog voices (Omni 1/2) clamp at about +-400 c (ratio_q30). */
static const unsigned short DEPTH_C[DEPTH_STEPS] = { 22, 32, 40, 48, 50, 60, 67, 74, 87, 100, 150, 190, 270 }; /* cents */
/* 2.49: speed menu steps -6..+6 (the user's table, 4.3125 / 5.9375 / 7.5625 Hz rounded to the mHz), mHz; touch swell on top */
static const unsigned int SPEED_MHZ[13] = { 500, 1000, 3500, 4313, 5125, 5938, 6750, 7563, 8180, 10000, 12000, 40000, 80000 };
/* 2.49: touch swell per step, +-mHz (light touch .. full touch): 0.5 Hz = 0.25-0.75, 1 Hz = 0.5-1.5, the rest +-1.25 Hz */
static const unsigned short SWELL_MHZ[13] = { 250, 500, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250 };
/* ---- plate tuning (all easy to change). Instant OFF readings at the 16x gain set in the effect modes
   (2.35dev2 measurements, 2026-10-06): hands off 0-7, a full touch 72-74, jitter about +-1, drift 2-3 counts over 8 s.
   The stock 8x scale is about half of this (hands off 0-6, strumming nearby <= 4, touch 33-39). ---- */
#define FADE_IN_MS   600  /* keyboard-mode menus only: audition fade-in */
#define FADE_OUT_MS  880  /* fade-out (S-curve) once the finger has left the plate (its inaudible last 5-10 % included: >= 0.8 s heard) */
#define REL_MS       800  /* while on the plate, depth may fall no faster than full -> 0 in this long */
#define LEGATO_MS    250  /* re-touch within this long after the fade-out ended: carry on (phase, speed) */
#define P_ON         15   /* the vibrato starts when the reading stays >= this for P_MS (about 2x strumming-nearby) ... */
#define P_OFF        11   /* ... and stops when it stays below this for P_MS */
#define P_MS         20
#define FULL_AT      68   /* depth rises from P_ON to full here (just under a full touch, 72-74) */
#define L_MAX        80
#define TAU_FAST     15   /* reading smoothing (ms) for real finger movement ... */
#define TAU_SLOW     60   /* ... and for jitter (changes within JIT_Q8 of the smoothed value) */
#define JIT_Q8       640  /* 2.5 counts */
#define ATTACK_MS    40   /* the finger's approach is the fade-in; just an anti-click ramp */
#define GLIDE_MS     100  /* speed changes glide */
/* ---- plate modes ----
   Auto (vibrato auto / tremolo auto): LFO; off the pad the touch continues for up to HOLD_MS (the roll-off hold) with the
   depth kept; a quick lift ends it after P_MS; then a PLATE_FADE_MS fade.
   Manual ("finger rock", no LFO): vibrato manual: output = depth setting x (smoothed reading - home) / ROCK_FULL, clamped to
   +-1, rolling in (more of the pad covered) = up; home follows the reading (ROCK_HOME), frozen while off the pad, = the
   reading for the first ROCK_LAND ms. Tremolo manual (2.77, "the floor"): home drops with the reading at once and rises
   back toward it over TREM_FLOOR_MS; dip = the roll inward from it, full at TREM_FULL counts; a still finger returns to
   normal. A fall as fast as a real lift (more than ROCK_FASTQ8 below a peak decaying ROCK_FDEC per ms) ends the touch after
   P_MS. Output slewed ROCK_SLEW ms. Menus audition the LFO in every mode.
   Measurements (242dev_rocking_1-3, 2026-10-09): still finger +-1, full 72-73; rocks 0.8-4.3 Hz from ~45-65 down to
   0-12, small edge rocks 3-28 (reference/plate_measurements.md). ---- */
#define PLATE_FADE_MS 200  /* fade-out after the finger has left the plate (normal play) */
#define HOLD_MS      350   /* roll-off hold */
#define ROCK_FULL    25    /* counts from home = the full depth setting */
#define ROCK_HOME    1000  /* ms */
#define ROCK_LAND    60    /* ms */
#define ROCK_FDEC    640   /* Q8 per ms = 2.5 counts per ms */
#define ROCK_FASTQ8  2560  /* 10 counts */
#define ROCK_SLEW    10
#define TREM_FLOOR_MS 1500
#define TREM_FULL    35    /* counts above the floor = the full depth setting (vibrato's ROCK_FULL is 25) */
static int is_rock(u32 var) { return var == 2u || var == 4u; }   /* the manual modes */
static int is_trem(u32 var) { return var >= 3u; }                 /* the tremolo modes */
/* 2.52 tremolo (modes 3 / 4): the same engine as vibrato auto / manual (gate, hold, finger depth, fades, speed, menus,
   keyboard effect) drives a volume dip instead of a pitch offset. Dip in % of the volume at full depth: TREM_PCT[step].
   Auto: dip = depth x (1 + LFO) / 2, so the level swings between normal and (1 - depth). Manual (no LFO): dip = depth x
   the roll inward from the floor. The level never goes above normal. Applied to the strum-voice bus (channel A) in
   mix_tail; drums and the analog (Omni 1/2) oscillators are not affected. */
static const u8 TREM_PCT[13] = { 10, 15, 20, 25, 30, 35, 40, 50, 60, 70, 80, 90, 100 };
/* 2.52 MIDI pitch bend (vibrato modes 1 / 2, the plate and the keyboard effect; never the menus): the pitch offset goes out
   as pitch bend on internal Main (0) and Sub (3) - the channels of the strum and keyboard notes - through mod 1's hook,
   so it is routed by the preset exactly like the notes. Scaled for a +-12 semitone bend range (1200 c = 8192). Sent when
   it changes, at most every PB_MS; centre when the effect ends. RPN 0 (bend range 12 semitones, then RPN null) is sent on
   the routed strum channels at the first chance in a vibrato mode after power-on and RPN_IDLE ms after each vibrato has
   ended (not at its start, so it never delays a strum). Mod 1 drops CCs, so these few go to the stock queue directly. */
#define PB_MS     10
#define RPN_IDLE  1000
#define FXB       0x20002BDCu      /* fx_t: free since 2.38 (32 B) */
#define FXMAGIC   0x46583532u
typedef struct { u32 magic; volatile u32 gain, gmix; unsigned short pb_last, pb_ms, idle; u8 rpn, r; } fx_t;
/* gain (Q15, control task) -> gmix (what the mixer applied last, mixer only); rpn: 0 sent, 1 send now, 2 send after idle */
_Static_assert(sizeof(fx_t) <= 0x20002BF0u - FXB, "fx_t overlaps mt_t (0x20002BF0)");
#define MIDIQ     0x2000681Cu      /* stock MIDI queue of the strum / keyboard notes (all message types forwarded) */
#define F_MHOOK   ((u32(*)(u32, void*))0x60039001)  /* mod 1: route by preset, drop CCs, then post */
#define F_MPOST   ((u32(*)(u32, void*))0x6000DB29)  /* stock queue post (returns 0 if full) */
#define ROUTE     0x60039100u                         /* mod 1 route[preset-1][internal ch], 0xFF = not sent */
typedef struct { u8 type, ch; unsigned short len; u8 *data; } mmsg_t;   /* type 4 = CC, 7 = pitch bend (0x60033460) */
#define VB      0x20002B90u      /* vibrato state (0x44 B max), in the stock page buffer after an_t (power-on/-off only; magic-checked) */
#define UIB     0x20002C08u
#define PLB     0x20002C20u      /* ST (restore) starts at 0x20002C54 */
#define ANB     0x20002B60u      /* analog-vibrato state, in the stock page buffer (used only at power-on/-off) */
typedef struct { u32 magic, tick, phase; int x, ls, rate, home; short base, last;
                 unsigned short mgate, off, land, hcnt; s8 de72; u8 active, gate, fast; unsigned short fo;
                 u8 hov, fmin; int lf, pkr, cout; } vib_t;
/* phase = LFO phase; x = fade (0..65536); ls = finger depth Q12 (after the release rule); rate = LFO rate (mHz); home =
   manual-mode reference reading (Q8); base / last = the sample tune without / with our offset; mgate = this gate is a menu
   audition; off = ms since the fade ended; land = ms since the manual touch landed; hcnt = gate persistence (ms); hov =
   the plate gate; fmin / fast = quick-lift detector; fo = fade-out ms; lf = smoothed plate reading, 16x scale, Q8; pkr =
   decaying peak of the reading (Q8); cout = slewed manual output */
_Static_assert(sizeof(vib_t) <= 0x44, "vib_t must end before 0x20002BD4");
_Static_assert(__builtin_offsetof(vib_t, phase) == 8 && __builtin_offsetof(vib_t, ls) == 16 && __builtin_offsetof(vib_t, rate) == 20
               && __builtin_offsetof(vib_t, gate) == 42 && __builtin_offsetof(vib_t, hov) == 46 && __builtin_offsetof(vib_t, lf) == 48,
               "test offsets");
typedef struct { u32 magic; u8 gbm, menu, ev, kst; u8 kon[2], koff[2]; u8 kmask[2]; s8 disp; u8 pad;
                 unsigned short bootwait, redraw; u32 hid; } ui_t;
_Static_assert(sizeof(ui_t) <= 0x20002C20u - 0x20002C08u, "ui_t overlaps the prompt player");
typedef struct { u32 magic, req, ack, pos, len; const u8 *p; u8 qa, qb, stage, k; unsigned short gap; u8 nx, r; } pl_t;
/* req / qa / qb: written only by the control task (prompt_req); everything else only by the mixer (after init). stage 0
   idle, 1 playing a clip, 2 the gap before the second word. */
_Static_assert(__builtin_offsetof(pl_t, qa) == 24 && __builtin_offsetof(pl_t, stage) == 26 && sizeof(pl_t) <= 0x20002C50u - 0x20002C20u,
               "test offsets / pl_t too big");
typedef struct { u32 magic; unsigned short b1[4], b2[4], w1[4], w2[4]; u8 err[4]; u8 mod, pad[3]; volatile u8 wr[4]; } an_t;   /* wr: set by pwm_hook when the stock code programs a channel */
#define VMAGIC 0x56494246u
#define UMAGIC 0x55493231u
#define MENU_DEPTH 1u
#define MENU_SPEED 2u
#define MENU_KVIB  3u    /* 2.48: keyboard vibrato (a note was held when the minor button went down): the effect only */
#define MENU_MODE  4u    /* 2.53: Gb 7th held (mode selection): no sound, LEDs only */
#define PMAGIC 0x504C3232u
#define AMAGIC 0x414E3232u
#define EV_UP 1u
#define EV_DN 2u
#define EV_BOTH 4u
static const short SINQ[65] = {           /* 8192 * sin(i * 90 deg / 64) */
    0, 201, 402, 603, 803, 1003, 1202, 1401, 1598, 1795, 1990, 2185, 2378, 2570, 2760, 2948,
    3135, 3320, 3503, 3683, 3862, 4038, 4212, 4383, 4551, 4717, 4880, 5040, 5197, 5351, 5501, 5649,
    5793, 5933, 6070, 6203, 6333, 6458, 6580, 6698, 6811, 6921, 7027, 7128, 7225, 7317, 7405, 7489,
    7568, 7643, 7713, 7779, 7839, 7895, 7946, 7993, 8035, 8071, 8103, 8130, 8153, 8170, 8182, 8190, 8192 };
static int sinp(u32 ph) {                /* phase 0..2^32 per cycle -> -8192..8192, interpolated (16384 steps/quarter) */
    u32 q = ph >> 30, x = (ph >> 16) & 0x3FFFu;
    if (q & 1u) x = 0x4000u - x;
    u32 k = x >> 8, f = x & 255u;
    int v = (k >= 64u) ? SINQ[64] : SINQ[k] + (((SINQ[k + 1u] - SINQ[k]) * (int)f) >> 8);
    return (q & 2u) ? -v : v;
}
static ui_t *ui(void) {
    ui_t *U = (ui_t *)UIB;
    if (U->magic != UMAGIC) {
        U->gbm = 1; U->menu = 0; U->ev = 0; U->kst = 0; U->kon[0] = U->kon[1] = 0; U->koff[0] = U->koff[1] = 255;
        U->kmask[0] = U->kmask[1] = 0; U->disp = 0; U->pad = 0; U->bootwait = 0; U->redraw = 0; U->hid = 0; U->magic = UMAGIC;
    }
    return U;
}
static int menu_on(void) { ui_t *U = (ui_t *)UIB; return (U->magic == UMAGIC && U->menu != MENU_MODE) ? (int)U->menu : 0; }   /* 0 or a sounding MENU_* */
static pl_t *pl(void) {
    pl_t *P = (pl_t *)PLB;
    if (P->magic != PMAGIC) {
        P->req = 0; P->ack = 0; P->stage = 0;
        __asm volatile ("dmb" ::: "memory"); P->magic = PMAGIC;
    }
    return P;
}

/* ---- analog voices: chord / strum oscillators (TMR1 ch 0-2, top-octave square waves divided down in hardware)
   and bass (ch 3), set by the stock code with QTMR_SetupPwm (0x600166D0): CMPLD1 (+0x10) = low count, CMPLD2 (+0x12) =
   high count. Vibrato rescales both preload registers (takes effect at the next edge, no reset, no click). Whatever the
   stock code writes is taken as the new base; idle = base written back exactly. Bass is reprogrammed from the rhythm
   timer interrupt, so its read-modify-write runs with interrupts off. Fractional counts are error-diffused. */
#define TMR1 0x401DC000u
static const int SEMI_Q30[13] = { 1518500250, 1433273380,      /* 2.68: -6 / -5 semitones added (the 2.37 values kept) */
    1352829926, 1276901417, 1205234447, 1137589835, 1073741824, 1013477949, 956595979, 902905867, 852226808,
    804397487, 759250125 };
static int ratio_q30(int c) {            /* 2^(-c/1200) in Q30, c in 1/256 cent, -600 <= c < 600 cents (2.68: was +-400) */
    int q = c / 25600, rem = c - q * 25600;
    if (rem < 0) { rem += 25600; q--; }
    if (q < -6) { q = -6; rem = 0; } if (q > 5) { q = 5; rem = 25599; }
    long long x = (long long)rem * 2423;                       /* rem * ln2/1200/256 in Q30 */
    int x2 = (int)((x * x) >> 30), x3 = (int)(((long long)x2 * x) >> 30);
    long long e = (1LL << 30) - x + (x2 >> 1) - (x3 / 6);
    return (int)(((long long)SEMI_Q30[q + 6] * e) >> 30);
}
/* The chord oscillators' pitch = the analog vibrato (vib_tick: an_apply, Omni 1/2 only) + the jazz voicing's whole
   semitones per oscillator (chord_tick: sh0 / sh1 / sh2), both written here. */
static void an_write(void) {
    an_t *A = (an_t *)ANB;
    if (A->magic != AMAGIC) { A->mod = 0; for (int i = 0; i < 4; i++) { A->err[i] = 0; A->wr[i] = 0; A->w1[i] = A->w2[i] = 0; } A->magic = AMAGIC; }
    r27_t *S = r27();
    int vc = S->von ? S->vc : 0, on = S->von || S->sh0 || S->sh1 || S->sh2;
    if (!on && !A->mod) return;
    for (u32 ch = 0; ch < 3; ch++) {                   /* 2.37: chord/strum oscillators only, never the bass (ch 3) */
        int c = vc + (int)(ch == 0u ? S->sh0 : ch == 1u ? S->sh1 : S->sh2) * 25600, oct = 0;   /* + whole semitones (jazz) */
        while (c >= 153600) { c -= 307200; oct++; }    /* whole octaves exactly (period / 2, x 2), the rest within +-600 cents */
        while (c < -153600) { c += 307200; oct--; }
        int r = on ? ratio_q30(c) : (1 << 30);
        volatile unsigned short *r1 = (volatile unsigned short *)(TMR1 + ch * 0x20u + 0x10u), *r2 = r1 + 1;
        u32 pm;
        __asm volatile ("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");   /* bass is reprogrammed from an interrupt */
        unsigned short v1 = *r1, v2 = *r2;
        u32 stock = A->wr[ch]; A->wr[ch] = 0;
        if (!A->mod || stock || v1 != A->w1[ch] || v2 != A->w2[ch]) { A->b1[ch] = v1; A->b2[ch] = v2; A->err[ch] = 0; }  /* stock wrote it */
        /* stored values are (count - 1) each: the real period is b1 + b2 + 2 */
        u32 b1 = A->b1[ch], b2 = A->b2[ch], n1 = b1, n2 = b2, tot = b1 + b2 + 2u;
        if (on && tot >= 16u && tot < 65536u) {
            u32 t8 = (u32)(((unsigned long long)tot * (u32)r) >> 22);                 /* new period in 1/256 count */
            if (oct > 0) t8 >>= oct; else if (oct < 0) t8 <<= -oct;
            if (S->von) { t8 += A->err[ch]; A->err[ch] = (u8)(t8 & 255u); }   /* vibrato: error-diffused (2.37) */
            else t8 += 128u;                     /* 2.68: a steady shift: rounded, no dither (as the stock's own counts) */
            u32 t = t8 >> 8;
            u32 f16 = (((b2 + 1u) << 16) + tot / 2u) / tot;                       /* high share, Q16 */
            u32 h = (u32)(((unsigned long long)t * f16 + 32768u) >> 16), l;
            if (h < 1u) h = 1u; if (h > t - 1u) h = t - 1u;
            l = t - h;
            if (h > 0x10000u) h = 0x10000u; if (l > 0x10000u) l = 0x10000u;
            n1 = l - 1u; n2 = h - 1u;
        }
        *r1 = (unsigned short)n1; *r2 = (unsigned short)n2;
        A->w1[ch] = (unsigned short)n1; A->w2[ch] = (unsigned short)n2;
        __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
    }
    A->mod = (u8)on;
}
static void an_apply(int on, int c) { r27_t *S = r27(); S->von = (u8)(on != 0); S->vc = on ? c : 0; an_write(); }

/* ---- the plate in the effect modes (normal play) ----
   Gate = the plate reading stays >= P_ON for P_MS (off: < P_OFF for P_MS), or a keyboard-mode menu is open. The vibrato
   starts as the finger arrives (ATTACK_MS anti-click ramp; the approach is the fade-in). Depth = depth setting x fade x
   finger: the reading (smoothed adaptively: TAU_FAST for movement, TAU_SLOW for jitter) mapped P_ON..FULL_AT -> 0..full.
   Depth follows the finger up at once; down no faster than REL_MS, kept while off the pad (hold). A re-touch
   during the fade-out (or within LEGATO_MS after it) carries on. Menus audition at full depth, fade out in FADE_OUT_MS.
   Speed (glides): the speed setting (SPEED_MHZ, 13 steps) with the per-step swell SWELL_MHZ from light to full touch
   (menus: no swell). The reading is used at the 16x scale whatever gain the chip really has.
   Pitch = sample tune 0x20007606 (all sampled strum voices) + analog oscillators (Omni 1/2). */
static int sstep(int x) { int t = (x * x) >> 12; return (t * (12288 - 2 * x)) >> 12; }   /* smoothstep, Q12 */
static int smooth_to(int cur, int tgt, u32 dt, int tau) {
    int d = (tgt - cur) * (int)dt / (tau + (int)dt);
    if (d == 0 && cur != tgt && dt) d = tgt > cur ? 1 : -1;
    return cur + d;
}
#define MAN_MENU_MHZ 5000   /* 2.53: manual modes have no speed: their depth menu auditions at a flat 5 Hz */
static int rate_target(vib_t *v, int menu, int man) {
    if (menu && man) return MAN_MENU_MHZ;
    int f = (int)SPEED_MHZ[spd() + 6], sw = (int)SWELL_MHZ[spd() + 6];
    return menu ? f : f - sw + (v->ls * (2 * sw)) / 4096;          /* swell: light .. full touch */
}
static int chip_gain(void);
static fx_t *fx(void) {
    fx_t *F = (fx_t *)FXB;
    if (F->magic != FXMAGIC) { F->gain = 32768u; F->gmix = 32768u; F->pb_last = 8192u; F->pb_ms = 0; F->idle = 0; F->rpn = 1; F->r = 0; F->magic = FXMAGIC; }
    return F;
}
static u32 mroute(u32 ch) { u32 p = R8(SETBUF) - 1u; if (p > 4u) p = 0; return R8(ROUTE + p * 16u + ch); }   /* as mod 1 */
/* 2026-10-17 (the user: "both omnis should bend to match"): with Omni 1 / Omni 2 the vibrato also bends the analog chord
   oscillators (an_apply), so the bend goes out on the Chord part too (internal ch 1, routed by mod 1 like the chord notes).
   Each MIDI channel gets each bend once (Main, Sub and Chord may share one in the tool's grid). F->r = the Chord channel is
   bent: when the voice changes away from Omni 1 / 2 during a bend, it is centred with the next bend. */
static int pb_one(u32 ch, u32 v14) {
    u8 d[2] = { (u8)(v14 & 0x7Fu), (u8)(v14 >> 7) };
    mmsg_t m = { 7, (u8)ch, 2, d };
    return F_MHOOK(MIDIQ, &m) != 0u;
}
static int pb_send(u32 v14) {             /* pitch bend on Main + Sub (+ Chord with Omni 1 / 2); mod 1 routes them */
    fx_t *F = (fx_t *)FXB;
    u32 r0 = mroute(0u), r1 = mroute(1u), r3 = mroute(3u);
    int ok = pb_one(0u, v14);
    if (!(r3 == r0 && r0 <= 15u) && !pb_one(3u, v14)) ok = 0;   /* Main and Sub on one MIDI channel: one bend, not two */
    int omni = R8(DE89) <= 1u, chord = omni || F->r;
    if (chord && !(r1 <= 15u && (r1 == r0 || r1 == r3))) {      /* (on a channel already sent: nothing more) */
        u32 v = omni ? v14 : 8192u;                             /* the voice left Omni 1 / 2: the chord centred */
        if (pb_one(1u, v)) F->r = (u8)(v != 8192u); else ok = 0;
    } else if (chord) F->r = (u8)(omni && v14 != 8192u);
    return ok;
}
static int rpn_send(void) {               /* RPN 0 = 12 semitones + 0 cents, then RPN null, on each routed bend channel */
    static const u8 RPN[6][2] = { {101, 0}, {100, 0}, {6, 12}, {38, 0}, {101, 127}, {100, 127} };
    u32 r[3] = { mroute(0), mroute(3), mroute(1) }; int ok = 1;   /* Main, Sub, Chord (2026-10-17: the Omni chord bend) */
    for (u32 j = 0; j < 3u; j++) {
        if (r[j] > 15u || (j >= 1u && r[j] == r[0]) || (j == 2u && r[2] == r[1])) continue;
        for (u32 i = 0; i < 6u; i++) {
            u8 d[2] = { RPN[i][0], RPN[i][1] };
            mmsg_t m = { 4, (u8)r[j], 2, d };
            if (F_MPOST(MIDIQ, &m) == 0u) ok = 0;
        }
    }
    return ok;
}
/* on = the bend is live (vibrato mode, not a menu); c = pitch offset in 1/256 cent; vib = a vibrato mode is selected */
static void pb_update(fx_t *F, int on, int c, u32 dt, int vib) {
    u32 tgt = 8192u;
    if (on) { int b = 8192 + c * 2 / 75; if (b < 0) b = 0; if (b > 16383) b = 16383; tgt = (u32)b; }
    if (F->pb_ms < 60000u) F->pb_ms = (unsigned short)(F->pb_ms + dt);
    if (tgt != F->pb_last) {
        if ((tgt == 8192u || F->pb_ms >= PB_MS) && pb_send(tgt)) {
            F->pb_last = (unsigned short)tgt; F->pb_ms = 0; F->idle = 0;
            if (tgt == 8192u) F->rpn = 2;
        }
        return;
    }
    if (tgt != 8192u) return;
    if (F->idle < 60000u) F->idle = (unsigned short)(F->idle + dt);
    if (vib && (F->rpn == 1u || (F->rpn == 2u && F->idle >= RPN_IDLE)) && rpn_send()) F->rpn = 0;
}
u32 vib_tick(u32 dea1) {
    vib_t *v = (vib_t *)VB;
    fx_t *F = fx();
    if (!MODE2()) {                                   /* Note Off mode: nothing of ours runs; undo anything left over */
        if (v->magic == VMAGIC) {
            if (v->active) { TUNE16 = v->base; v->active = 0; an_apply(0, 0); }
            v->x = 0; v->gate = 0; v->hov = 0; v->hcnt = 0; v->off = 0xFFFFu;   /* the next touch in an effect mode starts fresh */
            v->lf = 0; v->pkr = 0; v->cout = 0; v->ls = 0; v->fast = 0;
        }
        F->gain = 32768u;
        if (F->pb_last != 8192u && pb_send(8192u)) F->pb_last = 8192u;
        return dea1;
    }
    u32 now = TICKS;
    if (v->magic != VMAGIC) {
        v->tick = now; v->phase = 0; v->x = 0; v->ls = 0; v->rate = 0; v->mgate = 0; v->off = 0xFFFFu;
        v->hcnt = 0; v->active = 0; v->gate = 0; v->fo = FADE_OUT_MS; v->hov = 0; v->lf = 0;
        v->home = 0; v->land = 0; v->fast = 0; v->fmin = 0; v->pkr = 0; v->cout = 0;
        v->magic = VMAGIC;
    }
    u32 dt = now - v->tick; v->tick = now;
    if (dt > 50u) dt = 50u;
    int kb = LED(0x15) != 0, menu = menu_on();
    u32 var = rvar(), dc = DEPTH_C[depth_idx()];
    int L = SIG14; if (L < 0) L = 0;
    L = L * 16 / chip_gain(); if (L > L_MAX) L = L_MAX;        /* 16x scale */
    {   int d = L * 256 - v->lf, ad = d < 0 ? -d : d;           /* adaptive smoothing of the reading */
        v->lf = smooth_to(v->lf, L * 256, dt, ad > JIT_Q8 ? TAU_FAST : TAU_SLOW); }
    const int rock = is_rock(var), trem = is_trem(var);
    {   int pk = v->pkr - (int)dt * ROCK_FDEC; v->pkr = pk > L * 256 ? pk : L * 256;   /* rock: quick-lift detector */
        if (v->pkr - L * 256 >= ROCK_FASTQ8) { v->fast = 1; v->fmin = (u8)L; }
        else if (v->fast) { if (L < v->fmin) v->fmin = (u8)L; else if (L >= P_OFF && L >= v->fmin + 8) v->fast = 0; } }
    if (!kb && !menu) {                                         /* plate gate: hysteresis + persistence */
        int flip = v->hov ? L < P_OFF : L >= P_ON;
        u32 need = (v->hov && !v->fast) ? HOLD_MS : P_MS;   /* roll-offs keep the touch */
        if (flip) { v->hcnt = (unsigned short)(v->hcnt + dt); if (v->hcnt >= need) { v->hov ^= 1u; v->hcnt = 0; } }
        else v->hcnt = 0;
    } else { v->hov = 0; v->hcnt = 0; }
    int gate = menu ? 1 : (!kb && v->hov);
    /* ---- start: fresh (after a pause) or legato ---- */
    if (gate && !v->gate) {
        int fresh = v->x == 0 && v->off > LEGATO_MS;
        if (fresh) { v->phase = 0; v->rate = 0; v->ls = 0; }
        else if (!menu) {                              /* legato: keep phase and speed; depth continues from what is */
            v->ls = v->x > 0 ? v->ls * sstep(v->x >> 4) / 4096 : 0;   /* audible now, then follows the new touch */
            v->x = 65536;
        }
        v->land = 0; v->fast = 0;
        v->mgate = (unsigned short)menu;               /* this gate is a menu audition (its end always fades in FADE_OUT_MS) */
    }
    /* ---- release: choose the fade-out ---- */
    if (!gate && v->gate) {
        int plate = !kb && !v->mgate;                  /* menus (also closed by leaving keyboard mode), keyboard mode switched on mid-touch: 0.88 s */
        int fo = FADE_OUT_MS;
        if (plate) fo = PLATE_FADE_MS;
        v->fo = (unsigned short)fo;
    }
    v->gate = (u8)gate;
    /* ---- fade ---- */
    if (gate) { v->x += (int)dt * 65536 / (menu ? FADE_IN_MS : ATTACK_MS); if (v->x > 65536) v->x = 65536; }
    else      { v->x -= (int)dt * 65536 / (int)v->fo; if (v->x < 0) v->x = 0; }
    if (v->x == 0) { if (v->off < 0xFFF0u) v->off = (unsigned short)(v->off + dt); } else v->off = 0;
    int env = sstep(v->x >> 4);
    /* ---- finger -> depth: up at once; down no faster than full -> 0 in REL_MS ---- */
    if (gate && !menu) {
        if (rock) {                                    /* rock: home */
            if (v->land < ROCK_LAND) { v->land = (unsigned short)(v->land + dt); v->home = v->lf; }
            else if (trem) v->home = v->lf < v->home ? v->lf : smooth_to(v->home, v->lf, dt, TREM_FLOOR_MS);   /* the floor */
            else if (L >= P_OFF) v->home = smooth_to(v->home, v->lf, dt, ROCK_HOME);
        }
        int n = 0, lq = v->lf, from = P_ON * 256, full = FULL_AT * 256;
        if (lq > from) { n = (lq - from) * 4096 / (full - from);   /* <= 16640 * 4096, fits */ if (n > 4096) n = 4096; }
        int y = (n + sstep(n)) >> 1;
        if (y >= v->ls) v->ls = y;
        else if (L >= P_OFF) { int lo = v->ls - (int)dt * 4096 / REL_MS; v->ls = y > lo ? y : lo; }   /* depth kept while off the pad */
    }
    if (menu) v->ls = 4096;                           /* menus audition at full depth; leaving one fades out from there */
    int depth = v->ls;
    /* ---- LFO ---- */
    int c = 0;
    if (v->x > 0) {
        int tgt = rate_target(v, menu, rock);
        if (v->rate == 0) v->rate = tgt; else v->rate = smooth_to(v->rate, tgt, dt, GLIDE_MS);
        v->phase += dt * (u32)v->rate * 4295u;         /* 2^32 / 1e6 per mHz*ms */
        int w = sinp(v->phase);                        /* -8192..8192 */
        int man = rock && !v->mgate;
        if (man) {                                     /* finger rock: the roll from home, no LFO */
            w = (v->lf - v->home) * 32 / (trem ? TREM_FULL : ROCK_FULL);   /* Q8 counts -> +-8192 at full */
            if (w > 8192) w = 8192; if (w < -8192) w = -8192;
            depth = 4096;
        }
        if (trem) {                                    /* tremolo: dip 0..8192 (auto: (1 + LFO) / 2; manual: the roll inward) */
            int u = man ? (w > 0 ? w : 0) : (w + 8192) / 2;
            c = ((u * env) / 4096) * depth / 4096;
        } else c = (((w * (int)dc / 32) * env) / 4096) * depth / 4096;   /* 1/256 cent */
    }
    if (rock && !v->mgate) { v->cout = smooth_to(v->cout, c, dt, ROCK_SLEW); c = v->cout; } else v->cout = c;
    /* ---- write the pitch ---- */
    int running = gate || v->x > 0, pitch = running && !trem;
    F->gain = (running && trem) ? (u32)(32768 - c * (int)TREM_PCT[depth_idx()] * 4 / 100) : 32768u;
    pb_update(F, pitch && (v->mgate == 0u || v->mgate == MENU_KVIB), c, dt, !trem);
    if (pitch) {
        if (!v->active) { v->active = 1; v->base = TUNE16; v->last = v->base; v->de72 = RS8(DE72); }
        short cur = TUNE16;
        if (cur != v->last || RS8(DE72) != v->de72) { v->base = cur; v->de72 = RS8(DE72); }   /* master tune changed */
        int cents = c >= 0 ? (c + 128) >> 8 : -((-c + 128) >> 8);
        short nv = (short)(v->base + cents);
        TUNE16 = nv; v->last = nv;
        if (R8(DE89) <= 1u) an_apply(1, c);           /* 2.37: analog vibrato only with Omni 1 / Omni 2 (analog main voice) */
        else an_apply(0, 0);                          /* other voices: the analog chord stays steady (exact base back) */
    } else if (v->active) {
        TUNE16 = v->base; v->active = 0;
        an_apply(0, 0);
    }
    return kb ? dea1 : (dea1 & ~0x40u);
}

/* ---------------- spoken prompts ----------------
   Clip blob in flash (stage A data): "OMPR", count, {offset, length} x count, 256 x s16 mu-law decode table, then 8 kHz
   mu-law bytes. mix_tail (entry +0x1C) replaces the mixer's final "b.w memcpy" at 0x6000E1B4: it adds the prompt into the
   block that is about to go to the codec (16 frames x {A = strum voices, B = drums}, int32), then does the copy.
   8 kHz -> 48 kHz = 6 output frames per sample, linear interpolation. Fixed level, after the digital volume knobs. */
#define PBLOB   0x60F40000u
#define PB_MAG  0x52504D4Fu      /* "OMPR" */
#define CL_108 0u
#define CL_84  1u
#define CL_MODE 2u
#define CL_NOFF 3u               /* 2.53: the user's recordings (audio/prompts.py) */
#define CL_VIB  4u
#define CL_TREM 5u
#define CL_AUTO 6u
#define CL_MAN  7u
#define CL_27   8u               /* 2.60: the user's "27" (audio/prompts.py) */
#define PSH_A  11                /* prompt level on channel A (strum path is sample << 13 at full knob) */
#define PSH_B  12                /* channel B (drum path << 14) */
#define GAP_FRAMES 1920u         /* 40 ms between the two words */
static int clip_get(u32 id, const u8 **p, u32 *len) {
    const volatile u32 *h = (const volatile u32 *)PBLOB;
    if (h[0] != PB_MAG || id >= h[1] || h[1] > 16u) return 0;
    u32 off = h[2 + 2*id], n = h[3 + 2*id];
    if (off < 8u + 8u*h[1] + 512u || off + n > 0x10000u || n < 2u) return 0;
    *p = (const u8 *)(PBLOB + off); *len = n;
    return 1;
}
static void prompt_req(u32 a, u32 b) {
    if (!cf(CF_SPOKEN)) return;          /* Spoken Mode Names off: silent, everything else as it is */
    pl_t *P = pl();
    P->qa = (u8)a; P->qb = (u8)b;
    __asm volatile ("dmb" ::: "memory");
    P->req = P->req + 1u;
}
static int pl_start(pl_t *P, u32 id) {     /* mixer: start clip id (0xFF = none) */
    const u8 *p; u32 n;
    if (id == 0xFFu || !clip_get(id, &p, &n)) { P->stage = 0; return 0; }
    P->p = p; P->len = n; P->pos = 0; P->k = 0; P->stage = 1;
    return 1;
}
static void prompt_mix(int *buf) {
    pl_t *P = (pl_t *)PLB;
    if (P->magic != PMAGIC) return;
    if (P->req != P->ack) { P->ack = P->req; __asm volatile ("dmb" ::: "memory"); P->nx = P->qb; pl_start(P, P->qa); }
    if (P->stage == 0) return;
    const volatile short *T = (const volatile short *)(PBLOB + 8u + 8u * ((const volatile u32 *)PBLOB)[1]);
    for (int f = 0; f < 16; f++) {
        if (P->stage == 2) {
            if (P->gap) { P->gap--; continue; }
            u8 nx = P->nx; P->nx = 0xFFu;
            if (!pl_start(P, nx)) return;
        }
        u32 i = P->pos;
        int s0 = T[P->p[i]], s1 = (i + 1u < P->len) ? T[P->p[i + 1u]] : 0;
        int vv = ((s0 * (6 - (int)P->k) + s1 * (int)P->k) * 10923) >> 16;    /* linear interpolation, /6 */
        if (++P->k == 6u) {
            P->k = 0;
            if (++P->pos >= P->len) {
                if (P->nx != 0xFFu) { P->stage = 2; P->gap = GAP_FRAMES; }
                else P->stage = 0;
            }
        }
        buf[2*f]     = __builtin_arm_qadd(buf[2*f],     vv << PSH_A);
        buf[2*f + 1] = __builtin_arm_qadd(buf[2*f + 1], vv << PSH_B);
        if (P->stage == 0) return;
    }
}
static void trem_mix(int *buf) {          /* 2.52: tremolo gain on the strum-voice bus (A), ramped across the 16 frames */
    fx_t *F = (fx_t *)FXB;
    if (F->magic != FXMAGIC) return;
    int g0 = (int)F->gmix, g1 = (int)F->gain;
    if (g1 > 32768) g1 = 32768; if (g1 < 0) g1 = 0;
    if (g0 == 32768 && g1 == 32768) return;
    for (int f = 0; f < 16; f++) {
        int g = g0 + (g1 - g0) * (f + 1) / 16;
        buf[2*f] = (int)(((long long)buf[2*f] * g) >> 15);
    }
    F->gmix = (u32)g1;
}
void mix_tail(void *dst, int *src, u32 n) {
    trem_mix(src);
    prompt_mix(src);
    F_MEMCPY(dst, src, n);
}

/* ---------------- power-on: rhythm mode, bass, jazz, LED blink, panel restore ---------------- */
/* rhythm_setup (entry +0x24) replaces "bl 0x6000B350" (stock rhythm-tone keys) at 0x6000F2A0, in the stock power-on set-up
   after the factory-reset check (boot_b2c4) and before the battery-type keys, the stock settings save and the start of the
   tasks. */
static void restore_early(void);
void rhythm_setup(void) {
    /* The stock rhythm-tone keys exactly as 0x6000B350 (swing/waltz wins over country/bossanova; DE66 = the stock settings
       save + its "setting changed" blink), then slow rock/funk = 27 mode. Only the winner blinks and is spoken, as stock;
       this runs after the stock factory-reset check, so a factory reset + a mode key gives that mode, as stock. */
    if (R8(KEY(44u) + 4u))      { R8(SETBUF + 4u) = 1u; R8(DE66) = 1u; R8(BOOTF) |= BF_P84; R8(R27) = 0u; }
    else if (R8(KEY(43u) + 4u)) { R8(SETBUF + 4u) = 0u; R8(DE66) = 1u; R8(BOOTF) |= BF_P108; R8(R27) = 0u; }
    else if (cf(CF_EXTRAS) && R8(KEY(42u) + 4u)) { R8(R27) = 1u; R8(BOOTF) |= BF_P27 | BF_BL11; R8(DE2C) = 1u; }   /* 27 mode: its LED blinks */
    /* The bass, apart from the mode keys: yellow VOICE = normal, yellow PATTERN = the alternate bass (remembered; both
       yellows = the stock factory reset = normal); the chosen yellow button's LEDs blink in the stock setting blink (DE2C:
       it replaces the startup LED animation; the panel goes live when it ends). Silent. */
    u32 ex = cf(CF_EXTRAS), yv = ex && R8(KEY(0x27u) + 4u), yp = ex && R8(KEY(0x2Du) + 4u);   /* Extras off: stock (nothing) */
    if (yv && !yp) { R8(BSTY) = 0u; R8(BOOTF) |= BF_YV; R8(DE2C) = 1u; }
    else if (yp && !yv) { R8(BSTY) = 2u; R8(BOOTF) |= BF_YP; R8(DE2C) = 1u; }
    R8(JAZZ) = (ex && R8(KEY(0x31u) + 4u)) ? JAZZ_ON : JAZZ_OFF;   /* KEYBOARD: jazz chords, this power-on only (no blink, silent) */
    if (IS27()) R8(SETBUF + 4u) = 1u;   /* 27 mode uses the Classic (OM-84) drum sounds: the stock copies SETBUF[4] to DE65
                                           next. RAM only - 27 mode itself is kept in our settings record. */
    restore_early();                    /* 2.78: the saved panel, before the panel is shown */
}
/* Around every call of the stock power-on blink routine (blink_wrap, entry +0x20, replaces "bl 0x60004EF0" at 0x6000EA9E).
   Rhythm tone (stock): the stock routine itself blinks swing (LED 13) for Classic, country (LED 12) for Normal.
   Slow rock/funk: no stock flag is set, so the stock routine only runs its timing; we set LED 11 when its phase changes.
   2.75: the bass choice the same way: yellow VOICE (normal bass) = both its LEDs 7 / 8, yellow PATTERN (alternate) = both
   its LEDs 0x0E / 0x0F, blinking together.
   The stock routine restores all LED flags at its end. */
#define BLPH   (VT + 11u*0x40u + 0x2Du)   /* last blink phase applied + 1 (0 = none) */
void blink_after(void) {
    u32 f = R8(BOOTF) & (BF_BL11 | BF_YV | BF_YP);
    if (!f) return;
    if (!R8(DE2C)) { R8(BOOTF) &= (u8)~(BF_BL11 | BF_YV | BF_YP); R8(BLPH) = 0; return; }
    u8 ph = R8(DE2F) ? 2u : 1u;
    if (ph == R8(BLPH)) return;
    R8(BLPH) = ph;
    int on = ph == 2u;
    if (f & BF_BL11) F_LEDSET(11, on);
    if (f & BF_YV) { F_LEDSET(7, on); F_LEDSET(8, on); }
    if (f & BF_YP) { F_LEDSET(0x0E, on); F_LEDSET(0x0F, on); }
}
/* 2.79: the end of the startup LED animation (0x60004758) and of the setting blink (0x60004EF0). Both copy LEDSAVE to the
   LED flags, then the stock code switched on its factory-default flags one by one (voice 1, bank 1, pattern bank 1, pattern
   1, Auto, Hold, Sync; 2, 7, 0x0E, 9, 0x12, 0x10, 0x13 off, 0x14) before setting KEYS_EN. On a stock unit LEDSAVE always holds
   exactly those defaults at that point (nothing writes it), so that was a no-op; with the saved panel in LEDSAVE
   (restore_early) it lit a second voice / bank / pattern LED, Auto next to Manual (the rhythm player reads the Auto flag:
   auto bass under a held Manual chord), Hold and Sync. Both routines now call this right after their copy (bl + return,
   patched in mkimg.py): the flags shown as copied, with the stock's own redraw (0x60003C14), then the panel live. */
void panel_shown(void) {
    F_LEDREST();
    R8(KEYS_EN) = 1u;
}

/* ---------------- keyboard mode: Gb 7th = plate mode (2.52, any mode); in the effect modes hold Gb minor = speed, hold Db minor = depth ----------------
   Gb minor = key 0x32 (leftmost minor button), Db minor = key 12; both are unused in keyboard mode. While Db minor is
   held: reference tone (as the stock P key: voice slot 12, A, note 0x51) with auto vibrato at the current depth, LEDs
   show the step like master tune (step 0 = -6). Up (key 3) / Down (key 2) change it, both = step 0. These two keys are
   hidden from the stock key handler while the menu is up (and until released), so the octave doesn't change. */
static u32 colbit(u32 k) { return 1u << (R8(KEYBITS + (k % 9u) * 16u + 8u) & 31u); }
/* 2.48: the 20 keyboard-mode note keys (key ids 13-35 but 15, 19, 22) */
#define NOTE_KEYS 0xFFFB76000ull
#define BLOCK_KEYS (NOTE_KEYS | 0xFF0ull)   /* + the drum pads, keys 4-11 */
static int block_held(void) {           /* 2.52: any keyboard note or drum pad held (stock key state) */
    for (u32 k = 4u; k <= 35u; k++) if (((BLOCK_KEYS >> k) & 1ull) && R8(KEY(k) + 4u)) return 1;
    return 0;
}
static int note_held(void) {
    for (u32 k = 13u; k <= 35u; k++) if (((NOTE_KEYS >> k) & 1ull) && R8(KEY(k) + 4u)) return 1;
    return 0;
}

/* ---------------- 2.60: 27 mode - PATTERN buttons ----------------
   In 27 mode the six PATTERN buttons (yellow 0x2D = LATIN, 0x28-0x2C = ROCK FOXTROT SLOW-ROCK WALTZ SWING) never reach the
   stock key handler: they are debounced here (from the raw matrix rows) and handled by r27_tick. KEYBOARD (0x31) is hidden
   from the stock handler only when it is pressed while two or more rhythm buttons are held (= lock the combination), until
   it is let go. Only once the panel is live (KEYS_EN), so the power-on keys are never touched. */
static const u8 K27[7] = { 0x2D, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x31 };
#define G_ACT    0x01u   /* a press is going on (some rhythm button held since the first press) */
#define G_MULTI  0x02u   /* two or more were held at some point */
#define G_LOCKED 0x04u   /* KEYBOARD locked the held combination */
#define G_LOCKRQ 0x08u   /* KEYBOARD went down over a combination (row_filter -> r27_tick) */
#define KB_BLINK_MS 300u /* 2.65: lock blink: 2 blinks (4 phases); 2.67: in 0.3 s (the user) */
#define KB_LEDBIT  0x10u /* 2.65: keyboard LED (flag 0x15) = LED port 6 bit 4 (stock 0x60003C14) */
static u32 pop6(u32 m) { u32 n = 0; for (u32 i = 0; i < 6u; i++) n += (m >> i) & 1u; return n; }
static r27_t *r27(void) {
    r27_t *S = (r27_t *)R7B;
    if (S->magic != R7MAGIC) {
        volatile u8 *b = (volatile u8 *)R7B;
        for (u32 i = 4; i < sizeof(r27_t); i++) b[i] = 0;
        S->magic = R7MAGIC;
    }
    return S;
}
static u32 r27_keys(u32 row, u32 bits) {
    if (!IS27() || !R8(KEYS_EN) || (row != 4u && row != 5u)) return bits;
    r27_t *S = r27();
    if (!(S->kinit & (1u << (row - 4u)))) {     /* 2.64: the first scan of this row since power-on: buttons already down are
                                                   power-on keys (mode / bass style): ignored until let go */
        S->kinit |= (u8)(1u << (row - 4u));
        for (u32 i = 0; i < 7u; i++) if (K27[i] / 9u == row && (bits & colbit(K27[i]))) S->ign |= (u8)(1u << i);
    }
    for (u32 i = 0; i < 7u; i++) {
        u32 k = K27[i];
        if (k / 9u != row) continue;
        u32 b = colbit(k), raw = bits & b, m = 1u << i;
        if (S->ign & m) { if (!raw) S->ign &= (u8)~m; bits &= ~b; continue; }   /* 2.64 */
        if (raw) { if (S->on[i] < 255u) S->on[i]++; S->off[i] = 0; }
        else     { if (S->off[i] < 255u) S->off[i]++; S->on[i] = 0; }
        if (i == 6u && raw && !S->kbhide && !(S->held & m) && pop6(S->held) >= 2u) { S->kbhide = 1; S->gest |= G_LOCKRQ; }
        if (!(S->held & m) && S->on[i] >= 2u) S->held |= (u8)m;
        else if ((S->held & m) && S->off[i] >= 20u) { S->held &= (u8)~m; if (i == 6u) S->kbhide = 0; }
        if (i < 6u || S->kbhide) bits &= ~b;
    }
    return bits;
}
/* The keyboard LED blink (2.65: a 27-mode lock was set; 2.80: the third Manual/Auto state was switched): twice in 0.3 s, any
   mode. Its port bit only: the flag (the keyboard-mode state) is never touched, and the LED ends as the flag says. */
static void kb_blink_start(r27_t *S) { S->kbl = 1u; S->kb0 = (unsigned short)TICKS; }
static void kb_blink_tick(void) {
    r27_t *S = (r27_t *)R7B;
    if (S->magic != R7MAGIC || !S->kbl) return;
    u32 el = (unsigned short)((unsigned short)TICKS - S->kb0), on = LED(0x15) != 0u;
    if (el >= KB_BLINK_MS) S->kbl = 0; else on ^= (u32)(((el * 4u / KB_BLINK_MS) & 1u) == 0u);
    u8 v = R8(LEDSH + 2u), nv = on ? (u8)(v | KB_LEDBIT) : (u8)(v & ~KB_LEDBIT);
    if (nv != v) { R8(LEDSH + 2u) = nv; F_F518(6u, (u32)(u8)~nv); }
}
/* ---------------- 2.79: the third Manual/Auto state: hold Manual/Auto, press KEYBOARD ----------------
   Manual/Auto (0x2F) and KEYBOARD (0x31) are both in matrix row 5. The stock takes a button on the first scan it reads
   down while armed (KEY+3: 30 scans up since its last press), so ma_keys notes the state just before (mpre) on that very
   scan. A KEYBOARD press (armed, so the stock would take it now) while Manual/Auto is down and the stock has taken this
   press of it (the state changed) is the gesture: KEYBOARD is hidden from the stock until it has been up for 20 scans (no
   keyboard mode), as the 27-mode lock, and ma_tick (control task, before the snapshot) switches: from the third state (the
   press left it to Auto) back to the state it was turned on from (mfrom; kept in the panel record); otherwise to the third
   state. Each further KEYBOARD press while Manual/Auto is still held toggles again. The switches are the stock button's own
   Manual / Auto branches (ma_manual / ma_auto) plus the Auto flag. */
#define MF_TOOK 0x01u    /* the stock took this press of Manual/Auto (mpre valid) */
#define MF_HIDE 0x02u    /* KEYBOARD hidden from the stock until let go */
#define MF_REQ  0x04u    /* gesture seen (ma_keys -> ma_tick) */
#define MF_DONE 0x08u    /* toggled during this press of Manual/Auto: each further KEYBOARD press toggles again */
#define MF_GO3  0x10u    /* 2.81: switching to the third state (ma_hold_on: no held MIDI bass note) */
#define GPIO2_DR_SET 0x401BC084u
#define GPIO2_DR_CLR 0x401BC088u
#define DE3B 0x2000DE3Bu
#define DE38 0x2000DE38u          /* stock: bass gate open */
#define DE64 0x2000DE64u
#define DE58 0x2000DE58u          /* stock: the held Manual chord's MIDI notes are on */
#define MA_KB 0x2000681Cu        /* the stock Manual/Auto branch's arguments (keyboard / strum note queue and its state) */
#define MA_KS 0x200068CCu
#define F_8FB0 ((void(*)(u32, u32))0x60008FB1)                  /* stock: auto bass note off */
#define F_8B0C ((void(*)(u32, u32, u32, u32, u32))0x60008B0D)   /* stock: start the held Manual chord (Manual only) */
#define F_8D38 ((void(*)(u32, u32))0x60008D39)                  /* stock: held Manual chord off */
#define F_96A0 ((u32(*)(u32, u32, u32, u32, u32))0x600096A1)    /* stock: MIDI note-on (internal ch, note, velocity, queue, state) */
#define F_96D4 ((u32(*)(u32, u32, u32, u32))0x600096D5)         /* stock: MIDI note-off (internal ch, note, queue, state) */
#define F_D8F8 ((u32(*)(void))0x6000D8F9)                       /* stock: 4 = no MIDI notes for the chord / bass */
#define HOLDB  0x20002ACBu       /* stock: the held Manual chord's MIDI bass note (AC8 + 3; 0 = none, the stock skips it then) */
/* 2.81: MIDI out in the third state = what it sounds like: the held Manual chord (stock, 0x200009DA = 0) and the auto bass
   line. The stock auto bass MIDI (0x60008E74 rest, 0x60008F20 / 0x60008F70 / 0x60008FE8 notes, 0x6000906C keyboard-mode stop)
   asks 0x6000D8D0 (= 0x200009DA, Auto) first: those five calls go to ma_bass_auto. The held chord's bass note (internal ch 2,
   AC8 + 3, from 0x60008B0C) is not sent in the third state (ma_hold_on, the note set to 0; the stock note-offs skip a 0, and
   0x60008B0C's own one goes through ma_hold_off). The held chord's 3 notes are the stock ones. */
u32 ma_bass_auto(void) { return R8(0x200009DAu) || ma_state() == MA_THIRD; }
u32 ma_hold_on(u32 ch, u32 note, u32 vel, u32 q, u32 st) {      /* replaces bl 0x600096A0 (ch 2, the bass) at 0x60008BAE */
    const r27_t *S = (const r27_t *)R7B;
    if (ma_state() == MA_THIRD || (S->magic == R7MAGIC && (S->mflag & MF_GO3))) { R8(HOLDB) = 0; return 1u; }
    return F_96A0(ch, note, vel, q, st);
}
u32 ma_hold_off(u32 ch, u32 note, u32 q, u32 st) {              /* replaces bl 0x600096D4 (ch 2, the bass) at 0x60008C02 */
    return note ? F_96D4(ch, note, q, st) : 1u;
}
static void ma_hold_drop(void) {         /* the held chord's MIDI bass note off (entering the third state) */
    u32 n = R8(HOLDB);
    if (n && R8(DE58) && F_D8F8() != 4u) F_96D4(2u, n, MA_KB, MA_KS);
    R8(HOLDB) = 0;
}
/* 2.84: the rhythm's chord notes off when Manual is pressed. The stock Manual branch (0x6000CF9C) clears 0x200009DA (Auto)
   first and then calls 0x60008E2C, which sends those note-offs only while Auto is set: so it never did, the held chord's
   note-ons (0x60008B0C, the same notes) went over them, and the rhythm's own clean-up a moment later (0x6000928C, DE3B)
   stopped them - the held Manual chord silent on MIDI until the next chord press (found by the 2026-10-16 soak test).
   ma_chord_off (entry +0x60, the stock branch's bl at 0x6000CFCC) is 0x60008E2C without the Auto test. */
#define DE55 0x2000DE55u          /* stock: the rhythm's chord notes are on (MIDI) */
void ma_chord_off(u32 q, u32 st) {
    if (!R8(DE55)) return;
    for (u32 i = 0; i < 3u; i++) if (F_D8F8() != 4u) F_96D4(1u, R8(0x20002AC8u + i), q, st);
    R8(DE55) = 0;
}
static void ma_manual(void) {            /* the stock button's Manual branch (0x6000CF9C), as it is (2.84: as patched) */
    F_LEDSET(0x11, 1); F_LEDSET(0x12, 0);
    R32(GPIO2_DR_CLR) = 0x400000u; R8(0x200009DAu) = 0;
    F_8FB0(MA_KB, MA_KS); ma_chord_off(MA_KB, MA_KS);
    R8(DE3B) = 1u;
    if (R8(DE64)) F_8B0C(R8(0x20000378u), R8(0x20000379u), R8(0x2000037Au), MA_KB, MA_KS);
}
static void ma_auto(void) {              /* the stock button's Auto branch (0x6000D2A4), as it is */
    F_LEDSET(0x11, 0); F_LEDSET(0x12, 1);
    R32(GPIO2_DR_SET) = 0x400000u; R8(0x200009DAu) = 1u;
    F_8D38(MA_KB, MA_KS);
}
static void ma_set(u32 t) {
    r27_t *S = r27();
    u32 c = ma_state();
    if (t == c) return;
    if (t == MA_AUTO) ma_auto();
    else if (c == MA_AUTO) {             /* -> Manual or the third state: the stock Manual branch first */
        if (t == MA_THIRD) S->mflag |= MF_GO3;
        ma_manual();
        S->mflag &= (u8)~MF_GO3;
        if (t == MA_THIRD) F_LEDSET(0x12, 1);
    } else if (t == MA_THIRD) {          /* Manual -> the third state */
        F_LEDSET(0x12, 1);
        ma_hold_drop();                  /* 2.81: MIDI: the auto bass line takes over from the held bass note */
    } else {                             /* the third state -> Manual: the Auto flag off, then what the stock rhythm player does
                                            each step without it (0x6000759A): bass gate shut, chord line as Manual */
        F_LEDSET(0x12, 0);
        R32(GPIO2_DR_CLR) = 0x10u; R8(DE38) = 0; R32(GPIO2_DR_CLR) = 0x400000u;
        F_8FB0(MA_KB, MA_KS);            /* 2.81: MIDI: the auto bass note off, and the held chord again with its bass (as the
                                            stock Manual branch does) */
        if (R8(DE64)) F_8B0C(R8(0x20000378u), R8(0x20000379u), R8(0x2000037Au), MA_KB, MA_KS);
    }
}
static u32 ma_keys(u32 bits) {           /* row_filter, row 5, once the panel is live */
    if (!R8(KEYS_EN) || !cf(CF_EXTRAS)) return bits;   /* Extras off: KEYBOARD stays stock */
    r27_t *S = r27();
    u32 bm = colbit(0x2Fu), bk = colbit(0x31u), m = bits & bm, k = bits & bk;
    if (m && R8(KEY(0x2Fu) + 3u)) {                              /* the stock takes it on this scan */
        S->mpre = (u8)ma_state(); S->mflag = (u8)((S->mflag & ~MF_DONE) | MF_TOOK);
    } else if (!R8(KEY(0x2Fu) + 4u)) S->mflag &= (u8)~(MF_TOOK | MF_DONE);              /* let go (stock: 30 scans up) */
    if (k && m && !(S->mflag & MF_HIDE) && R8(KEY(0x31u) + 3u) && (S->mflag & MF_TOOK)
        && ((S->mflag & MF_DONE) || ma_state() != S->mpre))
        S->mflag |= MF_HIDE | MF_REQ;
    if (S->mflag & MF_HIDE) {
        if (k) S->moff = 0; else if (++S->moff >= 20u) S->mflag &= (u8)~MF_HIDE;
        bits &= ~bk;
    }
    return bits;
}
static void ma_tick(void) {
    r27_t *S = r27();
    if (!(S->mflag & MF_REQ)) return;
    u32 back = S->mfrom == MA_MANUAL ? MA_MANUAL : MA_AUTO, t;
    if (S->mflag & MF_DONE) t = ma_state() == MA_THIRD ? back : MA_THIRD;   /* again, Manual/Auto still held */
    else if (S->mpre == MA_THIRD) t = back;                                  /* the press left the third state */
    else { S->mfrom = S->mpre; t = MA_THIRD; }
    S->mflag = (u8)((S->mflag & ~MF_REQ) | MF_DONE);
    ma_set(t);
    kb_blink_start(S);                   /* 2.80: the keyboard LED blinks twice, as for the 27-mode lock */
}
static u32 ma_rec(void) {                /* panel record [7] */
    u32 m = ma_state();
    return m == MA_THIRD ? (((const r27_t *)R7B)->magic == R7MAGIC && ((const r27_t *)R7B)->mfrom == MA_MANUAL ? 3u : 2u) : m;
}
u32 row_filter(u32 row, u32 bits) {
    bits |= inj_bits(row);
    if (R8(ST) == PH_EARLY) return bits;  /* 2.78: power-on restore (restore_early): the injected presses only */
    if (row == 5u) bits = ma_keys(bits);   /* 2.79 */
    bits = r27_keys(row, bits);           /* 27 mode's PATTERN buttons */
    ui_t *U = (ui_t *)UIB;
    if (U->magic != UMAGIC) return bits;
    if (row == 4u || row == 5u) {       /* a voice button while Gb 7th is held (mode selection) = that mode */
        static const u8 VK[5] = { 0x24, 0x25, 0x26, 0x33, 0x34 };   /* voice buttons 1-5 = Note Off, Vib Auto, Vib Manual, Trem Auto, Trem Manual */
        r27_t *S = r27();
        for (u32 i = 0; i < 5u; i++) {
            u32 k = VK[i]; if (k / 9u != row) continue;
            u32 b = colbit(k), m = 1u << i;
            /* 2.78: only while Gb 7th is really down (its raw bit, last scan): the stock keeps it "pressed" for 30 scans after it
               is let go, and a voice button pressed in those 30 ms is a sound selection, not a mode pick */
            if (bits & b) { S->voff[i] = 0; if (U->menu == MENU_MODE && R8(KEY(0x35u)) && !(S->vdn & m)) { S->vdn |= (u8)m; S->vsel = (u8)(i + 1u); } }
            else if ((S->vdn & m) && ++S->voff[i] >= 20u) S->vdn &= (u8)~m;
            if (S->vdn & m) bits &= ~b;   /* hidden from the stock until let go: the voice doesn't change */
        }
    }
    u32 cfg = U->menu && U->menu != MENU_KVIB;       /* 2.48: the keyboard effect is no menu: Up/Down stay stock (2.53: mode selection is) */
    /* 2.85: with no menu up and nothing hidden this loop changes nothing (a press neither hides nor sets hid; a release
       clears a bit already clear), so it is skipped: it was most of our work per control tick (the 2.84 LED lag) */
    if (row <= 3u && (cfg || U->hid))                 /* 2.49: menus block the keyboard notes; hidden until let go */
        for (u32 c = 0; c < 9u; c++) {
            u32 k = row * 9u + c;
            if (k < 4u || !(((NOTE_KEYS >> k) & 1ull))) continue;     /* 2.53: notes only - drum pads play, as stock T / P */
            u32 b = colbit(k), m = 1u << (k - 4u);
            if (bits & b) { if (cfg) U->hid |= m; if (U->hid & m) bits &= ~b; }
            else U->hid &= ~m;
        }
    if (row != 0u) return bits;
    for (u32 j = 0; j < 2u; j++) {               /* j 0 = key 2 (down), 1 = key 3 (up) */
        u32 b = colbit(2u + j), raw = bits & b;
        if (raw) { if (U->kon[j] < 255u) U->kon[j]++; U->koff[j] = 0; if (cfg) U->kmask[j] = 1; }
        else { if (U->koff[j] < 255u) U->koff[j]++; U->kon[j] = 0; }
        u32 bit = 1u << j;
        if (!(U->kst & bit) && U->kon[j] >= 2u) {
            U->kst |= (u8)bit;
            if (cfg) U->ev |= (U->kst & (bit ^ 3u)) ? EV_BOTH : (j ? EV_UP : EV_DN);
        } else if ((U->kst & bit) && U->koff[j] >= 20u) {
            U->kst &= (u8)~bit;
        }
        if (U->koff[j] >= 20u) U->kmask[j] = 0;   /* released (also clears a one-scan bounce seen during the menu) */
        if (U->kmask[j] || cfg) bits &= ~b;
    }
    return bits;
}
/* 2.38/2.39: the stock battery check (0x600081C0) redraws the power LED with 0x60003A5C many times a second, and that
   redraws the whole voice LED row: from the master-tune display buffer (MTBUF) if DE73 is set, else from the transpose
   display buffer (TRBUF) if DE78 is set, else from the LED flags. Both menus set DE78 = 1 / DE73 = 0 (as a stock
   transpose display): the speed menu draws its value with the transpose display (voice row, fills TRBUF); the depth menu
   draws with the master-tune display (pattern row; voice row dark but for the power LED) and copies that voice-row byte
   into TRBUF. DE73 is never set: with DE73 = 1 the stock key handler treats keys 4-11 as the P key (reference tone,
   DE7C = 1), which silences keyboard-mode notes until P is pressed (found in review of 2.39). The previous flag values
   are put back when the menu closes (in keyboard mode). */
#define MTBUF 0x20000108u
#define TRBUF 0x2000010Cu
#define DE73 0x2000DE73u
#define DE78 0x2000DE78u
static void show_menu(ui_t *U) {
    if (U->menu == MENU_SPEED) { U->disp = (s8)spd(); F_LEDDISPT(&U->disp); }   /* voice LED row, as transpose */
    else { U->disp = (s8)((int)depth_idx() - 6); F_LEDDISP(&U->disp);           /* pattern LED row, as master tune */
           R8(TRBUF) = R8(MTBUF); }      /* 2.39: voice row as the master-tune display leaves it (dark but for the power LED) */
    U->redraw = 0;
}
static void menu_enter(ui_t *U, u32 which) {
    U->menu = (u8)which; U->ev = 0;
    if (which == MENU_KVIB) return;      /* 2.48: no test tone, no display, nothing stock touched */
    U->pad = (u8)(0x80u | (R8(DE78) ? 1u : 0u) | (R8(DE73) ? 2u : 0u));
    R8(DE73) = 0u; R8(DE78) = 1u;        /* both menus: the refresh redraws the voice row from the transpose display buffer */
    F_E850(1u); F_ALLOFF(); F_E7DC(12u); F_NOTEON(0u, 0x51u);   /* same sequence as the stock P key (0x6000C22C) */
    show_menu(U);
}
static void menu_exit(ui_t *U) {
    u32 was = U->menu;
    U->menu = 0; U->ev = 0;
    if (was == MENU_KVIB) return;
    if ((U->pad & 0x80u) && LED(0x15)) { R8(DE78) = (u8)(U->pad & 1u); R8(DE73) = (u8)((U->pad >> 1) & 1u); }
    U->pad = 0;                         /* closed by leaving keyboard mode: the stock code has already cleared DE78/DE73 */
    F_LEDREST(); F_E850(1u); F_ALLOFF();                          /* as the stock P release (0x6000C1DC) */
    F_E7DC(R8(DE89) + (LED(0x15) ? 12u : 0u));
}
/* ---- 2.53 modulation-mode selection (Gb 7th, keyboard mode, every mode) ----
   Press: the voice row shows the current mode on its 5 green LEDs (1 Note Off .. 5 tremolo manual; bank LEDs off), the
   pattern row goes dark (the Hold LED, on the same LED port, keeps its state). While held, Up (Bb major, key 3) / Down
   (Eb major, key 2) step the mode at once, wrapping; both = Note Off; silent. Release: if no arrow was used, the next mode
   (spoken); then the mode's LED blinks 3 times over BLINK_MS and the normal LEDs come back. The display uses the stock
   transpose-display path (DE78 + TRBUF, as the speed menu) so the battery-check refresh keeps it. Ignored while a menu /
   the keyboard effect is up or a keyboard note / drum pad is held; while held, keyboard notes are blocked (drums play)
   and Up / Down never reach the stock octave code (row_filter, as in the menus). */
#define MTB      0x20002BF0u             /* mt_t: free since 2.38 (after fx_t) */
#define MTMAGIC  0x4D543533u
typedef struct { u32 magic, t0; u8 used, blink, lock, pad, ph, r[3]; } mt_t;
_Static_assert(sizeof(mt_t) <= 0x20002C08u - MTB, "mt_t too big");
#define BLINK_MS  800u                   /* 3 blinks: 6 equal on / off phases */
static mt_t *mt(void) {
    mt_t *M = (mt_t *)MTB;
    if (M->magic != MTMAGIC) { M->t0 = 0; M->used = 0; M->blink = 0; M->lock = 0; M->pad = 0; M->ph = 0xFFu; M->magic = MTMAGIC; }
    return M;
}
static u32 cur_mode(void) { return MODE2() ? rvar() : 0u; }   /* 0 Note Off, 1-4 */
static void set_mode(u32 nv) {          /* switch, keeping each mode's own depth / speed */
    par_sync();
    if (nv) { R8(VMODE) = (u8)nv; R8(MODEV) = (u8)((R8(MODEV) & 0x40u) | 1u | ((u32)R8(PDEP + nv - 1u) << 2)); R8(SPD) = R8(PSPD + nv - 1u); }
    else R8(MODEV) &= (u8)~1u;
}
static void mode_say(u32 m) {
    static const u8 W1[5] = { CL_NOFF, CL_VIB, CL_VIB, CL_TREM, CL_TREM }, W2[5] = { 0xFFu, CL_AUTO, CL_MAN, CL_AUTO, CL_MAN };
    prompt_req(W1[m], W2[m]);
}
static void mode_show(u32 m, int on) {
    R8(DE73) = 0u; R8(DE78) = 1u;
    R8(TRBUF) = on ? (u8)(2u << m) : 0u;                  /* bits 1-5 = the 5 voice LEDs, 6 / 7 = bank LEDs (off) */
    F_3A5C(R8(0x200000F0u + 1u));                          /* voice row (+ the power LED as it is) */
    u8 pr = LED(0x10) ? 0x80u : 0u;                       /* pattern port: everything off but the Hold LED */
    R8(LEDSH + 1u) = pr; F_F518(5u, (u32)(u8)~pr);
}
static void mode_begin(mt_t *M) {       /* save the stock display flags (unless a blink already holds them) */
    if (!M->blink) M->pad = (u8)(0x80u | (R8(DE78) ? 1u : 0u) | (R8(DE73) ? 2u : 0u));
    M->blink = 0;
}
static void mode_end(mt_t *M) {         /* stock display flags back, LEDs redrawn from their flags */
    if ((M->pad & 0x80u) && LED(0x15)) { R8(DE78) = (u8)(M->pad & 1u); R8(DE73) = (u8)((M->pad >> 1) & 1u); }
    M->pad = 0; M->blink = 0;
    F_LEDREST();
}
static void ui_tick(void) {
    ui_t *U = ui();
    int kb = LED(0x15) != 0, m2 = MODE2() != 0;
    /* startup prompt after the rhythm toggle: once the power-on blink is over and the panel is live */
    if (R8(BOOTF) & (BF_P108 | BF_P84 | BF_P27)) {
        if (!R8(DE2C) && R8(KEYS_EN)) {
            if (++U->bootwait > 250u) {
                prompt_req((R8(BOOTF) & BF_P84) ? CL_84 : (R8(BOOTF) & BF_P27) ? CL_27 : CL_108, CL_MODE);
                R8(BOOTF) &= (u8)~(BF_P108 | BF_P84 | BF_P27);
            }
        }
    }
    par_sync();
    mt_t *M = mt();
    int dk = R8(KEY(12u) + 4u) != 0, sk = R8(KEY(0x32u) + 4u) != 0;
    if (!kb) {                                   /* left keyboard mode: mode selection / blink end (stock redraws) */
        if (U->menu == MENU_MODE) { U->menu = 0; U->ev = 0; }
        if (M->blink || M->pad) { M->pad = 0; M->blink = 0; F_LEDREST(); }
    }
    /* Gb 7th (key 0x35): press = mode selection display; release = next mode unless an arrow was used */
    u8 gb = R8(KEY(0x35u) + 4u) ? 1u : 0u;
    if (gb && !U->gbm && kb && cf(CF_MOD) && !U->menu && !block_held() && !dk && !sk) {   /* Modulation Modes off: stock */
        mode_begin(M); U->menu = MENU_MODE; U->ev = 0; U->redraw = 0; M->used = 0;
        mode_show(cur_mode(), 1);
    }
    U->gbm = gb;
    if (U->menu == MENU_MODE) {
        u8 ev = U->ev; U->ev = 0;
        r27_t *S7 = r27(); u32 vs = S7->vsel; S7->vsel = 0;
        if (vs) {                                /* a voice button: that mode, silently (as the arrows) */
            u32 n = vs - 1u;
            if (n != cur_mode()) set_mode(n);
            M->used = 1; mode_show(n, 1); U->redraw = 0;
        } else if (ev) {
            u32 c = cur_mode(), n = (ev & EV_BOTH) ? 0u : (ev & EV_UP) ? (c + 1u) % (NVAR + 1u) : (c + NVAR) % (NVAR + 1u);
            if (n != c) set_mode(n);
            M->used = 1; mode_show(n, 1); U->redraw = 0;
        } else if (++U->redraw >= 100u) { mode_show(cur_mode(), 1); U->redraw = 0; }
        if (!gb) {                               /* released */
            if (!M->used) { u32 n = (cur_mode() + 1u) % (NVAR + 1u); set_mode(n); mode_say(n); }
            U->menu = 0; U->ev = 0; M->blink = 1; M->t0 = TICKS; M->lock = (u8)(dk || sk); M->ph = 0xFFu;
        }
    }
    /* 2.78: a panel button pressed during the blink ends it at once, so its own LED shows (the blink owns the voice row) */
    if (M->blink) for (u32 k = 0x24u; k <= 0x34u; k++)
        if (k != 0x31u && k != 0x32u && R8(KEY(k) + 4u)) { mode_end(M); break; }
    if (M->blink) {
        u32 el = TICKS - M->t0;
        if (el >= BLINK_MS) mode_end(M);
        else { u32 ph = el * 6u / BLINK_MS; if (ph != M->ph || ++U->redraw >= 100u) { M->ph = (u8)ph; U->redraw = 0; mode_show(cur_mode(), (ph & 1u) == 0u); } }
    }
    /* hold Db minor (key 12) = depth menu, hold Gb minor (key 0x32) = speed menu (effect modes; 2.53: in the manual modes
       only the depth menu, at a flat 5 Hz, and no keyboard effect). A minor button held through the mode selection does
       nothing until let go (lock). */
    if (M->lock && !dk && !sk) M->lock = 0;
    u32 want = 0u;
    if (kb && m2 && U->menu != MENU_MODE) {
        int man = is_rock(rvar());
        if (U->menu == MENU_DEPTH && dk) want = MENU_DEPTH;
        else if (U->menu == MENU_SPEED && sk) want = MENU_SPEED;
        else if (U->menu == MENU_KVIB && (dk || sk)) want = MENU_KVIB;   /* until both are let go */
        else if (!U->menu && (dk || sk) && !M->lock)
            want = note_held() ? (man ? 0u : MENU_KVIB) : (dk ? MENU_DEPTH : (man ? 0u : MENU_SPEED));
    }
    if (U->menu != MENU_MODE) {
        if (want && !U->menu) { if (M->blink) mode_end(M); menu_enter(U, want); }
        else if (!want && U->menu) menu_exit(U);
    }
    if (U->menu == MENU_DEPTH || U->menu == MENU_SPEED) {
        u8 ev = U->ev; U->ev = 0;
        if (ev) {
            if (U->menu == MENU_SPEED) {
                int x = spd();
                if (ev & EV_BOTH) x = 0;
                else if ((ev & EV_UP) && x < 6) x++;
                else if ((ev & EV_DN) && x > -6) x--;
                R8(SPD) = (u8)(s8)x;
            } else {
                u32 d = depth_idx();
                if (ev & EV_BOTH) d = DEPTH_MID;          /* 2.53: both = centre, as stock transpose / master tune */
                else if ((ev & EV_UP) && d < 12u) d++;
                else if ((ev & EV_DN) && d > 0u) d--;
                R8(MODEV) = (u8)((R8(MODEV) & 0x41u) | (d << 2));
            }
            show_menu(U);
        } else if (++U->redraw >= 100u) show_menu(U);   /* keep the display up if something redrew the LEDs */
    }
}
/* ---------------- Instant OFF gain boost (effect modes only) ----------------
   The touch chip (onsemi LC717A10PJ, LPSPI3) is driven only by the stock touch task 0x6000FA1C. touch_hook (entry +0x2C)
   replaces its "bl 0x6000F90C" at 0x6000FB76 (after it has read the 15 levels): it calls F90C (stock palm check), then,
   in the effect modes only, makes sure the Instant OFF channel (Cin14) has 16x gain (reg 0x09 = 0xFF; the high nibble is
   the unused Cin15) and touch threshold 40 (reg 0x18; stock 20 at 8x = the same touch). It does this by replaying the
   stock chip set-up 0x6000ED18 transfer for transfer with just those two values changed (tested on the unit as
   2.35dev2 experiment 5). Note Off mode: nothing is written, the chip stays exactly as the stock set-up left it.
   Chip registers are volatile: every power-on the stock set-up runs first. About every CHK_CYC cycles reg 0x09 is read
   back; if it reads wrong twice in a row (the stock recovery routine 0x6000F858 re-set the chip), the boost is applied
   again - but only while no plate is touched, since applying includes the chip's static calibration. A chip that does
   not take it gets at most MAX_TRIES consecutive attempts per power-on, so it can never cause repeated re-calibration.
   The gain really in the chip is kept for vib_tick, which scales the reading to the 16x range whatever it is.
   Write = tx {reg, value}; read = tx {reg|0x80, 0}, second of two reads kept (as the stock code). */
#define F_SPIX  ((int(*)(u32,u32))0x60012889)      /* stock LPSPI blocking transfer (handle, xfer) */
#define F_DLY   ((void(*)(u32))0x60009261)          /* stock busy-wait loop used by the chip set-up */
#define SPIH    0x2000AFA8u
#define SPIXF   0x2000AF98u                         /* {tx, rx, len, flags} */
#define SPITX   0x2000AF94u
#define SPIRX   0x2000AF90u
#define PALM    0x200009E0u                         /* stock: value last written to reg 0x33 by the palm check (1 / 0xFE) */
#define F_F90C  ((void(*)(void*))0x6000F90D)
#define CSB     0x20002BD4u                         /* chip state, touch task only (stock page buffer, power-on/off only) */
#define CMAGIC  0x43483336u
typedef struct { u32 magic; u8 g09, tries, miss; u8 cnt; } cs_t;
#define DEA0 0x2000DEA0u                            /* stock touch masks (Cin0-7, Cin8-15), read this cycle */
#define DEA1 0x2000DEA1u
#define BOOST_G   0xFFu                             /* reg 0x09: Cin14 (and unused Cin15) 2nd gain code 15 = 16x */
#define BOOST_T   40u                               /* reg 0x18: Cin14 touch threshold */
#define CHK_CYC   250u                              /* read 0x09 back about once a second (chip ~250 measurements/s) */
#define MAX_TRIES 3u
#define POLL_MAX  5000u
static const u8 SETUP[38][2] = {                    /* the stock set-up (0x6000ED18), in order */
    {0x00,0xFE},{0x01,0x7F},{0x02,0x77},{0x03,0x77},{0x04,0x77},{0x05,0x77},{0x06,0x77},{0x07,0x77},{0x08,0x77},{0x09,0x77},
    {0x0B,20},{0x0C,20},{0x0D,20},{0x0E,20},{0x0F,20},{0x10,20},{0x11,20},{0x12,20},{0x13,20},{0x14,20},{0x15,20},{0x16,20},
    {0x17,20},{0x18,20},
    {0x30,0x08},{0x31,0x0C},{0x32,0x00},{0x33,0x01},{0x34,0x05},{0x35,0x05},{0x36,0x00},{0x37,0x00},{0x38,0x00},{0x39,0x40},
    {0x3A,0x98},{0x3B,0x00},{0x3C,0x18},{0x3D,0x00} };
static int chip_gain(void) {             /* the Instant OFF gain the chip really has (8 = stock / unknown) */
    const cs_t *C = (const cs_t *)CSB;
    if (C->magic != CMAGIC) return 8;
    int g = (int)(C->g09 & 15u) + 1;
    return g < 8 ? 8 : g;
}
static u8 spi_rw(u32 b0, u32 b1) {      /* one 2-byte transfer through the stock routine; returns rx[1] */
    volatile u32 *x = (volatile u32 *)SPIXF;
    *(volatile unsigned short *)SPITX = (unsigned short)((b0 & 0xFFu) | ((b1 & 0xFFu) << 8));
    x[0] = SPITX; x[1] = SPIRX; x[2] = 2u; x[3] = 0x500000u;
    F_SPIX(SPIH, SPIXF);
    return *(volatile u8 *)(SPIRX + 1u);
}
static u8 spi_rd(u32 r) { spi_rw(r | 0x80u, 0); return spi_rw(r | 0x80u, 0); }
static void chip_set(u32 g09, u32 t18) {   /* touch task: replay the stock set-up with reg 0x09 / 0x18 = these */
    spi_rw(0x2F, 0x88); F_DLY(0x320u); spi_rw(0x2F, 0x88);          /* halt, as the stock set-up */
    u32 polls = 0;
    while (spi_rd(0x2F) & 0x86u) { if (++polls >= POLL_MAX) { spi_rw(0x2F, 0x8F); return; } }   /* restart, settings unchanged */
    u32 palm = R8(PALM); if (palm != 0xFEu) palm = 1u;
    for (u32 k = 0; k < 38u; k++) {
        u32 r = SETUP[k][0], v = SETUP[k][1];
        if (r == 0x09u) v = g09;
        else if (r == 0x18u) v = t18;
        else if (r == 0x33u) v = palm;                              /* keep the palm check's current setting */
        spi_rw(r, v);
    }
    spi_rw(0x2F, 0x8F);                                             /* load parameters + static calibration + measure */
}
static void boost_apply(void) { chip_set(BOOST_G, BOOST_T); }
void touch_hook(void *arg) {             /* touch task, replaces bl 0x6000F90C at 0x6000FB76 */
    F_F90C(arg);
    cs_t *C = (cs_t *)CSB;
    if (!MODE2()) {                              /* Note Off mode: the chip stays stock - 2.52: put back if we changed it */
        if (C->magic != CMAGIC || C->g09 == 0x77u) return;          /* never touched this power-on, or stock again */
        if (C->tries >= MAX_TRIES) return;
        if (R8(DEA0) || R8(DEA1) || SIG14 >= 8) return;            /* only while nothing is touched (static calibration) */
        C->tries++; chip_set(0x77u, 20u);
        u8 g = spi_rd(0x09); C->g09 = g;
        if (g == 0x77u) { C->tries = 0; C->miss = 1; C->cnt = (u8)CHK_CYC; }   /* an effect mode re-applies at once */
        return;
    }
    if (C->magic != CMAGIC) { C->g09 = 0x77u; C->tries = 0; C->miss = 1; C->cnt = (u8)CHK_CYC; C->magic = CMAGIC; }  /* at power-on: apply at once */
    if (C->cnt < CHK_CYC) { C->cnt++; return; }
    if (C->miss < 2u) {                          /* periodic check read */
        u8 g = spi_rd(0x09);
        if (g == BOOST_G) { C->g09 = g; C->miss = 0; C->cnt = 0; return; }
        C->g09 = g;
        if (++C->miss < 2u) { C->cnt = 0; return; }   /* one wrong read: check again next time */
    }
    if (C->tries >= MAX_TRIES) { C->cnt = 0; return; }   /* the chip doesn't take it: leave it alone */
    if (R8(DEA0) || R8(DEA1) || SIG14 >= 8) return;      /* wait (re-check every cycle) until nothing is touched */
    C->tries++; boost_apply();
    u8 g = spi_rd(0x09); C->g09 = g;
    if (g == BOOST_G) { C->tries = 0; C->miss = 0; }   /* taken: tries count consecutive failures only */
    C->cnt = 0;
}

/* ================= 2.60: 27 mode - the OM-27 rhythm engine =================
   The OM-27 (1981): GI AY-5-1315-001 rhythm chip (32-step counter; each of the 6 rhythm buttons enables its ROM section,
   held buttons OR their outputs; the counter resets after the last step of any held rhythm, so the shortest wins) and
   AY-5-1317A chord chip (bass note from a 2-bit code: root / fifth / third; held rhythms OR the codes, so two different
   notes on one step give the third). Drums: BD, SD, HH, CY, CL; SD and HH share one noise trigger and CY covers HH.
   Tables below: audio/om27/gen_tables.py from the user's recordings (2026-10-14); every combination recorded matched.
   The stock rhythm player keeps doing all the work (timing, Classic drum sounds, chord gate, bass, MIDI): 27 mode only
   feeds it values. Host pattern: DE48 stays 0 (rock 1: 384 steps, normal step time, every row +-80 on each 3rd step,
   which the player reads back for its MIDI note-off flags). One OM-27 step = 3 stock steps = a 16th at the shown tempo
   (the OM-27 ran every rhythm on one step clock: swing and slow rock are 24 steps, so they run 4/3 as fast, as on it).
   - r27_step (rhythm interrupt, once per stock step, before the rows): on each 3rd stock step works out the OM-27 step;
   - r27_row / r27_bass (the player's row and bass calls): give the player our value instead of the pattern byte.
   108 / 84 mode: all three pass straight through. */
static const u8 L27[6] = { 32, 32, 32, 24, 24, 24 };
static const u8 P27[6][32] = {
    { 0x6B, 0x00, 0x04, 0x04, 0x24, 0x00, 0x02, 0x00, 0xC9, 0x00, 0x24, 0x04, 0x92, 0x00, 0x35, 0x00, 0x49, 0x00, 0x24, 0x04, 0x06, 0x00, 0x60, 0x00, 0x89, 0x00, 0x26, 0x04, 0x32, 0x00, 0x95, 0x00 },   /* latin */
    { 0x45, 0x00, 0x04, 0x00, 0x22, 0x00, 0x65, 0x00, 0x85, 0x00, 0x04, 0x00, 0x22, 0x00, 0x02, 0x00, 0x45, 0x00, 0x04, 0x00, 0x22, 0x00, 0x65, 0x00, 0x85, 0x00, 0x04, 0x00, 0xE2, 0x00, 0x82, 0x00 },   /* rock */
    { 0x4B, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x89, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x49, 0x00, 0x00, 0x00, 0x22, 0x00, 0x02, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x22, 0x02, 0x02, 0x02 },   /* foxtrot */
    { 0x65, 0x00, 0x24, 0x04, 0x24, 0x00, 0x22, 0x00, 0x24, 0x00, 0xE5, 0x00, 0xA5, 0x00, 0x24, 0x04, 0x24, 0x00, 0xE2, 0x00, 0x24, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* slowrock */
    { 0x49, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x89, 0x00, 0x00, 0x02, 0x22, 0x00, 0x00, 0x00, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* waltz */
    { 0x49, 0x00, 0x00, 0x22, 0x00, 0x24, 0x89, 0x00, 0x00, 0x34, 0x00, 0x14, 0x49, 0x00, 0x00, 0x22, 0x00, 0x24, 0x89, 0x00, 0x00, 0x34, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* swing */
};
#define B_BD 1u
#define B_SD 2u
#define B_HH 4u
#define B_CY 8u
#define B_CL 16u
#define B_CH 32u
#define LATIN_LED 0x0Eu          /* 2.61: the yellow button's top LED (in line with the other pattern LEDs; the user) */
#define BASS_MAX 4u              /* 2.61: the bass note value: this many OM-27 steps, or up to the next note (the OM-27 in the demo
                                    video: every bass note fades for about 4 steps, then is cut; a new note cuts it sooner) */
/* 2.63: 27-mode bass = the OM-84 rock programming: the gate opens on each new note and the stock Classic-tone tick closes it
   after 50 ms; the OM-108's analog bass then dies away on its own. 2.64: with a pluck style (bsty) the gate is held instead
   (27 mode: BASS_MAX or to the next note; 84 mode: the pattern's note length, as the 108 mode) and the pluck shapes it. */
#define DE34 0x2000DE34u         /* stock: Classic-tone bass gate tick count (the tick cuts the bass gate at 0xFA = 50 ms; above, never) */
#define DE33 0x2000DE33u         /* stock: chord gate tick count (the tick cuts the chord gate at 0x46 = 14 ms; above 0x46 it never does) */
#define STEPLEFT 0x20002AA0u     /* stock: rhythm ticks left before the next stock step (u32) */
/* 2.64: the pluck bass (2.62's idea, reworked against the thump). The bass voice is a square wave from QTimer1 channel 3 (stock
   0x60008E74: SetupPwm, 50 % duty) behind an on/off gate (GPIO2 bit 4); the firmware has no level control. A narrower
   pulse has a weaker fundamental (sin(pi d)), so the duty d falls during each note - but d is also the wave's average, and
   a fast change of it is the thump. 2.62 jumped (the steep start of its curve, and back to 50 % at once when the gate
   closed). Now the duty target falls exponentially from 50 % on each note (PK: time constant, floor), goes back to 50 %
   while the gate is closed, and the duty follows the target through two smoothing stages (PK: 1024 / k ms each; four times
   as slow while closed), so it never steps. A new pitch (stock SetupPwm = 50 %) gets our duty back at once. Period (pitch)
   never changed. Runs every control tick (pluck_tick) in any mode, only with bsty() != 0. */
#define QT1C3_LD1 0x401DC070u     /* QTimer1 channel 3 CMPLD1 (low time - 1), CMPLD2 (high time - 1) at +2 */
#define BVAL 0x200009D5u          /* stock: the bass value now playing */
typedef struct { unsigned short f, floor, k; } pk_t;
static const pk_t PK = { 64453u, 1024u, 85u };   /* the alternate bass (2.64 "strong"): exp(-1 / 60 ms), floor 1/64 (-26 dB),
                                                     smoothing 12 ms. (2.64-2.72 soft: { 65033, 5243, 41 }: 130 ms, -12 dB, 25 ms) */
static void bass_duty(u32 q) {             /* set the bass square's high time to q / 65536 of its period, period unchanged */
    u32 pm; __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(pm) :: "memory");
    volatile unsigned short *ld = (volatile unsigned short *)QT1C3_LD1;
    u32 per = (u32)ld[0] + ld[1] + 2u;
    if (per >= 64u) {                        /* only once the stock code has set the timer up */
        u32 hi = (per * q) >> 16;
        if (hi < 2u) hi = 2u;
        if (hi > per - 2u) hi = per - 2u;
        ld[0] = (unsigned short)(per - hi - 1u); ld[1] = (unsigned short)(hi - 1u);
    }
    __asm volatile("msr primask, %0" :: "r"(pm) : "memory");
}
#define HIT  80                  /* the stock patterns' velocity */
#define BK_OFF  0u               /* bass value kinds: 127 = off (stock rest), note */
#define BK_NOTE 2u
#define DE0E 0x2000DE0Eu         /* stock step 0-383 (u16) */
#define DE47 0x2000DE47u         /* rhythm running */
#define DE49 0x2000DE49u         /* chord type 0-8 (bass row = 19 + type) */
#define F_7574 ((void(*)(u32, const volatile s8 *))0x60007575)        /* stock row player */
#define F_7B34 ((void(*)(u32, const volatile s8 *, u32))0x60007B35)   /* stock bass player */
static const u8 THIRD[9] = { 5, 4, 5, 5, 4, 4, 5, 6, 3 };   /* bass values (1 = root) per chord type, from the stock bass rows */
static const u8 FIFTH[9] = { 8, 8, 8, 8, 8, 7, 9, 8, 8 };
static u32 frame27(u32 m, u32 c) {         /* the chip's outputs at count c with rhythms m held */
    u32 f = 0;
    for (u32 r = 0; r < 6u; r++) if (((m >> r) & 1u) && c < L27[r]) f |= P27[r][c];
    return f;
}
static u32 next27(u32 m, u32 c) {          /* count after c: reset after the last step of any held rhythm, else +1 (5 bits) */
    for (u32 r = 0; r < 6u; r++) if (((m >> r) & 1u) && c + 1u == L27[r]) return 0;
    return m ? (c + 1u) & 31u : 0u;
}
void r27_step(void) {
    if (!r27_ready()) return;
    r27_t *S = (r27_t *)R7B;
    if (S->rst) {                                   /* 2.61: a fresh press starts the rhythm from the top, as the OM-27 does */
        S->rst = 0; S->c = 0; S->bleft = 0;
        *(volatile unsigned short *)DE0E = 0;      /* this stock step = step 0 (tempo LED and beat in line, as at start) */
    }
    u32 sub = *(volatile unsigned short *)DE0E % 3u, m = S->play & 0x3Fu;
    if (sub == 0u) {
        u32 c = S->c & 31u, f = frame27(m, c), d = f & 0x1Fu;
        if (d & (B_SD | B_CY)) d &= ~B_HH;          /* one noise trigger: a snare or cymbal covers the hi-hat */
        S->hits = (u8)d; S->chord = (f & B_CH) ? 1u : 0u;
        if (f >> 6) { S->bcode = (u8)(f >> 6); S->bleft = (u8)(BASS_MAX * 3u); }
        S->c = (u8)next27(m, c);
    }
    /* 2.61: a new bass note next step: the gate goes off for this last stock step so the note re-attacks (as the stock
       patterns, which always rest before a note); the OM-27 re-attacks every bass note */
    u32 nf = sub == 2u ? frame27(m, S->c & 31u) : 0u;      /* the next OM-27 step's outputs (on the last stock step) */
    if (nf >> 6) { S->bleft = 0; S->bkind = BK_OFF; }
    else if (S->bleft) { S->bleft--; S->bkind = BK_NOTE; }
    else S->bkind = BK_OFF;
    /* 2.61: the chord gate stays open the whole OM-27 step (the OM-27's stab: about one step, then cut), closing for the
       last stock step only when the next step has a stab too (so that one re-attacks) */
    S->cg = (u8)(S->chord && !(nf & B_CH));
}
static s8 row27(const r27_t *S, u32 row) {
    u32 sub = *(volatile unsigned short *)DE0E % 3u;
    if (row == 18u) return S->cg ? 127 : 0;                         /* chord stab gate (r27_step) */
    if (sub) return 0;
    u32 b = row == 0u ? B_BD : row == 2u ? B_SD : row == 7u ? B_HH : row == 6u ? B_CY : row == 16u ? B_CL : 0u;
    return (b && (S->hits & b)) ? HIT : -HIT;     /* -80 elsewhere, as the stock patterns (keyboard-mode drum-pad rolls) */
}
void r27_row(u32 row, const volatile s8 *p) {      /* replaces bl 0x60007574 at 0x60007E98 */
    if (r27_ready()) {
        r27_t *S = (r27_t *)R7B; S->rv = (u8)row27(S, row); p = (const volatile s8 *)&S->rv;
        F_7574(row, p);
        if (row == 18u && S->rv) R8(DE33) = 0x47u;  /* 2.61: no 14 ms cut: the chord gate stays open until our 0 closes it */
        return;
    }
    F_7574(row, p);
}
void r27_bass(u32 root, const volatile s8 *p, u32 de42) {   /* replaces bl 0x60007B34 at 0x60007FFE (every mode) */
    r27_t *S = (r27_t *)R7B;
    u32 st = bsty(), pk = st && S->magic == R7MAGIC && S->pinit, g0 = R8(DE38), v0 = R8(BVAL);
    if (r27_ready()) {
        u32 t = R8(DE49); if (t > 8u) t = 0;
        u32 v = 127u;
        if (S->bkind == BK_NOTE) v = S->bcode == 1u ? 1u : S->bcode == 2u ? FIFTH[t] : THIRD[t];
        S->bv = (u8)v; p = (const volatile s8 *)&S->bv;
    }
    F_7B34(root, p, de42);
    /* a pluck style holds the gate (no Classic 50 ms cut: the tick only cuts at 0xFA) for the pluck to shape */
    if (st && R8(DE38)) R8(DE34) = 0xFBu;
    if (pk) {
        u32 g1 = R8(DE38), v1 = R8(BVAL);
        if (g1 && (!g0 || (v1 != v0 && (s8)v1 < 0x7C))) S->pnew = 1u;   /* gate opened, or a new pitch under it */
        bass_duty((u32)S->py2);              /* a new pitch reset the timer to 50 %: our duty back at once */
    }
}
/* ---------------- 2.86: Brothers = jazz mode (KEYBOARD at power-on, this power-on only) ----------------
   Barry Harris' sixth diminished harmony, as the minichord's "Brothers" (Key & Cable advanced firmware 96, firmware/src/
   main.cpp: brothers_scale, apply_brothers, cantus_follow; the user's brief 2026-10-19). Replaces the 2.71 jazz voicing.
   - Each chord has an 8-note scale: its 6th (or 7th) chord's 4 notes alternating with a diminished 7th (its "brother").
     Chord types (DE49, stock order): major, maj7, aug, sus4, add9 = the major 6th scale (their 7th / #5 / 4th / 9th sound
     as brother notes, as the minichord's maj7); minor = minor 6th; 7th and m7 = the computed 7th scales; dim = dim scale.
   - The strum plates (13) are tuned to that scale from the root (plate 1) up; the plate a finger lands on (or slides to)
     picks a 4-note close block with that note on top, down the scale every other note. The block's lowest note is the
     bass, the 3 notes above it the 3 chord oscillators (by register): all 4 voices move, as on the minichord (its bass is
     the lowest of its 4 chord voices). The rhythm's bass line plays the block too: root -> its lowest note, 3rd -> the
     next, 5th -> the next, other notes kept at their distance from the root.
   - Before any touch (and after a chord change that drops the note): the "home" block = the middle plate's note on top =
     the root-position 6th / 7th chord (C6, Cm6, G7, Cm7, Cdim7), the bass on the root.
   - A touch counts if 80 ms or more have passed since the plate before (a fast strum: its first plate only). A finger
     landing (no other plate touched) re-strikes the chord in Manual; a slide moves it in place. A chord change keeps the
     touched note if it is a chord tone of the new chord (a held common tone); a released chord forgets it.
   - 2.86dev3 (the user, after 2.86dev2's "Omni 2 sounds awesome": the melody on the main voice, the subs ring the chords;
     the bass moving only on chord tones; the chord part a 5th and an octave, one voice muted, following the bass): every
     plate plays its scale note on the whole voice (main + sub) and the 3 notes below it in its 4-note block on the sub
     layers only (the main layer switched off for those note-ons: strum_chord); the plate's own note slot + 3 more, the
     oldest not held; a plate let go releases all 4. The bass follows the block only when its top note is a chord tone
     (a diminished block passes over the bass held; bro_bblock). Sampled voices: the chord oscillators play the bass
     note's 5th and octave, the third oscillator stopped. Omni 1 and 2 = "analog melody": each plate retunes its own
     oscillator to its scale note (the Omni plates gate them: an analog line), the sub rings the chords. Keyboard mode: stock.
   - MIDI: the chord channel plays the oscillators' notes (none in (c)), the bass the block's lowest note (note-offs as
     their note-ons went out); a touch that moves the held Manual chord sends the moved notes again. Strum: the 4 notes of
     each plate's block, released with the plate.
   - 2.86dev5 (the user: dev4's moving chords sound great but aren't fun to play; "finger movements remain similar" to the
     stock): Brothers stays on Omni 2 only. Every other voice (Omni 1 too) = jazz voicings: each chord button plays a
     4-note jazz voicing (JV4: 6, m9, 13, maj9, m11, dim7, 9#5, 9sus4, 6/9), voice-led from the chord before; the chord
     oscillators play 3 of its notes, the plates strum its 4 notes upward as the stock strums root / 3rd / 5th. A touch
     never moves the chord (no re-strike). Bass: the stock's.
   - 2.86dev6 (the user: "make it work for the analog voices on omni 1 and 2"): Omni 1 and 2 = the jazz voicings too
     (Brothers no longer selected). Each Omni plate retunes its own oscillator (plates 1, 4, 7.. ch 2; 2, 5.. ch 0; 3, 6..
     ch 1; its octave group added by the Omni plates) to the plate's strum note, so the analog plate strums the voicing as
     the sampled plates do; a chord change puts the oscillators back on the voicing.
   - 2.86dev7 (the user: after strumming, the chord voices end up an octave lower): a high plate's oscillator sits its octave
     group(s) below its note (the Omni plates add them back), and the chord section sounds the oscillator as it is. So
     OM_BACK ms after the last plate is let go, the oscillators go back on the voicing.
   - 2.86dev8: the m6 combination (chord_dec; not on Omni 1); Omni 2 = the "register-safe" strum (om_note: 3 of the 4 notes
     per plate group, the oscillators kept in the chord's register: no return needed); Omni 1 keeps dev7's.
   - 2.86dev10: 3 more combinations (chord_dec): 13b9, half-diminished, minor-major 9; m6/9 on every voice (A. piano too).
   - 2.86dev9: the combination = m6/9 on every voice (Omni 1 too), m7b5 on the A. piano (to compare); Omni 1 and 2 back to
     dev6 (exact strum, the oscillators stay until the next chord: OM_RETURN 0, jsafe 0).
   Runtime state in the stock page buffer (free at runtime). */
#define BROB     0x20002B10u              /* bro_t: page buffer (0x20002AD4-0x20002B5F free at runtime; sl_t before it) */
#define BROMAGIC 0x42523836u
#define CLS      0x20000378u              /* stock: the chord's classes (root, 3rd-type, 5th-type), 0-11; oscillator = 54 + class */
#define PLATES   0x20006AD8u              /* stock plate notes [1..13] (sample note = MIDI - 12) */
#define PTOUCH   0x20006A65u              /* stock: plate 1..13 touched (debounced) */
#define AUTOF    0x200009DAu              /* stock: Auto */
#define DE47B    0x2000DE47u              /* stock: rhythm running */
#define F_8E74   ((u32(*)(u32, u32, u32))0x60008E75)   /* stock: bass pitch (bass table index = MIDI - 24; >= 100 rest) */
#define BRO_GAP  80u                      /* ms: a touch this soon after the one before is part of the same strum */
#define BRO_RS   12u                      /* ms: the chord oscillators stopped for a re-strike (Manual) */
typedef struct {
    u32 magic;
    u8 note, kr, kt, live;     /* brothers note (MIDI, 0 = none) and the chord (class, type) it holds for; state applied */
    u8 b[4];                   /* the block applied: bass-role note, then the oscillators' notes bottom-up (MIDI) */
    u8 osc[3];                 /* per oscillator channel 0-2: its note (MIDI) */
    u8 bn;                     /* the bass index the stock last asked for (rhythm interrupt; 0xFF none) */
    unsigned short tl;         /* TICKS of the last plate touch (low 16 bits) */
    unsigned short rs0;        /* re-strike started (TICKS low 16) */
    u8 rs, tinit, pl, raw;     /* re-strike running; tl valid; plates written by us; our own MIDI sends (midi_hook passes) */
    u8 bset, r0, r1, r2;       /* the bass index last programmed (by the stock through bass_note, or by us) */
    u32 sig;                   /* what the block was last applied for (chord, held, note, voice): unchanged = nothing to do */
    u8 mel[3], cstop;          /* Omni 2: each oscillator's melody note (MIDI, 0 = from the block); we stopped the oscillators ((c)) */
    u8 lastv, mk;              /* the voice last seen (a voice change: the extra strum notes' MIDI off); the chord mel is for */
    unsigned short rt;         /* 2.86dev7: TICKS (low 16) a plate was last held (Omni: the oscillators back after OM_BACK) */
    u8 bnote, bkr, bkt, r6;    /* the bass's note: the last chord-tone touch (MIDI, 0 = home) and the chord it was taken on */
    u8 bb[4];                  /* the bass block applied (bass-role note, then the 3 above it) */
    u8 jv[4];                  /* 2.86dev5: the jazz voicing sounding (MIDI, bottom-up) */
    u8 jk, xt, r8, r9;         /* the chord it is for (root | type << 4; 0xFF = none yet); 2.86dev8/10: the extra combination held (XT_*) */
} bro_t;
_Static_assert(sizeof(bro_t) <= 0x20002B60u - BROB, "bro_t overlaps an_t");
static const u8 SC_MAJ6[8] = { 0, 2, 4, 5, 7, 8, 9, 11 };   /* C6 + B dim7 */
static const u8 SC_MIN6[8] = { 0, 2, 3, 5, 7, 8, 9, 11 };   /* Cm6 + B dim7 */
static const u8 SC_DOM7[8] = { 0, 2, 4, 5, 7, 8, 10, 11 };  /* C7 + D dim7 (the brief's computed scale, offset 2) */
static const u8 SC_MIN7[8] = { 0, 2, 3, 5, 7, 8, 10, 11 };  /* Cm7 + D dim7 */
static const u8 SC_DIM[8]  = { 0, 2, 3, 4, 6, 7, 9, 11 };   /* Cdim7 + D E G B (G6) */
static const u8 *bro_scale(u32 t) {
    switch (t) { case 1: return SC_MIN6; case 2: return SC_DOM7; case 4: return SC_MIN7; case 5: return SC_DIM; default: return SC_MAJ6; }
}
static bro_t *bro(void) {
    bro_t *B = (bro_t *)BROB;
    if (B->magic != BROMAGIC) {
        volatile u8 *p = (volatile u8 *)BROB;
        for (u32 i = 4; i < 0x3Au; i++) p[i] = 0;                    /* bro_t + the bass MIDI map (BIN / BOUT) */
        B->bn = B->bset = B->lastv = B->jk = 0xFFu; B->magic = BROMAGIC;
    }
    return B;
}
static u32 m12(int x) { x %= 12; return (u32)(x < 0 ? x + 12 : x); }
static u32 bro_on(void) { return jazz() && R8(KEYS_EN) && R8(ST) == PH_DONE && !LED(0x15); }
/* 2.86dev2: per voice (DE89: omni1 omni2 harp celeste A.piano guitar FMpiano organ vibes banjo) what the analog side plays */
#define VM_OM2 2u   /* Omni 1 / 2: each plate retunes its oscillator to its scale note (an analog line) */
#define VM_JV  7u   /* 2.86dev5: every other voice (Omni 1 too): jazz voicings, voice-led; the plates strum them */
#define BASS_MOVES 0 /* 2.86dev4: the bass stays the stock's (root / its line: "the static bass"); 1 = it follows the block */
static u32 vmode(void) { return VM_JV; }    /* 2.86dev6: jazz voicings on every voice (Brothers / VM_OM2 kept, unused) */
static u32 jomni(void) { return R8(DE89) < 2u; }   /* 2.86dev6: Omni 1 / 2: each plate retunes its own oscillator to its strum note */
#define OM_RETURN 0    /* 2.86dev9: off: the oscillators stay where the strum left them until the next chord (2.86dev6) */
#define OM_BACK 400u   /* 2.86dev7: ms after the last plate is let go: the Omni oscillators back on the chord (the user: after a strum the chord sat an octave low) */
static u32 mute_mask(void) { return 0u; }   /* (2.86dev4 stopped the sampled voices' oscillators: "muted chords"; 2.86dev5: all on) */
static u32 ctype(void) { u32 t = R8(DE49); return t > 8u ? 0u : t; }
static u32 croot(void) { return R8(CLS) % 12u; }
static u32 rootpc(void) { return (croot() + 6u) % 12u; }             /* oscillator 54 + class: the class's pitch class */
static int plate_note(u32 i) {                                        /* plate i (0-12) tuned to the scale, MIDI */
    const u8 *S = bro_scale(ctype());
    return 66 + (int)croot() + S[i % 8u] + 12 * (int)(i / 8u);
}
static int bro_degree(u32 pc, const u8 *S) {                          /* the scale degree of pitch class pc, -1 none */
    u32 rel = m12((int)pc - (int)rootpc());
    for (u32 d = 0; d < 8u; d++) if (S[d] == rel) return (int)d;
    return -1;
}
static u32 note_now(u32 note, u32 kr, u32 kt) {   /* a held note for the chord now (pure: the interrupt uses it too) */
    if (!note || !R8(DE64)) return 0;
    if (kr == croot() && kt == ctype()) return note;
    int d = bro_degree(note % 12u, bro_scale(ctype()));
    return (d >= 0 && !(d & 1)) ? note : 0u;                           /* a chord tone of the new chord: kept */
}
static u32 bro_note_now(const bro_t *B) { return note_now(B->note, B->kr, B->kt); }
/* 2.86dev5: jazz voicings (the user: "finger movements remain similar, but yield more of these great jazzy sounding
   progressions"). Per chord type (DE49) 4 notes above the root (semitones; the bass plays the root): the sampled voices'
   plates strum them, the 3 chord oscillators play 3 of them (JVDROP left out: the most "plain" note). Voice-led: each new
   chord takes the inversion (close position, its lowest note within JV_LO..JV_LO+11) nearest the voicing before it. */
static const u8 JV4[13][4] = {
    { 4, 7, 9, 12 },     /* major: 6       (C6:    E G A C)  */
    { 3, 7, 10, 14 },    /* minor: m9      (Cm9:   Eb G Bb D) */
    { 4, 10, 14, 21 },   /* 7th:   13      (C13:   E Bb D A) */
    { 4, 7, 11, 14 },    /* maj7:  maj9    (Cmaj9: E G B D)  */
    { 3, 10, 14, 17 },   /* m7:    m11     (Cm11:  Eb Bb D F) */
    { 3, 6, 9, 12 },     /* dim:   dim7    (Cdim7: Eb Gb A C) */
    { 4, 8, 10, 14 },    /* aug:   9#5     (C9#5:  E G# Bb D) */
    { 5, 7, 10, 14 },    /* sus4:  9sus4   (C9sus4: F G Bb D) */
    { 4, 7, 9, 14 },     /* add9:  6/9     (C6/9:  E G A D)  */
    /* 2.86dev10: the extra combinations (chord_dec), each "a chord + the button to its left / right" */
    { 3, 7, 9, 14 },     /* 9  minor + the major to its left: m6/9 (Cm6/9: Eb G A D; the user's pick over dev8's m6 / dev9's m7b5) */
    { 3, 6, 10, 12 },    /* 10 minor + the 7th to its left: half-diminished m7b5 (Cm7b5: Eb Gb Bb C) */
    { 4, 10, 13, 21 },   /* 11 7th + the major to its left: 13b9 (C13b9: E Bb Db A) */
    { 3, 7, 11, 14 } };  /* 12 minor + the 7th to its right: minor-major 9 (Cm(maj9): Eb G B D) */
static const u8 JVDROP[13] = { 7, 7, 14, 7, 14, 12, 14, 7, 7, 7, 12, 21, 7 };
#define JT_M6 9u                                  /* (the first extra type) */
#define XT_M69 1u
#define XT_HD  2u
#define XT_7B9 3u
#define XT_MM  4u
static u32 jtype(void) {                          /* the voicing's type: DE49, or an extra one (chord_dec found its combination) */
    u32 t = ctype();
    const bro_t *B = (const bro_t *)BROB;
    if (B->magic != BROMAGIC || !B->xt) return t;
    if (B->xt == XT_7B9) return t == 2u ? 11u : t;
    if (t != 1u) return t;
    return B->xt == XT_M69 ? 9u : B->xt == XT_HD ? 10u : 12u;
}
#define JV_LO 52                                  /* the voicing's lowest note: E3 .. D#4 */
#define JP_LO 54                                  /* the plates start at the first voicing note from F#3 (the stock's register) */
static const u8 JV_HOME[4] = { 55, 59, 62, 66 };  /* (the first chord: nearest this) */
static u32 jv_key(void) { return croot() | (jtype() << 4); }
static u32 jv_mask(void) {                        /* the voicing's pitch classes (bit per class) */
    u32 t = jtype(), r = rootpc(), mk = 0;
    for (u32 j = 0; j < 4u; j++) mk |= 1u << ((r + JV4[t][j]) % 12u);
    return mk;
}
static void jv_next(const u8 *prev, u8 *out) {    /* the voicing for the chord now, led from prev. Pure. */
    u32 mk = jv_mask(); int best = 0x7FFFFFFF, bt = 0;
    for (u32 lo = 0; lo < 12u; lo++) {            /* each voicing note as the lowest: one close inversion */
        int n = JV_LO + (int)lo;
        if (!(mk & (1u << ((u32)n % 12u)))) continue;
        int c[4]; c[0] = n;
        for (u32 j = 1; j < 4u; j++) { int x = c[j - 1] + 1; while (!(mk & (1u << ((u32)x % 12u)))) x++; c[j] = x; }
        int cost = 0; for (u32 j = 0; j < 4u; j++) cost += c[j] > prev[j] ? c[j] - prev[j] : prev[j] - c[j];
        int ctr = c[0] + c[3] - 2 * 62, tb = ctr < 0 ? -ctr : ctr;   /* a tie: the one nearer the middle */
        if (cost < best || (cost == best && tb < bt)) { best = cost; bt = tb; for (u32 j = 0; j < 4u; j++) out[j] = (u8)c[j]; }
    }
}
static void jv_get(const bro_t *B, u8 *v) {       /* the voicing now (the stored one, or the next one if the chord just changed) */
    if (B->jk == (u8)jv_key()) { for (u32 j = 0; j < 4u; j++) v[j] = B->jv[j]; return; }
    jv_next(B->jk == 0xFFu ? JV_HOME : B->jv, v);
}
static int jv_plate(u32 i) {                      /* plate i (0-12): the voicing's notes upward from JP_LO (MIDI) */
    u32 mk = jv_mask(); int n = JP_LO;
    for (u32 k = 0;; n++) if (mk & (1u << ((u32)n % 12u))) { if (k == i) return n; k++; }
}
/* 2.86dev8, Omni 2 ("register-safe"): the Omni plates add an octave per group of 3 (plate i sounds its oscillator + 12 (i / 3)),
   the voicing has 4 notes an octave: each group plays 3 of the 4 voicing notes in the octave its oscillators already sound in
   (the window from the voicing's lowest note), skipping a different one each group (3, 2, 1, 0: all 4 heard over a strum).
   The oscillators never leave the chord's register, so nothing needs to come back after the strum (Omni 1: dev7's exact
   strum, back after OM_BACK). */
static u32 jsafe(void) { return 0u; }   /* 2.86dev9: off (the user: dev8's Omni 2 and dev7's return both sound bad; dev6's kept) */
static int om_note(u32 i) {                       /* Omni 2: plate i's oscillator note (MIDI; the plate sounds it + 12 (i / 3)) */
    u8 v[4]; jv_get(bro(), v);
    u32 mk = jv_mask(), g = i / 3u, w = i % 3u, skip = 3u - (g & 3u), k = 0; int c[4];
    for (int x = v[0]; k < 4u; x++) if (mk & (1u << ((u32)x % 12u))) c[k++] = x;
    return c[w >= skip ? w + 1u : w];
}
static int plate_val(u32 i) {
    if (vmode() != VM_JV) return plate_note(i);
    return jsafe() ? om_note(i) + 12 * (int)(i / 3u) : jv_plate(i);
}
/* The block for the chord now: b[0] bass-role note, b[1..3] the oscillators' notes (MIDI), and per oscillator channel its
   note (osc[ch]). Pure. */
static void block_of(int note, u8 *b) {    /* the close block with note on top (0 = home: the middle plate's), placed */
    const u8 *S = bro_scale(ctype());
    u32 rpc = rootpc(), c[3] = { R8(CLS) % 12u, R8(CLS + 1u) % 12u, R8(CLS + 2u) % 12u };
    if (!note) note = plate_note(6u);                                  /* home: the middle plate on top */
    int d = bro_degree((u32)note % 12u, S); if (d < 0) d = 6;
    u32 mx = c[0] > c[1] ? c[0] : c[1]; if (c[2] > mx) mx = c[2];
    int ref = 54 + (int)mx, mid = plate_note(6u), lo = ref - 5;      /* minichord: the middle plate within half an octave of the plain chord's top */
    int k[4];
    k[3] = note + (lo + (int)m12(mid - lo)) - mid;
    for (u32 j = 1; j < 4u; j++) {
        int pc = (int)rpc + S[(u32)(d + 8 - 2 * (int)j) % 8u], above = k[4 - j];
        k[3 - j] = above - (int)m12(above - pc);
    }
    while (k[1] < 48) for (u32 j = 0; j < 4u; j++) k[j] += 12;         /* the oscillators within 48-84 */
    while (k[3] > 84) for (u32 j = 0; j < 4u; j++) k[j] -= 12;
    for (u32 j = 0; j < 4u; j++) b[j] = (u8)k[j];
}
/* The bass's block: the last chord-tone touch's (a diminished block passes over it), or home. Pure. */
static void bro_bblock(const bro_t *B, u8 *bb) { block_of((int)note_now(B->bnote, B->bkr, B->bkt), bb); }
static void bro_block(const bro_t *B, u8 *b, u8 *osc) {
    u32 c[3] = { R8(CLS) % 12u, R8(CLS + 1u) % 12u, R8(CLS + 2u) % 12u };
    block_of((int)bro_note_now(B), b);
    /* oscillator channels bottom-up by their stock pitch (ch 2 = root, ch 0 = 3rd-type, ch 1 = 5th-type): a step of the
       block is a step of every voice */
    u32 base[3], ord[3];
    base[0] = 54u + c[1]; base[1] = 54u + c[2]; base[2] = 54u + c[0]; ord[0] = 0; ord[1] = 1; ord[2] = 2;
    for (u32 i = 0; i < 3u; i++) for (u32 j = i + 1u; j < 3u; j++)
        if (base[ord[j]] < base[ord[i]] || (base[ord[j]] == base[ord[i]] && ord[j] < ord[i])) { u32 x = ord[i]; ord[i] = ord[j]; ord[j] = x; }
    for (u32 i = 0; i < 3u; i++) osc[ord[i]] = b[i + 1u];
    if (vmode() == VM_OM2) { for (u32 ch = 0; ch < 3u; ch++) if (B->mel[ch]) osc[ch] = B->mel[ch]; }
    else {                                                             /* 2.86dev5: 3 notes of the jazz voicing, bottom-up */
        u8 v[4], a[3]; u32 n = 0, dp = (rootpc() + JVDROP[jtype()]) % 12u;
        jv_get(B, v);
        for (u32 j = 0; j < 4u && n < 3u; j++) if (v[j] % 12u != dp || n + (4u - j) <= 3u) a[n++] = v[j];
        for (u32 i = 0; i < 3u; i++) osc[ord[i]] = a[i];
        if (jomni()) { for (u32 ch = 0; ch < 3u; ch++) if (B->mel[ch]) osc[ch] = B->mel[ch]; }
    }
}
/* A bass note (MIDI or bass index: the same pitch class) moved to the block, in its register (-6..+5 semitones). */
static u32 bro_bass(const u8 *b, u32 n) {
    if (!BASS_MOVES) return n;                                         /* 2.86dev4: the stock's bass */
    u32 t = ctype(), iv = m12((int)(n % 12u) - (int)rootpc()), tgt;
    if (iv == 0u) tgt = b[0] % 12u;
    else if (iv == (u32)THIRD[t] - 1u) tgt = b[1] % 12u;
    else if (iv == (u32)FIFTH[t] - 1u) tgt = b[2] % 12u;
    else tgt = (b[0] + iv) % 12u;                                      /* other notes: kept at their distance from the root */
    int dd = (int)m12((int)tgt - (int)(n % 12u)); if (dd > 5) dd -= 12;
    int r = (int)n + dd; if (r < 0) r += 12;
    return (u32)r;
}
u32 bass_note(u32 n, u32 q, u32 st) {        /* replaces bl 0x60008E74 (bass pitch) in the rhythm tick: 4 sites (mkimg.py) */
    bro_t *B = (bro_t *)BROB;
    if (B->magic == BROMAGIC) B->bn = (u8)(n > 0xFEu ? 0xFEu : n);
    if (n < 100u && B->magic == BROMAGIC && bro_on()) {
        u8 bb[4]; bro_bblock(B, bb);
        u32 m = bro_bass(bb, n); if (m > 99u) m -= 12u;
        n = m;
    }
    if (B->magic == BROMAGIC) B->bset = (u8)(n > 0xFEu ? 0xFEu : n);
    return F_8E74(n, q, st);
}
static void plates_write(const bro_t *B, u32 on) {   /* the 13 plate notes: the scale (every voice), or stock */
    (void)B; u32 om = 0;
    u8 c[3] = { (u8)(R8(CLS) % 12u), (u8)(R8(CLS + 1u) % 12u), (u8)(R8(CLS + 2u) % 12u) };
    for (u32 i = 0; i < 13u; i++) {
        int v;
        if (on && !om) v = plate_val(i) - 12;
        else {
            u32 g = i / 3u, w = i % 3u;                                 /* stock 0x6000BD8C: root / 3rd / 5th + 42 + 12 g */
            v = (int)c[w] + 42 + 12 * (int)g;
        }
        if (v < 1) v = 1; if (v > 120) v = 120;
        if (R8(PLATES + 1u + i) != (u8)v) R8(PLATES + 1u + i) = (u8)v;
    }
}
static void bro_midi(bro_t *B, u32 strike);
#define TMRCTL(ch) (*(volatile unsigned short *)(TMR1 + (ch) * 0x20u + 0x0Cu))
static void bro_cstop(bro_t *B, u32 mask) {  /* the oscillators in mask stopped (silent); the others back as the stock had them */
    u32 run = (TMRCTL(0) & 0xE000u) || (TMRCTL(2) & 0xE000u);         /* the stock has the chord oscillators running (ch 0 / 2 are never ours to stop for long) */
    for (u32 ch = 0; ch < 3u; ch++) {
        u32 m = 1u << ch;
        if (mask & m) { if (TMRCTL(ch) & 0xE000u) { TMRCTL(ch) = (unsigned short)(TMRCTL(ch) & 0x1FFFu); B->cstop |= (u8)m; } }
        else if (B->cstop & m) {
            B->cstop &= (u8)~m;
            if (run || ((mask & 5u) == 0u && (R8(DE64) || R8(AUTOF))))   /* as the stock has them (released in Manual: it stopped them) */
                TMRCTL(ch) = (unsigned short)((TMRCTL(ch) & 0x1FFFu) | 0x2000u);
        }
    }
}
/* Apply the block for the chord now: oscillator shifts (an_write), the bass pitch, plates, MIDI. */
static void bro_apply(bro_t *B, r27_t *S, u32 strike) {
    u8 b[4], o[3];
    bro_block(B, b, o);
    u8 bb0[4]; bro_bblock(B, bb0);
    u32 moved = !B->live || b[0] != B->b[0] || b[1] != B->b[1] || b[2] != B->b[2] || b[3] != B->b[3]
              || o[0] != B->osc[0] || o[1] != B->osc[1] || o[2] != B->osc[2]   /* (the voice's oscillators / melody too) */
              || bb0[0] != B->bb[0] || bb0[1] != B->bb[1] || bb0[2] != B->bb[2];
    u32 c[3] = { R8(CLS + 1u) % 12u, R8(CLS + 2u) % 12u, R8(CLS) % 12u };      /* per channel 0-2: its stock class */
    for (u32 ch = 0; ch < 3u; ch++) ((volatile u8 *)B->osc)[ch] = o[ch];     /* (volatile: no library memcpy) */
    for (u32 i = 0; i < 4u; i++) ((volatile u8 *)B->b)[i] = b[i];
    B->live = 1u;
    S->sh0 = (s8)((int)o[0] - (54 + (int)c[0])); S->sh1 = (s8)((int)o[1] - (54 + (int)c[1])); S->sh2 = (s8)((int)o[2] - (54 + (int)c[2]));
    an_write();
    bro_cstop(B, mute_mask());
    for (u32 i = 0; i < 4u; i++) ((volatile u8 *)B->bb)[i] = bb0[i];  /* the bass's block (its lowest note: the bass) */
    if (B->bn < 100u) {                                                 /* the bass to the block (the stock's last note), if it moved */
        u32 pm;
        __asm volatile ("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");   /* the rhythm interrupt programs the bass too */
        u32 bn = B->bn;
        if (bn < 100u) {
            u32 n = bro_bass(bb0, bn); if (n > 99u) n -= 12u;
            if (n != B->bset) { B->bset = (u8)n; F_8E74(n, MA_KB, MA_KS); }
        }
        __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
    }
    plates_write(B, 1u); B->pl = 1u;
    if (moved || strike) bro_midi(B, strike);
}
static void strum_flush(void);
static void bro_off(bro_t *B, r27_t *S) {    /* Brothers not active now (keyboard mode): everything stock */
    if (B->live) {
        B->live = 0; B->note = 0; B->sig = 0; B->mel[0] = B->mel[1] = B->mel[2] = 0;
        S->sh0 = S->sh1 = S->sh2 = 0; an_write(); bro_cstop(B, 0u); strum_flush(); B->bnote = 0;
        u32 pm;
        __asm volatile ("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");
        if (B->bn < 100u && B->bset != B->bn) { B->bset = B->bn; F_8E74(B->bn, MA_KB, MA_KS); }
        __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
    }
    if (B->pl) { plates_write(B, 0u); B->pl = 0; }
}
static void bro_strike(bro_t *B) {           /* Manual: the chord oscillators stopped for BRO_RS ms, then counting again */
    if (R8(AUTOF) || !R8(DE64) || !(TMRCTL(2) & 0xE000u)) return;
    for (u32 ch = 0; ch < 3u; ch++) if (!(mute_mask() & (1u << ch))) TMRCTL(ch) = (unsigned short)(TMRCTL(ch) & 0x1FFFu);
    B->rs = 1u; B->rs0 = (unsigned short)TICKS;
}
static void bro_strike_tick(bro_t *B) {
    if (!B->rs || (unsigned short)((unsigned short)TICKS - B->rs0) < BRO_RS) return;
    B->rs = 0;
    if (R8(AUTOF) || !R8(DE64)) return;                                 /* released / Auto meanwhile: the stock decides */
    for (u32 ch = 0; ch < 3u; ch++) if (!(mute_mask() & (1u << ch)) && !(TMRCTL(ch) & 0xE000u)) TMRCTL(ch) = (unsigned short)((TMRCTL(ch) & 0x1FFFu) | 0x2000u);
}
static void chord_tick(r27_t *S, u32 jz) {   /* every control tick with jazz mode or the alternate bass (from pluck_tick) */
    bro_t *B = bro();
    if (!jz || !bro_on()) { bro_off(B, S); return; }
    if (!R8(DE64)) B->note = B->bnote = 0;                              /* chord released: back to the home block */
    else if (B->bnote && (B->bkr != croot() || B->bkt != ctype())) {
        B->bnote = (u8)note_now(B->bnote, B->bkr, B->bkt); B->bkr = (u8)croot(); B->bkt = (u8)ctype();
    }
    else if (B->note && (B->kr != croot() || B->kt != ctype())) {      /* chord changed: keep a common chord tone */
        B->note = (u8)bro_note_now(B); B->kr = (u8)croot(); B->kt = (u8)ctype();
    }
    if (B->mk != (u8)jv_key() || !R8(DE64)) { B->mel[0] = B->mel[1] = B->mel[2] = 0; B->mk = (u8)jv_key(); }
    if (B->lastv != R8(DE89)) { B->lastv = R8(DE89); strum_flush(); }  /* a voice change chokes the strum: its MIDI too */
    if (B->jk != (u8)jv_key()) { u8 v[4]; jv_get(B, v); for (u32 j = 0; j < 4u; j++) ((volatile u8 *)B->jv)[j] = v[j]; B->jk = (u8)jv_key(); }
    bro_strike_tick(B);
    if (OM_RETURN && jomni() && !jsafe() && (B->mel[0] | B->mel[1] | B->mel[2])) {   /* 2.86dev7 (Omni 1): the strum over: the oscillators back on the voicing */
        u32 held = 0; for (u32 k = 0; k < 13u; k++) held |= R8(PTOUCH + k);
        unsigned short now = (unsigned short)TICKS;
        if (held) B->rt = now;
        else if ((unsigned short)(now - B->rt) >= OM_BACK) { B->mel[0] = B->mel[1] = B->mel[2] = 0; bro_apply(B, S, 0u); }
    }
    /* 2.86: only when something changed (the LED-lag lesson: the control loop's time is precious): the chord, held or not,
       the note, the voice (Omni), a stock write of the chord oscillators (pwm_hook), or the stock's plate notes back */
    const an_t *A = (const an_t *)ANB;
    u32 sig = croot() | (jtype() << 4) | ((u32)R8(DE64) << 8) | (vmode() << 9) | ((u32)bro_note_now(B) << 12) | ((u32)(note_now(B->bnote, B->bkr, B->bkt) & 0x7Fu) << 20) | (jomni() << 27) | ((u32)(jtype() >= JT_M6) << 28) | ((u32)(R8(DE89) == 1u) << 29) | 0x80000000u;
    u32 wr = A->magic == AMAGIC ? (u32)(A->wr[0] | A->wr[1] | A->wr[2]) : 1u;
    u32 pl = R8(PLATES + 1u) != (u8)(plate_val(0u) - 12);             /* (the stock rewrites all 13 at once) */
    if (sig != B->sig || wr || pl || !B->live) { B->sig = sig; bro_apply(B, S, 0u); }
}
/* 2.86dev8/10: the chord decoder (0x6000B468, at its 2 calls 0x6000CCFE / 0x6000D750, entry +0x74): 4 extra combinations the
   stock leaves unused (it plays the button pressed last), recognised from either button, so either order:
     minor + the major to its left -> m6/9        minor + the 7th to its left  -> half-diminished
     7th + the major to its left   -> 13b9        minor + the 7th to its right -> minor-major 9
   The chord's root / type become the "anchor" button's (the minor, or the 7th for 13b9), so the bass, MIDI and the stock all
   take it as that plain chord; xt picks the voicing (jtype). Only when the stock decoded the plain chord of the button (its own
   combinations untouched). Every voice. Button table 0x2000029C: 3 bytes per button {class, row 0 major / 1 minor / 2 7th,
   held}; button = row * 12 + column, columns up the circle of fifths, left = column - 1. Not across the row's ends. */
#define BTN    0x2000029Cu
#define F_B468 ((void(*)(u32, u32))0x6000B469)
static u32 bh(u32 r, int col) { return col >= 0 && col < 12 && R8(BTN + (r * 12u + (u32)col) * 3u + 2u) && R8(BTN + (r * 12u + (u32)col) * 3u + 1u) == r; }
void chord_dec(u32 p, u32 x) {
    F_B468(p, x);
    if (!(jazz() && bro_on()) || p >= 36u) return;
    bro_t *B = bro(); B->xt = 0;
    int c = (int)(p % 12u); u32 row = R8(BTN + p * 3u + 1u);
    if (row > 2u || R8(0x2000037Du) != row) return;                    /* a stock combination (or not a plain chord) */
    u32 xt = 0, ar = row; int ac = c;
    if (row == 1u)      { if (bh(0, c - 1)) xt = XT_M69; else if (bh(2, c - 1)) xt = XT_HD; else if (bh(2, c + 1)) xt = XT_MM; }
    else if (row == 0u) { if (bh(1, c + 1)) { xt = XT_M69; ar = 1u; ac = c + 1; } else if (bh(2, c + 1)) { xt = XT_7B9; ar = 2u; ac = c + 1; } }
    else {
        if (bh(0, c - 1)) xt = XT_7B9;
        else if (bh(1, c + 1)) { xt = XT_HD; ar = 1u; ac = c + 1; }
        else if (bh(1, c - 1)) { xt = XT_MM; ar = 1u; ac = c - 1; }
    }
    if (!xt) return;
    R8(0x2000037Cu) = R8(BTN + (ar * 12u + (u32)ac) * 3u); R8(0x2000037Du) = (u8)ar; B->xt = (u8)xt;
}
/* 2.86dev2: the strum plays the 4-note block of each plate: the plate's own note slot (the stock's) + 3 more, the oldest not
   held by a touched plate (the engine has 13, one per plate: about 3 chords ring at once). sl_t: per slot its owner plate
   (+1) and the MIDI note we sent for it (an extra), so a plate let go releases its 4 notes (plate_off). */
#define SLB      0x20002AD8u
#define SLMAGIC  0x534C3836u
typedef struct { u32 magic; u8 own[13], mn[13]; unsigned short st[13]; } sl_t;
_Static_assert(sizeof(sl_t) <= BROB - SLB, "sl_t overlaps bro_t");
#define F_REL    ((void(*)(u32))0x60018709)   /* stock: strum slot released (veneer -> 0x2020030C) */
#define STRUM_MIDI 0x200009D9u                 /* stock: strum notes on MIDI */
static sl_t *sl(void) {
    sl_t *L = (sl_t *)SLB;
    if (L->magic != SLMAGIC) {
        volatile u8 *p = (volatile u8 *)SLB;
        for (u32 i = 4; i < sizeof(sl_t); i++) p[i] = 0;
        L->magic = SLMAGIC;
    }
    return L;
}
static void strum_midi(u32 on, u32 note) {    /* an extra strum note on MIDI, as the stock sends the plate's own (0x60009B20) */
    if (on) F_96A0(0u, note, 100u, MA_KB, MA_KS); else F_96D4(0u, note, MA_KB, MA_KS);
    u32 p = F_D8F8();
    if (p != 1u && p != 4u) { if (on) F_96A0(3u, note, 100u, MA_KB, MA_KS); else F_96D4(3u, note, MA_KB, MA_KS); }
}
static void strum_flush(void) {              /* the extra strum notes' MIDI off (a voice change / keyboard mode chokes them) */
    sl_t *L = sl();
    for (u32 s = 0; s < 13u; s++) { if (L->mn[s]) strum_midi(0u, L->mn[s]); L->mn[s] = 0; L->own[s] = 0; }
}
static void strum_chord(u32 i) {              /* plate i (0-12): its block, top = its scale note, on 4 slots */
    sl_t *L = sl();
    const u8 *S = bro_scale(ctype());
    int k[4]; k[3] = plate_note(i);
    int d = bro_degree((u32)k[3] % 12u, S); if (d < 0) d = 0;
    for (u32 j = 1; j < 4u; j++) { int pc = (int)rootpc() + S[(u32)(d + 8 - 2 * (int)j) % 8u], above = k[4 - j]; k[3 - j] = above - (int)m12(above - pc); }
    unsigned short now = (unsigned short)TICKS;
    if (L->mn[i]) { strum_midi(0u, L->mn[i]); L->mn[i] = 0; }         /* its own slot: the stock's note (and MIDI) */
    L->own[i] = (u8)(i + 1u); L->st[i] = now;
    F_NOTEON(i, (u32)(k[3] - 12));
    u32 used = 1u << i, midi = R8(STRUM_MIDI) && !R8(0x20006AC8u + i + 1u);
    for (u32 j = 0; j < 3u; j++) {
        int best = -1, bage = -1;
        for (u32 pass = 0; pass < 2u && best < 0; pass++)              /* not a slot a touched plate holds; else any */
            for (u32 s = 0; s < 13u; s++) {
                if (used & (1u << s)) continue;
                u32 o = L->own[s] ? L->own[s] : s + 1u;
                if (!pass && R8(PTOUCH + o - 1u)) continue;
                int age = (unsigned short)(now - L->st[s]);
                if (age > bage) { bage = age; best = (int)s; }
            }
        u32 s = (u32)best; used |= 1u << s;
        if (L->mn[s]) { strum_midi(0u, L->mn[s]); L->mn[s] = 0; }
        F_REL(s);                                                       /* the note it had: released, as a re-pluck does */
        /* 2.86dev3: the sub layers only: the voice's main layer (layer 0 of the engine's voice, 0x20007604) switched off for
           this note-on (the engine picks the layers at the note-on, 0x20200114) */
        volatile u8 *ml = (volatile u8 *)(VT + (u32)R8(0x20007604u) * 0x40u);
        u8 m0 = *ml; *ml = 0;
        F_NOTEON(s, (u32)(k[j] - 12));
        *ml = m0;
        L->own[s] = (u8)(i + 1u); L->st[s] = now;
        if (midi) { strum_midi(1u, (u32)k[j]); L->mn[s] = (u8)k[j]; }
    }
}
/* A plate touched (replaces bl 0x600186D8 = the strum note-on, in the plate scanner 0x60009B20, at 0x6000A2E4 / 0x6000A47E;
   control task; the scanner calls it only with a chord held). i = plate - 1. */
u32 plate_on(u32 i, u32 note) {
    if (!(jazz() && bro_on() && i < 13u)) { F_NOTEON(i, note); return 0; }
    bro_t *B = bro(); r27_t *S = r27();
    u32 md = vmode();
    unsigned short now = (unsigned short)TICKS, gap = (unsigned short)(now - B->tl);
    u32 news = !B->tinit || gap >= BRO_GAP;
    B->tl = now; B->tinit = 1u;
    plates_write(B, 1u);
    if (md == VM_JV) {                                                  /* 2.86dev5: a plain strum note (the voicing's), as the stock */
        if (jomni() && R8(DE64)) {                                      /* 2.86dev6: Omni: the plate's own oscillator to the same note */
            u32 w = i % 3u, ch = w == 0u ? 2u : w - 1u; int n = jsafe() ? om_note(i) : jv_plate(i) - 12 * (int)(i / 3u);   /* (the plate's octave group adds back) */
            while (n < 40) n += 12; while (n > 90) n -= 12;
            B->mel[ch] = (u8)n; B->mk = (u8)jv_key(); B->rt = (unsigned short)TICKS;
            bro_apply(B, S, 0u);
        }
        F_NOTEON(i, R8(PLATES + 1u + i)); return 0;
    }
    if (R8(DE64)) {
        u32 mel = 0;
        if (md == VM_OM2) {                                             /* its own oscillator to its scale note */
            u32 w = i % 3u, ch = w == 0u ? 2u : w - 1u; int n = plate_note(i) - 12 * (int)(i / 3u);
            while (n < 40) n += 12; while (n > 90) n -= 12;
            B->mel[ch] = (u8)n; B->mk = (u8)(croot() | (ctype() << 4)); mel = 1;
        }
        if (news) {
            u32 landed = 1;
            for (u32 k = 0; k < 13u; k++) if (k != i && R8(PTOUCH + k)) landed = 0;
            B->note = (u8)plate_note(i); B->kr = (u8)croot(); B->kt = (u8)ctype();
            { int dg = bro_degree(B->note % 12u, bro_scale(ctype())); if (dg >= 0 && !(dg & 1)) { B->bnote = B->note; B->bkr = B->kr; B->bkt = B->kt; } }
            if (landed) bro_strike(B);
            bro_apply(B, S, landed);
        } else if (mel) bro_apply(B, S, 0u);
    }
    strum_chord(i);
    return 0;
}
/* A plate let go (replaces bl 0x60018708 = the strum slot release, in the plate scanner at 0x6000A506 / 0x6000A52E): its
   own slot as the stock, and the 3 extra notes it started (with their MIDI). i = plate - 1. */
u32 plate_off(u32 i) {
    F_REL(i);
    sl_t *L = (sl_t *)SLB;
    if (L->magic == SLMAGIC && i < 13u) {
        for (u32 s = 0; s < 13u; s++) if (L->own[s] == i + 1u) {
            if (s != i) { F_REL(s); if (L->mn[s]) strum_midi(0u, L->mn[s]); L->mn[s] = 0; }
            L->own[s] = 0;
        }
    }
    return 0;
}
/* MIDI. The stock sends the chord on internal channel 1 as its 3 classes + 54 (AC8: root, 3rd-type, 5th-type) and the held
   Manual chord's bass on channel 2 (AC8 + 3); the rhythm's bass notes on channel 2 too. midi_hook (entry +0x3C, in the
   note-on / note-off builders, before mod 1's hook) sends each as the block plays it; its note-off as its note-on went
   out (mm_t: in / out per chord note and the bass). */
#define AC8    0x20002AC8u
#define MMB    0x20002C00u        /* mm_t: free (between mt_t and ui_t); runtime only, magic-checked */
#define MMMAGIC 0x4A5Bu
typedef struct { unsigned short magic; u8 in[3], out[3]; } mm_t;
_Static_assert(sizeof(mm_t) <= UIB - MMB, "mm_t overlaps ui_t");
static mm_t *mm(void) {
    mm_t *M = (mm_t *)MMB;
    if (M->magic != MMMAGIC) { for (u32 r = 0; r < 3u; r++) M->in[r] = M->out[r] = 0; M->magic = MMMAGIC; }
    return M;
}
/* bass note map: in bro_t? no room in mm_t: the bass's in / out live in bro_t's spare bytes */
#define BIN  (BROB + 0x38u)
#define BOUT (BROB + 0x39u)
_Static_assert(sizeof(bro_t) <= 0x38u, "bro_t reaches the bass MIDI map");
_Static_assert(BROB + 0x3Au <= 0x20002B60u, "bro_t + the bass map overlap an_t");
u32 midi_hook(u32 q, mmsg_t *m) {             /* replaces bl 0x60039000 (mod 1) in the note-on / note-off builders */
    u8 *p = m->data, n0 = p[0];
    u32 undo = 0;                             /* a note-off's map entry taken (restored if the queue was full) */
    bro_t *B = (bro_t *)BROB;
    if (jazz() && (m->ch == 1u || m->ch == 2u) && (B->magic == BROMAGIC || bro_on())) B = bro();   /* (ready from the first note) */
    if (jazz() && B->magic == BROMAGIC && !B->raw && (m->ch == 1u || m->ch == 2u)) {
        mm_t *M = mm();
        if (p[1]) {                           /* note-on */
            if (bro_on()) {
                u8 b[4], o[3]; bro_block(B, b, o);
                if (m->ch == 1u) {
                    for (u32 r = 0; r < 3u; r++) if (R8(AC8 + r) == n0) {
                        u32 ch = r == 0u ? 2u : r - 1u;              /* root = ch 2, 3rd-type = ch 0, 5th-type = ch 1 */
                        u32 x = (mute_mask() & (1u << ch)) ? 0u : o[ch];
                        for (u32 q = 0; q < 3u; q++) if (q != r && M->in[q] && M->out[q] == x) x = 0;   /* already sounding: once */
                        M->in[r] = n0; M->out[r] = (u8)x;
                        if (!x) return 1u;                            /* the stopped oscillator: no MIDI either */
                        p[0] = (u8)x;
                        break;
                    }
                } else if (n0) {
                    u8 bb[4]; bro_bblock(B, bb);
                    u32 x = bro_bass(bb, n0); if (x > 127u) x -= 12u;
                    R8(BIN) = n0; R8(BOUT) = (u8)x; p[0] = (u8)x;
                }
            }
        } else if (n0) {                      /* note-off: as its note-on went out */
            if (m->ch == 1u) { for (u32 r = 0; r < 3u; r++) if (M->in[r] == n0) { p[0] = M->out[r]; M->in[r] = 0; if (!p[0]) { p[0] = n0; return 1u; } undo = r + 1u; break; } }
            else if (R8(BIN) == n0) { p[0] = R8(BOUT); R8(BIN) = 0; undo = 4u; }
        }
    }
    u32 rv = F_MHOOK(q, m);
    if (!rv && undo) { if (undo == 4u) R8(BIN) = n0; else mm()->in[undo - 1u] = n0; }   /* queue full: the builder retries the same note-off */
    p[0] = n0;
    return rv;
}
/* A touch moved (or re-struck) the held Manual chord: its MIDI notes that moved go again (off, then on). Control task,
   Manual only (the rhythm's chord / bass notes are sent from the rhythm interrupt: they take the block at their next one). */
static void bro_midi(bro_t *B, u32 strike) {
    if (R8(AUTOF) || !R8(DE58) || F_D8F8() == 4u) return;
    mm_t *M = mm();
    u8 nx[3];
    for (u32 r = 0; r < 3u; r++) {                                     /* (two oscillators on one pitch: one MIDI note) */
        u32 ch = r == 0u ? 2u : r - 1u; nx[r] = (mute_mask() & (1u << ch)) ? 0u : B->osc[ch];
        for (u32 q = 0; q < r; q++) if (nx[q] == nx[r]) nx[r] = 0;
    }
    B->raw = 1u;
    /* all note-offs first, then the note-ons (a note another voice moves onto is never started twice); a slide: only the
       notes that change; a strike: all of them */
    for (u32 r = 0; r < 3u; r++) {
        if (!M->in[r] || !M->out[r]) continue;
        u32 keep = nx[r] && !strike && (M->out[r] == nx[0] || M->out[r] == nx[1] || M->out[r] == nx[2]);
        if (!keep) F_96D4(1u, M->out[r], MA_KB, MA_KS);
        if (!nx[r]) M->out[r] = 0;
    }
    u8 was[3] = { M->in[0] ? M->out[0] : 0u, M->in[1] ? M->out[1] : 0u, M->in[2] ? M->out[2] : 0u };
    for (u32 r = 0; r < 3u; r++) {
        if (!M->in[r] || !nx[r]) continue;
        u32 had = !strike && nx[r] && (nx[r] == was[0] || nx[r] == was[1] || nx[r] == was[2]);
        if (!had) F_96A0(1u, nx[r], 0x40u, MA_KB, MA_KS);
        M->out[r] = nx[r];
    }
    if (R8(BIN)) {
        u32 x = bro_bass(B->bb, R8(BIN)); if (x > 127u) x -= 12u;
        if (strike || R8(BOUT) != x) { F_96D4(2u, R8(BOUT), MA_KB, MA_KS); F_96A0(2u, x, 0x40u, MA_KB, MA_KS); R8(BOUT) = (u8)x; }
    }
    B->raw = 0;
}
void strum_shift(u32 *r) { (void)r; }   /* 2.73-2.85: the jazz voicing's plate shift; 2.86: the plates are written after (plates_write) */
static void pluck_tick(void) {               /* every control tick, any mode: the jazz voicing and the alternate bass pluck */
    u32 st = bsty(), jz = jazz();
    if ((!st && !jz) || !R8(KEYS_EN) || R8(ST) != PH_DONE) return;
    r27_t *S = r27();
    u32 now = TICKS;
    if (!S->pinit) { S->py1 = S->py2 = S->ptgt = 32768; S->pms = (unsigned short)now; S->pinit = 1u; return; }
    u32 n = (unsigned short)(now - S->pms); if (n > 50u) n = 50u;
    if (!n) return;
    S->pms = (unsigned short)now;
    chord_tick(S, jz);
    if (!st) return;                          /* jazz without the alternate bass: the pulse untouched */
    const pk_t *K = &PK;
    u32 pm, nw;                               /* 2.78: take the rhythm interrupt's "new note" flag atomically (never lost) */
    __asm volatile ("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");
    nw = S->pnew; S->pnew = 0;
    __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
    if (nw) S->ptgt = 32768;
    for (u32 i = 0; i < n; i++) {
        int k = K->k;
        if (R8(DE38)) { int t = (int)(((u32)S->ptgt * K->f) >> 16); S->ptgt = (unsigned short)(t > (int)K->floor ? t : (int)K->floor); }
        else { S->ptgt = 32768; k >>= 2; }   /* gate closed: back to 50 % four times as slowly */
        S->py1 = (unsigned short)((int)S->py1 + (((int)S->ptgt - (int)S->py1) * k) / 1024);
        S->py2 = (unsigned short)((int)S->py2 + (((int)S->py1 - (int)S->py2) * k) / 1024);
    }
    bass_duty((u32)S->py2);
}
/* Control task, every tick (before the stock key handler). Selection rules (the user's, 2026-10-14):
   - press one button: that rhythm (as stock); hold two or more: they play combined while held; let go: back to what
     played before (unless locked);
   - KEYBOARD pressed while two or more are held: locks that combination (keyboard mode not toggled);
   - a new single press clears a lock. 2.61: a fresh press (nothing held before) restarts the rhythm from its first step at
     once, as the OM-27 in the demo video; buttons added to held ones keep the count (as the recordings); stopped = step 1. */
static int busy_display(void) {
    const ui_t *U = (const ui_t *)UIB; const mt_t *M = (const mt_t *)MTB;
    return (U->magic == UMAGIC && U->menu) || (M->magic == MTMAGIC && (M->blink || M->pad));
}
static void r27_tick(void) {
    if (!IS27()) return;
    r27_t *S = r27();
    if (!R8(DE47)) { S->c = 0; S->bleft = 0; S->bkind = BK_OFF; S->cg = 0; S->rst = 0; }   /* stopped: the chip counter sits at step 1 */
    if (!S->init) {
        if (!R8(KEYS_EN)) return;                                /* 2.78: as soon as the panel is live (its LEDs: restore_early) */
        int next, last = scan(&next); (void)next;
        const volatile u8 *o = last >= 0 ? (const volatile u8 *)(STATE_SECTOR + 16u*last) : (const volatile u8 *)0;
        u32 m = (o && (o[5] & 0xC0u) == 0x40u) ? (o[5] & 0x3Fu) : 0u;
        if (!m) m = 2u;                                          /* nothing saved in 27 mode: ROCK */
        S->cur = S->play = (u8)m; S->gest = 0; S->init = 1;
    }
    u32 h = S->held & 0x3Fu;
    if (!(S->gest & G_ACT) && h) {
        S->gest = G_ACT; S->prev = S->cur; S->first = (u8)h;
        /* 2.61: a fresh press restarts the count at once (the OM-27 in the demo video: a pressed rhythm starts on its first
           step, at the press); buttons added to a held one keep the count (as the recordings) */
        /* 2.65: not when the pressed button is already playing (alone or in the locked group): fill in other rhythms
           without restarting, as the 108 / 84 modes (the user) */
        if (R8(DE47) && !(h & S->play)) { S->rst = 1; if (*(volatile u32 *)STEPLEFT > 1u) *(volatile u32 *)STEPLEFT = 1u; }
    }
    if (S->gest & G_ACT) {
        if (pop6(h) >= 2u) S->gest |= G_MULTI;
        if (S->gest & G_LOCKRQ) {
            S->gest &= (u8)~G_LOCKRQ;
            if (pop6(h) >= 2u) { S->cur = (u8)h; S->gest |= G_LOCKED; kb_blink_start(S); }   /* 2.65: + blink */
        }
        if (h) S->play = (S->gest & G_LOCKED) ? S->cur : (u8)h;
        else {
            if (S->gest & G_LOCKED) S->play = S->cur;
            else if (S->gest & G_MULTI) S->play = S->cur = S->prev;   /* a held combination ends: back to before */
            else S->play = S->cur = S->first;                          /* a single press: that rhythm */
            S->gest = 0;
        }
    } else S->gest &= (u8)~G_LOCKRQ;
    if (R8(DE2C) || busy_display()) return;                          /* LEDs: not over the power-on blink or a display */
    static const u8 L[6] = { LATIN_LED, 9, 10, 11, 12, 13 };
    for (u32 i = 0; i < 6u; i++) { u32 w = (S->play >> i) & 1u; if (LED(L[i]) != w) F_LEDSET(L[i], (int)w); }
    if (LED(0x0Eu ^ 0x0Fu ^ LATIN_LED)) F_LEDSET(0x0Eu ^ 0x0Fu ^ LATIN_LED, 0);   /* the yellow button's other LED */
}
/* ---------------- 2.78: battery check LED writes only on change ----------------
   The stock battery check 0x600081C0 (control loop, about every 7 ms) re-sends the battery LED register (0x40) and redraws
   the voice LED row with the power LED (0x60003A5C) every time, unchanged or not: two writes into the stock LED-write
   queue, which the lowest-priority LED task sends one per millisecond over I2C - so every other LED change waited behind
   them (the user: "button presses ... take a half second too long to register occasionally"). bat_led / bat_row (entry
   +0x44 / +0x48, its four bl sites) skip a write that would not change anything, and send it anyway every BAT_REFRESH
   calls (about once a second). The check itself, its low-battery warning and the low-battery shutdown are unchanged. */
#define BATB        0x20002C50u   /* bat_t: free bytes before ST (stock page buffer, power-on / power-off only) */
#define BMAGIC      0xB7u
#define BAT_REFRESH 128u
typedef struct { u8 magic, last, c40, crow; } bat_t;
_Static_assert(BATB + sizeof(bat_t) <= ST && BATB >= PLB + sizeof(pl_t), "bat_t overlaps");
static u32 row_value(u32 pwr) {         /* the voice-row byte 0x60003A5C(pwr) would write, computed as it does */
    if (R8(DE73)) return R8(MTBUF) | pwr;
    if (R8(DE78)) return R8(TRBUF) | pwr;
    u32 x = R8(0x200000F7u) | ((u32)R8(0x200000F8u) << 1);
    for (u32 i = 6u; i >= 2u; i--) x = R8(0x200000F0u + i) | (x << 1);
    return (pwr | (x << 1)) & 0xFFu;
}
void bat_led(u32 port, u32 val) {       /* replaces bl 0x6000F518(0x40, val) in the battery check */
    bat_t *B = (bat_t *)BATB;
    if (B->magic == BMAGIC && B->last == (u8)val && ++B->c40 < BAT_REFRESH) return;
    B->magic = BMAGIC; B->last = (u8)val; B->c40 = 0;
    F_F518(port, val);
}
void bat_row(u32 pwr) {                 /* replaces bl 0x60003A5C(1) in the battery check */
    bat_t *B = (bat_t *)BATB;
    if (B->magic == BMAGIC && R8(LEDSH) == row_value(pwr) && LED(1) == pwr && ++B->crow < BAT_REFRESH) return;
    if (B->magic != BMAGIC) { B->magic = BMAGIC; B->last = 0; B->c40 = 0; }
    B->crow = 0;
    F_3A5C(pwr);
}
#ifdef DIAG
/* ---- 2.85dev diagnostic (build with DIAG=1): every 100 ms, on MIDI channel 16, CC 20 the most LED writes waiting in the
   stock LED queue, CC 21 the LED writes the LED task sent, CC 22 control ticks run, CC 23 the longest gap between two
   control ticks (ms), CC 24 our own code's longest tick (control loop + vib_tick, CPU cycles / 256), CC 25 the whole
   control-loop call incl. the stock button handler (cycles / 256). Values capped at 127. */
#define DGB      0x20002C40u               /* 16 free bytes */
#define DGMAGIC  0xD6u
#define CYCCNT   R32(0xE0001004u)
typedef struct { unsigned short t0, last, rd0; u8 our, all, ticks, gap, qmax, magic; } dg_t;   /* times: TICKS low 16 bits */
_Static_assert(sizeof(dg_t) <= 16, "dg_t");
static u8 cap7(u32 v) { return (u8)(v > 127u ? 127u : v); }
static void dg_cc(u32 cc, u32 v) { u8 d[2] = { (u8)cc, cap7(v) }; mmsg_t m = { 4, 15, 2, d }; F_MPOST(MIDIQ, &m); }
static void dg_begin(void) {
    dg_t *D = (dg_t *)DGB;
    if (D->magic != DGMAGIC) {
        R32(0xE0001FB0u) = 0xC5ACCE55u; R32(0xE000EDFCu) |= 1u << 24; R32(0xE0001000u) |= 1u;   /* DWT cycle counter on */
        D->t0 = (unsigned short)TICKS; D->last = (unsigned short)TICKS; D->rd0 = (unsigned short)R32(0x2000AB88u); D->our = D->all = 0; D->ticks = 0; D->gap = 0; D->qmax = 0;
        D->magic = DGMAGIC;
    }
    u32 now = TICKS & 0xFFFFu, g = (now - D->last) & 0xFFFFu; D->last = (unsigned short)now;
    if (g > D->gap) D->gap = cap7(g);
    u32 q = (R32(LEDQ_W) - R32(0x2000AB88u)) & 0x1FFu; if (q > D->qmax) D->qmax = cap7(q);
    D->ticks++;
    if (((now - D->t0) & 0xFFFFu) >= 100u) {
        u32 rd = R32(0x2000AB88u);
        dg_cc(20, D->qmax); dg_cc(21, (rd - D->rd0) & 0x1FFu); dg_cc(22, D->ticks); dg_cc(23, D->gap);
        dg_cc(24, D->our); dg_cc(25, D->all);
        D->t0 = (unsigned short)now; D->rd0 = (unsigned short)rd; D->our = D->all = 0; D->ticks = 0; D->gap = 0; D->qmax = 0;
    }
}
static void dg_ours(u32 cyc) { dg_t *D = (dg_t *)DGB; u32 v = cap7(cyc >> 8); if (D->magic == DGMAGIC && v > D->our) D->our = (u8)v; }
static void dg_all(u32 cyc) { dg_t *D = (dg_t *)DGB; u32 v = cap7(cyc >> 8); if (D->magic == DGMAGIC && v > D->all) D->all = (u8)v; }
#endif
void c274_wrap(void) {                   /* replaces bl c274 in the control loop */
#ifdef DIAG
    if (R8(KEYS_EN)) dg_begin();
    u32 c0 = CYCCNT;
#endif
    restore_tick();
    ma_tick();                           /* 2.79 (before the snapshot) */
    r27_tick();                          /* 2.60 (before the snapshot: it saves the 27 selection) */
    kb_blink_tick();                     /* 2.65 / 2.80: the keyboard LED blink, any mode */
    pluck_tick();                        /* 2.64 */
    if (R8(ST) == 0x5A) snapshot();
    ui_tick();
#ifdef DIAG
    u32 c1 = CYCCNT; dg_ours(c1 - c0);
#endif
    F_C274();
#ifdef DIAG
    dg_all(CYCCNT - c0);
#endif
}
