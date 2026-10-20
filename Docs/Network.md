# LineOS IPv4 네트워크 클라이언트

2026-10-20

## 구성

`VirtIONetwork` 드라이버 위에 Ethernet/ARP/IPv4/ICMP/UDP, DNS A 조회, TCP 클라이언트, HTTP GET을 구현했다.

- `Network.c`: 프레임 분배, ARP 캐시·재조회, IPv4 기본 경로, ICMP echo 요청·응답, UDP request/response, HPET 기반 제한 시간, 폴링 및 서비스 태스크.
- `DNS.c`: IPv4 문자열·도메인 처리, UDP DNS, 요청 ID·질문·응답 검증, 압축 이름과 응답 내 CNAME 추적, A 레코드 반환, 최대 3회 시도.
- `TCP.c`: 단일 연결의 SYN/SYN-ACK/ACK, in-order 수신, 중복·부분 중복 처리, 순서 밖 패킷의 ACK, 체크섬·ACK·RST 검증, 8KiB 수신 ring과 window 갱신, MSS 협상, 작은 단위의 stop-and-wait 송신, 1초부터 증가하는 재전송, FIN/ACK 종료, timeout/abort 정리.
- `HTTP.c`: HTTP/1.0·1.1 GET, 상태 코드, Content-Length/chunked/EOF framing, 제한된 interim 응답·trailer, 본문 용량 검사, 잘못된/중복 framing 거부.

공개 API는 `Kernel/Include/Network/Network.h`, 내부 상태는 `NetworkInternal.h`에 있다. 인라인 ASM은 기존 드라이버와 CPU 도우미의 ASM만 사용한다. 신규 프로토콜 코드는 byte 단위 읽기/쓰기로 byte order와 비정렬 패킷을 처리한다.

## 기본 설정과 실행

`NetworkInit()`의 기본값은 현재 Run.sh의 QEMU user backend에 맞춘 고정 설정이다.

| 항목 | 값 |
|---|---|
| Address | 10.0.2.15 |
| Netmask | 255.255.255.0 |
| Gateway | 10.0.2.2 |
| DNS | 10.0.2.3 |
| MTU | 1500 |

다른 환경은 `NetworkConfigure()`에 주소·마스크·게이트웨이·DNS를 명시한다. DHCP를 사용하지 않는다. InitKernel에서 드라이버 다음 NetworkInit을 호출하고, 현재 KMain은 스케줄러 시작 전에 NetworkTask를 생성·등록한다. 기존 사용자 프레임 출력 코드는 유지했다. 테스트용 HTTP 요청을 일반 부팅 경로에 자동 실행하도록 넣지는 않았다.

서비스 태스크는 수신 패킷을 소비하고 ARP/ICMP/TCP를 처리한다. 서비스 태스크가 없어도 요청 API가 대기 중 NetworkPoll을 호출한다. 초기화는 BSP 부팅 시 한 번 수행하며, blocking API는 태스크/일반 실행 문맥에서 사용한다. IRQ handler 안에서 호출하지 않는다.

## 사용 API

```c
#include <Network/Network.h>

STATIC UINT8 Body[32768];

VOID NetworkExampleTask(VOID)
{
    UINT32 Length;
    UINT16 Status;
    UINT32 Address;
    UINT32 RoundTripMls;

    NetworkPing(NETWORK_IPV4(10, 0, 2, 2), 3000, &RoundTripMls);
    NetworkDNSResolve((CONST CHAR8 *) "example.com", &Address, 3000);

    if (NetworkHTTPGet((CONST CHAR8 *) "example.com", 80, (CONST CHAR8 *) "/", Body, sizeof(Body), &Length, &Status, 12000))
    {
    }
}
```

이 예시는 API 형태를 보여준다. 실제 호출자는 각 반환값과 상태 코드를 확인해야 한다.

