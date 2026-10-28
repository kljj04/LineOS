# 빌드와 실행

[문서 목록](README.md) · [한국어](../ko_KR/Build.md) · [English](../en_US/Build.md)

기본 개발 환경은 Linux입니다. 아래 명령은 프로젝트 루트에서 실행합니다. 실행기는 `Scripts/Run.sh`, Make 진입점은 `Makefiles/Makefile`입니다.

## 필요한 도구

- 빌드에는 Bash, `make`, `clang`, `ld.lld`, `lld-link`, `llvm-ar`를 사용합니다.
- Builder 화면에는 `node`가 필요합니다. Node.js 기본 모듈만 사용하므로 npm 설치는 필요하지 않습니다.
- 디스크 준비에는 `sudo`, `truncate`, `parted`, `losetup`, `mkfs.vfat`, `mkfs.ext2`, `mount`, `umount`, `findmnt`, `fuser`,
  `install`과 Linux 기본 파일
  도구를 사용합니다.
- VM에는 `qemu-system-x86_64`, 접근 가능한 `/dev/kvm`, 설정된 호스트 CPU 기능과 번들 `UEFI/OVMF_CODE.fd`·`UEFI/OVMF_VARS.fd`가 필요합니다.
- 소스 정렬에는 `clang-format`을 사용합니다.

기본 실행기는 KVM 실패 시 소프트웨어 에뮬레이션으로 자동 전환하지 않습니다.

## 실행 명령

```bash
./Scripts/Run.sh --help
./Scripts/Run.sh -build
./Scripts/Run.sh
```

| 옵션                 | 동작                                                                         |
|----------------------|------------------------------------------------------------------------------|
| 옵션 없음 또는 `all` | 관리 마운트 해제 → 빌드·이미지 갱신 → QEMU → EFI·루트 재마운트입니다         |
| `-build`             | 증분 빌드만 수행합니다                                                       |
| `-image`             | 이미지가 없으면 생성하고 기존 빌드 결과물을 복사합니다. 빌드는 하지 않습니다 |
| `-m`, `-mount`       | 기존 이미지의 EFI·루트 파티션을 마운트하며 빌드나 파일 갱신은 하지 않습니다  |
| `-u`, `-umount`      | 관리 EFI·루트 마운트와 loop 연결을 해제합니다                                |
| `-qemu`              | 마운트 해제 → QEMU → 재마운트입니다. 빌드·이미지 갱신은 하지 않습니다        |
| `-clean`             | 빌드 오브젝트·프로그램 결과물과 두 부팅 결과물을 제거합니다. 디스크 이미지는 보존합니다      |
| `-format`            | 번들 EDK2를 제외한 프로젝트 C·헤더 소스를 직접 정렬합니다                    |
| `-h`, `--help`       | 도움말을 표시합니다                                                          |

Builder는 Make를 `-j1`로 순차 실행하며 `[전체/현재] 경로` 활성 행, 단계별 시간과 QEMU 종료 진단을 표시합니다. 비대화형 출력은 일반 행으로 기록합니다.

Node.js 화면 없이 직접 빌드할 때는 다음 명령을 사용합니다.

```bash
make -f Makefiles/Makefile -j1
```

결과물에는 `LineOS/EFI/BOOT/BOOTX64.EFI`, `LineOS/KERNEL/LINEOS_KERNEL.ELF`,
`LineOS/Programs/<프로그램 상대 경로>/<디렉터리명>.lix`가 포함됩니다. 오브젝트와 의존성 파일은 `Build/Objects/`에 생성합니다.
`UserPrograms/`를 재귀 탐색하여 `Program.cfg`가 있는 디렉터리를 프로그램 루트로 인식합니다. 전체 상대 경로로 구분하며,
하위 프로그램 루트의 소스는 부모 프로그램에 포함하지 않습니다. 실행 파일은 정적 ELF64 형식을 유지합니다.

```ini
CopyPath = /sbin/
Dependencies = TrueType, UserDB
Enable = true
```

키·값·쉼표 주변 공백을 허용합니다. 생략 시 `CopyPath=/bin/`, 빈 의존성 목록, `Enable=true`를 사용합니다.
LOSLibC는 항상 링크하며 선택 가능한 라이브러리 이름은 `TrueType`, `UserDB`, `LOSLibC`입니다.
알 수 없는 키·의존성, 중복 키, 잘못된 값은 오류로 처리합니다. `Enable=false`는 빌드와 배포에서 제외합니다.
이미 설치된 파일을 자동 삭제하지는 않습니다.

