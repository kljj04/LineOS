# Program.cfg

[문서 목록](README.md) · [한국어](../ko_KR/ProgramConfig.md) · [English](../en_US/ProgramConfig.md)

프로그램 소스 디렉터리의 `Program.cfg`는 빌드, 설치 및 실행 초기화를 선언합니다.

## 예제

```ini
[Program]
Name = Example
CopyPath = /bin/lsh
LibC = musl
Enable = true
Mode = 0755

[Dependencies]
Libraries =
    LOSLib
    TrueType
Assets =
    Fonts/JetBrainsMono
    Fonts/Pretendard

[Runtime]
DynamicLinker = auto
```

## 문법

- 섹션과 키의 대소문자를 구분합니다. 섹션과 키는 들여쓰지 않습니다.
- 값 앞뒤 공백, 빈 줄 및 `#`으로 시작하는 주석 줄을 허용합니다.
- 목록은 `키 =` 다음 줄부터 공백이나 탭으로 들여씁니다.
- 중복 섹션, 중복 키, 중복 목록 항목 및 알 수 없는 섹션·키는 오류입니다.
- 줄 끝 주석, 따옴표 문법, 변수 치환, 명령 실행 및 평면 키 형식은 지원하지 않습니다.
- 생략한 값에는 기본값을 적용합니다. 빈 목록은 허용합니다.
- 경로는 ASCII 영문·숫자·밑줄·하이픈·마침표로 구성합니다. `.`·`..` 경로 요소와 반복 `/`는 거부합니다.
- 프로그램·라이브러리 의존성과 혼동하지 않도록 `Programs` 키는 지원하지 않습니다.

## Program

| 키 | 의미 | 기본값 |
| --- | --- | --- |
| `Name` | `.lix`를 제외한 실행 파일명 | 소스 디렉터리명 |
| `CopyPath` | LOS 내부 설치 디렉터리 | `/bin` |
| `LibC` | 현재 지원값은 `musl` | `musl` |
| `Enable` | `true` 또는 `false` | `true` |
| `Mode` | 네 자리 8진수 권한 | `0755` |

`Name`은 영문·숫자·밑줄·하이픈을 허용합니다. 소스 디렉터리가 `UserPrograms/Example`이고 `Name=renamed`이면 결과는 `LineOS/Programs/Example/renamed.lix`입니다. `CopyPath=/bin`이면 `/bin/renamed.lix`로 설치합니다. 동일 설치 경로를 사용하는 프로그램은 오류입니다. 비활성 프로그램은 빌드·설치 및 게시자 중복 검사에서 제외합니다. 설정 자체의 문법은 검사합니다.

## Dependencies

`Libraries`에는 활성화된 프로젝트 라이브러리의 논리 이름을 씁니다. 예: `LOSLib`, `TrueType`, `Window`, `Locale`, `UserDB`. `liblos.so` 같은 출력 파일명은 쓰지 않습니다. libc는 `LibC`에서 자동 선택하므로 `musl`이나 `libc.so`를 적지 않습니다. [Library.cfg](LibraryConfig.md)의 빌드 순서·전이 헤더·게스트 검색 경로와 직접 링크에 연결됩니다. 소스의 include나 함수 사용을 분석해 자동 추가하지는 않습니다. 현재 프로그램 링크 입력에는 LOSLib도 기본 포함하며 사용하지 않는 입력은 --as-needed로 제거합니다. 프로그램의 LibC = none은 아직 지원하지 않습니다.

`Assets`에는 디스크 경로 대신 `Fonts/JetBrainsMono` 같은 게시 이름을 씁니다. 이름은 선행 `/` 없는 ASCII 경로 형태이며 최대 63바이트입니다. 필요한 에셋이 없거나 획득·해제에 실패하면 `main`에 진입하지 않고 상태 127로 종료합니다. 자동 대기나 서비스 실행은 하지 않습니다. 이 검사는 에셋을 계속 매핑해 두지 않으므로 프로그램에서 사용할 때는 기존 `LOSAssetAcquire`·`LOSAssetRelease` API를 호출합니다.

에셋 의존성 또는 게시 설정이 있으면 초기화에 필요한 `LOSLib` 링크 의존성은 자동 추가합니다. 외부에서 게시할 에셋도 사용할 수 있으므로 설정에 게시자가 없다는 이유만으로 빌드를 거부하지는 않습니다.

## Publish

게시 담당 프로그램의 예:

```ini
[Program]
Name = Init
CopyPath = /sbin

[Publish]
Assets =
    Fonts/JetBrainsMono = /sys/fonts/JBMNFSB.ttf
    Fonts/Pretendard = /sys/fonts/PTDSB.ttf
```

`Assets` 항목은 `게시이름 = /디스크/절대경로`입니다. 활성 설정들 사이의 동일 게시 이름 중복은 빌드 오류입니다. 이미지 업데이트는 게시 원본이 EXT2 내부의 일반 파일인지 검사하며 심볼릭 링크·다른 마운트 경유를 허용하지 않습니다. 호스트 파일을 임의로 복사하는 규칙은 아니며, 원본 파일의 이미지 배치는 기존 설치 경로를 따릅니다.

프로그램이 시작되면 `main` 전에 파일을 열어 최대 16 MiB의 내용을 읽고 `LOSAssetPublish`에 전달합니다. 커널이 데이터 복사를 마치면 임시 버퍼를 해제합니다. 게시를 먼저 완료한 뒤 해당 프로그램의 에셋 의존성을 검사합니다. 빈 파일, 읽기 오류, 동일 이름 재게시 및 게시 권한 부족 등은 상태 127로 종료합니다.

설정은 게시 권한을 부여하지 않습니다. 기존 커널 정책에 따라 boot Init 같은 게시 권한을 가진 프로세스만 성공합니다. 여러 게시 중 일부가 성공한 뒤 다른 게시가 실패한 경우, 앞서 게시한 에셋은 남습니다. 현재 API에는 게시 취소가 없습니다.

## Runtime

`DynamicLinker=auto`는 musl의 `/lib/libc.so`를 선택합니다. 직접 지정할 때는 LOS 내부 절대경로를 쓰며 실제 ELF의 `PT_INTERP`에 반영됩니다. 해당 파일의 존재와 libc 호환성은 배포·실행 환경에서 보장해야 합니다.

설정으로 생성한 초기화 코드는 실행 파일에 포함하므로 실행 시 `Program.cfg` 파일을 읽지 않습니다. 원본 설정을 바꾼 뒤에는 프로그램을 다시 빌드해야 합니다.

## 검증

`python3 Scripts/Validation/Programs.py`로 문법·생성 초기화 코드·배포를 검사합니다. 사본 VM fixture의 배치와 결과 코드는 [검증 안내](Validation.md)를 참고합니다.