- `NetworkPing(Address, TimeoutMls, &RoundTripMls)`: ARP와 echo 요청. RTT는 ARP 이후부터 측정하며 timeout은 ARP를 포함한다.
- `NetworkUDPExchange(Address, Port, Request, RequestLength, Response, Capacity, &Length, TimeoutMls)`: 임시 로컬 포트를 사용해 첫 번째 일치하는 datagram 응답을 반환한다. 일반 bind/listen UDP socket API는 아니다. 요청 재시도 여부는 호출자가 결정한다.
- `NetworkDNSResolve(Name, &Address, TimeoutMls)`: IPv4 literal 또는 DNS A 조회. DNS over TCP fallback이나 TTL 결과 캐시는 없다.
- `NetworkTCPConnect`, `NetworkTCPSend`, `NetworkTCPReceive`, `NetworkTCPClose`, `NetworkTCPAbort`: 단일 전역 TCP 연결. Receive 성공 + Length 0은 EOF. Send 실패는 일부 데이터가 이미 상대에게 전달됐을 수 있으며 acknowledgment timeout 시 연결을 정리한다.
- `NetworkHTTPGet(Host, Port, Path, Body, Capacity, &Length, &Status, TimeoutMls)`: DNS부터 연결 종료까지 제한 시간 안에 수행한다. 상태 코드는 호출자에게 반환하며 3xx 자동 추적은 하지 않는다. 본문은 바이너리이며 NUL 종료하지 않는다. 실패 시 일부 본문과 상태가 남을 수 있다.
- `NetworkGetConfig`, `NetworkGetStats`, `NetworkGetLastError`: 설정·통계 snapshot과 마지막 오류.

긴 blocking 연산은 전역 busy 표시로 한 번에 하나만 실행한다. 다른 호출자는 busy로 실패한다. 패킷 처리와 연결 상태는 별도 IRQ-save lock으로 보호하며 sleep/긴 대기 중에는 lock을 잡지 않는다. 여러 호출자가 TCP 슬롯을 공유하려면 호출자 측에서 세션 소유권을 관리해야 한다. VirtQueue 공용 QueueSelect는 기존 드라이버 lock으로 직렬화된다.

## 검증

전체 make 컴파일·링크, 신규 프로토콜 파일 Werror 문법 검사, clang-format, diff 공백 검사와 clang 정적 분석을 통과했다. 기존 VirtIOBlock.c의 volatile 포인터 한정자 경고는 이번 변경과 별개로 남아 있다.

QEMU 8.2.2 / KVM, `-netdev user,id=net0 -device virtio-net-pci,netdev=net0`에서 다음을 확인했다. 원본 이미지와 firmware variables는 사용자가 작업 중인 상태를 보존하고 격리 복사본으로 부팅했다.

| 시나리오 | 1 CPU 격리 하네스 | 16 CPU 실제 스케줄러 + NetworkTask |
|---|---|---|
| gateway ICMP echo | PASS | PASS, RTT 5ms 관찰 |
| UDP 1001바이트 호스트 echo | 전체 바이트 일치 | 전체 바이트 일치 |
| example.com 외부 DNS | PASS | PASS |
| Content-Length HTTP 20,000바이트 | 전체 바이트 일치 | 전체 바이트 일치 |
| chunked HTTP 20,000바이트 | 전체 바이트 일치 | 전체 바이트 일치 |
| EOF HTTP 20,000바이트 | 전체 바이트 일치 | 전체 바이트 일치 |
| 외부 example.com:80 GET / | 200 / 577바이트 | 200 / 577바이트 |

16 CPU 최종 실행은 RX 189 / TX 255 / drop 0 / retransmission 0을 기록했다. 이는 NetworkTask와 검사 태스크가 함께 실행된 결과이며, 여러 TCP 클라이언트의 동시 부하 검증을 뜻하지 않는다. 1 CPU 하네스는 스케줄러를 분리한 초기 단계에서 통과했고, 최종 timeout·캐시 보강은 16 CPU 스케줄러 하네스에서 다시 검증했다.

호스트 C 하네스는 실제 프로토콜 소스를 사용하고 장치·시간·락만 대체했다. ASan/UBSan에서 다음을 통과했다.

