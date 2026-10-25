# 장치·그래픽·입력

[문서 목록](README.md) · [한국어](../ko_KR/Devices.md) · [English](../en_US/Devices.md)

## PCI와 VirtIO 공통 기반

PCI 설정 공간 탐색, Q35 지원, 공통 VirtIO PCI 전송 계층과 VirtQueue를 제공합니다. 구현은 `Kernel/Source/PCI/`와 `Kernel/Source/VirtIO/`에 있습니다.

| 장치           | 구현 기능                               | 관련 위치                   |
|----------------|-----------------------------------------|-----------------------------|
| VirtIO GPU     | 프레임버퍼 생성·scanout·전송·flush·커서 | `Kernel/Source/Render/GPU/` |
| VirtIO 입력    | 키보드·태블릿 입력                      | `Kernel/Source/Input/`      |
| VirtIO block   | 동기·비동기 읽기·쓰기·flush             | `Kernel/Source/Disk/`       |
| VirtIO network | RX/TX 큐와 네트워크 장치                | `Kernel/Source/Network/`    |

블록 계층 위 파일시스템은 [저장소](Storage.md), 네트워크 프로토콜은 [네트워크](Network.md)에서 설명합니다.

## 그래픽과 텍스트

픽셀 읽기·쓰기, 사각형·선·원, 알파 합성, 프레임버퍼 복사와 dirty rectangle 추적을 제공합니다. TrueType 글꼴은 assembler `.incbin`으로 포함하고 실행 중 파싱·래스터화해 GPU
프레임버퍼에 합성합니다.

커널은 패닉 출력용으로 `Kernel/Source/Render/TrueType/`과 `Kernel/Assets/Fonts/`의 내장 폰트를 유지합니다. 유저 공간은 별도 TrueType 정적 라이브러리와 `/sys/fonts/`의 폰트 파일을 사용합니다. 현재 Shell은 ASCII 출력만 유저 프레임버퍼에 그린 뒤 디스플레이 시스템 호출로 표시하며 대화형 터미널은 아닙니다. [유저 프로그램](UserLibraries.md)과 [제3자 고지](../THIRD_PARTY_NOTICES.md)를 참고합니다.

## 입력 큐와 블로킹

키보드 IRQ 벡터 65와 태블릿 벡터 66은 각각 별도 VirtIO 큐·64칸 이벤트 링·IRQ 저장 스핀락을 사용합니다. 링의 실사용 용량은 63개이며 가득 차면 새 이벤트를 버리는 기존 정책을 유지합니다.
PushKeyEvent·PushPointerEvent는 해당 이벤트 잠금 안에서 이벤트를 발행한 뒤 대기 작업을 깨웁니다.

`InputMouseHandler()`는 포인터 이벤트를 소진한 뒤 `InputWaitMouseEvents()`, `InputKeyboardHandler()`는 키 이벤트를 소진한 뒤
`InputWaitKeyboardEvent()`를 호출합니다. VirtIO 대기 함수는 같은 조건 잠금으로 링 인덱스를 확인하고 비어 있을 때만 WaitQueueSleep을 호출하므로 검사와 블로킹 사이의 wake를
놓치지 않습니다. IRQ 플래그·잠금 순서·스택 소유권은 [스케줄러](Scheduler.md)를 참고합니다.

현재 KMain은 키보드·마우스 소비자를 등록하지 않습니다. 드라이버 IRQ 처리만으로 Shell에 키보드 입력을 전달하거나 이벤트 링을 상위 핸들러로 소진하지는 않습니다.

## 지원 범위

기본 대상은 QEMU Q35의 VirtIO 장치입니다. 실제 PC의 GPU·랜카드·입력 컨트롤러 지원에는 해당 드라이버가 필요합니다. 드라이버 초기화와 기본 커널의 입력 UI 실행 여부는 별개이며, 현재 실행
경로는 [아키텍처](Architecture.md)에 정리되어 있습니다.
