# 메모리 관리

[문서 목록](README.md) · [한국어](../ko_KR/Memory.md) · [English](../en_US/Memory.md)

## 물리 메모리와 힙

UEFI 메모리 맵을 바탕으로 Buddy 할당기를 초기화합니다. 2의 거듭제곱 단위 물리 페이지 할당, 주소 상한이 있는 할당, 커널 페이지 할당·반환과 힙을 제공합니다. `KAlloc()`·`KFree()` 및 `KMemCpy`·`KMemMove`·`KMemSet`을 사용합니다. 동기화에는 스핀락 기본 기능을 사용합니다.

## 가상 메모리

`VMMInit`, `VMMGetCurrentCR3`, `VMMTranslate`, `VMMMapPage`, `VMMUnmapPage`, `VMMHandleShootdown`을 통해 초기화, 주소 변환, 페이지 매핑·해제와 SMP TLB shootdown을 처리합니다.

물리 메모리 할당과 가상 주소 매핑은 별도 작업입니다. 커널 이미지와 부팅 예약 영역은 일반 물리 할당 대상에서 제외해야 합니다.

## 관련 소스

공개 인터페이스는 `Kernel/Include/Memory/`, 구현은 `Kernel/Source/Memory/`에 있습니다. ELF 유저 작업은 커널 매핑을 공유하는 별도 주소 공간을 사용합니다. VMM은 주소 공간 생성, 비활성 주소 공간의 매핑과 유저 메모리 복사를 제공하며 관리 중인 유저 주소 공간에서도 다른 비활성 공간을 생성·수정할 수 있습니다. 프로세스 격리는 아직 완전한 보안 경계가 아닙니다.