- HTTP 정상 framing, interim 응답, 중복 Content-Length, TE+CL, 잘린 본문, 작은 버퍼, 잘못된 chunk, 미지원 gzip 거부.
- 잘못된 IP/TCP 체크섬, IP fragmentation, 잘린 헤더, 순서 밖·중복·겹치는 TCP 데이터, 미래 ACK, RST sequence 검증.
- SYN 1회 유실과 데이터 1회 유실 후 재전송 성공. 연결 timeout 후 상태 정리와 재접속.
- DNS 압축 이름·루프·잘린 이름과 20,000개 생성 입력에 대한 DNS/IPv4/transport 파서 경계 검사.

기존 VirtIO 드라이버의 modern-only 16 CPU 160프레임 왕복 테스트도 통과했다. 마지막 HTTP 버퍼 초기화 변경은 프로토콜 전용 변경이며 raw 드라이버 소스는 변경하지 않았다.

최종 PCAP의 모든 IPv4/ICMP/UDP/TCP 체크섬을 검사하고, 네 TCP 흐름을 재조립해 HTTP 200 응답을 확인했다. 외부 주소로 직접 TCP 패킷이 나간 것도 확인했다. DNS는 시점에 따라 다른 A 레코드를 반환하므로 테스트 로그의 첫 조회 주소와 HTTP 연결 주소가 달라질 수 있다.

## 현재 제한

이 구현은 외부 요청이 가능한 작은 IPv4 클라이언트다. 범용 TCP/IP socket 계층 전체를 완성했다는 뜻은 아니다.

- 고정 주소 설정만 지원. DHCP, IPv6, VLAN, IP fragmentation/reassembly 미지원.
- TCP 단일 연결·보수적인 stop-and-wait 송신. 서버 listen/accept, 여러 연결, SACK, window scaling, 혼잡 제어 최적화, zero-window persist probe, 완전한 TIME_WAIT 유지 미지원. 상대가 window를 다시 알리지 않으면 제한 시간에 실패할 수 있다.
- ARP 캐시는 30초 이후 재조회하고 60초에 만료한다. 장시간/고손실/복잡한 LAN 환경에 대한 검증은 아직 없다.
- DNS는 UDP 512바이트 A 조회. TC 응답은 명시적으로 실패하고 TCP DNS fallback은 없다. CNAME은 같은 응답에 포함된 체인을 최대 8단계 따라간다.
- HTTP는 평문 GET. HTTPS/TLS, 인증, 압축 본문 해제, 자동 redirect, proxy, HTTP/2·3 미지원. body buffer와 header/trailer 길이를 제한한다.
- ICMP는 echo 중심. ICMP unreachable/PMTU 오류를 socket 오류로 전달하는 기능은 없다.
- Run.sh 전체 sudo 이미지 복사 절차는 이번 검증에서 실행하지 않았다. 구성은 기존 그대로 유지했다.

## 재현

테스트는 기존 gitignore에 의해 제외되는 Tests/NetworkStack 아래에 있다. 결과 묶음 ZIP에도 포함했다.

```bash
python3 Tests/NetworkStack/Run.py
python3 Tests/NetworkStack/Run.py --cpus 16 --scheduled
python3 Tests/NetworkStack/Analyze.py Tests/NetworkStack/work/Stack-16-scheduled/Packets.pcap
clang -DLINEOS_KERNEL_BUILD -ICommon -IKernel/Include -fshort-wchar -Wall -Wextra -Werror -fsanitize=address,undefined -g Tests/NetworkStack/Host.c -o Tests/NetworkStack/work/Host
Tests/NetworkStack/work/Host
```

외부 DNS·HTTP 테스트는 실행 호스트의 인터넷 연결이 필요하다. 로컬 echo/HTTP 서버는 테스트 스크립트가 loopback의 임시 포트에 띄우고 종료한다.

참고 규격: [ARP RFC 826](https://www.rfc-editor.org/rfc/rfc826), [IPv4 RFC 791](https://www.rfc-editor.org/rfc/rfc791), [UDP RFC 768](https://www.rfc-editor.org/rfc/rfc768), [DNS RFC 1035](https://www.rfc-editor.org/rfc/rfc1035), [TCP RFC 9293](https://www.rfc-editor.org/rfc/rfc9293), [HTTP/1.1 RFC 9112](https://www.rfc-editor.org/rfc/rfc9112), [QEMU network documentation](https://www.qemu.org/docs/master/system/devices/net.html).