```bash
./Scripts/Run.sh -build System/Shell
make -f Makefiles/Makefile -j1 UserProgram Program=System/Shell
```

위 명령은 선택한 활성 프로그램과 필요한 라이브러리만 빌드합니다. 설치 경로는 `CopyPath/<디렉터리명>.lix`입니다.
모든 활성 프로그램 및 폰트의 설치 경로에 대해 동일 경로와 상하위 경로 충돌을 디스크 복사 전에 검사합니다.
전체·개별 빌드마다 `LineOS/Programs/`를 먼저 비우고 대상 실행 파일을 다시 링크합니다. 오브젝트와 라이브러리는 증분 빌드를 유지합니다.
개별 빌드 후에는 선택한 프로그램의 실행 파일만 남습니다. 현재 활성 설정에 대응하는 결과물만 복사하며 `-clean`은 오래된 결과물을 포함하여 `LineOS/Programs/`도 삭제합니다.
빌드만으로 이미지 내부 파일을 갱신하거나 QEMU를 실행하지는 않습니다.

## 설정 파일과 현재 값

`Scripts/Backend/Config.sh`가 `Configs/QEMUSettings.sh`를 Bash로 읽고 빌드·이미지·QEMU 작업 전에 검증합니다. VM 변경은 이 파일에서 수행하며 배열에는 Bash
배열 문법을 사용합니다. 이전 TOML 설정과 `LINEOS_DISPLAY` 재정의는 현재 실행기에서 사용하지 않습니다.

| 설정                                     | 현재 값                                                                 |
|------------------------------------------|-------------------------------------------------------------------------|
| `MachineType` / `MachineAcceleration`    | `q35` / `kvm`                                                           |
| `CPUModel` / `CPUCount`                  | `host` / `16`                                                           |
| `CPUFlags`                               | `+tsc-deadline`, `+invtsc`, `+rdtscp`, `+arat`, `+fsgsbase`, `+pdpe1gb` |
| `MemorySize`                             | `4G`                                                                    |
| `GraphicsDisplay` / `GraphicsResolution` | `gtk` / `2560x1440`                                                     |
| `GraphicsDevice` / `GraphicsHostMemory`  | `virtio-gpu-pci` / `1G`                                                 |
| `NetworkBackend` / `NetworkMAC`          | `user` / `52:54:00:4c:4f:53`                                            |
| `InputKeyboard` / `InputMouse`           | `virtio-keyboard-pci` / `virtio-tablet-pci`                             |
| `RTCBase` / `RTCClock`                   | `utc` / `host`                                                          |
| `DiskImage` / `DiskSize`                 | `LineOS/LineOS.img` / `8G`                                              |
| `DiskEFISizeMiB`                         | `300`                                                                   |
| `DebugFlags`                             | `int`, `guest_errors`, `cpu_reset`, `unimp`                             |
| `DebugNoReboot` / `DebugNoShutdown`      | `true` / `true`                                                         |

`DiskSize`와 `DiskEFISizeMiB`는 이미지가 없을 때 생성에만 사용합니다. 값을 바꿔도 기존 디스크 크기나 파티션은 변경하지 않습니다. QEMU 사용자 NAT에는 호스트 브리지나 TAP 설정이
필요하지 않습니다. 자세한 내용은 [네트워크](Network.md)를 참고합니다.

## 이미지와 권한

실행기는 데스크톱 사용자로 실행합니다. 전체 실행에서는 관리 마운트 해제와 빌드 전에 전경의 `sudo -v`로 인증하고 이후 디스크 작업은 비대화형 sudo로 처리합니다. QEMU는 데스크톱 사용자 권한으로
실행합니다.

이미지가 없으면 임시 raw GPT 디스크를 만들고 1 MiB부터 301 MiB까지 FAT32 ESP 1번 파티션, 나머지 공간에 EXT2 2번 파티션을 준비합니다. 기존 경로를 덮어쓰지 않는 방식으로 새 이미지를
배치합니다. 기존 이미지는 포맷 없이 재사용합니다. 갱신은 ESP에 `EFI/BOOT/BOOTX64.EFI`와 `KERNEL/LINEOS_KERNEL.ELF`를 복사하고 EXT2에 유저 프로그램과
`Assets/Fonts/*.ttf`를 배치합니다. 프로그램 위치는 `Program.cfg`를 따르며 기본값은 `/bin/`, 폰트 위치는 `/sys/fonts/`입니다. 기존 EXT2는 포맷하지 않습니다. 자세한
내용은 [프로그램 배치](UserLibraries.md)를 참고합니다. `-mount`로 연결한 관리 경로는 갱신 전에 마운트와 loop 연결을 해제합니다. 그 외의 기존 loop 연결은 거부합니다. 정리 단계에서
마운트와 loop 연결을 해제하며 실패 시 확인할 자원을 출력합니다.

