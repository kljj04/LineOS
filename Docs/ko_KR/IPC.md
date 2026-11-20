# IPC와 창 프로토콜

[문서 목록](README.md) · [한국어](../ko_KR/IPC.md) · [English](../en_US/IPC.md)

시스템 호출 1036은 메시지 복사 방식의 엔드포인트를 제공합니다. 공통 ABI는 `Common/Syscall/IPC/IPC.h`, 구현은 `Kernel/Source/Syscall/IPC`·`Kernel/Include/Syscall/IPC`, LOSLib 클라이언트 API는 `<IPC.h>`에 있습니다.

## 소유권과 전송

엔드포인트의 소유 태스크만 수신·바인딩·닫기를 수행합니다. 이름 조회는 실제 UID별로 분리합니다. 일반 전송은 같은 UID 사이 또는 root 발신을 허용하며, 클라이언트가 명시적으로 바인딩한 상대는 해당 클라이언트에 답신할 수 있습니다. 사용자 lwm과 root Terminal은 이 제한된 답신 관계를 사용합니다.

Terminal은 임의 이름 조회 대신 현재 화면 소유자가 등록한 compositor 엔드포인트를 마스터 TTY ioctl로 얻습니다. 엔드포인트 ID는 증가하며 재사용하지 않습니다. fork는 부모 엔드포인트의 소유권을 부여하지 않고, exec 성공·exit·태스크 해제 시 엔드포인트를 정리합니다.

## API

Program.cfg Libraries에 LOSLib를 선언합니다. 아래 코드는 호출 순서 예시이며 실제 프로그램에서는 각 반환값을 확인해야 합니다.

```c
INT64 Endpoint = LOSIPCCreate(NULL);
INT64 Server = LOSIPCLookup("lwm");
LOSIPCBind(Endpoint, Server);
LOSIPCSend(Endpoint, Server, Data, Size);
LOSIPCReceive(Endpoint, Buffer, Capacity, &Sender);
LOSIPCClose(Endpoint);
```

## 용량과 오류

| 자원 | 제한 |
| --- | --- |
| 전역 엔드포인트 | 64개 |
| 수신 큐 메시지 | 16개 |
| 메시지 크기 | 64 KiB |
| 전체 대기 payload | 16 MiB |

빈 수신 큐와 가득 찬 송신 큐는 EAGAIN을 반환합니다. 수신 버퍼가 작으면 메시지를 유지하고 EMSGSIZE를 반환합니다. 커널은 검사한 유저 메모리로 데이터를 복사하며 다른 프로세스의 포인터를 직접 참조하지 않습니다.

## 창 프로토콜

공통 ABI는 `Common/Window/Protocol.h`, Window 라이브러리의 클라이언트 API는 `<Window.h>`입니다. 프로그램은 Program.cfg에 Window를 선언하며 Window의 Library.cfg가 LOSLib 의존성을 연결합니다. 서버 이름은 lwm입니다.

```c
LOS_WINDOW Window;
LOSWindowOpen(&Window, 0, "My app", Width, Height, 0);
LOSWindowPresent(&Window, Pixels);
LOSWindowPoll(&Window, &Event);
LOSWindowClose(&Window);
```

CREATE/CREATED, PIXELS/PRESENT, KEY/POINTER, CLOSE/DESTROY를 사용합니다. 픽셀은 행 단위 청크로 전송하고 PRESENT에서 확정합니다. KEY는 입력 바이트, POINTER는 클라이언트의 원래 내용 좌표와 버튼·휠 상태입니다. 클라이언트는 이벤트를 주기적으로 읽어야 합니다. CLOSE는 협력적 요청이며 프로세스를 강제 종료하지 않습니다.

Terminal은 LOS_WINDOW_BACKGROUND로 배경 레이어에 등록합니다. lwm은 실제 UID 0인 서비스에만 이 플래그를 허용하며 일반 창 목록과 별도로 배경 하나를 유지합니다. 배경 좌표에는 창 장식 오프셋을 적용하지 않습니다. LOSWindowPresentRows는 호출자가 지정한 변경 행만 비교·전송하여 입력마다 전체 프레임을 비교하지 않도록 합니다.

## 제한

공유 메모리 전송·소켓 API·블로킹 poll·복수 독립 터미널 PTY·강제 앱 종료는 지원하지 않습니다. 합성과 입력은 [lwm](Lwm.md), 검증 범위는 [검증 안내](Validation.md)를 참고합니다.
