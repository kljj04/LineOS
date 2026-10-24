# 시계·경과 시간·NTP

[문서 목록](README.md) · [한국어](../ko_KR/Clock.md) · [English](../en_US/Clock.md)

## 시계 기준

`ClockInit()`은 UTC로 읽은 RTC 시각과 보정된 TSC 주파수로 날짜·시각을 초기화합니다. `ClockGetTime()`은 Unix 초와 나노초, `ClockGetUnixTime()`은 UTC Unix 초를 반환합니다. `ClockGetDateTime()`의 오프셋은 표시 변환에만 적용합니다. EXT2 timestamp는 UTC를 유지합니다.

`ClockGetUptimeMls()`는 시계 초기화 시점을 기준으로 HPET 경과 시간을 반환합니다. NTP로 날짜·시각을 앞이나 뒤로 보정해도 이 값과 스케줄러·네트워크 타임아웃의 단조 시간은 변경하지 않습니다.

## 자동 동기화

`ClockNTPTask`는 구현되어 있지만 현재 `KMain()`에서 등록하지 않습니다. 호출자가 네트워크 작업과 함께 등록하고 주소 설정이 준비되면 `pool.ntp.org`에 SNTPv4 요청을 보내고, 성공한 뒤 600초(10분)마다 다시 동기화합니다. 요청 전체 제한 시간은 5초이며 실패 시 현재 시계를 유지하고 64→128→256→512→600초 간격으로 재시도합니다.

`ClockSyncNTP(Server, TimeoutMls)`는 서버 이름 또는 IPv4 문자열을 받습니다. `ClockGetNTPStatus()`로 동기화 진행 여부, 마지막 성공, 실패 수, 서버 주소, 왕복 지연과 다음 시도 시점을 확인합니다. 호출은 직렬화되며 다음 예정 시점 전에는 새 요청을 보내지 않습니다. `CLOCK_NTP_SERVER`와 `CLOCK_NTP_PORT`의 기본값은 각각 `pool.ntp.org`와 UDP 123입니다.

요청 timestamp와 응답 originate 필드, 송신 주소·포트, 길이, 버전·모드·stratum·동기화 상태 및 서버 timestamp 순서를 검사합니다. 네 timestamp를 이용해 지연과 오프셋을 계산하고 UTC 기준점을 갱신합니다. 2036년 NTP 초 필드 순환은 기존 RTC 시각에 가장 가까운 era로 해석합니다. RATE 등 KoD 응답은 재시도를 늦추며 DENY·RSTR 응답에는 24시간 대기합니다.

## 화면

현재 Kernel.c는 CPU 사용률과 마우스 입력을 처리하며 ClockDisplayTask나 시계·NTP 상태 자동 표시는 없습니다. KST(UTC+09:00)가 필요한 호출자는 `ClockGetDateTime()`에 540분 오프셋을 전달합니다.

## 제한과 관련 소스

초기 구현은 RTC 초기화가 성공해야 동작하는 단일 서버 SNTP 클라이언트입니다. 시각 보정은 즉시 적용하므로 UTC 날짜·시각이 뒤로 이동할 수 있습니다. RTC 하드웨어에는 쓰지 않으며, 주파수 보정·점진적 보정·다중 서버 선택·NTS 인증은 제공하지 않습니다. 응답 검사만으로 서버 인증이 성립하지는 않습니다.

구현과 인터페이스는 `Kernel/Source/Timer/Clock.c`와 `Kernel/Include/Timer/Clock.h`에 있습니다.

패킷과 계산 방식은 [RFC 4330](https://www.rfc-editor.org/rfc/rfc4330.html)을 참고합니다.