## 로그와 펌웨어 상태

| 파일                                | 내용                                    |
|-------------------------------------|-----------------------------------------|
| `Logs/Build.log`                    | 빌드 출력                               |
| `Logs/Format.log`, `Logs/Clean.log` | 해당 작업의 출력                        |
| `Logs/Image.log`, `Logs/Mount.log`  | 이미지 준비·갱신 또는 수동 마운트입니다 |
| `Logs/Remount.log`                  | QEMU 종료 후 재마운트 결과입니다        |
| `Logs/QEMU.log`                     | 설정한 QEMU 진단                        |
| `Logs/DebugCon.log`                 | 커널 포트 `0xE9` 출력                   |
| `Logs/QEMUProcess.log`              | QEMU 프로세스 출력                      |

각 단계는 실행할 때 자신의 로그를 덮어씁니다. QEMU 로그는 시작 시 초기화하며 실제 메시지는 커널 작업 구성에 따라 달라집니다. 실행기는 매번 `UEFI/OVMF_VARS.fd`를 임시 디렉터리에 복사하여 원본
변수 템플릿을 보존합니다. 일반 실행의 디스크는 쓰기 가능하므로 디스크 상태를 보존해야 할 때는 별도 이미지를 사용합니다.

## 관련 소스

`Scripts/Run.sh`가 작업을 연결합니다. `Scripts/Backend/`는 설정·빌드·이미지·마운트·QEMU 처리를, `Scripts/Frontend/UI.js`는 화면 출력을 담당합니다. 컴파일·링크
설정과 대상은 `Makefiles/`, C·헤더 정렬 설정은 `.clang-format`에 있습니다.

## 수동 마운트

`./Scripts/Run.sh -mount`는 기존 이미지의 두 파티션을 마운트하고 종료 후에도 유지합니다. `-m`·`-Mount`·`-M`도 사용할 수 있습니다. EFI 파일시스템은 실행 사용자가 접근할 수
있게 연결하고 EXT2는 기존 소유권·권한을 유지합니다. 포맷이나 빌드 파일 복사는 하지 않으며 다른 프로세스가 이미지를 사용 중이면 거부합니다.

이후 `-mount`·이미지 갱신·전체 실행·`-qemu` 실행은 이 관리 경로를 먼저 해제합니다. 디렉터리나 파일을 사용 중이어서 해제할 수 없으면 작업을 중단합니다. `LineOS/Mount/EFI`·
`LineOS/Mount/Root` 밖의 마운트는 자동 해제하지 않습니다. `./Scripts/Run.sh -umount`로 관리 마운트와 loop 연결을 직접 해제할 수 있으며 `-u`도 대소문자 구분 없이
지원합니다.

전체 실행과 `-qemu`는 QEMU 종료 후 두 경로를 다시 마운트합니다. 실행 오류와 처리 가능한 SIGINT·SIGTERM도 포함하며 QEMU 프로세스를 종료·대기한 뒤 디스크를 마운트합니다. 필요하면 sudo
인증을 갱신하고 결과를 `Logs/Remount.log`에 기록합니다. 재마운트 실패는 오류로 알리며 기존 QEMU 실패 코드는 유지합니다. SIGKILL 같은 강제 종료에서는 정리 코드를 실행할 수 없으므로 남아
있는 QEMU 프로세스를 종료한 뒤 `-umount`로 남은 관리 마운트를 해제하고 다시 실행합니다.

수동 마운트와 QEMU 종료 후 마운트는 임시 `.Mount.*` 디렉터리에서 두 파티션을 먼저 연결한 뒤 완성된 디렉터리를 `LineOS/Mount`로 교체합니다. IDE 파일 감시가 감지할 수 있는 디렉터리 교체
이벤트를 발생시킵니다. 이전 마운트 경로는 빈 디렉터리만 제거하며 예상하지 못한 파일이 있으면 삭제하지 않고 작업을 중단합니다.
