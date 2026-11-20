# 검증 안내

[문서 목록](README.md) · [한국어](../ko_KR/Validation.md) · [English](../en_US/Validation.md)

검증 결과는 빌드 성공, 호스트에서 실제 소스를 실행한 테스트, 사본 VM에서 관찰한 결과를 구분합니다. fixture 생성 스크립트는 그 자체로 QEMU를 부팅하거나 실제 디스크를 갱신하지 않습니다. VM 결과는 사용한 커널·라이브러리·설정·이미지에만 적용합니다.

## 호스트 검증

프로젝트 루트에서 실행합니다. 라이브러리·프로그램 소스를 바꾼 뒤에는 먼저 빌드합니다.

```bash
make -f Makefiles/Makefile -j8 All
python3 Scripts/Validation/Libraries.py
python3 Scripts/Validation/Programs.py
python3 Scripts/Validation/Locale.py
python3 Scripts/Validation/Interactive.py
bash Scripts/Validation/LinuxSyscalls.sh
```

| 검사 | 확인 범위 |
| --- | --- |
| Libraries.py | 섹션 문법·충돌·순환 검사, 실제 freestanding ELF 빌드, 의존성 링크·실행·RUNPATH, custom Makefile·Artifact·dry-run |
| Programs.py | 설정·생성 초기화 코드·sanitizer, 배포 사전 검사·CopyPath 전달·오래된 staging 제외, Name/PT_INTERP 링크 |
| Locale.py | 실제 musl에서 로켈 파일 재적용, 환경변수 우선순위, 내장 번역, 명령의 저장 이름 검증. 파일 접근은 임시 디렉터리로 치환합니다. |
| Interactive.py | 실제 로그인 입력 안내의 flush·비밀번호 숨김, 실제 lsh 오브젝트를 musl로 실행한 ANSI 출력 순서 |
| LinuxSyscalls.sh | 실제 syscall/VFS 소스의 ASan·UBSan fixture. 메모리·태스크·디스크·유저 복사를 대체하므로 실제 페이지 테이블·IRQ 동기화 검증은 아닙니다. |

Library.cfg·Program.cfg의 CLI 매니페스트도 읽기 전용으로 검사할 수 있습니다.

```bash
bash Scripts/Backend/Libraries.sh .
bash Scripts/Backend/Programs.sh .
make -f Makefiles/Makefile -n All
```

## VM fixture 생성

| 생성 명령 | 안내·결과물 |
| --- | --- |
| `bash Scripts/Validation/LinuxSyscallsGuest.sh` | [Linux syscall 검증](../../Scripts/Validation/LinuxSyscalls/README.md), Build/Validation/LinuxSyscalls |
| `bash Scripts/Validation/MuslCompat.sh` | [musl 회귀 검증](../../Scripts/Validation/MuslCompat/README.md), Build/Validation/MuslCompat |
| `python3 Scripts/Validation/ProgramConfigGuest.py` | 아래 배치·결과 코드, Build/Validation/ProgramConfig |
| `bash Scripts/Validation/SecureExec.sh` | Build/Validation/SecureExec의 setuid·AT_SECURE·주입 차단·실제 sudo 인증 fixture |

## Program.cfg VM fixture

사본 이미지에 ConfigInit.lix를 `/sbin/Init.lix`, Consumer.lix·Missing.lix·Denied.lix를 `/cfg-consumer.lix`·`/cfg-missing.lix`·`/cfg-denied.lix`, Payload를 `/cfg-payload`에 설치합니다. 현재 커널과 `/lib/libc.so`·`/lib/liblos.so`도 필요합니다.

성공하면 `/program-cfg-PASS`에 8바이트 0을 기록합니다. `/program-cfg-FAIL`은 1=게시·획득 실패, 2=소비 실행 실패, 3=누락 에셋 차단 실패, 4=무권한 게시 차단 실패입니다.

## VM 실행과 결과 보고

fixture마다 현재 커널·인터프리터·필요 라이브러리·전용 테스트 계정과 파일을 **폐기 가능한 사본 이미지**에 설치합니다. 일반 Init이나 실제 계정 DB를 테스트 fixture로 교체하지 않습니다. 사본 VM 종료 후 EXT2의 PASS/FAIL 결과 파일을 읽고, 실패한 소스 행과 errno를 기록합니다. 결과 파일을 이전 실행에서 남기지 않습니다. `sudo`·setuid fixture는 테스트 계정과 root 소유권·권한도 필요하며 실행 파일 하나만 복사해서 검증할 수 없습니다.

`Scripts/Validation/Musl/README.md`와 `Libraries/Loader/README.md`의 이전 검증은 당시 구성의 기록입니다. 과거 glibc fixture 성공은 현재 glibc 런타임을 채택한다는 의미가 아닙니다. Memfd/sealing처럼 ENOSYS를 정상적으로 보고하는 선택 테스트를 전체 지원 성공으로 기록하지 않습니다.
