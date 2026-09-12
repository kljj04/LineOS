# Third-Party Notices

LineOS includes third-party fonts for kernel text rendering tests.

## JetBrains Mono Nerd Font

- File: `kernel/assets/fonts/JBMNFSB.ttf`
- Base font: JetBrains Mono
- Patched font: Nerd Fonts
- License: SIL Open Font License 1.1
- License file: `LICENSES/OFL-1.1`
- Upstream: https://www.jetbrains.com/lp/mono/
- Source: https://github.com/JetBrains/JetBrainsMono
- Nerd Fonts: https://www.nerdfonts.com/

JetBrains Mono is distributed under the SIL Open Font License 1.1. The bundled file is a Nerd Fonts patched variant and must remain under the applicable font license terms.

## Pretendard

- File: `kernel/assets/fonts/PTDSB.ttf`
- Font: Pretendard SemiBold
- License: SIL Open Font License 1.1
- License file: `LICENSES/OFL-1.1`
- Source: https://github.com/orioncactus/pretendard

Pretendard is distributed under the SIL Open Font License 1.1.

## EDK II (Headers)

- Project: EDK II
- Copyright: Copyright (c) TianoCore and contributors
- License: BSD 2-Clause Plus Patent License
- SPDX Identifier: BSD-2-Clause-Patent
- License file: `LICENSES/BSD-2-Clause-Patent`
- Upstream: https://github.com/tianocore/edk2
- Project website: https://www.tianocore.org/

LineOS uses headers and interfaces from EDK II for its UEFI boot environment.
The applicable EDK II files are distributed under the BSD 2-Clause Plus Patent License.

## OVMF Firmware

- Project: OVMF (Open Virtual Machine Firmware)
- Part of: EDK II
- Upstream: https://github.com/tianocore/edk2
- Project website: https://www.tianocore.org/

LineOS includes OVMF firmware images for development and testing with QEMU.
The firmware images are third-party components and are not covered by the
LineOS project license.

See the applicable upstream license notices for the EDK II components used
to build these firmware images.