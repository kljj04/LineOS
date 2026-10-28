# 아키텍처와 부팅

[문서 목록](README.md) · [한국어](../ko_KR/Architecture.md) · [English](../en_US/Architecture.md)

## 역할과 부팅 흐름

LineOS는 x86_64 UEFI 환경의 모놀리식 커널입니다. 부트로더가 ELF64 커널을 적재하고 UEFI 메모리 맵, ACPI RSDP 등 `LINEOS_BOOT_INFO`를 전달합니다. 커널 진입에는 Microsoft x64 ABI를 사용하며 레거시 BIOS 부팅 경로는 없습니다.

`InitKernel()`은 VFS를 초기화하고 GPT 2번 파티션을 EXT2 루트로, DeviceFS를 /dev에 연결합니다. `KMain()`은 키보드·마우스 소비자를 시작하고 /sbin/Init.lix를 정적 ELF64 유저 태스크로 적재한 뒤 LBPWRR을 시작합니다. Init은 글꼴을 게시하고 터미널 장치를 연 다음 fork와 자식의 execve로 /sbin/Terminal.lix와 /sbin/Login.lix를 실행합니다. Terminal은 /dev/fb0를 매핑하고 FD 3에서 읽은 출력을 표시합니다. Login은 인증 후 /bin/lsh.lix를 실행합니다. 자세한 제한은 [유저 프로그램](UserLibraries.md)을 참고합니다.

NetworkTask와 ClockNTPTask는 현재 시작 경로에서 등록하지 않습니다. 장치 초기화만으로 해당 작업이 시작되지는 않습니다. Wait Queue와 CPU 회계는 [스케줄러](Scheduler.md)를 참고합니다.

## SMP

MADT에서 CPU를 탐색하고 INIT-SIPI-SIPI와 저주소 트램펄린으로 AP를 시작합니다. AP는 16→32→64비트 전환 후 `APMain()`으로 진입합니다. 초기에는 BSP 페이지 테이블을 공유하며 AP별 커널 스택을 사용합니다. CPU별 시설을 초기화하고 online 상태를 기록한 뒤 스케줄러에 합류합니다. CPU 테이블 용량은 현재 256입니다.

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
| `UserPrograms/`, `LOSLibC/`, `Libraries/` | 유저 프로그램과 정적 라이브러리 |
| `Kernel/Include/`, `Link/` | 공개 인터페이스와 링커 설정 |

장치 초기화와 사용자에게 보이는 기능 실행은 별개입니다. 현재 기본 동작은 `Kernel/Kernel.c`를 기준으로 확인합니다.

날짜·시각과 경과 시간은 [시계·NTP](Clock.md)에서 설명합니다.

## 호스트 설정

실행기는 `Configs/QEMUSettings.sh`를 읽습니다. 빌드 플래그와 대상 탐색은 `Makefiles/BuildSettings.mk`, Bash 처리부와 Node.js 화면은 `Scripts/Backend/`·`Scripts/Frontend/`에 있습니다. 전체 현재 설정값은 [빌드와 실행](Build.md)에 정리했습니다.
