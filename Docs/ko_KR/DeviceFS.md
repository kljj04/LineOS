# DeviceFS와 유저 ABI

[문서 목록](README.md) · [한국어](../ko_KR/DeviceFS.md) · [English](../en_US/DeviceFS.md)

## 런타임과 시스템 호출

프로젝트 musl이 표준 C·POSIX API, TLS와 스레드별 errno를 제공합니다. `LOSLib`의 `liblos.so`는 LOS 전용 API만 제공합니다. 커널은 Linux x86-64 번호와 레지스터 규약을 사용하는 syscall 부분집합을 구현하며, musl에 함수가 있다고 해당 커널 기능까지 지원되는 것은 아닙니다. `RAX`는 번호와 반환값, 인자는 `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`이고 `RCX`·`R11`은 파괴됩니다. 일반 오류는 음수 errno이며 libc가 -1과 errno로 변환합니다. `brk`는 예외적으로 실패 시 이전 break를 반환합니다.

전체 디스패치는 `Kernel/Source/Syscall/Syscall.c`, 번호는 `Common/Syscall/Numbers.h`가 기준입니다. [시스템 호출 목록](Syscalls.md)은 번호와 주요 제한을 함께 정리합니다.

## 파일과 디렉터리

프로세스마다 FD 슬롯 16개를 보유합니다. fork와 `F_DUPFD`는 열린 파일 객체와 오프셋·상태 플래그를 공유하고, FD별 CLOEXEC는 독립적입니다. 성공한 execve는 CLOEXEC FD를 닫으며 실패한 execve는 기존 상태를 보존합니다. 일반 파일의 식별자는 경로·파티션·inode·inode generation을 검사합니다. 무관한 디렉터리 변경 때문에 FD가 무효화되지는 않지만, rename/unlink 뒤 열린 inode를 유지하는 완전한 POSIX 수명은 지원하지 않습니다.

`open`/`openat`은 접근 모드, CREATE·EXCL·TRUNC·APPEND·DIRECTORY·NOFOLLOW·CLOEXEC를 처리하고 x86-64 O_LARGEFILE을 허용합니다. `openat`은 AT_FDCWD 또는 디렉터리 FD 기준 상대 경로를 해석하며 절대 경로에서는 dirfd를 무시합니다. O_PATH·O_TMPFILE 등 나머지 플래그는 지원하지 않습니다. `pread64`는 일반 파일의 오프셋을 바꾸지 않고 읽습니다. `access`는 실제 UID/GID로 접근을 검사합니다.

`stat`·`lstat`·`fstat`·`newfstatat`은 144바이트 Linux x86-64 stat 배치를 사용합니다. `newfstatat`은 AT_SYMLINK_NOFOLLOW·AT_EMPTY_PATH를 처리하며 AT_NO_AUTOMOUNT를 허용합니다. `getdents64`는 Linux dirent64 레코드를 반환하므로 musl의 디렉터리 API가 이를 사용합니다.

`fcntl`은 GETFD·SETFD·GETFL·SETFL·DUPFD·DUPFD_CLOEXEC를 지원합니다. SETFL은 APPEND와 일반 파일 NONBLOCK 상태를 처리하지만 장치 비동기/nonblocking 동작·파일 잠금·O_DIRECT는 제공하지 않습니다. `poll`은 timeout=0, events=0인 FD 유효성 검사만 지원하며 일반 준비 상태 대기 기능은 아닙니다.

## DeviceFS와 터미널

DeviceFS는 `/dev`에 마운트합니다. `/dev/null`, `/dev/fb0`, `/dev/lsh1`, `/dev/lsh1-master` 등 고정 노드를 사용합니다. 장치 동작 테이블은 open/read/write/close/ioctl/map을 제공하고 마지막 열린 객체 참조가 해제될 때 장치를 닫습니다. 동적 장치 등록과 mknod는 지원하지 않습니다.

Init은 `/dev/lsh1`을 FD 0·1·2, `/dev/lsh1-master`를 FD 3으로 엽니다. Terminal만 master FD를 상속하며, Login·셸은 일반 끝점을 사용합니다. 입력과 출력은 각각 64 KiB 큐이며 빈 읽기와 가득 찬 쓰기는 Wait Queue에서 대기합니다. 부팅 Init만 master를 열 수 있고 마지막 master가 닫히면 입력을 소진한 뒤 EOF가 됩니다. `ioctl` 명령은 하위 32비트로 해석하므로 musl의 signed int 명령 전달과 호환됩니다.

