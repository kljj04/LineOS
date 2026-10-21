# 스케줄러와 작업

[문서 목록](README.md) · [한국어](../ko_KR/Scheduler.md) · [English](../en_US/Scheduler.md)

## 실행 정책

LBPWRR(Load-Balanced Primary Weighted Round Robin)은 CPU별 실행 큐, 현재 작업과 idle 작업을 관리합니다. 우선순위 1~6을 가중치로 사용하며 실행 청크에 해당 횟수만큼 작업을 배치합니다. 작업 추가 시 실행 가능 작업 수가 가장 적은 CPU를 선택하고 큐 잠금 아래 청크를 갱신합니다.

## 작업 전환

`LBPWRRStart()`는 로컬 작업을 선택하고 저장된 인터럽트 프레임을 복원합니다. 첫 복원 직전 TSC deadline을 설정합니다. 타이머 인터럽트는 프레임을 저장하고 작업 상태와 청크를 갱신한 후 다음 작업 프레임을 공통 ISR 복원 경로에 전달합니다. 일반 작업이 없을 때 idle 작업을 선택합니다.

명시적 yield와 TSC deadline 기반 sleep·깨우기를 지원합니다. sleeping·terminated·killed 작업은 실행 가능 청크에 남기지 않습니다.

## 수명과 이동

작업 저장소와 스택을 해제하기 전에 스케줄러 소유권에서 제거해야 합니다. 이동은 소스 CPU의 현재 작업이 아닌 등록된 `TASK_READY` 작업으로 제한됩니다. 두 실행 큐의 잠금은 CPU 인덱스 순서로 획득하며 목록과 청크를 함께 갱신합니다.

## 관련 소스와 제한

인터페이스는 `Kernel/Include/Scheduler/`, 구현은 `Kernel/Source/Scheduler/`에 있습니다. CPU affinity, 시간 기반 sleep 이외의 일반 blocking, 공정성 측정과 추가 SMP 강화는 후속 과제입니다. 기본 등록 작업은 [아키텍처](Architecture.md)를 참고합니다.
