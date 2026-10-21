# LineOS 문서

[한국어](../ko_KR/README.md) · [English](../en_US/README.md) · [Project](../../README.md)

빌드·실행부터 시작하고 필요한 기능별 문서를 찾아보시면 됩니다. 문서의 소스 경로와 명령은 프로젝트 루트를 기준으로 합니다.

| 문서 | 내용 |
| --- | --- |
| [아키텍처·부팅](Architecture.md) | 초기화 흐름·SMP·인터럽트·소스 구조 |
| [빌드·실행](Build.md) | 의존 도구·디스크 이미지·VM 구성·로그 |
| [메모리](Memory.md) | 물리 할당·힙·가상 메모리 |
| [스케줄러](Scheduler.md) | LBPWRR·문맥 전환·작업 수명·이동 |
| [장치·그래픽·입력](Devices.md) | PCI·VirtIO·GPU·입력·글꼴 |
| [저장소·파일시스템](Storage.md) | 블록 장치·GPT·EXT2·VFS·timestamp |
| [네트워크](Network.md) | VirtIO-Net·NAT·DHCP·UDP·DNS·TCP |
| [검증](Testing.md) | 로컬 테스트·과거 검증 범위 |
| [코드 규칙](Directions.md) | 네이밍·형식·커널 매크로 |

[제3자 고지](../THIRD_PARTY_NOTICES.md)와 `Docs/LICENSES/`는 공통 자료입니다. 구현 범위와 과거 검증 기록은 각 기능 문서와 검증 문서에서 구분합니다.
