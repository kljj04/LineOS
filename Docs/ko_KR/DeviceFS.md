# DeviceFS와 프로세스 ABI

## 오류 규약

Linux x86-64의 오류 번호를 공통 `LineOS/Errno.h`에 정의합니다. 커널 시스템 호출은 `-errno`를 반환하며 미구현 번호는 `-ENOSYS`를 반환합니다. libc 래퍼는 -4095~-1을 감지하여 `errno`를 설정하고 -1 또는 `MAP_FAILED`를 반환합니다. 성공 시 `errno`를 지우지 않습니다. `LineOSSyscall0/1/2/3/6`는 원시 반환값을 유지합니다.

`errno`는 `__errno_location()`과 libc 내부 `LIBC_THREAD_STATE`를 통해 접근합니다. 현재는 단일 스레드 프로세스마다 독립적인 저장소이며 fork 시 복제되고 execve 시 새 이미지의 초기값을 사용합니다. pthread/TLS 지원은 구현하지 않았으며, 스레드를 추가할 때 접근 함수가 TLS의 스레드 상태를 선택하도록 변경해야 합니다.

파일 오류는 `EBADF`, `EACCES`, `EFAULT`, `ENOENT`, `EEXIST`, `ENOTDIR`, `EISDIR`, `EMFILE`, `EROFS` 등으로 구분합니다. FD 접근 권한을 검사하고 부분 전송이 있으면 전송 길이를 우선 반환합니다. 일반 파일의 쓰기 오류는 기존 방식대로 열린 객체에 보존하며 close는 FD를 소비한 뒤 오류를 반환할 수 있습니다. VFS 내부 경합은 IRQ 상태를 보존하는 잠금으로 직렬화하며 대기 중 TLB shootdown을 처리합니다.

## 고정 /dev 하위 트리

일반 마운트 API나 루트 구조를 다시 설계하지 않고, 부팅 시 DeviceFS 백엔드를 등록하여 `/dev` 하위 트리에 연결합니다. `/dev/..`는 기존 루트로 돌아가며 절대·상대 경로, 심볼릭 링크 처리와 디렉터리 열거에 VFS를 사용합니다. 루트 디렉터리에 `dev`를 한 번 표시합니다. DeviceFS 노드 목록은 읽기 전용입니다. 이 특수 노드는 실행 중인 커널 VFS에 존재하므로 호스트에서 EXT2 이미지를 마운트해도 `/dev`는 나타나지 않습니다. EXT2에 일반 파일이나 장치용 자리 표시 파일을 만들지 않습니다. 키보드 입력은 기존 lsh1과 master 큐를 사용합니다. 마우스는 커널 태스크가 포인터 이벤트를 소비하고 하드웨어 커서를 갱신하며, 별도의 raw 키보드·마우스 장치 파일 프로토콜은 등록하지 않습니다.

| 노드 | 동작 |
| --- | --- |
| `/dev/null` | 읽기는 EOF, 쓰기는 버립니다. |
| `/dev/zero` | 읽기는 0으로 채우며 쓰기는 버립니다. |
| `/dev/full` | 읽기는 0으로 채우며 0바이트가 아닌 쓰기는 ENOSPC입니다. |
| `/dev/lsh1` | 기존 프로그램용 터미널 큐를 사용합니다. |
| `/dev/lsh1-master` | 기존 master 큐를 사용하며 부팅 Init만 열 수 있고 중복 열기는 EBUSY입니다. |
| `/dev/fb0` | framebuffer 읽기·쓰기, 화면 정보 ioctl, 표시 ioctl, shared mmap을 제공합니다. |

기존 비표준 `/dev/lsh1/master` 대신 문자 장치와 디렉터리를 구분할 수 있는 `/dev/lsh1-master`를 사용하며 Init도 수정했습니다. 예전 경로는 별도 호환 별칭으로 남기지 않습니다.

열린 파일 객체의 장치 동작 테이블은 open/read/write/close/ioctl/map을 제공합니다. fork는 동일한 열린 객체를 참조하여 오프셋과 master 수명을 공유하며 마지막 참조에서 장치를 닫습니다. 디렉터리 FD는 Linux 번호 217의 `getdents64`로 열거하며 Linux x86-64 dirent64 레코드를 반환합니다. stat/lstat/fstat는 번호 4/6/5와 144바이트 x86-64 배치를 사용하여 노드 종류·권한·식별자·크기·시각을 반환합니다. 장치 st_rdev는 고정 레지스트리 노드를 식별하며 장치 크기와 시각은 0입니다. 일반 파일 fstat는 기존 경로와 namespace generation 검증을 유지합니다. opendir/readdir, 동적 장치 등록과 mknod는 지원하지 않습니다.

