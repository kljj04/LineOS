# Library.cfg

[문서 목록](README.md) · [한국어](../ko_KR/LibraryConfig.md) · [English](../en_US/LibraryConfig.md)

`Libraries/<Directory>/Library.cfg`는 다음 섹션 문법을 사용합니다. 설정 파일을 자동 발견하므로 일반 라이브러리와 의존성을 추가할 때 공통 Makefile을 수정할 필요가 없습니다.

## 예제

```ini
[Library]
Name = UserDB
Output = libuserdb.so
CopyPath = /lib
LibC = musl
Enable = true

[Build]
CFlags =
    -O2

[Dependencies]
Libraries =
    LOSLib
    Locale
```

## 설정

| 항목 | 기본값 | 의미 |
| --- | --- | --- |
| `[Library] Name` | 디렉터리 이름 | 다른 설정에서 사용하는 논리 이름. 대소문자를 구분합니다. |
| `Output` | `lib<소문자 이름>.so` | 설치 파일명이며 일반 빌드의 ELF SONAME입니다. `.so`와 숫자 버전 접미사를 지원합니다. |
| `CopyPath` | `/lib` | 게스트 OS 안의 설치 디렉터리입니다. |
| `LibC` | `musl` | `musl` 또는 `none`. |
| `Enable` | `true` | `false`이면 빌드와 설치에서 제외합니다. |
| `[Build] CFlags` | 빈 목록 | 일반 빌드에 추가할 컴파일 옵션. 한 줄에 한 옵션입니다. |
| `Makefile` | 없음 | 라이브러리 루트 기준 자체 Makefile 경로입니다. |
| `Artifact` | 없음 | 자체 make가 만든 공유 라이브러리의 상대 경로입니다. `Makefile`과 함께 지정합니다. |
| `[Dependencies] Libraries` | 빈 목록 | 활성 라이브러리의 `Name` 목록입니다. 파일명이나 디스크 경로가 아닙니다. |

## 문법

`[Library]`는 필수입니다. 다른 섹션과 기본값 항목은 생략할 수 있습니다. 빈 줄, 전체 행 `#` 주석, CRLF를 지원합니다. 목록은 빈 `Key =` 다음에 들여쓴 행으로 작성합니다. 섹션·키·의존성 중복, 알 수 없는 키, 비활성/없는 의존성, 순환 의존성, 이름·출력 파일명 충돌은 거부합니다. 예전 flat 설정은 지원하지 않습니다. 변수 치환, 인라인 주석, 따옴표, 공백 포함 경로, 상대 설치 경로, `.`·`..` 경로 및 심볼릭 링크는 지원하지 않습니다. 컴파일 옵션에는 셸 문자, 공백, 쉼표를 넣을 수 없습니다. 여러 인자가 필요한 옵션은 각각의 행에 분리하지 말고 `-DNAME=value`, `-Ipath`처럼 한 옵션으로 작성합니다.

## 일반 빌드와 의존성

일반 빌드는 `Source/` 아래 `.c`와 `.S`를 탐색합니다. 자신의 `Include/` 및 선언한 의존성의 전이적 `Include/`를 사용하며, 선언한 직접 의존성을 링크합니다. 의존 라이브러리 빌드를 먼저 완료하므로 병렬 make에서도 생성 헤더를 사용할 수 있습니다. `-z defs`로 해결되지 않은 심볼을 검사하고, `--as-needed`로 사용하지 않는 동적 의존성을 제거합니다. 다른 라이브러리의 심볼을 직접 호출한다면 해당 이름을 직접 의존성에 넣습니다.

`LibC = musl`은 프로젝트 musl 헤더와 libc를 자동으로 추가합니다. `LibC = none`은 Clang의 freestanding 기본 헤더와 프로젝트 Common 헤더를 사용하며 musl을 자동으로 추가하지 않습니다. 명시적으로 hosted 라이브러리에 의존하면 그 라이브러리의 libc 의존성은 남을 수 있습니다. 완전한 freestanding 구성이 필요하면 의존성도 `none`으로 구성해야 합니다. `none`에서 `memcpy` 등 필요한 런타임 심볼은 직접 제공해야 합니다.

## 설치와 검색 경로

모든 결과물은 링크용으로 `LineOS/Libraries/<Output>`에 모입니다. 이미지 업데이트는 활성 매니페스트의 결과물만 `<CopyPath>/<Output>`으로 설치하며, staging 디렉터리에 남은 예전 `.so`를 자동 설치하지 않습니다. 프로그램·폰트와 설치 위치가 충돌하면 업데이트를 거부합니다. 일반 라이브러리와 프로그램에는 의존 라이브러리의 게스트 설치 경로를 RUNPATH로 기록하므로 `/opt/...` 같은 별도 경로도 찾을 수 있습니다. 프로그램은 명시한 라이브러리와 그 전이 의존성의 경로를 포함합니다. 기존 이미지에 이미 설치된 옛 경로의 파일을 자동 삭제하지는 않습니다.

## 자체 빌드

자체 빌드는 다음과 같이 설정합니다.

```ini
[Library]
Name = musl
Output = libc.so
CopyPath = /lib
LibC = none

[Build]
Makefile = Makefile
Artifact = lib/libc.so
```

`$(MAKE) -C Libraries/<Directory> -f <Makefile>`을 실행하고 성공하면 `Artifact`를 `Output` 이름으로 복사합니다. 실패와 dry-run은 상위 make에 전달됩니다. Artifact는 최초 빌드에 없어도 되며 복사 시 일반 파일인지 검사합니다. 자체 빌드의 컴파일 옵션·헤더·SONAME·RUNPATH는 자체 Makefile이 담당합니다. 의존성 빌드 순서는 상위 빌드가 보장하며, 자체 빌드에도 `Include/`가 있다면 의존 사용자에게 공개합니다.

## 제한과 검증

현재 프로그램 시작 코드와 secure 실행 정책은 `/lib/libc.so`를 사용하므로 이름 `musl`은 `Output = libc.so`, `CopyPath = /lib`, `LibC = none`으로 고정합니다. 별도 LineOS 로더는 기존 전용 빌드로 유지하며 `/lib/ld-lineos-x86_64.so.1`에 설치합니다. build-only 자체 타깃, 정적 라이브러리, 에셋 게시 설정은 이 문법에 포함하지 않습니다.

문법·의존성 빌드·배포 검사 명령은 [검증 안내](Validation.md)를 참고합니다.
