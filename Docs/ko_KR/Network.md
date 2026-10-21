# LineOS 네트워크

[문서 목록](README.md) · [한국어](../ko_KR/Network.md) · [English](../en_US/Network.md)

기본 QEMU 연결은 `-netdev user,id=net0`과 `-device virtio-net-pci,netdev=net0,mac=52:54:00:4c:4f:53`입니다. 호스트 브리지나 TAP 설정 없이 QEMU 사용자 네트워크를 사용합니다. 외부에서 시작하는 연결을 받으려면 별도 포트 전달 설정이 필요합니다.

## 구현 범위

VirtIO-Net, Ethernet, ARP, IPv4, ICMP, UDP, DNS A 조회, DHCPv4 및 TCP 클라이언트를 제공합니다. HTTP와 테스트 웹 렌더러는 커널에서 제거했습니다. HTTP/HTTPS/TLS는 이후 유저스페이스 범위입니다.

`NetworkTask`가 수신 처리와 DHCP 임대 갱신을 수행합니다. DHCP로 주소, 서브넷 마스크, 게이트웨이와 DNS를 얻습니다. `NetworkConfigure()`로 정적 설정을 적용하면 DHCP가 비활성화됩니다. 설정 완료 여부와 값은 `NetworkGetConfig()`로 확인합니다.

UDP는 최대 8개 소켓과 소켓별 8개 수신 큐를 제공합니다. `NetworkUDPOpen`, `NetworkUDPSendTo`, `NetworkUDPReceiveFrom`, `NetworkUDPClose`를 사용합니다. 수신 시 송신 주소와 포트를 반환하며, 작은 버퍼에는 필요한 길이를 알리고 패킷을 보존합니다. `NetworkUDPExchange`는 단일 요청/응답 편의 API입니다.

TCP는 한 번에 하나의 클라이언트 연결을 지원합니다. `NetworkTCPConnect`, `NetworkTCPSend`, `NetworkTCPReceive`, `NetworkTCPClose`와 `NetworkTCPAbort`를 제공합니다. 재전송, 순서 및 중복 검사, 부분 ACK, 수신 윈도우, zero-window probe와 제한된 TIME_WAIT 보존을 처리합니다.

## 제한

TCP listen 및 다중 연결, SACK, 윈도우 확장과 완전한 혼잡 제어는 구현하지 않았습니다. IPv6, IPv4 조각 재조립 및 DHCP 주소 충돌 탐지도 미지원입니다. TCP 전송은 MSS 536의 단순한 순차 전송 방식입니다. NAT는 패킷 파서의 보안을 대신하지 않습니다.

## 검증

`Tests/NetworkStack/Run.py --cpus 16 --scheduled`는 격리된 QEMU에서 DHCP, 게이트웨이 ICMP, UDP echo, 외부 DNS와 12,000바이트 TCP echo/EOF를 검사합니다. 테스트 산출물은 해당 테스트 작업 디렉터리에 저장됩니다. 호스트 sanitizer 검증은 잘못된 패킷, UDP 큐 및 핸들, DHCP 임대 상태와 TCP 재전송·종료 경로를 검사합니다.

호스트의 기존 libvirt `virbr0`는 기본 실행과 무관합니다.
