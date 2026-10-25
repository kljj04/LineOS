# 스케줄러와 작업

[문서 목록](README.md) · [한국어](../ko_KR/Scheduler.md) · [English](../en_US/Scheduler.md)

LBPWRR(Load-Balanced Primary Weighted Round Robin)은 CPU별 실행 큐·현재 작업·idle 작업을 관리하며 우선순위 1~6을 그대로 가중치로 사용합니다. 작업을 추가할 때 등록된 실행 목록의 작업 수가 가장 적은 CPU를 선택합니다. 이 수에는 등록 상태인 sleeping·blocked 작업도 포함됩니다.

## 선택과 스택 소유권

작업 풀과 CPU별 실행 목록은 `LBPWRR_MAX_TASKS`인 1024개, 가중 실행 청크는 중복 가중치 자리를 포함하여 `CHUNK_MAX_COUNT`인 1024칸으로 제한합니다. 각 작업에는 4 GiB 아래에서 할당한 16페이지 스택을 제공하며 첫 인터럽트 프레임도 이 스택에 복사합니다.

`LBPWRRStart()`는 로컬 작업을 즉시 선택·복원하므로 필요한 작업을 먼저 생성·등록합니다. 첫 문맥 복원 직전에 TSC deadline을 설정합니다. 벡터 64의 타이머 스케줄링 간격은 명목상 5ms이며 명시적 yield는 소프트웨어 인터럽트 67(`0x43`)을 사용합니다. 일반 READY 작업을 우선하고 없으면 로컬 idle을 선택합니다. BLOCKED와 SLEEPING은 선택하지 않습니다.

작업 스택을 사용하는 동안에는 다음 작업을 선택한 뒤에도 `OnCPU`를 유지합니다. 공통 ISR이 RSP를 다음 스택으로 바꾼 뒤 `LBPWRRCompleteSwitch()`를 호출하고 레지스터와 `iretq` 문맥을 복원합니다. 이전 작업은 스택 소유권이 해제된 뒤에만 이동하거나 스택을 해제할 수 있습니다. idle 작업은 이동과 Wait Queue 진입을 금지합니다. idle까지 사용할 수 없으면 기존 “No runnable task” 패닉으로 불변식 위반을 알리며, 패닉 회피를 위해 BLOCKED 작업을 실행하지 않습니다.

## 범용 Wait Queue

`Kernel/Include/Scheduler/WaitQueue.h`는 재사용 가능한 FIFO 대기 목록을 제공합니다.

| 인터페이스 | 계약 |
| --- | --- |
| `WaitQueueInit(Queue)` | 공개 전에 초기화하며 Queue는 모든 대기 작업보다 오래 유지합니다 |
| `WaitQueueSleep(Queue, ConditionLock)` | 현재 작업을 BLOCKED로 등록하고 잠금을 해제한 뒤 yield하며, 복귀 전에 조건 잠금을 다시 획득합니다 |
| `WaitQueueWakeOne(Queue)` | 가장 먼저 등록한 대기 작업을 분리하고 깨웁니다 |
| `WaitQueueWakeAll(Queue)` | 현재 대기 작업을 모두 분리하고 깨웁니다 |
| `WaitQueueFinishTask(Task, State)` | 스케줄러 동기화 아래 대기 목록에서 분리하고 종료 상태를 적용하며 kill·terminate에서 사용합니다 |

Sleep은 IRQ를 끄고 조건 잠금을 획득하여 조건을 검사한 뒤 호출합니다. 관련 없는 다른 잠금은 보유하지 않습니다. 조건 잠금·공통 WaitLock·RunQueue 잠금을 모두 해제한 뒤 yield합니다. 복귀 시에는 IF=0을 유지한 채 조건 잠금만 다시 획득합니다. 호출자는 while로 조건을 재검사한 뒤 저장했던 IRQ 플래그를 복원합니다. 현재 작업이 없거나 idle이거나 현재 작업 상태가 부적절하면 조건 잠금을 유지하며 FALSE를 반환합니다. Wake는 IRQ 플래그를 저장·복원하므로 IRQ 문맥에서 사용할 수 있습니다.

