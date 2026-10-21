# 아키텍처와 부팅

[문서 목록](README.md) · [한국어](../ko_KR/Architecture.md) · [English](../en_US/Architecture.md)

## 역할과 부팅 흐름

LineOS는 x86_64 UEFI 환경의 모놀리식 커널입니다. 부트로더가 ELF64 커널을 적재하고 UEFI 메모리 맵, ACPI RSDP 등 `LINEOS_BOOT_INFO`를 전달합니다. 커널 진입에는 Microsoft x64 ABI를 사용하며 레거시 BIOS 부팅 경로는 없습니다.

`InitKernel()`은 물리·가상 메모리와 힙, TrueType, PCI와 VirtIO GPU, GDT·IDT, LAPIC·HPET·TSC 타이머, UTC 시계, SMP, LBPWRR, VirtIO 블록·네트워크·입력을 초기화합니다.

현재 `KMain()`은 프레임 값을 화면에 출력하고 네트워크·NTP·시계 화면 작업을 등록한 뒤 스케줄러를 시작합니다. 예전 셸·파일시스템 테스트·웹 뷰어는 기본 작업에 포함되지 않습니다.

## SMP

MADT에서 CPU를 탐색하고 INIT-SIPI-SIPI와 저주소 트램펄린으로 AP를 시작합니다. AP는 16→32→64비트 전환 후 `APMain()`으로 진입합니다. 초기에는 BSP 페이지 테이블을 공유하며 AP별 커널 스택을 사용합니다. CPU별 시설을 초기화하고 online 상태를 기록한 뒤 스케줄러에 합류합니다. CPU 테이블 용량은 현재 256이며, 16 vCPU QEMU 실행 검증 기록이 있습니다.

## 인터럽트와 타이머

IDT의 예외·인터럽트 처리, 커널 panic, LAPIC 초기화와 EOI, HPET, TSC 보정과 Deadline Timer를 제공합니다. 타이머 기반 작업 전환은 [스케줄러](Scheduler.md)에서 설명합니다.

## 소스 구조

| 위치 | 역할 |
| --- | --- |
| `BootLoader/`, `Common/` | UEFI 부팅과 공통 전달 구조 |
| `Kernel/Kernel.c` | BSP/AP 진입과 기본 작업 |
| `Kernel/Source/Arch/AMD64/` | CPU와 동기화 기본 기능 |
| `Kernel/Source/ACPI/`, `Multicore/` | 펌웨어 테이블과 CPU 기동 |
| `Kernel/Source/Interrupt/`, `Timer/` | 인터럽트와 시계 |
| `Kernel/Source/Memory/`, `Scheduler/` | 메모리와 작업 실행 |
| `Kernel/Source/PCI/`, `VirtIO/` | 장치 공통 기반 |
| `Kernel/Source/Disk/`, `FS/` | 블록 장치와 파일시스템 |
| `Kernel/Source/Network/`, `Render/`, `Input/` | 네트워크·그래픽·입력 |
| `Kernel/Include/`, `Link/` | 공개 인터페이스와 링커 설정 |

장치 초기화와 사용자에게 보이는 기능 실행은 별개입니다. 현재 기본 동작은 `Kernel/Kernel.c`를 기준으로 확인합니다.

날짜·시각과 경과 시간은 [시계·NTP](Clock.md)에서 설명합니다.
