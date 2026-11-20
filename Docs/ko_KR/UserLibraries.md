# 유저 프로그램과 라이브러리

[문서 목록](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## 런타임 구성

모든 기본 프로그램은 프로젝트 musl을 사용합니다. stdio·malloc·문자열·pthread·errno·TLS는 musl이 제공하고, `LOSLib`는 LOS 전용 syscall API만 제공합니다. 호스트 Ubuntu glibc는 현재 기본 런타임이 아닙니다. 실행 파일은 musl Scrt1.o·crti.o·crtn.o와 `_start`를 사용하며 PT_INTERP는 기본 `/lib/libc.so`입니다.

| 논리 이름 | 결과물 | 역할 |
| --- | --- | --- |
| musl | libc.so | libc와 동적 인터프리터 |
| LOSLib | liblos.so | 에셋·화면·IPC·시스템 모니터·LOS 확장 |
| Window | libwindow.so | IPC 기반 창 클라이언트 API |
| Locale | liblocale.so | 내장 번역과 LOS 로켈·시간대 기본 설정 |
| TrueType | libtruetype.so | 유저 공간 글꼴 파싱·측정·래스터화 |
| UserDB | libuserdb.so | LOS 계정 DB·Argon2id 인증·계정 관리 |
| VirGL | libvirgl.so | LOS VirGL 장치 접근 API |

Loader는 별도 전용 빌드의 `ld-lineos-x86_64.so.1`이며 기본 musl 인터프리터를 대체하지 않습니다. VirGL 장치 API가 있다는 사실만으로 Mesa OpenGL/EGL 전체가 지원되는 것은 아닙니다.

## 설정과 자동 처리 범위

[Program.cfg](ProgramConfig.md)는 실행 파일명·설치·라이브러리·에셋·게시·인터프리터를 선언하고, [Library.cfg](LibraryConfig.md)는 공유 라이브러리 빌드·설치·libc·다른 라이브러리 의존성을 선언합니다. 새 설정 파일을 자동 발견하므로 일반 프로그램·라이브러리 추가에 공통 Makefile 수정은 필요하지 않습니다.

`LibC = musl`이면 musl을 자동 연결하고, 라이브러리의 `LibC = none`은 freestanding 빌드를 선택합니다. 프로그램의 `LibC = none`은 아직 지원하지 않습니다. 프로그램은 현재 LOSLib를 기본 링크 입력에 포함하고 에셋 사용·게시 설정은 의존성 목록에도 LOSLib를 추가합니다. 나머지 직접 사용하는 라이브러리는 `[Dependencies] Libraries`에 명시합니다. 소스의 include나 함수 호출을 분석하여 자동 등록하지 않습니다. 선언한 라이브러리의 전이적 헤더·빌드 순서·검색 경로는 자동 연결합니다. 사용하지 않는 링크 입력은 `--as-needed`로 제거합니다.

`Source/`의 일반 빌드 또는 `[Build] Makefile`·`Artifact` 자체 빌드를 선택합니다. 결과물은 `LineOS/Libraries/`에 모으고 `CopyPath`에 따라 이미지에 설치합니다. 프로그램과 일반 라이브러리에는 의존성의 게스트 경로를 RUNPATH로 기록합니다. 자체 Makefile은 자신의 SONAME·RUNPATH와 컴파일 옵션을 담당합니다.

## 에셋과 글꼴

Init의 `[Publish] Assets`는 `/sys/fonts/JBMNFSB.ttf`와 `/sys/fonts/PTDSB.ttf`를 `Fonts/JetBrainsMono`·`Fonts/Pretendard`로 게시합니다. 생성한 constructor가 main 전에 파일을 읽어 게시하고 이어서 필요한 에셋을 검사합니다. 커널이 읽기 전용 공유 데이터를 보관하며 소비자는 LOSAssetAcquire/Release로 매핑 수명을 관리합니다. 설정은 게시 권한을 부여하지 않습니다.

TrueType은 이 바이트를 읽지만 글꼴 객체·글리프 캐시·할당자·렌더링 버퍼는 프로세스별입니다. 커널 패닉 출력용 내장 폰트는 별도입니다. API는 `Libraries/TrueType/Include/`, 구현은 `Source/`에 있습니다.

## 시작과 대화형 환경

커널이 `/sbin/Init.lix`를 시작합니다. Init이 TTY를 열고 Terminal과 Login을 실행합니다. Login은 UserDB 인증 후 UID/GID와 홈 디렉터리를 설정하고 사용자 autostart 및 `/bin/lsh/lsh.lix`를 실행합니다. 기본 autostart는 `/sys/lwm/lwm.lix`이며 기존 사용자 파일은 덮어쓰지 않습니다.

Terminal은 마스터 출력·폰트·framebuffer를 관리하고 UTF-8, ANSI 색·커서·화면 갱신을 처리합니다. lwm이 화면을 소유하면 Terminal은 IPC 배경 레이어로 프레임을 보내고 입력을 셸에 전달합니다. 창 포커스·독·크기 변경의 실제 범위는 [lwm](Lwm.md)·[IPC](IPC.md), 세션 수명은 [autostart](Autostart.md)를 참고합니다.

lsh는 명령 입력·인용/확장·변수·히스토리·경로 검색·fork/execve/wait를 사용하는 대화형 셸입니다. 두 줄 프롬프트는 `┏━[경로]━(사용자)`와 `┗━>`이며 표준 출력은 프롬프트와 ANSI 커서 순서를 유지하도록 버퍼링하지 않습니다. Login·sudo의 입력 안내는 읽기 전에 flush합니다. 셸 변수 문법은 [별도 문서](ShellVariables.md)를 참고합니다. 전체 POSIX 셸, 파이프·작업 제어·일반 시그널 전달을 지원한다는 의미는 아닙니다.

## 계정·번역·지원 범위

[사용자와 sudo](Users.md)는 자체 인증과 표준 계정 조회의 경계를 설명합니다. [로켈](Locale.md)은 파일 기본값과 환경변수 우선순위, 내장 번역의 한계를 설명합니다. 커널 기능과 제한·오류값은 [DeviceFS와 ABI](DeviceFS.md)·[시스템 호출 목록](Syscalls.md), 검증 명령은 [검증 안내](Validation.md)를 참고합니다.

## TrueType API와 수명

`<TrueTypeEngine.h>`를 포함하고 할당·해제 콜백과 문맥이 있는 TTF_ALLOCATOR, 연속된 32비트 BGRA8888 버퍼의 TRUE_TYPE_SURFACE를 제공합니다. `TrueTypeInit(&Allocator, &Surface)`는 상태를 설정하며 파일을 열지 않습니다. 0으로 초기화한 TRUE_TYPE_FONT마다 `TrueTypeRegisterFont(&Font, FontData)`를 호출한 뒤 DrawTrueTypeCodepoint·DrawTrueTypeText·MeasureTrueTypeText를 사용합니다. 문자열은 CHAR16/UTF-16이며 surrogate pair를 처리합니다. 글꼴 바이트·객체·할당자·surface는 사용 중 살아 있어야 합니다.

그리기는 CPU 버퍼만 수정하므로 화면 소유권·pitch·용량을 확인한 뒤 DisplayPresent 또는 framebuffer/창 API로 별도 표시합니다. 엔진은 프로세스별 surface·할당자 상태 하나를 사용하므로 호출을 직렬화합니다. 입력 글꼴은 신뢰할 수 있어야 하며 완전한 길이 검증·할당 실패 복구·자동 fallback 선택을 제공하지 않습니다. 셸은 글꼴 advance 측정에 이 라이브러리를 사용할 수 있지만 실제 글리프 렌더링은 Terminal이 담당합니다.

## 에셋 용량과 소유권

커널 게시 슬롯은 16개, 전체 페이지 합계는 64 MiB, 한 에셋은 최대 16 MiB, 이름은 NUL을 제외한 최대 63바이트입니다. 프로세스당 서로 다른 매핑은 8개이고 같은 에셋의 반복 acquire는 참조 횟수를 늘립니다. release는 해당 프로세스의 매핑 참조만 해제하며 게시 자체를 삭제하지 않습니다. 게시 취소·교체·파일 형식 검증은 지원하지 않습니다. 디스크 경로와 게시 이름은 별도 이름 공간입니다.
