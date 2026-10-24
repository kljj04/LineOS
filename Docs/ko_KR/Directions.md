# 코드 작성 규칙

[문서 목록](README.md) · [한국어](../ko_KR/Directions.md) · [English](../en_US/Directions.md)

## 네이밍

함수·변수·인자·구조체 멤버는 PascalCase를 사용하며 CPU·GPU·PCI·VirtIO 등 기존 약어 표기를 보존합니다. 구조체·enum·typedef 타입, enum 항목과 매크로는 SCREAMING_SNAKE_CASE를 사용합니다. 네이밍 규칙에는 예외를 두지 않습니다. 기존 인터페이스를 호출할 때는 실제 선언된 이름을 재사용하며 API를 추측해서 만들지 않습니다.

## 형식과 주석

Allman 중괄호와 공백 4칸을 사용하며 탭은 사용하지 않습니다. 함수 이름과 괄호는 붙이고 제어문 뒤 괄호는 띄웁니다. 인자가 없는 선언·정의에는 `VOID`를 명시합니다. 인자·매개변수 목록은 길어도 줄바꿈하지 않습니다. 기존 3줄 파일 헤더를 보존합니다. 구현 본문 주석은 추가하지 않으며 헤더의 인터페이스 설명 주석은 허용합니다.

자동 정렬은 루트 `.clang-format`을 기준으로 합니다.

| 설정 | 값 |
| --- | --- |
| 기본 스타일 | LLVM입니다 |
| IndentWidth / TabWidth / UseTab | 4 / 4 / Never입니다 |
| ColumnLimit | 12000입니다 |
| PointerAlignment / DerivePointerAlignment | Right / false입니다 |
| BreakBeforeBraces | Allman입니다 |
| SpaceBeforeParens / SpaceAfterCStyleCast | ControlStatements / true입니다 |
| BinPackParameters / BinPackArguments | true / true입니다 |
| AllowShortFunctionsOnASingleLine | Empty입니다 |
| AlwaysBreakAfterReturnType | None입니다 |
| AlignConsecutiveAssignments | None입니다 |
| AlignConsecutiveDeclarations | Enabled·AlignCompound·AlignFunctionPointers·PadOperators를 켜며 빈 줄·주석을 가로질러 정렬하지 않습니다 |
| AlignConsecutiveMacros | Enabled만 켜며 빈 줄·주석을 가로지르는 정렬과 AlignCompound·AlignFunctionPointers·PadOperators는 끕니다 |
| SortIncludes | false입니다 |
| AttributeMacros | `__attribute__`입니다 |
| StatementMacros | `CLI`·`HLT`입니다 |

## 커널 정의

`CONST`, `NULL`, `TRUE`, `FALSE`, `STATIC`, `EXTERN`, `INLINE`, `PACKED`, `MS_ABI`, `SYSV_ABI` 등 프로젝트 정의를 사용합니다. 인라인 어셈블리는 `ASM`을 사용합니다. 호출부를 수정하기 전에 실제 헤더에서 타입과 인터페이스를 확인합니다.

## 작업 범위와 문서

사용자 변경과 기존 하위 시스템의 역할을 보존합니다. 컴파일·링크, 이미지 배치, 실제 실행 관찰을 구분합니다. README는 개요로 유지하고 기능별 문서는 `Docs/en_US/`·`Docs/ko_KR/`에 대응하도록 작성합니다. 한국어 문서는 합니다/습니다 문체를 사용합니다. 구현 존재만으로 기본 KMain에서 서비스를 시작한다고 설명하지 않습니다.