입력 생산자와 소비자가 같은 이벤트 조건 잠금을 사용하므로 빈 링 검사와 대기 등록 사이의 이벤트를 놓치지 않습니다. 이벤트를 먼저 발행한 뒤 깨웁니다. 잠금 순서는 입력 virtqueue 잠금 → 이벤트 조건 잠금 → 공통 WaitLock → 대상 RunQueue 잠금입니다. RunQueue 코드는 자신의 잠금을 보유한 채 WaitLock이나 입력 잠금을 획득하지 않습니다.

Wake가 yield보다 먼저 발생하면 CurrentTasks에 남은 작업을 RUNNING으로 되돌리고, 이미 전환된 작업은 READY로 만듭니다. 현재 작업 검사와 OnCPU로 실행·ISR 스택 전환이 끝나기 전의 이동을 막습니다. 원격 wake는 기존 타이머의 스케줄링을 사용하며 재스케줄 IPI는 보내지 않습니다.

## 청크 용량·우선순위·수명

Wait Queue의 BLOCKED 작업은 청크의 가중치 자리를 예약하지만 READY 선택 조건에서 제외됩니다. Wake에는 삽입이 필요 없어 청크가 가득 차도 깨울 수 있습니다. TSC deadline을 사용하는 SLEEPING 작업은 청크에서 제거하고 기한이 지나면 다시 삽입합니다.

우선순위 변경은 해당 RunQueue 잠금 아래 청크 갱신이 성공한 뒤 Priority·Weight에 반영합니다. 용량 부족으로 실패하면 BLOCKED 상태를 포함해 이전 값을 유지합니다. NotifyTaskChanged도 Wait Queue에 등록된 대기 작업의 예약 자리를 보존합니다. 대기 작업을 kill·terminate하면 목록에서 분리하고 청크 자리를 제거하므로 이후 wake가 종료 작업을 되살리지 않습니다.

스케줄러에서 제거하려면 종료 상태이며 현재 작업도 OnCPU도 아니어야 합니다. 이동은 등록된 non-idle READY 작업 중 현재 작업과 OnCPU가 아닌 대상으로 제한하며 두 큐 잠금을 CPU 인덱스 순서로 획득합니다. 로드 밸런서 함수는 존재하지만 현재 시작·인터럽트 경로에서 주기적으로 호출하지 않습니다.

## CPU 사용률과 기본 작업

`Usage.c`는 스케줄링 경계에서 TSC 시간을 busy·idle로 누적하고 약 100ms 간격으로 표본을 갱신합니다. `UsageGetCPU()`는 정수 백분율, `UsageGetTotal()`은 활성 CPU의 산술 평균을 반환합니다. TASK의 CPUTimeTSC·CPUUsage는 작업별 시간과 백분율입니다. 호스트 QEMU 프로세스의 사용률이 아닌 스케줄러 회계 값입니다.

현재 시작 경로는 Init을 등록하고 Init이 Shell을 실행합니다. 사용률 화면과 키보드·마우스 소비자 작업은 등록하지 않습니다. Shell은 출력 스트림을 반복 확인하며 유저 yield 시스템 호출이 현재 실패를 반환하므로 CPU를 사용할 수 있습니다. [장치](Devices.md)·[아키텍처](Architecture.md)를 참고합니다.

## 관련 소스와 제한

인터페이스는 `Kernel/Include/Scheduler/`, 구현은 `Kernel/Source/Scheduler/`, 스택 복원은 `Kernel/Source/Interrupt/ISR.S`에 있습니다. CPU affinity 정책, 추가 공정성 분석과 작업 풀·시간 기반 sleep의 동시성 강화는 후속 과제입니다. 대기 목록은 공통 잠금 하나를 공유하며 대규모 경합은 별도 평가가 필요합니다.
