# Builds our code for every factory firmware version in lib/stock.py (or only the one given: bash build.sh 30).
# 1.0.2.9: mod2.bin / mod2.elf, mod1.bin; another version NN: mod2_vNN.bin / .elf, mod1_vNN.bin. Each from the same sources,
# with that version's stock addresses (stock_vNN.h, written here from lib/stock.py).
set -e
S=${OM108_STOCKPY:-../lib/stock.py}; [ -f "$S" ] || S=stock.py    # the table: the dev kit's lib/, or next to this file (a copy)
for v in ${1:-$(python3 $S versions)}; do
  s=; [ "$v" = 29 ] || s=_v$v
  python3 $S header $v mod2.c entry.s mod1.s mod1_syms.s > stock_v$v.h
  clang --target=thumbv7em-none-eabi -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -nostdlib -fno-pic -mno-unaligned-access -Wall ${DIAG:+-DDIAG} -include stock_v$v.h -c mod2.c -o mod2$s.o
  clang --target=thumbv7em-none-eabi -mcpu=cortex-m7 -mthumb -x assembler-with-cpp -include stock_v$v.h -c entry.s -o entry$s.o
  ld.lld -T link.ld entry$s.o mod2$s.o -o mod2$s.elf 2>/dev/null
  llvm-objcopy -O binary --only-section=.text mod2$s.elf mod2$s.bin
  test -z "$(llvm-nm -u mod2$s.elf)"
  llvm-objdump -h mod2$s.elf | grep -qE " \.(bss|data) +0*[1-9a-f]" && { echo "has data/bss!"; exit 1; } || true
  clang --target=thumbv7em-none-eabi -mcpu=cortex-m7 -mthumb -x assembler-with-cpp -include stock_v$v.h -c mod1.s -o mod1$s.o
  clang --target=thumbv7em-none-eabi -mcpu=cortex-m7 -mthumb -x assembler-with-cpp -include stock_v$v.h -c mod1_syms.s -o mod1_syms$s.o
  ld.lld -Ttext=0x60039000 mod1$s.o mod1_syms$s.o -o mod1$s.elf 2>/dev/null
  llvm-objcopy -O binary --only-section=.text mod1$s.elf mod1$s.bin
  echo "$(wc -c < mod2$s.bin | tr -d ' ') bytes$( [ "$v" = 29 ] || echo " (factory firmware 1.0.3.0 = v$v: mod2$s.bin)")"
done
