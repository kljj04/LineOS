# 저장소와 파일시스템

[문서 목록](README.md) · [한국어](../ko_KR/Storage.md) · [English](../en_US/Storage.md)

## GPT와 EXT2

GPT 헤더와 파티션 배열 CRC를 검사하고 두 번째 GPT 파티션의 EXT2를 마운트합니다. EXT2 revision 0/1, 1·2·4 KiB 블록과 128·256바이트 inode 검증 기록이 있습니다.

파일·디렉터리 생성, 읽기·쓰기, sparse 확장, truncate, 삭제, 같은 볼륨 rename·이동·대상 교체, hard link와 inline·block 기반 symbolic link를 제공합니다. 직접 및 단일·이중·삼중 간접 블록 주소를 처리합니다.

32비트 UID/GID와 owner/group/other 권한, sticky 디렉터리와 setgid 상속을 지원합니다. 쓰기 마운트 시 블록 소유권, 할당 수, 디렉터리 참조와 부모 관계를 검증합니다.

## VFS와 시각

VFS는 파티션별 루트, 경로 해석, 디렉터리 열거와 명령 문맥을 제공합니다. `/`는 선택한 파티션의 루트이며 `^`는 관리자용 가상 파티션 관리 영역입니다. 일반 사용자에게는 해당 영역을 not-found로 처리합니다.

`ClockGetUnixTime()`은 RTC 기반 UTC Unix 시간을 제공합니다. EXT2는 inode 변경에 ctime, 내용 변경에 mtime, 쓰기 가능 마운트에서 relatime atime을 갱신합니다. 화면의 지역 시각 오프셋은 디스크 timestamp에 영향을 주지 않습니다.

## 쓰기와 장애 한계

한 트랜잭션은 최대 32768개 변경 블록을 준비하며, 이를 넘으면 commit 전에 실패합니다. dirty 표시와 flush 순서를 적용하지만 저널은 없으므로 실제 전원 손실 시 원자적 복구나 데이터 보존을 보장하지 않습니다. commit 실패 후에는 해당 마운트 접근을 차단하며 다음 쓰기 마운트 전에 오프라인 검사가 필요합니다.

활성 확장 속성·ACL, supplementary groups, journaling 및 지원하지 않는 feature·inode flag의 수정은 지원하지 않습니다. 프로세스 파일 디스크립터, 열린 파일 수명 계층, 유저 모드 인증도 후속 범위입니다. 호스트가 EFI FAT32 파티션을 준비하는 것과 커널 FAT32 백엔드 구현은 별개입니다.

## 관련 소스와 실행 상태

GPT·블록 드라이버는 `Kernel/Source/Disk/`, EXT2·VFS는 `Kernel/Source/FS/`, 인터페이스는 `Kernel/Include/FS/`에 있습니다. 예전 개발 셸과 파일시스템 테스트 작업은 현재 기본 `KMain()`에 포함되지 않습니다. 과거 검증 기록은 [검증 문서](Testing.md)를 참고합니다.
