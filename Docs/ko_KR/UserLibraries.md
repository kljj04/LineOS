# 유저 프로그램과 라이브러리

[문서 목록](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## 프로그램 배치

프로그램별 `UserPrograms/<ProgramName>/Program.conf`에 다음 값을 지정합니다.

```ini
CopyPath=/usr/bin/
```

이미지 갱신 시 `LineOS/Programs/<ProgramName>.lix`를 EXT2의 `/usr/bin/<ProgramName>.lix`에 복사합니다. ELF64 내용과 이름을 유지합니다. 설정 파일이 없으면 `/bin/`을 사용합니다. 루트 `/`도 허용합니다. 설정 파일이 존재하면 `CopyPath` 한 개가 필요하며 빈 줄과 `#`으로 시작하는 주석을 허용합니다. CRLF도 지원합니다. 설정은 셸 코드로 실행하지 않습니다.

경로 구성 요소에는 영문자, 숫자, `_`, `-`, `.`만 허용합니다. 상대 경로, 독립적인 `.` 및 `..`, 중복 슬래시, 중복 키, 알 수 없는 키, 빈 값, 설정 파일 심볼릭 링크를 거부합니다. 프로그램 디렉터리 이름에는 영문자, 숫자, `_`, `-`를 허용합니다. 대상 디렉터리의 심볼릭 링크 및 별도 마운트, 대상 파일의 심볼릭 링크와 하드 링크를 거부합니다. 기존 이미지 포맷과 마운트 수명 관리는 변경하지 않습니다. 경로를 변경해도 이전 위치의 파일은 자동 삭제하지 않습니다. 이미지 갱신은 게스트가 실행되지 않고 다른 프로세스가 이미지 내용을 동시에 변경하지 않는 기존 관리 흐름을 전제로 합니다.

## 유저 TrueType

`Libraries/Include/TrueType`과 `Libraries/Source/TrueType`은 커널 포트를 기반으로 한 유저 전용 TrueType 구현입니다. `Build/Libraries/libtruetype.a`를 생성하며 모든 유저 프로그램 링크에 libc 앞 순서로 추가합니다. 실제 사용한 오브젝트만 ELF에 포함합니다. 커널의 PMM, VirtIO GPU, 잠금, 폰트 심볼을 참조하지 않습니다. 커널 TrueType 및 패닉 출력은 유지합니다. 커널 콘솔은 제거했습니다. `write`는 임시 전역 출력 버퍼에 기록하고 Shell이 `LOSOutputStreamRead`로 읽습니다. 버퍼는 64 KiB이며 가득 차면 일부 기록 또는 실패, 비어 있으면 읽기 실패를 반환합니다.

`TrueType/TrueTypeEngine.h`를 포함하고 다음 리소스를 호출자가 준비합니다.

- 신뢰할 수 있는 TTF 데이터 버퍼. 사용할 폰트마다 하나씩 준비합니다
- `TTF_ALLOCATOR`의 `Allocate`, `Free`, `Context`: 정렬된 메모리를 충분히 제공하고 개별 할당을 반환하는 콜백
- `TRUE_TYPE_SURFACE`: `UINT32` BGRA8888 픽셀 버퍼와 폭·높이. 행 패딩 없이 `폭 * 높이 * 4` 바이트 이상을 할당합니다.

`TrueTypeInit(&Allocator, &Surface)`는 할당자와 화면 버퍼를 설정하며 파일을 읽지 않습니다. 각 폰트 데이터를 `TRUE_TYPE_FONT Font = {0};`와 `TrueTypeRegisterFont(&Font, FontData)`로 등록한 뒤 `DrawTrueTypeCodepoint(&Font, ...)`, `DrawTrueTypeText(&Font, ...)`, `MeasureTrueTypeText(&Font, ...)`에 전달합니다. 폰트마다 별도 객체를 사용하며 데이터 버퍼와 객체는 사용이 끝날 때까지 유지해야 합니다. 폰트 자동 대체는 없습니다. 텍스트는 `CHAR16` 배열이며 UTF-16 서로게이트를 처리합니다. 모든 리소스는 사용 중 유효해야 합니다. 저수준 `FONT_INFO` API를 직접 사용한다면 `InitFont` 이후 `Info.UserData`에 `TTF_ALLOCATOR *`를 지정해야 합니다. 팩 API에는 해당 할당자를 `AllocContext`로 전달합니다.

렌더링 결과는 CPU 메모리에만 기록됩니다. 기존 `DisplayPresent(Surface.FrameBuffer, (UINT64) Surface.FrameBufferWidth * Surface.FrameBufferHeight * 4)`로 표시할 수 있습니다. 호출자가 Display 화면 크기를 조회하고 크기·피치 및 버퍼 용량을 확인해야 합니다. 라이브러리가 디스플레이 syscall을 직접 호출하거나 커널 화면 소유권을 변경하지 않습니다.

현재 엔진은 프로세스별 단일 화면·할당자 상태를 사용하므로 호출을 직렬화해야 합니다. 신뢰하지 않는 TTF 입력의 길이 검증이나 메모리 부족에서의 완전한 복구는 보장하지 않습니다. 기존 수학 근사와 단일 행 중심 측정 동작을 유지합니다. Shell은 자체 라이브러리 상태를 초기화하며 Init은 에셋 등록만 수행합니다. QEMU/KVM 단일 CPU 실행에서 Shell 출력 화면을 확인했지만 이 결과로 다중 CPU 동작까지 보장하지는 않습니다.

## 파일과 프로세스 생성

이미지 갱신 시 `Assets/Fonts/*.ttf`를 EXT2의 `/sys/fonts/`에 같은 이름과 0644 권한으로 복사합니다. 유저 프로그램은 `fcntl.h`의 `open(Path, O_RDONLY)`과 `unistd.h`의 `read`, `close`로 읽을 수 있습니다. 현재 파일 디스크립터는 태스크당 8개이고 읽기 전용입니다. 실패는 -1, EOF는 0을 반환하며 `errno`는 지원하지 않습니다. 파일을 읽을 때 경로를 다시 해석하므로 열린 뒤 이름 변경이나 삭제를 유지하는 POSIX 파일 핸들 동작은 지원하지 않습니다. 접근 권한은 실행 생성 시 전달된 VFS 컨텍스트를 따릅니다. 현재 Init은 관리자 컨텍스트로 실행됩니다.

`spawn.h`의 `spawn(Path)`은 호출자의 VFS 컨텍스트로 별도 주소 공간의 정적 ELF를 실행하고 자식 PID 또는 실패 시 -1을 반환합니다. 이는 LineOS의 간단한 확장 API이며 POSIX `posix_spawn`은 아닙니다. 인자 전달, 대기, 종료 상태 수집 및 파일 디스크립터 상속은 아직 지원하지 않습니다. 자식 실행 정보는 최대 16개이며 종료된 자원은 이후 spawn 호출 시 스케줄러에서 완전히 빠진 경우 회수합니다. 태스크별 SSE 상태 저장은 아직 구현되지 않아 여러 유저 태스크의 부동소수점/SIMD 작업은 검증되지 않았습니다. TrueType 상태와 글리프 캐시는 프로세스별이며, TTF 원본은 아래 공용 에셋 API로 공유합니다. 에셋 등록 권한은 자식에게 상속하지 않습니다.

## 공용 에셋

`Asset.h`에서 다음 함수를 제공합니다. 성공은 0, 실패는 -1이며 errno는 없습니다.

- `LOSAssetPublish(Name, Data, Size)`: 부팅 Init만 호출할 수 있습니다. 원본을 커널 소유 페이지로 한 번 복사하므로 반환 후 원본 버퍼를 재사용할 수 있고 Init 종료에도 유지됩니다. 이름은 비어 있지 않은 최대 63바이트 문자열입니다. 이름은 불변이며 중복 등록과 교체를 거부합니다.
- `LOSAssetAcquire(Name, &Asset)`: 호출 프로세스에 같은 물리 페이지를 읽기 전용으로 매핑하고 `LOS_ASSET`의 `Data`, `Size`, `Handle`을 반환합니다. 같은 프로그램에서 같은 이름을 다시 획득하면 같은 매핑의 참조 수를 증가시킵니다. 각 획득에는 별도 구조체를 사용하고 각각 해제합니다. 구조체를 복사하는 것만으로 참조가 늘어나지는 않습니다.
- `LOSAssetRelease(&Asset)`: 해당 프로세스의 참조를 해제합니다. 마지막 참조에서 매핑을 제거하고 호출자의 구조체를 비웁니다. 다른 프로세스의 매핑과 등록된 원본은 유지합니다. 프로세스 주소 공간 파괴 시 남은 매핑과 페이지 테이블도 정리하며 커널의 에셋 원본은 해제하지 않습니다.

등록 상한은 16개, 개별 크기는 16MiB, 원본 페이지 총량은 페이지 반올림 후 64MiB입니다. 프로세스별 서로 다른 에셋 매핑은 8개입니다. 이 상한과 별도로 페이지 테이블·핸들 메타데이터가 사용됩니다. 첫 버전은 재부팅까지 유지하며 등록 해제·교체·동적 퇴출을 제공하지 않습니다. 읽기 전용 영역에 쓰면 보호 예외가 발생합니다. 프로그램은 `Size` 밖의 데이터를 사용하면 안 됩니다.

```c
LOS_ASSET Asset = {0};

if (LOSAssetAcquire("Fonts/JetBrainsMono", &Asset) == 0)
{
    TrueTypeRegisterFont(&Font, Asset.Data);
}
```

폰트를 사용하는 동안 매핑을 유지하고, 폰트 객체와 글리프 캐시가 해당 데이터를 더 참조하지 않을 때 해제합니다. TTF 바이트만 공유하며 `TRUE_TYPE_FONT`, 할당자, 화면, 글리프 캐시는 각 프로세스가 소유합니다. 에셋 API는 파일 형식을 검증하지 않습니다.

## 시스템 호출 인터페이스

현재 `int 0x80` 호출은 종료, 읽기, 쓰기, 열기, 닫기, PID 조회, 화면 정보·표시, 출력 스트림 읽기, spawn과 에셋 등록·획득·해제를 제공합니다. yield 번호는 예약되어 있지만 현재 실패를 반환합니다. 파일과 프로세스 인터페이스는 여기서 설명한 부분집합이며 `errno`와 일반적인 디스크립터 상속은 없습니다.

## 출력과 Shell

libc의 `printf`, `puts`, `putchar`는 stdout에 `write`를 사용합니다. 커널은 디스크립터 1과 2의 바이트를 전역 스트림에 복사하며 현재 시도한 바이트를 debugcon에도 기록합니다. 스트림은 공유되고 읽으면 데이터가 소비됩니다. 프로세스별 라우팅과 블로킹 읽기는 없으며 완전한 터미널, 파이프 또는 POSIX stdio 구현이 아닙니다. debugcon에는 스트림 용량 부족으로 기록되지 않은 바이트도 남을 수 있습니다.

Init은 Pretendard와 JetBrains Mono를 파일에서 한 번 읽고 `Fonts/Pretendard`, `Fonts/JetBrainsMono`로 등록한 뒤 `/bin/Shell.lix`를 실행하고 종료합니다. Shell은 `Fonts/JetBrainsMono`를 읽기 전용으로 매핑하고 자체 TrueType 화면·할당자·폰트 객체를 초기화합니다. Shell은 출력 스트림의 ASCII·줄바꿈·캐리지 리턴을 처리하고 스크롤 후 프레임버퍼를 표시합니다. 키보드 입력과 명령 해석기는 아직 없습니다. 최대 2560 × 1440 화면을 지원하며 빈 스트림을 반복 확인합니다. yield는 현재 -1을 반환하므로 CPU를 사용할 수 있습니다.

libc는 제한된 freestanding 부분집합입니다. `open`, `read`, `write`, `printf` 같은 표준 이름이 완전한 C 또는 POSIX 호환성을 의미하지는 않습니다. `spawn`, 디스플레이와 스트림 인터페이스는 LineOS 전용이며 `exec`, 프로세스 대기, 범용 유저 할당자와 터미널 입력은 구현되지 않았습니다.
