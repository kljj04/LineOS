# LineOS 문서

[언어 선택](../README.md) · [English](../en_US/README.md) · [프로젝트](../../README.md)

처음 실행한다면 [빌드와 실행](Build.md)부터 읽습니다. 코드 경로와 명령은 프로젝트 루트를 기준으로 합니다. 각 문서 상단에서 같은 주제의 영어 문서로 이동할 수 있습니다.

## 시작하기

| 문서 | 내용 |
| --- | --- |
| [빌드와 실행](Build.md) | 도구·명령·이미지·IDE 설정 |
| [아키텍처와 부팅](Architecture.md) | 초기화·유저 세션·소스 구조 |

## 커널

| 문서 | 내용 |
| --- | --- |
| [메모리](Memory.md) | PMM·힙·주소 공간 |
| [스케줄러](Scheduler.md) | LBPWRR·대기·프로세스와 스레드 |
| [장치와 렌더링](Devices.md) | VirtIO·GPU·글꼴·입력 |
| [저장소와 파일시스템](Storage.md) | GPT·EXT2·VFS·열린 파일 수명 |
| [네트워크](Network.md) | 패킷·DHCP·UDP·TCP와 제한 |
| [시계와 NTP](Clock.md) | UTC·단조 시간·동기화 |
| [DeviceFS와 유저 ABI](DeviceFS.md) | FD·mmap·ELF·스레드 규약 |
| [시스템 호출 목록](Syscalls.md) | 번호·지원 연산·LOS 확장 |

## 유저 공간

| 문서 | 내용 |
| --- | --- |
| [유저 프로그램과 라이브러리](UserLibraries.md) | musl·LOSLib·TrueType·에셋 |
| [계정과 sudo](Users.md) | UserDB·권한 상승·표준 인증과의 경계 |
| [로켈과 시간대](Locale.md) | 기본 설정·환경변수·번역 범위 |
| [셸 변수](ShellVariables.md) | 대입·확장·입력 표시 |
| [사용자 시작 프로그램](Autostart.md) | 설정 문법·시작 앱·세션 수명 |
| [IPC와 창 프로토콜](IPC.md) | 소유권·API·메시지와 오류 |
| [lwm](Lwm.md) | 창 조작·입력·합성·크기 제한 |

## 빌드 설정 참조

| 문서 | 내용 |
| --- | --- |
| [Program.cfg](ProgramConfig.md) | 이름·설치·의존성·에셋 게시 |
| [Library.cfg](LibraryConfig.md) | musl/none·의존성·자체 Makefile |

## 개발과 검증

| 문서 | 내용 |
| --- | --- |
| [코드 작성 규칙](Directions.md) | 네이밍·형식·파일 헤더 |
| [검증 안내](Validation.md) | 호스트 검사·사본 VM·결과 보고 |

## 라이선스

[제3자 고지](../THIRD_PARTY_NOTICES.md)와 [라이선스 원문](../LICENSES/)은 공통 자료입니다.
