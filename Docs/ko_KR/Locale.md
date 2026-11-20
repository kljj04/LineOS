# 로켈과 시간대

[문서 목록](README.md) · [한국어](../ko_KR/Locale.md) · [English](../en_US/Locale.md)

표준 `setlocale`·`strftime`·`strerror`·`localeconv` 등은 musl이 제공합니다. `liblocale.so`는 표준 심볼을 덮어쓰지 않으며 LOS 기본 설정과 `LOSGettext` 내장 번역을 제공합니다. `/etc/locale`은 기본 이름 한 줄, `/etc/timezone`은 프로젝트 시간대 이름 한 줄입니다. 이미지 업데이트는 기존 설정을 보존합니다.

## 명령

```text
locale
locale -a
sudo locale set ko_KR.UTF-8
sudo locale set en_US.UTF-8
```

LOS 설정에서 허용하는 이름은 `C`, `C.UTF-8`, `en_US.UTF-8`, `ko_KR.UTF-8`이며 `POSIX`는 `C`로 정규화합니다. `locale set`은 root 권한을 확인하고 임시 파일과 rename으로 설정을 교체합니다. 저장할 값은 명시적으로 정규화한 단일 이름입니다. musl의 `setlocale(LC_ALL, ...)` 반환값은 범주별 값을 세미콜론으로 조합할 수 있으므로 이를 파일 형식으로 저장하지 않습니다.

## 적용 API와 우선순위

```c
#include <LOSLocale.h>
#include <locale.h>

if (LOSLocaleRefresh() == NULL)
{
    setlocale(LC_ALL, "C");
}
```

`LOSLocaleRefresh(VOID)`는 `/etc/locale`을 읽고 범주별로 다음 순서의 **비어 있지 않은** 값을 선택한 뒤 musl에 적용합니다.

1. `LC_ALL`
2. 해당 `LC_*`
3. `LANG`
4. 유효한 `/etc/locale`
5. `C`

존재하지 않거나 잘못된 파일은 C로 돌아갑니다. 명시한 환경변수가 LOS 지원 이름이 아니면 실패하며 기존 범주 선택은 유지합니다. 빈 환경변수는 다음 우선순위로 넘어갑니다. 파일 기본값을 LANG에 자동 저장하지 않으므로 이전 설정이 환경변수처럼 남아 갱신을 가리지 않습니다. 사용자가 실제로 지정한 LANG·LC_*는 계속 파일보다 우선합니다.

Locale constructor는 시작 시 적용하며, 기본 프로그램도 이 API로 초기화합니다. lsh는 매 프롬프트에 다시 적용하므로 `sudo locale set ...` 후 다음 프롬프트부터 내장 문구가 변경됩니다. 이미 실행 중인 다른 프로그램은 재적용하거나 재시작해야 합니다. `setlocale(LC_ALL, "")` 자체는 musl의 환경변수 선택 API이며 LOS 파일을 직접 읽지 않습니다.

`LOSLocaleNormalize(Name)`는 지원 이름을 정규화하며 지원하지 않는 입력에는 NULL을 반환합니다. 프로젝트 로켈 적용을 원하는 프로그램은 `[Dependencies] Libraries`에 `Locale`을 선언합니다. musl 자체의 임의 로켈 이름 허용 여부를 LOS 설정 검증으로 사용하지 않습니다.

## 번역 범위

`LOSGettext(Message)`는 LC_MESSAGES가 `ko_KR.UTF-8`이면 `Libraries/Locale/Source/Gettext.c`의 영어 원문 순서 번역표를 이진 검색하고, 없으면 원문을 반환합니다. 서식 인자 수와 타입은 원문과 같아야 합니다. lsh의 직접 메시지도 현재 LC_MESSAGES에 따라 선택합니다. 범주별 사용자 환경과 sudo가 전달하는 로켈 환경은 이 선택에 영향을 줍니다.

내장 번역은 musl 전체 번역 데이터가 아닙니다. musl의 날짜·요일·월 이름, strerror·perror, 숫자·통화·정렬 규칙이 한국어로 모두 바뀌는 것은 아닙니다. 현재 프로젝트는 완전한 한국어 musl locale 데이터나 외부 gettext 도메인·복수형 API를 제공하지 않습니다. 따라서 LC_MESSAGES=ko_KR.UTF-8과 LOS 문구 번역이 적용되어도 `date`의 표준 strftime 출력은 영어일 수 있습니다.

## 시간대

Locale constructor는 TZ가 설정되지 않은 경우 `/etc/timezone`의 고정 시간대 표를 POSIX TZ 문자열로 변환하고 tzset을 호출합니다. 명시한 TZ는 보존합니다. 고정 UTC 오프셋과 약어를 사용하며 지역별 DST 전환 데이터베이스는 제공하지 않습니다. 로켈 변경은 UTC 커널 시계·EXT2 timestamp를 변경하지 않습니다. [시계](Clock.md)를 참고합니다.

## 검증

`python3 Scripts/Validation/Locale.py`는 실제 프로젝트 musl과 프로젝트 소스를 사용하여 동일 프로세스에서 파일 변경 후 재적용, 환경변수 우선순위·빈 값, 내장 번역과 명령의 단일 이름 저장을 검사합니다. 설정 파일 접근은 임시 경로로 치환하여 호스트 설정을 수정하지 않습니다. 호스트 실행 검사이며 VM 화면 검증과 구분합니다.
