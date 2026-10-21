# 코드 작성 규칙

[문서 목록](README.md) · [한국어](../ko_KR/Directions.md) · [English](../en_US/Directions.md)

## 네이밍

함수·변수·인자·구조체 멤버는 PascalCase를 사용합니다. CPU·GPU·PCI·VirtIO 등 기존 약어 표기를 보존합니다. 구조체·enum·typedef 타입, enum 항목과 매크로는 SCREAMING_SNAKE_CASE를 사용합니다. 런타임 심볼 충돌 가능성이 있으면 `KMemSet`처럼 K 접두사를 사용합니다.

## 형식

Allman 중괄호와 공백 4칸을 사용합니다. 함수 이름과 괄호는 붙이고 제어문과 괄호는 띄웁니다. 인자가 없어도 `VOID`를 명시합니다. 인자·매개변수 목록은 길어도 줄바꿈하지 않습니다. 기존 3줄 파일 헤더를 보존하고 본문 주석을 추가하지 않습니다. 상세 자동 형식은 루트 `.clang-format`을 기준으로 합니다.

## 커널 매크로

`CONST`, `NULL`, `TRUE`, `FALSE`, `STATIC`, `EXTERN`, `INLINE`, `PACKED`, `MS_ABI`, `SYSV_ABI` 등 프로젝트 정의를 사용합니다. 인라인 어셈블리는 `ASM`을 사용합니다. 타입과 API는 실제 헤더를 확인하고 기존 이름을 재사용합니다.

## 작업 범위

사용자의 수정사항과 기존 파일 구조를 보존합니다. 빌드 결과와 실제 런타임 검증을 구분하고, 구현되지 않은 기능을 완료된 것으로 기록하지 않습니다.
