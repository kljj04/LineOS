# IPC와 창 프로토콜

`SYSCALL_IPC = 1036`을 통해 프로세스별 메시지 엔드포인트를 제공합니다. 공통 ABI는 `Common/Syscall/IPC/IPC.h`, 커널은 `Kernel/{Source,Include}/Syscall/IPC`, libc API는 `<IPC.h>`에 있습니다.

엔드포인트는 소유 태스크만 수신·닫기·연결할 수 있습니다. 이름 검색은 실제 사용자 UID별로 분리합니다. 일반 전송은 같은 UID 사이에서 허용하며 root 발신을 허용합니다. 클라이언트가 명시적으로 바인딩한 상대는 그 클라이언트에 답신할 수 있습니다. 이 경로로 사용자 lwm과 root Terminal 서비스가 제한된 답신 관계를 맺습니다. Terminal은 임의 이름 검색 대신 현재 그래픽 소유자가 등록한 compositor 엔드포인트를 마스터 TTY ioctl로 받습니다.

엔드포인트 ID는 재사용하지 않는 증가 값입니다. 전역 엔드포인트 64개, 수신 큐 16개 메시지, 메시지 최대 64KiB, 대기 payload 총 16MiB로 제한합니다. 빈 수신 큐와 가득 찬 송신 큐는 EAGAIN을 반환합니다. 수신 버퍼가 작으면 메시지를 유지하고 EMSGSIZE를 반환합니다. 커널은 사용자 메모리를 복사하며 프로세스 간 포인터를 직접 참조하지 않습니다. 엔드포인트는 fork로 사용할 권한을 상속하지 않으며 exec 성공, exit, 태스크 해제 시 정리합니다.

```c
INT64 Endpoint = LOSIPCCreate(NULL);
INT64 Server = LOSIPCLookup("lwm");
LOSIPCBind(Endpoint, Server);
LOSIPCSend(Endpoint, Server, Data, Size);
LOSIPCReceive(Endpoint, Buffer, Capacity, &Sender);
LOSIPCClose(Endpoint);
```

창 ABI는 `Common/Window/Protocol.h`, 클라이언트 API는 `<Window.h>`입니다. 서버 이름은 lwm입니다.

```c
LOS_WINDOW Window;
LOSWindowOpen(&Window, 0, "My app", Width, Height, 0);
LOSWindowPresent(&Window, Pixels);
LOSWindowPoll(&Window, &Event);
LOSWindowClose(&Window);
```

CREATE/CREATED, PIXELS/PRESENT, KEY/POINTER, CLOSE/DESTROY를 사용합니다. 픽셀은 행 단위 청크로 전송하고 PRESENT에서 확정합니다. KEY는 입력 바이트, POINTER는 내용 영역의 원래 해상도 좌표와 버튼·휠 상태입니다. 클라이언트는 이벤트를 주기적으로 읽어야 합니다. CLOSE는 협력적 요청이며 앱을 강제 종료하지 않습니다.

현재 IPC는 복사 기반입니다. 다른 프로세스와 같은 물리 페이지를 매핑하는 공유 메모리 전송, 소켓 API, 블로킹 poll, 복수 터미널 PTY 및 임의 앱에 대한 강제 종료는 구현하지 않았습니다.

빌드 및 실행 검증은 수행하지 않았습니다.

공용 Terminal은 LOS_WINDOW_BACKGROUND 플래그로 배경 레이어에 등록합니다. lwm은 실제 UID 0의 서비스에만 이 플래그를 허용하고, GUI 창 목록과 별도의 단일 레이어로 유지합니다. 배경 좌표에는 창 제목이나 테두리 오프셋을 적용하지 않습니다.

LOSWindowPresentRows는 호출자가 이미 알고 있는 변경 행 구간만 비교·전송하는 경로입니다. Terminal의 입력 응답 경로가 전체 프레임 비교에 묶이지 않도록 이 API를 사용합니다.
