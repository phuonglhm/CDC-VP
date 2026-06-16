# PWM Firmware Test

Bare-metal RISC-V firmware to verify the PWM TLM peripheral inside the Bremen virtual platform.

## Test Path

RISC-V firmware
-> CPU TLM initiator
-> bus_router
-> PWM MMIO registers (base: 0x10050000)

## Build

From the repository root:

```bash
make -C fw/pwm_riscv
```

This produces `fw/pwm_riscv/pwm_test.elf`.

## Run

```bash
./build/bremen/platforms/tests/pwm_platform/pwm_platform \
  -c platforms/tests/pwm_platform/configs/default.yaml \
  --fw fw/pwm_riscv/pwm_test.elf \
  --sim-ms 5
```

## What the Test Does

1. Writes period=100, duty cycle=50, enable=1 to PWM registers
2. Reads back all three registers
3. Prints `PWM PASS` if values match, `PWM FAIL` otherwise

## Expected Output

PWM platform start
WRITE PWM_PERIOD [0x10050030] <= 0x00000064
WRITE PWM_DUTY0  [0x1005002C] <= 0x00000032
WRITE PWM_CFG    [0x10050008] <= 0x00000000
WRITE PWM_PARAM0 [0x10050014] <= 0x00000000
WRITE PWM_INVERT [0x10050010] <= 0x00000000
WRITE PWM_EN     [0x1005000C] <= 0x00000001
READ  PWM_PERIOD [0x10050030] => 0x00000064
READ  PWM_DUTY0  [0x1005002C] => 0x00000032
READ  PWM_EN     [0x1005000C] => 0x00000001

PWM PASS