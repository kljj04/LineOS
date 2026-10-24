# LineOS 네트워크

[문서 목록](README.md) · [한국어](../ko_KR/Network.md) · [English](../en_US/Network.md)

기본 QEMU 연결은 `-netdev user,id=net0`과 `-device virtio-net-pci,netdev=net0,mac=52:54:00:4c:4f:53`입니다. 호스트 브리지나 TAP 설정 없이 QEMU 사용자 네트워크를 사용합니다. 외부에서 시작하는 연결을 받으려면 별도 포트 전달 설정이 필요합니다.

## 구현 범위

VirtIO-Net, Ethernet, ARP, IPv4, ICMP, UDP, DNS A 조회, DHCPv4 및 TCP 클라이언트를 제공합니다. HTTP와 웹 렌더러는 커널에서 제거했습니다. HTTP/HTTPS/TLS는 이후 유저스페이스 범위입니다.

`NetworkTask`가 NetworkPoll과 1ms 시간 대기로 수신 처리와 DHCP 임대 갱신을 수행합니다. 현재 KMain은 이 작업을 등록하지 않으며, `NetworkInit()`만으로 작업을 생성하거나 DHCP 임대를 획득하지 않습니다. 입력 Wait Queue 변경은 네트워크 작업까지 인터럽트 기반 블로킹으로 전환하지 않습니다. DHCP로 주소, 서브넷 마스크, 게이트웨이와 DNS를 얻습니다. `NetworkConfigure()`로 정적 설정을 적용하면 DHCP가 비활성화됩니다. 설정 완료 여부와 값은 `NetworkGetConfig()`로 확인합니다.

UDP는 최대 8개 소켓과 소켓별 8개 수신 큐를 제공합니다. `NetworkUDPOpen`, `NetworkUDPSendTo`, `NetworkUDPReceiveFrom`, `NetworkUDPClose`를 사용합니다. 수신 시 송신 주소와 포트를 반환하며, 작은 버퍼에는 필요한 길이를 알리고 패킷을 보존합니다. `NetworkUDPExchange`는 단일 요청/응답 편의 API입니다.

## TCP 연결과 API

TCP는 소켓 핸들로 구분하는 최대 16개 클라이언트 연결을 지원합니다. 연결마다 로컬 포트, 순서 번호·윈도우, 재전송 상태와 8 KiB 수신 버퍼를 보유합니다. 상대 주소·상대 포트·로컬 포트로 패킷을 분배하므로 같은 서버의 같은 포트에도 여러 연결을 유지할 수 있습니다.

| API | 동작 |
| --- | --- |
| `NetworkTCPConnect(Address, Port, &Socket, TimeoutMls)` | handshake 후 핸들을 반환하며 실패 시 `UINT32_MAX`를 기록합니다 |
| `NetworkTCPSend(Socket, Buffer, Length, TimeoutMls)` | 지정한 연결로 전송합니다 |
| `NetworkTCPReceive(Socket, Buffer, Capacity, &Length, TimeoutMls)` | 지정한 연결에서 수신하며 성공 시 길이 0은 정상 EOF입니다 |
| `NetworkTCPClose(Socket, TimeoutMls)` | 진행 중인 송수신이 끝나고 미수신 데이터를 읽은 후 정상 종료합니다 |
| `NetworkTCPAbort(Socket)` | 해당 연결을 취소하고 핸들을 무효화합니다 |

기존 전역 연결 API에서 변경되었으므로 호출부는 반환받은 핸들을 보관하고 각 작업에 전달해야 합니다. 슬롯 재사용 시 generation으로 이전 핸들을 거부합니다. 다른 연결은 독립적으로 진행하며, 같은 연결에서도 reader 하나와 writer 하나는 동시에 실행할 수 있습니다. 중복 reader·writer 또는 송수신 중 close 요청은 busy 실패를 반환합니다. abort는 진행 중인 작업을 중단할 수 있으며, 해당 작업이 소유권을 반환하기 전에는 슬롯을 재사용하지 않습니다.

재전송, 순서·중복 검사, 부분 ACK, 수신 윈도우와 zero-window probe를 처리합니다. 송신 타임아웃은 해당 연결만 취소하며 수신 타임아웃은 연결을 유지합니다. 주소 설정 변경은 전체 연결을 무효화합니다. `NetworkGetLastError()`는 공유 진단값이며 작업별 오류 저장소는 아닙니다.

TIME_WAIT는 최대 64개 기록을 120초간 보존하며 활성 연결의 종료에 필요한 공간도 예약합니다. 용량이 소진되면 만료 전 기록을 덮어쓰지 않고 새 연결을 거부합니다. 연결 실패와 명시적 abort는 슬롯을 반환합니다.

## 제한

TCP listen, SACK, 윈도우 확장과 완전한 혼잡 제어는 구현하지 않았습니다. IPv6, IPv4 조각 재조립 및 DHCP 주소 충돌 탐지도 미지원입니다. TCP 전송은 MSS 536의 단순한 순차 전송 방식입니다. NAT는 패킷 파서의 보안을 대신하지 않습니다.

## 관련 소스

인터페이스는 `Kernel/Include/Network/`, 구현은 `Kernel/Source/Network/`에 있습니다. 장치는 `VirtIONetwork.c`, 공통 계층은 `Network.c`, 프로토콜은 `UDP.c`·`TCP.c`·`DHCP.c`·`DNS.c`로 나뉩니다.

호스트의 기존 libvirt `virbr0`는 기본 실행과 무관합니다.

UDP 기반 시각 동기화와 경과 시간은 [시계·NTP](Clock.md)를 참고합니다.
