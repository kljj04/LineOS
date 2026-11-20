# 계정과 sudo

[문서 목록](README.md) · [한국어](../ko_KR/Users.md) · [English](../en_US/Users.md)

## 계정 저장소

`UserDB`는 musl로 빌드하는 LOS 공유 라이브러리입니다. musl로 빌드한다는 것과 표준 Unix 인증 시스템을 모두 제공한다는 것은 별개입니다.

| 경로·기능 | 현재 역할 |
| --- | --- |
| `/etc/userdb` | LOS 계정·UID/GID·salt·Argon2id 해시의 원본 DB |
| `/etc/passwd` | UserDB가 계정 이름 조회용으로 생성하는 표준 형식 파일. 비밀번호 필드는 `x`입니다. |
| `/etc/sudoers` | 자체 sudo가 읽는 허용 사용자 목록. 일반 sudoers 정책 문법이 아닙니다. |
| Login | UserDB 조회·비밀번호 검증 후 사용자 세션 시작 |
| sudo | UserDB로 호출자를 찾고 비밀번호 검증 후 root 명령 실행 |
| adduser·deluser | UserDB 갱신·홈 디렉터리 등 LOS 계정 관리 |

UserDB는 root 권한으로 원본을 수정하며 임시 파일·배타 잠금·이름 변경을 사용합니다. 인증은 자체 Argon2id 형식(`m=65536,t=3,p=1`)을 사용합니다. 기존 데이터를 임의로 `/etc/shadow`나 다른 해시 형식으로 변환하지 않습니다. `/etc/passwd`의 공개 이름 정보와 비밀 해시 원본은 분리되어 있습니다.

## sudo 사용

이 프로젝트의 sudo는 upstream sudo를 포팅한 프로그램이 아니라 LOS 전용 구현입니다. 기본 명령은 다음과 같습니다.

```text
sudo cat /etc/sudoers
sudo locale set ko_KR.UTF-8
sudo COMMAND [ARGUMENT ...]
sudo -k
sudo cd [PATH]
```

실행 파일은 root 소유·4755로 `/bin/lsh/sudo.lix`에 설치합니다. 호출자의 실제 UID로 UserDB 계정을 찾고 `/etc/sudoers`를 확인합니다. 인증 캐시는 커널의 LOS 전용 API와 로그인 세션 식별자를 사용합니다. `-k`는 캐시를 비웁니다. 명령은 기본 `/bin/lsh` 또는 직접 지정한 경로에서 찾으며, UID/GID를 root로 전환하고 제한된 환경으로 execve합니다. `sudo cd`는 LOSParentChdir 확장으로 부모 셸의 디렉터리를 바꾸는 LOS 전용 동작입니다.

## 미지원 upstream 기능

일반 sudo의 전체 옵션·sudoers 문법·플러그인·PAM·다른 대상 사용자 선택·정책 감사·I/O 로깅을 제공하지 않습니다. 일반 sudo를 사용하려면 UserDB와 표준 계정·인증 API의 연결, 터미널·시그널·프로세스 그룹·작업 제어 및 필요한 커널 API를 별도로 검토하고 포팅해야 합니다. musl의 계정 조회 API가 있다는 사실만으로 PAM 인증이 준비되지는 않습니다.

## setuid와 동적 실행

커널은 root 소유 setuid 실행과 실제/유효 UID·GID 및 AT_SECURE를 전달합니다. secure 실행 파일과 경로 구성요소는 root 소유이며 group/other 쓰기 권한이 없어야 합니다. 심볼릭 링크나 `.`·`..` 경유는 허용하지 않습니다. secure 인터프리터는 현재 신뢰 경로의 `/lib/libc.so`로 제한하며, secure 실행의 파일 mmap도 신뢰 경로를 검사합니다. musl은 AT_SECURE로 로더 환경변수의 보안 처리를 선택합니다. setgid 실행은 지원하지 않습니다.

현재 sudo가 실행되는 것과 전체 upstream sudo 기능·보안 모델을 구현한 것은 구분합니다. 전체 Linux 보안 경계·capabilities·supplementary groups·일반 시그널 전달은 제공하지 않습니다.

## 남은 호환 범위

`/etc/shadow`, 표준 그룹 DB와 supplementary groups, PAM, NSS 플러그인 체계, 표준 암호 해시와의 호환을 완성하지 않았습니다. Login과 sudo는 `getpwnam`/PAM을 통해 인증하지 않고 UserDB API를 직접 사용합니다. 계정 관리 변경은 이 원본 DB와 공개 이름 파일의 일관성을 유지해야 하며 기존 사용자 데이터를 보존해야 합니다. setuid·인증 fixture의 배치 조건은 [검증 안내](Validation.md)를 참고합니다.
