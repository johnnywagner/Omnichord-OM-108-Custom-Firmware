# Touch chip: onsemi LC717A10PJ (strum plate + Instant OFF pad)

Collected 2026-10-06 from the 2.35dev register dump, the 2.35dev2 experiments and onsemi documents. How 2.37 uses it: `START_HERE.md` §4.

## Read-only register dump (2.35dev)

The recording was `235dev_calibration.mmon`: 81 s, 110 registers (0x1A–0x28, 0x2A, 0x2B and 0x40 not read). Reads are `reg|0x80` 2-byte transfers on LPSPI3, the same as the stock code. **Chip identified (2026-10-06, from the user's look inside):** marked "717A10P 9SN0B", on the strumplate PCB. This is the **onsemi LC717A10PJ**: a 16-channel capacitance-to-digital converter for touch sensors, with I2C/SPI. Datasheet facts: 8-bit measurement data; detection sensitivity 0.052–0.108 LSB/fF at minimum gain (so gain is adjustable); input offset adjustment ±8 pF (8-bit); measurement interval 30 ms typical (6 ms minimum); a "dynamic offset calibration" that gradually corrects the reference toward the measured value. That calibration probably explains why a held press drifted from 37 down to 33. The register description is not in the public datasheet.

## Register documentation found (2026-10-06)
The full LC717A10 register description is still missing. It's on Sekorm (doc 146623, VIP-only) and possibly in onsemi AND9197JP (403 from onsemi). Two public sources cover most of it:

- **onsemi LC717A30 Application Note, AND9346JP-D** (https://www.onsemi.com/pub/Collateral/AND9346JP-D.PDF): the full register map of the 8-channel sister chip.
- **onsemi LC717A Series Design Note** (https://static5.arrow.com/pdfs/2013/10/21/7/10/15/3/ons_/manual/132dnlc717a-d.pdf): covers the LC717A00/LC717A10. Its sample program confirms these A10 addresses:
  - 0x00/0x01 UseChannel 1/2;
  - **0x09 = Cin14/Cin15 2nd Gain** ("increase the GAIN when sensitivity is low");
  - 0x19 = Cin15 Threshold;
  - 0x2B = Result Data 2;
  - 0x2F Control 1 (WriteReq, ParaCh, StaCal);
  - 0x30 Average Count;
  - 0x31 Filter Parameter.

  The gain-tuning procedure is to adjust gain until a finger touch reads about 20–25.

Caution: these details came through an automated PDF summary, not a line-by-line read. Verify against a downloaded copy before writing anything.

### Our dump against the A30 map (tentative)
- **0x00/0x01 = 0xFE/0x7F:** UseChannel. Cin1–Cin14 are enabled. Cin14 is the Instant OFF plate: data 0x28, threshold 0x18, gain = low nibble of 0x09 (A30 puts the lower channel in the low nibble).
- **0x02–0x09 = 0x77: 2nd-stage gain code 7 = 8×.** The range is 1×–16× (code 0–15), so **gain can go up to 2× the stock setting**. That would put a full touch around 66–78 instead of 33–39, with 127 the ceiling.
- **0x0A–0x19:** per-channel ON threshold. The A10 has a single threshold per channel; the A30 has separate ON and OFF thresholds.
- **0x2F = 0x09 (stock writes 0x8D):** WriteReq (b7) + IntMode (b3) + ParaCh (b2) + Measure (b0). WriteReq and ParaCh self-clear, which matches the readback. **The parameter-update procedure:** write the registers, then 0x2F with ParaCh + WriteReq.
- **0x30 = 0x08: averaging = 8 samples, the minimum.** The design note recommends 0x80 (128 samples). Raising it should cut the ±1 jitter, but each measurement takes longer.
- **0x31 = 0x0C:** filter parameter, the design note's recommended value.
- **0x33 = 0x01 / 0xFE (stock "palm/noise" mode):** in the A30 map this is debounce count ON→OFF (value + 1 samples), so palm mode may hold releases for 255 samples. Unverified for the A10.
- **0x34/0x35 = 5/5:** short/long measurement interval. The A30 counts about 1 ms per LSB for the short interval.
- **0x39 = 0x40:** static offset-calibration CDAC base, 2 pF (the alternatives are 1 or 4 pF).
- **0x3A = 0x98:** measurement mode 1. In the A30 layout that is INTMD1 + DmCyc + LIVALB. Unverified for the A10.
- **0x3C = 24, 0x3F = 6:** unknown for the A10. The A30's 0x3C is the long-interval start count. The A10's dynamic offset-calibration threshold, cycle and count registers can't be at the A30 addresses (0x01, 0x36–0x38), so their location is unknown. Candidates are 0x3E/0x3F or 0x5B+.
- **0x40 bit 7:** soft reset (used by the stock recovery path `0x6000F858`). **Power-on reset restores defaults, and the stock code rewrites its full configuration at every boot (see below), so a power cycle undoes any experiment.**
- **0x5B–0x7A:** probably the A10's CDAC/offset and internal state. In the A30, the CDAC registers sit at 0x22–0x29, where the A10 has its data registers.

## Static registers (never changed during the recording)
| Reg | Value | Likely meaning |
|---|---|---|
| 0x00, 0x01 | 0xFE, 0x7F | **Channel-enable mask**: channels 1–14 on, channel 0 and channel 15 off. This matches the hardware: 13 plates on channels 1–13, Instant OFF on channel 14. |
| 0x02–0x09 | 0x77 ×8 | 16 nibbles = 7 for every channel: **2nd-stage gain 8×** (see above). |
| 0x0A–0x19 | 50, then 20 ×14, then 50 | **Per-channel touch threshold** (0x0A + channel): 20 for channels 1–14, 50 for the unused channels 0 and 15. This matches the measured touch-bit threshold of about 20 on the Instant OFF pad (channel 14 = reg 0x18). |
| 0x29 | 0 | level of channel 15 (unused) |
| 0x2F | 9 | Control 1 (see above) |
| 0x30, 0x31 | 8, 12 | averaging count, filter parameter |
| 0x33 | 1 | the stock "palm/noise" mode register (written 0x01/0xFE) |
| 0x34, 0x35 | 5, 5 | measurement intervals |
| 0x39, 0x3A, 0x3C, 0x3F | 64, 152, 24, 6 | static calibration base, measurement mode, ?, ? |
| 0x5B, 0x5C | 128, 1 | ? |
| 0x69, 0x6B | 3, 112 | ? |
| 0x7B–0x7F | 3, 4, 54, 6, 22 | possibly ID/version bytes |
| All others listed | 0 | |

## Changing registers (sampled about every 0.3–1 s, so too coarse to correlate with single touches)
- **0x5D:** 0–255, looks random (a counter or noise).
- **0x5E, 0x70–0x76, 0x78:** single-bit patterns (1, 2, 4 … 128) that rotate. These look like scan/state pointers.
- **0x6A:** 96–99 (slow, perhaps a reference or temperature).
- **0x6C:** 32–37 or 56. **0x6D:** 72 or 91–96. **0x7A:** 1 or 3. These alternate about every second, regardless of touch.

## Conclusions
- No higher-resolution raw-count register is visible. The 8-bit level registers 0x1A+ch remain the only usable reading.
- **The best lever is the Cin14 2nd gain** (low nibble of 0x09, 7 → up to 15): about 2× the counts across a touch. Next is **averaging** (0x30): less jitter, at the cost of speed.
- **Remaining unknowns:** where the A10 keeps its dynamic-calibration controls, and the exact bit layout of 0x3A.
- **Done:** the experiment build 2.35dev2 measured gain 8–16× (results below). 2.36+ uses 16× on Cin14 in Vibrato mode only.

## Stock chip set-up, from the firmware itself (2026-10-06)
The stock code **does** write the chip's full configuration at every power-on. The earlier note that it "never writes 0x02–0x19" was wrong. The set-up routine is `0x6000ED18`, called at the start of the touch task and from the error-recovery routine `0x6000F858`.

**Sequence:**
1. 0x2F = 0x88 (halt), busy delay 0x320, 0x2F = 0x88 again.
2. Poll 0x2F until bits 7, 2 and 1 clear.
3. Write the configuration below.
4. 0x2F = 0x8F (load parameters + static calibration + measure).

**Values written:**
- **Channels and gain:** 00=FE, 01=7F, 02–09=77.
- **Thresholds:** 0x0B–0x18 = 0x14 (20 decimal). 0x0A and 0x19 are not written; they read 50.
- **Global:** 30=08, 31=0C, 32=00, 33=01, 34=05, 35=05, 36=00, 37=00, 38=00, 39=40, 3A=98, 3B=00, 3C=18, 3D=00.

From 0x30 to 0x3D this matches the LC717A30 register layout one for one:
- averaging;
- filter;
- debounce ON/OFF;
- short/long interval;
- **dynamic offset-calibration cycle and counts (0x36–0x38), all set to 0 by stock;**
- static-calibration base;
- measurement modes 1 and 2;
- long-interval start count;
- 1st gain.

**Palm check (`0x6000F90C`, every touch cycle):** if 10 or more of plates 1–13 read above 20, it writes 0x33 = 0xFE, otherwise 0x01. It then writes 0x2F = 0x8D (a live parameter load). The value last written is kept at 0x200009E0.

**SPI write format:** tx = {reg, value}. Read format: tx = {reg|0x80, 0}.

**2.35dev2 (experiment build):** it replays this exact routine with only 0x09, 0x18, 0x30 and 0x34 changed (see `claude/mod2_35dev2_flash_guide.md`).

## 2.35dev2 run 1 (`235dev2_experiments.mmon`, 2026-10-06): writes confirmed, pad data missing
Only MIDI channel 16 was recorded (the channel filter in MIDI Monitor), so the channel 13 pad readings and the channel 15 values (including reg 0x09 ≥ 128) are missing.

What channel 16 does show:
- **Every experiment was taken by the chip.** Reg 0x18 went 20→25→30→35→40, reg 0x30 went 8→16→32 and reg 0x34 went 5→1, at the expected times. Reg 0x2F briefly read 11 (busy) during one apply.
- **A power cycle restored every stock value** (0x18 = 20, 0x30 = 8, 0x34 = 5, 0x09 = 0x77). This confirms the writes are volatile.
- **The strumplate sounded normal throughout** (user report).
- **Registers 0x5D–0x7A** behaved the same in every experiment, so they don't track gain.

A re-record with all channels is needed.

## 2.35dev2 run 2 (`235dev2_experiments2.mmon`, 173 s, all channels): RESULTS
User request: experiments 6–8 are skipped, and no future build changes averaging or interval (they affect the strumplate). Each experiment applied in 7–47 status polls, with no timeouts.

| Exp | Gain (reg 0x09) | Threshold | Hands off | Full contact (median, p10–p90) | Jitter (sd) | Drift over an ~8 s hold |
|---|---|---|---|---|---|---|
| stock | 8× | 20 | 0–6 (mean 0.2) | 39 (39–40) | 0.57 | 40 → 38 |
| 1 | 8× | 20 | 0–6 | 39 (38–39) | 0.44 | 39 → 38 |
| 2 | 10× | 25 | 0–6 | 49 (49–49) | 0.51 | 50 → 47 |
| 3 | 12× | 30 | 0–5 | 58 (57–59) | 0.86 | 59 → 57 |
| 4 | 14× | 35 | 0–6 | 65 (64–66) | 0.68 | 66 → 64 |
| 5 | 16× | 40 | 0–7 | 73 (72–73) | 0.70 | 74 → 72 |

**Findings:**
- **Reg 0x09 really is the Instant OFF gain** (code + 1 = gain). The reading scales almost linearly: 16× gives 1.87× the stock counts. The strumplate was unaffected (user report).
- **The noise doesn't grow with the gain** (hands off 0–7, jitter about ±1). So 16× gives **about 1.9× finer resolution** across a touch, with no saturation (the ceiling is 127).
- **Drift during a hold is small at every gain:** about 2–3 % over 8 s.
- **Chip measurement rate:** 252 or more per second at every setting. CC 3 was capped at 126 per 0.5 s.
- **Recommendation for the next build:** apply gain 16× and threshold 40 to the Instant OFF channel only, once after the stock set-up (and again if the stock recovery routine re-sets the chip). The coverage range used by the pad-driven modes becomes about 0–73 instead of 0–39. That's what 2.36/2.37 do. In 2.37, mode 4 is finger rock, unrelated to the old 2.35 mode 4.

## Still unknown / ideas
- **Dynamic offset calibration on the A10.** The A30's threshold, cycle and count registers sit at 0x01 and 0x36–0x38. On the A10, 0x01 is UseChannel 2, and the stock set-up writes 0x36–0x38 = 0, so dynamic calibration is probably off in short-interval mode. The 2–3 % drift during a hold may just be the finger. Candidates for the A10's dynamic-calibration threshold: 0x3E/0x3F (0x3F reads 6).
- **The exact meaning of 0x3A = 0x98** on the A10 (A30 layout: INTMD1 + DmCyc + LIVALB).
- **The user declined global changes** (averaging 0x30, interval 0x34) because they affect the strum plate. Experiments 6–8 of 2.35dev2 (averaging 16/32, interval 1 ms) were never recorded.
- **Experiment tooling:** the 2.35dev2 experiment code (apply any register set via the stock set-up replay, stream pad readings as MIDI CCs on channel 13, rate-limited) was kept in the original dev setup (not included). Reuse it for any new chip measurement.
