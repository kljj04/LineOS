# 빌드와 실행

[문서 목록](README.md) · [한국어](../ko_KR/Build.md) · [English](../en_US/Build.md)

## 준비 환경

기본 개발 환경은 Linux입니다. `clang`, `ld.lld`, `lld-link`, `make`, `qemu-system-x86_64`, `qemu-img`, `parted`, `mkfs.vfat`, `mkfs.ext2`, `losetup`과 OVMF 펌웨어가 필요합니다. 기본 실행에는 사용 가능한 KVM과 설정된 호스트 CPU 기능이 필요하며, 소프트웨어 에뮬레이션으로 자동 전환하지 않습니다.

## 빌드

프로젝트 루트에서 실행합니다.

```bash
make -j"$(nproc)"
```

결과물은 `LineOS/EFI/BOOT/BOOTX64.EFI`와 `LineOS/KERNEL/LINEOS_KERNEL.ELF`입니다. 빌드만으로 디스크 이미지 내부 파일이 갱신되지는 않습니다.

## 실행과 디스크

```bash
./Run.sh
```

실행기는 빌드 후 필요한 경우 8 GiB raw GPT 이미지를 만들고, 300 MiB FAT32 EFI 파티션과 EXT2 루트 파티션을 준비합니다. 부팅 파일을 복사한 뒤 QEMU를 시작합니다. loop 장치와 마운트 작업에는 `sudo` 또는 동등한 로컬 권한이 필요합니다.

기존 `LineOS/LineOS.img`는 재사용하며, 실행기가 이미지 내부 부팅 파일을 갱신합니다.

## 기본 VM 구성

| 항목 | 기본값 |
| --- | --- |
| 머신·펌웨어 | Q35·번들 OVMF UEFI |
| CPU·가속 | 호스트 CPU·16 vCPU·KVM |
| 메모리 | 4 GiB |
| 화면 | GTK·QHD 2560×1440·VirtIO GPU |
| 입력 | VirtIO 키보드·태블릿 |
| 디스크 | VirtIO block·raw GPT |
| 네트워크 | VirtIO-Net·QEMU 사용자 NAT |

`LINEOS_DISPLAY`로 화면 백엔드를 변경합니다. 나머지 기본값은 `Run.sh` 상단에 있습니다. NAT에는 호스트 브리지·TAP 설정이 필요하지 않습니다. 자세한 내용은 [네트워크](Network.md)를 참고합니다.

## 로그와 테스트 격리

QEMU 로그는 `Logs/QEMU.log`, 포트 `0xE9` 커널 출력은 `Logs/DebugCon.log`에 기록합니다. 표시되는 메시지는 실행 중인 커널 작업에 따라 달라집니다.

기본 실행기는 `UEFI/OVMF_VARS.fd`를 쓰기 가능한 펌웨어 상태로 사용합니다. 정상 VM 상태를 보존하는 테스트는 별도 디스크와 복사한 펌웨어 변수를 사용해야 합니다.