## ioctl과 framebuffer

번호 16을 사용하며 잘못된 FD는 EBADF, 제어를 지원하지 않는 파일·장치 또는 명령은 ENOTTY입니다. 모든 사용자 포인터는 기존 VMM 복사 함수를 사용합니다.

| 명령 | 분류 | 동작 |
| --- | --- | --- |
| FBIOGET_VSCREENINFO (0x4600) | Linux | 160바이트 fb_var_screeninfo 배치로 현재 고정 모드를 조회합니다. |
| FBIOGET_FSCREENINFO (0x4602) | Linux x86-64 | 80바이트 fb_fix_screeninfo 배치로 stride·크기 등을 조회합니다. 실제 물리 주소와 MMIO 주소는 노출하지 않습니다. |
| LOS_FBIO_PRESENT | LineOS, `_IOW('L', 1, ...)` | 기존 전체 화면 표시 경로를 사용합니다. |
| LOS_FBIO_PRESENT_RECT | LineOS, `_IOW('L', 2, ...)` | 기존 사각형 표시 경로를 사용합니다. |
| LOS_FBIO_FLUSH | LineOS, `_IO('L', 3)` | 전체 화면을 dirty로 표시하고 VirtIO transfer/flush를 실행합니다. |

화면 모드 변경, pan, colormap, DRM/KMS 및 vsync API는 구현하지 않았습니다. LineOS 표시 명령은 쓰기 가능한 FD를 요구합니다. `fb0` write도 전송한 픽셀을 transfer/flush하며, mmap으로 쓴 픽셀은 명시적으로 LOS_FBIO_FLUSH를 호출해야 합니다. 사용자들은 공유 화면의 동시 쓰기를 조정해야 합니다. Terminal은 fb0를 O_RDWR로 열고 두 화면 정보 ioctl로 32비트 배치를 확인한 뒤 픽셀 범위를 MAP_SHARED로 매핑합니다. 커서 복구가 표시 중인 픽셀을 바꾸지 않도록 기존 렌더링 버퍼를 유지하며, 완성된 화면을 매핑에 복사한 뒤 LOS_FBIO_FLUSH를 호출합니다. 기존 read(3) 입력, TTF 글꼴, UTF-8 처리와 스크롤을 유지합니다. 다른 클라이언트가 사용하는 기존 Display 라이브러리와 DISPLAY_INFO/PRESENT/PRESENT_RECT 호출은 유지합니다.

## mmap과 munmap

Linux 번호 9와 11을 사용하며 6개 인자를 Linux x86-64 레지스터 순서로 전달합니다. 익명 MAP_PRIVATE 매핑은 0으로 초기화하고 R/W/X 권한을 설정합니다. `/dev/fb0`는 MAP_SHARED로 framebuffer RAM 페이지를 직접 매핑합니다. 읽기 FD에서 쓰기 매핑을 요청하면 EACCES입니다. 페이지 정렬과 페이지 단위로 올림한 전체 범위를 프레임버퍼 할당 범위와 비교합니다. 페이지 할당기는 마지막 페이지의 여유 바이트까지 0으로 초기화합니다. 실행 가능한 매핑과 private 매핑은 거부합니다. 매핑 수명을 보존하기 위해 프레임버퍼 할당 후 재생성을 거부하며 동적 해상도 변경은 지원하지 않습니다. FD를 닫아도 매핑은 유지되며 munmap은 부분 범위와 이미 해제된 구간을 처리합니다. 매핑은 프로세스 종료 시 실행 기록 정리와 함께 해제합니다.

현재 매핑 주소 영역은 0x600000000000~0x600100000000이며 호출당 최대 64 MiB, 프로세스당 예약 구간 합계 최대 128 MiB입니다. 주소는 힌트이며 반환 주소를 사용해야 합니다. PROT_NONE, MAP_FIXED, MAP_FIXED_NOREPLACE, 익명 MAP_SHARED와 일반 파일 매핑은 지원하지 않습니다. 고정 플래그와 지원하지 않는 매핑 조합은 EOPNOTSUPP, 일반 파일은 ENODEV를 반환합니다. munmap은 이 매핑 영역에 한정하며 ELF나 에셋 매핑의 임의 해제는 지원하지 않습니다. mprotect, msync 및 파일 writeback은 미구현입니다.

