# UniXNU-ace6

OnePlus Ace 6 (**PLQ110** / `OP6113L1`) device support for
[UniXNU](https://github.com/UniXNU/UniXNU).

## Identity

| Field | Value |
| --- | --- |
| Model | `PLQ110` |
| Device name | `OP6113L1` |
| Board model string | `Qualcomm Technologies, Inc. Sun MTP,ktm-g_ti` |
| SoC | SM8750 (`sun`), soc_id 618 (`0x26a`), revision 2.0 |
| oplus project | `0x6113` = 24851 |
| board-id | `0x50008 0x00` |
| GPU | Adreno830v2, chipid `0x44050001`, no driver available |
| RAM | 12 GiB physical, 10.85 GiB `MemTotal`, 16-segment map |
| Panel | AA605_P_7_A0020 (tianma), 1272x2800, DSC, cmd mode |
| Touch | Synaptics S3910 on `qupv3_se4_spi` (`0xA90000`) |

## Provenance

Nothing here was guessed. Every hardware value came off this device:

- `/sys/firmware/fdt` (1401819 B) — base DT with overlays merged
- `dtbo_b` — DT_TABLE, 10 entries, all `board-id 0x50008`
- `vendor_boot_b` — dtb at `0x11a1000`, 13 concatenated FDTs
- `boot_b`, `init_boot_b` — header v4, GKI layout
- GPT on `sda` — 4096-byte logical sectors, 32 entries

Full tables in [docs/hardware.md](docs/hardware.md).

## Status

**Nothing has been booted.** This is a skeleton built from measured data plus a
mechanical port of the OnePlus 13T drivers.

| Area | Status |
| --- | --- |
| Configuration | complete, statically validated |
| Device tree | compiles with dtc 1.8.1 |
| Board constants | compile-checked |
| Drivers | renamed and ported, not compiled against XNU |
| Firmware | **not written** — needs `ace6.dsc` and a Mu patch |
| CPU | DTS present, untested |
| Memory | 2 windows declared, untested |
| Storage | UFS node with SID 0x60, untested |
| Display | panel params measured, DCS init sequence missing |
| Input | S3910 protocol driver ready, SMMU SID unknown |
| GPU | **no support** |

## Layout

```
board.toml                    device configuration
feeds.toml                    pins UniXNU-sm8750-common b79d675
configs/ace6_defconfig        kernel configuration
dts/ace6.dts                  board root, CPU, GPU, memory windows
dts/ace6-touch.dtsi           S3910 wiring
dts/ace6-panel.dtsi           AA605 timings and DSC
dts/sm8750.dtsi               from the SoC feed
package/Ace6Peripherals.h     every board constant, value + source
drivers/                      ported from UniXNU-Pagani-Drivers
kits/                         NativePrelink, Ace6VerifiedRoot
docs/hardware.md              measured hardware
docs/diff-vs-13t.md           node-by-node comparison with the 13T
docs/driver-reuse.md          what could be reused and why
docs/touch-bringup.md         how to find the SMMU SID
```

## Differences from the OnePlus 13T

Closest reference is `IssacL04/UniXNU-Pagani_SM8750` (PKX110).

Identical and reused as-is: `0x980000000` 6 GiB memory segment, `chipid
0x44050001`, GMU registers, GPU SMMU, apps-SMMU QTB quotas, UFS SID `0x60`,
touch SE base `0xA90000`, SPI clock 19 MHz, CS setup 50, tlmm pins 48-51 and
reset/attn 161/162, DSI base `0xae94000`.

Different and changed here: panel is AA605 not AA594; brightness range 0xffe
not 0xdbb; touch drive strength `{6,4,6,4,3,3}` not `{2,1,2,1,3,3}`; power is a
single fixed 1.8 V L4B rail instead of dual RPMh VRMs; userdata LBA
`4602604`–`61655034` not the 64 GiB layout; product model `PLQ110`.

## Build

```sh
git clone --recurse-submodules https://github.com/UniXNU/UniXNU
cd UniXNU
./unixnu defconfig ace6_defconfig -D ../UniXNU-ace6
./unixnu fetch
./unixnu status --json
```

## Known unknowns

| Item | Why |
| --- | --- |
| Touch SMMU SID | not in the device tree; probe at runtime |
| AA605 DCS init sequence | only DT params captured, command stream still in stock kernel |
| `L4B` default power state | assumed on at PMIC default, unverified |
| Button PPIDs | inherited from the shared PM8550 topology, unverified here |
| root volume UUID | placeholder, filled from the first successful boot |
