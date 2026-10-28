# 유저 프로그램과 라이브러리

[문서 목록](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## 프로그램 배치

프로그램별 `UserPrograms/<ProgramName>/Program.conf`에 다음 값을 지정합니다.

```ini
CopyPath=/usr/bin/
```

이미지 갱신 시 `LineOS/Programs/<ProgramName>.lix`를 EXT2의 `/usr/bin/<ProgramName>.lix`에 복사합니다. ELF64 내용과 이름을 유지합니다. 설정 파일이 없으면
`/bin/`을 사용합니다. 루트 `/`도 허용합니다. 설정 파일이 존재하면 `CopyPath` 한 개가 필요하며 빈 줄과 `#`으로 시작하는 주석을 허용합니다. CRLF도 지원합니다. 설정은 셸 코드로 실행하지
않습니다.

경로 구성 요소에는 영문자, 숫자, `_`, `-`, `.`만 허용합니다. 상대 경로, 독립적인 `.` 및 `..`, 중복 슬래시, 중복 키, 알 수 없는 키, 빈 값, 설정 파일 심볼릭 링크를 거부합니다. 프로그램
디렉터리 이름에는 영문자, 숫자, `_`, `-`를 허용합니다. 대상 디렉터리의 심볼릭 링크 및 별도 마운트, 대상 파일의 심볼릭 링크와 하드 링크를 거부합니다. 기존 이미지 포맷과 마운트 수명 관리는 변경하지
않습니다. 경로를 변경해도 이전 위치의 파일은 자동 삭제하지 않습니다. 이미지 갱신은 게스트가 실행되지 않고 다른 프로세스가 이미지 내용을 동시에 변경하지 않는 기존 관리 흐름을 전제로 합니다.

## 유저 TrueType

`Libraries/Include/TrueType`과 `Libraries/Source/TrueType`은 커널 포트를 기반으로 한 유저 전용 TrueType 구현입니다.
`Build/Libraries/libtruetype.a`를 생성하며 `Program.cfg`의 `Dependencies`에 `TrueType`을 명시한 프로그램에 링크합니다. 실제 사용한 오브젝트만 ELF에 포함합니다. 커널의 PMM, VirtIO GPU, 잠금,
폰트 심볼을 참조하지 않습니다. 커널 TrueType 및 패닉 출력은 유지합니다. 커널 콘솔은 제거했습니다. Terminal 유저 프로그램이 터미널 master 디스크립터에서 받은 바이트를 렌더링하며 일반 프로그램은
`read`와 `write`를 사용합니다.

`TrueType/TrueTypeEngine.h`를 포함하고 다음 리소스를 호출자가 준비합니다.

- 신뢰할 수 있는 TTF 데이터 버퍼. 사용할 폰트마다 하나씩 준비합니다
- `TTF_ALLOCATOR`의 `Allocate`, `Free`, `Context`: 정렬된 메모리를 충분히 제공하고 개별 할당을 반환하는 콜백
- `TRUE_TYPE_SURFACE`: `UINT32` BGRA8888 픽셀 버퍼와 폭·높이. 행 패딩 없이 `폭 * 높이 * 4` 바이트 이상을 할당합니다.

`TrueTypeInit(&Allocator, &Surface)`는 할당자와 화면 버퍼를 설정하며 파일을 읽지 않습니다. 각 폰트 데이터를 `TRUE_TYPE_FONT Font = {0};`와
`TrueTypeRegisterFont(&Font, FontData)`로 등록한 뒤 `DrawTrueTypeCodepoint(&Font, ...)`, `DrawTrueTypeText(&Font, ...)`,
`MeasureTrueTypeText(&Font, ...)`에 전달합니다. 폰트마다 별도 객체를 사용하며 데이터 버퍼와 객체는 사용이 끝날 때까지 유지해야 합니다. 폰트 자동 대체는 없습니다. 텍스트는
`CHAR16` 배열이며 UTF-16 서로게이트를 처리합니다. 모든 리소스는 사용 중 유효해야 합니다. 저수준 `FONT_INFO` API를 직접 사용한다면 `InitFont` 이후 `Info.UserData`에
`TTF_ALLOCATOR *`를 지정해야 합니다. 팩 API에는 해당 할당자를 `AllocContext`로 전달합니다.

렌더링 결과는 CPU 메모리에만 기록됩니다. 기존
`DisplayPresent(Surface.FrameBuffer, (UINT64) Surface.FrameBufferWidth * Surface.FrameBufferHeight * 4)`로 표시할 수 있습니다.
호출자가 Display 화면 크기를 조회하고 크기·피치 및 버퍼 용량을 확인해야 합니다. 라이브러리가 디스플레이 syscall을 직접 호출하거나 커널 화면 소유권을 변경하지 않습니다.

현재 엔진은 프로세스별 단일 화면·할당자 상태를 사용하므로 호출을 직렬화해야 합니다. 신뢰하지 않는 TTF 입력의 길이 검증이나 메모리 부족에서의 완전한 복구는 보장하지 않습니다. 기존 수학 근사와 단일 행 중심
측정 동작을 유지합니다. Terminal이 자체 라이브러리 상태를 초기화하고 Init이 등록한 폰트를 매핑합니다. Shell은 TrueType이나 디스플레이 API를 사용하지 않습니다.

## 파일과 프로세스 생성

이미지 갱신은 `Assets/Fonts/*.ttf`를 EXT2 `/sys/fonts/`에 원래 이름과 0644 권한으로 복사합니다. `fcntl.h`의 `open(Path, O_RDONLY)`와 `unistd.h`의
`read`·`close`로 읽습니다. 태스크당 16개 FD 슬롯에서 파일과 터미널 연결을 함께 관리합니다. 일반 파일은 지원 플래그로 읽기·쓰기·생성·잘라내기를 제공합니다. 오류는 -1과 `errno`, EOF는 0을 반환합니다.
매번 경로와 노드 식별자를 다시 확인하므로 이름 변경·삭제 이후에도 열린 파일을 유지하는 POSIX 의미론은 구현하지 않았습니다. 실행 시 제공된 VFS 문맥으로 권한을 검사하며 Init은 현재 관리자 문맥을
사용합니다.

`fork()`는 호출자를 복제하고 부모에 자식 PID, 자식에 0을 반환합니다. 자식은 `execve(Path, Argv, Envp)`로 이미지를 교체하며 PID, 상속된 열린 파일 객체, 자격 정보와 cwd를 유지합니다. Init, Login, UserRescue와 Shell 모두 이 순서를 사용하며 spawn/spawnv 확장은 제거했습니다. execve 실패는 자식에 -1과 errno를 반환하며 각 호출부는 오류를 출력하고 127로 종료합니다. waitpid/wait로 자식 종료 상태를 수집하며 셸은 외부 자식이 종료된 뒤 프롬프트를 출력합니다. 자식 실행 기록은 최대 16개이며 이후 fork에서 종료 기록을 정리합니다. 에셋 게시 권한은 fork 자식에 상속하지 않습니다. 태스크별 SSE 상태 보존은 미구현이며 동시 부동소수점·SIMD 실행은 검증하지 않았습니다. TrueType 상태와 캐시는 독립적이며 원본 글꼴 에셋은 공유합니다.

## 공용 에셋

`Asset.h`에서 다음 함수를 제공합니다. 성공은 0이며 실패하면 `errno`를 설정하고 -1을 반환합니다.

- `LOSAssetPublish(Name, Data, Size)`: 부팅 Init만 호출할 수 있습니다. 원본을 커널 소유 페이지로 한 번 복사하므로 반환 후 원본 버퍼를 재사용할 수 있고 Init 종료에도
  유지됩니다. 이름은 비어 있지 않은 최대 63바이트 문자열입니다. 이름은 불변이며 중복 등록과 교체를 거부합니다.
- `LOSAssetAcquire(Name, &Asset)`: 호출 프로세스에 같은 물리 페이지를 읽기 전용으로 매핑하고 `LOS_ASSET`의 `Data`, `Size`, `Handle`을 반환합니다. 같은
  프로그램에서 같은 이름을 다시 획득하면 같은 매핑의 참조 수를 증가시킵니다. 각 획득에는 별도 구조체를 사용하고 각각 해제합니다. 구조체를 복사하는 것만으로 참조가 늘어나지는 않습니다.
- `LOSAssetRelease(&Asset)`: 해당 프로세스의 참조를 해제합니다. 마지막 참조에서 매핑을 제거하고 호출자의 구조체를 비웁니다. 다른 프로세스의 매핑과 등록된 원본은 유지합니다. 프로세스 주소
  공간 파괴 시 남은 매핑과 페이지 테이블도 정리하며 커널의 에셋 원본은 해제하지 않습니다.

등록 상한은 16개, 개별 크기는 16MiB, 원본 페이지 총량은 페이지 반올림 후 64MiB입니다. 프로세스별 서로 다른 에셋 매핑은 8개입니다. 이 상한과 별도로 페이지 테이블·핸들 메타데이터가 사용됩니다. 첫
버전은 재부팅까지 유지하며 등록 해제·교체·동적 퇴출을 제공하지 않습니다. 읽기 전용 영역에 쓰면 보호 예외가 발생합니다. 프로그램은 `Size` 밖의 데이터를 사용하면 안 됩니다.

```c
LOS_ASSET Asset = {0};

if (LOSAssetAcquire("Fonts/JetBrainsMono", &Asset) == 0)
{
    TrueTypeRegisterFont(&Font, Asset.Data);
}
```

폰트를 사용하는 동안 매핑을 유지하고, 폰트 객체와 글리프 캐시가 해당 데이터를 더 참조하지 않을 때 해제합니다. TTF 바이트만 공유하며 `TRUE_TYPE_FONT`, 할당자, 화면, 글리프 캐시는 각 프로세스가
소유합니다. 에셋 API는 파일 형식을 검증하지 않습니다.

## 시스템 호출 인터페이스

현재 `syscall`/`sysretq` 호출은 종료, 읽기, 쓰기, 열기, 닫기, PID 조회, 화면 정보·표시, fork/execve, chdir, stat/lstat/fstat와 에셋 등록·획득·해제를 제공합니다. yield는 스케줄러에 CPU를 양보하고 성공 시 0을 반환합니다.
파일과 프로세스 인터페이스는 여기서 설명한 부분집합입니다. 커널은 Linux의 음수 오류 번호를 반환하고 libc는 -4095~-1을 `errno`로 변환하여 -1을 반환합니다. `LOSOutputStreamRead`는 제거했습니다. 번호 10은 Linux `mprotect`용으로 비워 두며 현재는 지원하지 않습니다.

`RAX`는 시스템 호출 번호와 반환값을 전달하며, 인자는 `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`를 사용합니다. `RCX`와 `R11`은 보존하지 않습니다.
각 CPU에서 진입 MSR을 설정하고 현재 태스크의 커널 스택으로 전환합니다. 기존 사용자 호출용 `int 0x80` 게이트는 제거했습니다. 표준 호출 번호는 Linux x86-64와 일치합니다. LineOS 전용 호출은 1024번부터 사용하며, 기존 화면·에셋 호출은 유지합니다. 장치 `ioctl`, 익명 private `mmap`, `/dev/fb0` shared `mmap`을 구현했으며 [DeviceFS와 프로세스 ABI](DeviceFS.md)에 범위를 설명합니다.

| 호출 | 번호 |
| --- | --- |
| `read` | 0 |
| `write` | 1 |
| `open` | 2 |
| `close` | 3 |
| `stat` | 4 |
| `fstat` | 5 |
| `lstat` | 6 |
| `mmap` | 9 |
| `munmap` | 11 |
| `ioctl` | 16 |
| `sched_yield` | 24 |
| `getpid` | 39 |
| `fork` | 57 |
| `execve` | 59 |
| `exit` | 60 |
| `wait4` | 61 |
| `getcwd` | 79 |
| `chdir` | 80 |
| `rename` | 82 |
| `mkdir` | 83 |
| `setuid` | 105 |
| `setgid` | 106 |
| `getdents64` | 217 |
| `getrandom` | 318 |
| `display_info` | 1024 |
| `display_present` | 1025 |
| `display_present_rect` | 1026 |
| `asset_publish` | 1027 |
| `asset_acquire` | 1028 |
| `asset_release` | 1029 |
| `remove (LineOS)` | 1032 |

`remove`는 파일과 디렉터리를 모두 처리하므로 LineOS 전용 호출로 유지합니다. Linux의 `unlink` 또는 `rmdir`에 해당하지 않습니다.
지원하는 호출 번호, 오류 번호와 화면 정보 구조체의 바이너리 배치를 맞추었으나 Linux 실행 파일 전체와 호환되는 단계는 아닙니다. [지원 범위와 제한](DeviceFS.md)을 참고합니다.

## 터미널과 Shell

Init은 `/dev/lsh1`을 `O_RDONLY`로 열어 FD 0에, `O_WRONLY`로 두 번 열어 FD 1과 2에 연결합니다. 폰트를 등록하고 `/dev/lsh1-master`를 `O_RDWR`로 FD 3에
연 뒤 `/sbin/Terminal.lix`를 실행합니다. Init의 master 참조를 닫은 다음 `/bin/Shell.lix`를 실행하므로 Shell과 자식은 프로그램용 연결만 상속합니다.
`Program.cfg`는 Init과 Terminal을 `/sbin/`, Shell을 `/bin/`에 설치하며 커널은 `/sbin/Init.lix`를 실행합니다.

프로그램 쪽 `read`는 입력을 받고 `write`는 출력을 보냅니다. master 쪽 `read`는 출력을 받고 `write`는 입력을 주입합니다. 독립적인 64 KiB 큐 두 개로 저장 공간을 제한합니다. 빈
큐 읽기와 가득 찬 큐 쓰기는 대기 큐에서 잠들고 반대쪽 전송으로 깨어납니다. 일부 전송 후 복사 또는 대기가 실패하면 전송한 길이를 반환합니다. 수락한 출력만 debugcon에 복제합니다. 일반 프로그램은
프로그램용 장치를 `O_RDONLY`·`O_WRONLY`·`O_RDWR`로 열 수 있습니다. master는 부팅 Init만 열 수 있으며 동시에 하나의 열린 객체만 허용합니다. 상속된 참조가 객체 수명을 유지하며
마지막 master 참조를 닫으면 대기자를 깨웁니다. 남은 입력을 읽은 뒤 EOF가 되고 새 쓰기는 실패합니다. 큐 저장 공간은 재부팅까지 유지합니다.

DeviceFS는 VFS의 고정 `/dev` 하위 트리로 연결되며 `getdents64`로 디렉터리를 열거합니다. 터미널 FD는 장치 동작 테이블을 사용하고 기존 큐와 상속 동작을 유지합니다. 키보드 작업자는 US 배열 키 이벤트를 터미널 입력 큐에 전달합니다. 자세한 내용은 [DeviceFS](DeviceFS.md)에 설명합니다.

Terminal은 TrueType 상태, 읽기 전용 JetBrains Mono 매핑과 프레임버퍼를 소유합니다. 최대 2560 × 1440 화면에서 글자 크기 36과 줄 높이 40을 사용하고 가로 이동 폭은 폰트
메트릭으로 계산하며 ASCII·줄바꿈·캐리지 리턴·백스페이스·스크롤을 처리합니다. 출력이 없으면 FD 3 읽기에서 잠듭니다. Shell은 libc 입출력, fork/execve와 chdir를 사용합니다. 길이를 제한한 줄
편집, `help`, `cd [PATH]`, 상대·절대 실행 경로의 실행과 종료 대기를 제공하며 ls 내장 명령은 제공하지 않습니다. 전경 작업 관리는 없으므로 같은 터미널을 읽는 프로그램끼리는 입력을 경쟁합니다.

libc는 제한된 freestanding 부분집합이며 표준 함수명이 완전한 C·POSIX 호환성을 뜻하지 않습니다. 파이프, FD 복제, `termios`, 정규 입력 모드, 시그널, 작업 제어,
범용 유저 할당자는 구현 범위에 포함하지 않습니다. 디스플레이와 에셋 API는 LineOS 확장입니다. 사용자 메모리 복사와 관리 주소 공간 연산은 VMM 잠금 대기 중 TLB
shootdown 요청을 처리하며 일시적인 잠금 경합을 잘못된 사용자 버퍼나 ELF 매핑 실패로 취급하지 않습니다.

## 터미널 ANSI 색상

Terminal은 read 경계에 걸쳐 SGR 색상 코드를 처리합니다. 30–37/40–47, 90–97/100–107, 38;5;N / 48;5;N(256색), 38;2;R;G;B / 48;2;R;G;B(24비트 RGB 전체)를 지원합니다. SGR 0 또는 빈 인자는 속성을 초기화하며 39/49는 기본 글자색·배경색을 복원합니다. 1/22는 기본 글자색의 밝은 색상, 7/27은 반전을 켜고 끕니다. 배경색은 글꼴 메트릭에 따른 영역과 공백에 칠합니다. 스크롤은 표시된 픽셀을 보존하고 새 영역을 현재 배경색으로 채웁니다.

예를 들어 `printf("\033[38;2;145;231;57mColor\033[0m\n");`로 RGB 글자색을 지정합니다. 인자 개수와 수치 범위를 제한하고 미지원 CSI 명령과 OSC 문자열을 소비하여 제어 코드가 글자로 출력되지 않게 합니다. 이 색상 확장에는 커서 위치 이동, 화면 지우기, 밑줄, 이탤릭 등은 포함하지 않습니다. 기존 UTF-8, 글꼴, 입력, 커서와 공유 프레임버퍼 표시는 유지합니다. SGR 규약은 [XTerm 제어 코드](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)를 참고합니다.

프롬프트는 원래 TTF의 굵은 ┏━와 ┗━> 글리프를 사용합니다. 배경 지우기 영역을 실제 폰트 상승 높이와 줄 높이로 계산하여 현재 줄 바깥의 연결 픽셀을 보존하며 글리프를 교체하거나 늘리지 않습니다. 가로 이동 폭은 글꼴 메트릭을 유지합니다.

첫 명령어만 강조합니다. help/cd/pwd/echo에 정확히 일치하면 RGB(214,122,122), 실행과 같은 PATH 검색으로 찾은 실행 파일이면 RGB(128,184,136)입니다. 없거나 아직 찾지 못한 명령어는 흰색 RGB(255,255,255) 글씨와 RGB(255,95,95) 배경으로 표시합니다. 뒤의 인자는 기본 흰색입니다. 분류가 바뀌면 CR/CSI 2 K로 다시 칠하며 일반 입력과 한글 조합은 점진적으로 표시하고 출력 전에는 속성을 초기화합니다. 외부 명령의 인자는 공백으로 구분해 최대 64개를 전달하며 따옴표·이스케이프 해석은 미구현입니다.
