# 시스템 호출 목록

[문서 목록](README.md) · [한국어](../ko_KR/Syscalls.md) · [English](../en_US/Syscalls.md)

`Common/Syscall/Numbers.h`의 번호와 `Kernel/Source/Syscall/Syscall.c`의 디스패치를 대조한 목록입니다. 번호가 존재해도 Linux의 모든 플래그·연산을 지원하는 것은 아닙니다. 수명·오류 규칙은 [DeviceFS와 ABI](DeviceFS.md), 검증 범위는 [검증 안내](Validation.md)를 참고합니다. LOS 확장은 1024부터입니다.

## Linux x86-64 부분집합

| 번호 | 이름 | 범위 |
| --- | --- | --- |
| 0 | `read` | 부분 구현; ABI·소스 참고 |
| 1 | `write` | 부분 구현; ABI·소스 참고 |
| 2 | `open` | 부분 구현; ABI·소스 참고 |
| 3 | `close` | 부분 구현; ABI·소스 참고 |
| 4 | `stat` | 부분 구현; ABI·소스 참고 |
| 5 | `fstat` | 부분 구현; ABI·소스 참고 |
| 6 | `lstat` | 부분 구현; ABI·소스 참고 |
| 7 | `poll` | timeout=0·events=0 FD 검사만 지원 |
| 9 | `mmap` | 익명·파일 private 및 fb0 공유; 제한된 고정 매핑 |
| 10 | `mprotect` | 부분 구현; ABI·소스 참고 |
| 11 | `munmap` | 부분 구현; ABI·소스 참고 |
| 12 | `brk` | 실패 시 이전 break 반환 |
| 14 | `rt_sigprocmask` | 스레드별 64비트 마스크; 시그널 전달 미지원 |
| 16 | `ioctl` | 부분 구현; ABI·소스 참고 |
| 17 | `pread64` | 오프셋을 유지하는 일반 파일 읽기 |
| 20 | `writev` | 부분 구현; ABI·소스 참고 |
| 21 | `access` | 부분 구현; ABI·소스 참고 |
| 24 | `yield` | 부분 구현; ABI·소스 참고 |
| 35 | `nanosleep` | 부분 구현; ABI·소스 참고 |
| 39 | `getpid` | 부분 구현; ABI·소스 참고 |
| 56 | `clone` | 스레드용 플래그 조합만 지원 |
| 57 | `fork` | 즉시 복사; 다중 스레드 프로세스는 EBUSY |
| 59 | `execve` | ELF/PT_INTERP; shebang 미지원; 다중 스레드는 EBUSY |
| 60 | `exit` | 부분 구현; ABI·소스 참고 |
| 61 | `wait4` | 자식 PID 또는 -1·WNOHANG; 그룹·rusage 미지원 |
| 72 | `fcntl` | FD 플래그·파일 상태 일부·복제; 파일 잠금 미지원 |
| 79 | `getcwd` | 부분 구현; ABI·소스 참고 |
| 80 | `chdir` | 부분 구현; ABI·소스 참고 |
| 82 | `rename` | 부분 구현; ABI·소스 참고 |
| 83 | `mkdir` | 부분 구현; ABI·소스 참고 |
| 92 | `chown` | 단일 UID/GID 자격 정보 모델 |
| 99 | `sysinfo` | 부분 구현; ABI·소스 참고 |
| 102 | `getuid` | 부분 구현; ABI·소스 참고 |
| 105 | `setuid` | 부분 구현; ABI·소스 참고 |
| 106 | `setgid` | 부분 구현; ABI·소스 참고 |
| 107 | `geteuid` | 부분 구현; ABI·소스 참고 |
| 158 | `arch_prctl` | FS base 연산 일부 |
| 186 | `gettid` | 부분 구현; ABI·소스 참고 |
| 202 | `futex` | private WAIT/WAKE 일부 |
| 217 | `getdents64` | 부분 구현; ABI·소스 참고 |
| 218 | `set_tid_address` | 부분 구현; ABI·소스 참고 |
| 228 | `clock_gettime` | 부분 구현; ABI·소스 참고 |
| 231 | `exit_group` | 부분 구현; ABI·소스 참고 |
| 257 | `openat` | AT_FDCWD·디렉터리 FD 기준 상대 경로 |
| 262 | `newfstatat` | NOFOLLOW·EMPTY_PATH 및 NO_AUTOMOUNT 허용 |
| 273 | `set_robust_list` | robust mutex 목록 등록 |
| 274 | `get_robust_list` | 제한된 태스크 조회 |
| 280 | `utimensat` | 부분 구현; ABI·소스 참고 |
| 318 | `getrandom` | 부분 구현; ABI·소스 참고 |
| 324 | `membarrier` | QUERY=0; barrier 명령 미지원 |
| 435 | `clone3` | 스레드용 플래그 조합만 지원 |

## LOS 확장

| 번호 | 이름 | 범위 |
| --- | --- | --- |
| 1024 | `display_info` | LOS 전용 인터페이스 |
| 1025 | `display_present` | LOS 전용 인터페이스 |
| 1026 | `display_present_rect` | LOS 전용 인터페이스 |
| 1027 | `asset_publish` | LOS 전용 인터페이스 |
| 1028 | `asset_acquire` | LOS 전용 인터페이스 |
| 1029 | `asset_release` | LOS 전용 인터페이스 |
| 1032 | `remove` | LOS 파일·디렉터리 삭제 확장 |
| 1033 | `parent_chdir` | 권한 검사 후 부모 셸 cwd 변경 |
| 1034 | `sudo_cache` | LOS 인증 세션 캐시 |
| 1035 | `system_monitor` | LOS 전용 인터페이스 |
| 1036 | `ipc` | LOS 복사 메시지·용량 제한 큐 |

## 미지원 동작

memfd_create(319)와 sealing은 구현하지 않았습니다. membarrier(324)는 지원 기능 조회만 처리합니다. 일반 시그널 전달·핸들러·작업 제어, 일반 poll 대기, Linux socket syscall, 일반 파일 MAP_SHARED/writeback, O_PATH/O_TMPFILE 및 전체 Linux 보안·인증 API를 제공하지 않습니다.

미처리 번호는 ENOSYS를 반환하고 Unhandled syscall 로그가 남을 수 있습니다. 이 목록은 부분 구현의 범위이며 Linux 규약 전체 준수를 의미하지 않습니다. 빌드·호스트 실행·사본 VM 관찰은 구분합니다.
