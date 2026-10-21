# 검증과 체크포인트

[문서 목록](README.md) · [한국어](../ko_KR/Testing.md) · [English](../en_US/Testing.md)

## 결과를 읽는 기준

컴파일·링크 성공, 부팅 이미지 갱신, QEMU에서 관찰한 실행 성공은 별도 단계입니다. 과거 테스트 결과는 이후 모든 커널 작업에 대한 보장이 아닙니다. 이번 문서 정리에서 이전 파일시스템 테스트를 다시 실행하지 않았습니다.

## 현재 네트워크 체크포인트

QEMU 사용자 NAT와 16 CPU 스케줄러 환경에서 DHCP 주소 획득, 게이트웨이 ICMP, 1,001바이트 UDP echo, 외부 DNS, 12,000바이트 TCP stream·EOF가 통과했습니다. 이전 단일 연결 검사 기록은 RX 47·TX 50·drop 0·재전송 0입니다. 다중 연결 검증 범위와 실행 방법은 [네트워크](Network.md)에 정리합니다. 전체 TCP 규격 적합성 검증은 아닙니다.

| 로컬 경로 | 목적 |
| --- | --- |
| `Tests/Network/` | VirtIO-Net 드라이버 검증 |
| `Tests/NetworkStack/` | 프로토콜 host 검사와 QEMU 통합 검증 |
| `Tests/VMM/` | 메모리 host·QEMU 검사 |

`Tests/NetworkStack/`의 C·Python 테스트 소스는 저장소에 포함합니다. 생성된 작업 결과와 다른 로컬 테스트 디렉터리는 ignore 대상이므로 새 clone에 없을 수 있습니다. 실행 전에 존재 여부를 확인합니다. 네트워크 실행 예시는 [네트워크](Network.md)를 참고합니다.

## 과거 EXT2 검증

이전 README가 기록한 2026-10-19 체크포인트에는 실제 EXT2/GPT 소스의 ASan·UBSan·경고 오류 처리 아래 182개 host 이미지 시나리오가 포함됩니다. 블록·inode 크기, revision 0, 삼중 간접 sparse 파일, 할당·반환, 경로·링크·권한·재마운트와 손상 메타데이터 거부를 검사했습니다.

할당·디스크 쓰기·flush 실패 및 해당 경계의 host 프로세스 종료를 검사하고 필요 시 `e2fsck`로 복구했습니다. 1·16 CPU QEMU 커널에서 EXT2 PASS, 결과 파티션의 `e2fsck -fn` 통과 기록이 있습니다. 복사한 디스크·펌웨어를 사용했으며 실제 장치 캐시와 모든 전원 손실 상황을 재현하지는 않습니다.

후속 Clock/VFS/셸 체크포인트는 clock 13개, 블록 크기별 FS/VFS 144개, 셸 117개 host 검사와 1·16 CPU 각각 27개 키보드 명령, 결과 이미지 검사를 기록했습니다. 당시 명령은 `python3 Tests/FS/Run.py --work /tmp/LineOSFSTests`였으나 현재 `Tests/FS/Run.py`와 옛 `Docs/VFS.md`는 없습니다. 현재 구현 개요는 [저장소](Storage.md)를 참고합니다.
