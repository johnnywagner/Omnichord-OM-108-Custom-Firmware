set -e
clang --target=thumbv7em-none-eabi -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -nostdlib -fno-pic -mno-unaligned-access -Wall ${DIAG:+-DDIAG} -c mod2.c -o mod2.o
clang --target=thumbv7em-none-eabi -mcpu=cortex-m7 -mthumb -c entry.s -o entry.o
ld.lld -T link.ld entry.o mod2.o -o mod2.elf 2>/dev/null
llvm-objcopy -O binary --only-section=.text mod2.elf mod2.bin
test -z "$(llvm-nm -u mod2.elf)"
llvm-objdump -h mod2.elf | grep -qE " \.(bss|data) +0*[1-9a-f]" && { echo "has data/bss!"; exit 1; } || true
ls -l mod2.bin | awk '{print $5" bytes"}'