프레임버퍼는 Linux 화면 정보 ioctl과 LOS flush/present·화면 소유권·입력 전달 확장을 제공합니다. `/dev/fb0` MAP_SHARED는 framebuffer RAM을 공유하며 쓰기 매핑에는 쓰기 FD가 필요합니다. 실행/private 매핑, 동적 모드 전환, DRM/KMS와 vsync는 지원하지 않습니다. 변경 후 LOS flush/present가 필요합니다. GUI·TTY 소유권과 IPC의 상세는 [lwm](Lwm.md)·[IPC](IPC.md)를 참고합니다.

## mmap과 brk

익명 MAP_PRIVATE, 일반 파일 MAP_PRIVATE, framebuffer MAP_SHARED, PROT_NONE와 mprotect를 제공합니다. 일반 파일은 매핑할 때 데이터를 읽어 private 페이지에 복사하고 나머지를 0으로 채우며 FD 오프셋을 바꾸지 않습니다. 디스크 변경과 자동 동기화하거나 EOF 이후 접근에 Linux SIGBUS를 발생시키는 demand paging 구현은 아닙니다. 일반 파일 MAP_SHARED, msync와 writeback은 지원하지 않습니다.

매핑 영역은 `0x600000000000..0x600100000000`이고 호출당 64 MiB, 프로세스당 예약 구간 합계 128 MiB 제한이 있습니다. MAP_FIXED와 MAP_FIXED_NOREPLACE는 이 영역 안에서 지원합니다. FIXED 교체는 지원하는 기존 private 매핑에 한정하며 다른 소유권의 매핑을 임의로 교체하지 않습니다. 부분 munmap과 이미 비어 있는 구간을 처리하고 ELF·에셋 영역의 임의 해제는 허용하지 않습니다. VMM 잠금과 SMP TLB shootdown으로 공유 주소 공간 변경을 동기화합니다.

`brk(0)`은 현재 break를 조회하고 성공은 요청 주소, 실패는 이전 break를 반환합니다. 힙은 초기 break를 기준으로 64 MiB 한도를 적용합니다. musl의 malloc은 이 커널 기능 위에서 동작합니다.

## 프로세스·스레드·ELF

프로그램은 동적 PIE ELF이며 커널이 PT_INTERP를 적재하고 argc·argv·envp·auxv가 있는 Linux 초기 스택을 구성합니다. 기본 인터프리터 `/lib/libc.so`가 공유 라이브러리와 TLS를 준비합니다. 별도 LineOS 로더는 실험·검증용으로 유지합니다. 임의 Linux 프로그램과 완전한 Linux ABI 호환을 뜻하지 않으며 shebang 스크립트 실행은 지원하지 않습니다.

PROCESS는 주소 공간·FD·cwd·자격 정보·매핑을 공유하고 TASK는 TID·레지스터·FS base·커널 스택·시그널 마스크를 보유합니다. clone/clone3의 스레드 플래그 조합, FS 기반 TLS, private FUTEX_WAIT/WAKE, clear_child_tid와 robust list를 구현합니다. musl pthread 생성·join의 기본 경로를 지원하지만 시그널 전달·비동기 취소·전체 futex 연산을 지원하지 않습니다. `rt_sigprocmask`는 스레드별 64비트 마스크와 BLOCK/UNBLOCK/SETMASK·조회만 처리하며 시그널 핸들러 실행 기능은 아닙니다. `membarrier(QUERY)`는 지원 명령 비트 0을 반환합니다.

fork는 private 페이지를 즉시 복사하며 COW는 없습니다. 열린 객체와 framebuffer·공유 에셋은 공유합니다. 다중 스레드 프로세스의 fork/execve는 EBUSY로 거부합니다. execve는 새 이미지를 준비한 후 PID를 유지하며 교체하고, argv/envp 각각 64개·문자열당 4096바이트·합계 16384바이트로 제한합니다. 프로세스 실행 기록은 128개 슬롯을 사용합니다. wait4는 자식 PID 또는 -1, WNOHANG, 종료 상태 수집을 지원하며 프로세스 그룹·rusage·재부모화는 지원하지 않습니다.

root 소유 setuid 실행과 AT_SECURE 전달 및 신뢰 경로 검사를 구현했습니다. setgid 실행은 지원하지 않습니다. 자체 sudo의 범위·인증 구조는 [사용자와 sudo](Users.md)를 참고합니다. memfd_create·sealing, 일반 poll 대기, 소켓 syscall, 작업 제어와 일반 시그널 전달 등은 여전히 미지원입니다.