페이지 테이블 변경은 VMM 잠금 안에서 수행하고 현재 주소 공간의 TLB를 갱신한 뒤 물리 페이지를 해제합니다. 현재 CPU 외에서 동일 주소 공간이 실행 중이면 변경하지 않습니다. 다중 스레드 공유 주소 공간은 지원하지 않습니다.

## fork와 execve

Linux 번호 57과 59를 사용합니다. fork는 부모에 자식 PID, 자식에 0을 반환합니다. 사용자 레지스터·스택·쓰기 가능한 private 메모리를 복제하며 COW를 사용하지 않습니다. 에셋과 framebuffer shared 매핑은 공유를 유지하고 FD의 열린 객체와 오프셋도 공유합니다. cwd와 자격 정보는 복제합니다. 부팅 Init의 에셋 게시 권한은 fork 자식에게 부여하지 않습니다.

execve는 새 이미지를 먼저 준비한 뒤 성공 시 같은 PID의 주소 공간을 교체합니다. 실패 시 원래 이미지와 FD를 유지합니다. argv/envp는 각각 64개 이하, 문자열당 최대 4096바이트, 두 문자열 영역 합계 16384바이트로 제한합니다. FD·cwd·자격 정보를 유지하고 기존 private/에셋 매핑을 제거합니다. 현재 `_Start` 규약에 따라 RDI=argc, RSI=argv, RDX=envp를 전달하며 Linux의 초기 스택/auxv ABI는 구현하지 않습니다. 따라서 정적 LineOS ELF 실행 파일을 지원하며 임의 Linux 바이너리, 동적 링커, 스크립트와 ELF TLS는 지원하지 않습니다.

프로세스 생성·교체는 대기 큐로 직렬화하며 실행 슬롯 부족은 EAGAIN입니다. fork 자식 실행 기록은 최대 16개이며 다음 생성 호출에서 종료 기록을 정리합니다. 기본 프로그램의 모든 실행은 fork 후 자식의 execve를 사용하며 argv[0]을 전달합니다. Login은 셸에 USER와 PATH=/bin/lsh 환경 변수를 전달하고 셸은 자식 실행 시 환경을 유지하며 다른 호출부는 빈 환경을 전달합니다. spawn/spawnv 확장과 번호 1030/1031은 제거했습니다. execve 실패는 자식이 출력하고 127로 종료하며 부모는 fork 결과만 받습니다. 셸은 외부 자식을 waitpid로 기다린 뒤 다음 프롬프트를 출력하며 상대 실행 경로와 chdir(80)을 사용하는 cd [PATH]를 지원합니다. 인자 없는 cd는 /로 이동하며 ls 내장 명령은 제공하지 않습니다. 두 줄 프롬프트는 getcwd(79)의 현재 경로와 USER 환경 값을 사용하여 `┏━[경로]━(유저네임)`과 `┗━>`를 표시합니다. waitpid/wait는 wait4(61)를 사용하며 양수 자식 PID 또는 -1, 블로킹 대기 또는 WNOHANG, 선택적인 종료 상태 반환을 지원합니다. 프로세스 그룹 선택과 rusage는 EOPNOTSUPP, 미지원 플래그는 EINVAL입니다. 부모가 살아 있는 동안 미수집 자식 상태를 보존하며 한 번 수집한 뒤에는 ECHILD입니다. 부모가 종료된 기록은 이후 생성 호출에서 회수하며 재부모화는 구현하지 않았습니다. 시그널 전달과 스레드는 지원하지 않습니다. 일반 파일 FD는 기존 경로·namespace generation 검증을 유지하므로 rename/unlink 후 열린 inode 수명은 Linux와 같지 않습니다.

## 기준 자료와 다음 단계

번호와 구조체는 Linux의 [x86-64 번호표](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl), [errno](https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/errno.h), [framebuffer UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/fb.h)를 기준으로 합니다. 다음 단계는 TLS errno, wait/종료 회수, 파일 설명자 기반 inode 수명, 파일 mmap/mprotect/msync, 화면 소유권과 표준 장치 경로의 검증을 강화한 뒤 기존 디스플레이·에셋 전용 호출을 대체하는 것입니다.

/가 없는 명령은 콜론으로 구분한 PATH에서 원래 파일명과 .lix 접미사 순서로 검색하며, 이미 .lix가 있으면 다시 붙이지 않습니다. 빈 PATH 항목은 현재 디렉터리이며 PATH가 없으면 /bin/lsh를 사용합니다. /가 포함된 경로는 입력 그대로 실행합니다. 검색은 fork 자식에서 수행하며 환경 상속과 waitpid 대기 순서를 유지합니다.
