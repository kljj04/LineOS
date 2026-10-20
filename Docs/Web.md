# LineOS HTTP 문서 뷰어

2026-10-20

Kernel.c에서 WebTask와 키보드·마우스 입력 태스크를 시작하도록 연결했다. 기존 네트워크 태스크와 스케줄러를 그대로 사용한다. 렌더러는 Kernel/Source/Render/Web/Web.c, 공개 선언은 Kernel/Include/Render/Web/Web.h에 있다.

기본 페이지는 http://info.cern.ch/hypertext/WWW/TheProject.html 이다. 실제 외부 HTTP 응답 2217바이트를 받아 120개 DOM 노드와 166개 텍스트 run으로 처리하는 것을 확인했다. 페이지 본문을 커널에 내장한 방식이 아니다. 왼쪽 Example Domain 항목도 별도 HTTP 요청으로 열린다.

## 조작

- 링크 클릭: 새 페이지 요청. Back 버튼·Backspace·마우스 뒤로 버튼으로 이전 페이지.
- Reload 버튼·F5: 다시 받기.
- Ctrl+L: 빈 주소창. HTTP 주소를 입력하고 Enter. 주소창 클릭은 현재 주소 편집. Esc는 편집 취소.
- 방향키, Page Up/Down, Home/End, 마우스 휠: 문서 스크롤. 휠 입력 경로는 구현했으며 이번 자동 입력 검증은 PageDown/Home을 사용했다.

## 지원 범위

HTML 태그·텍스트를 작은 DOM 트리로 만들고, 블록/인라인 흐름과 공백 정리·단어 줄바꿈을 적용한다. 제목, 문단, div, section/article, 목록/설명 목록, pre의 줄바꿈, 줄 바꿈 태그와 링크를 처리한다. UTF-8의 BMP 문자와 기본 named/numeric entity를 지원한다. 목록 번호나 bullet, pre의 정확한 공백 간격 등은 완전하지 않다.

style 태그와 inline style, link rel=stylesheet의 별도 HTTP CSS를 읽는다. CSS는 tag, .class, #id, 복합·후손·쉼표 선택자와 기본 specificity/source order, 색상·글자 크기 상속을 지원한다. color, background/background-color, font-size, 단일 margin/padding, max-width, text-align, display:none을 제한적으로 적용한다. 크기는 단일 정수 px 또는 단위 없는 값만 지원한다. 색은 #RGB/#RRGGBB와 일부 named color다. 폰트 크기와 UI는 넓은 화면에서 확대한다.

현대 브라우저나 CSS box model 전체 구현이 아니다. 기본 reader 스타일과 단순한 블록 레이아웃을 사용한다. 실제 DOM API, JavaScript, HTTPS/TLS, 이미지·영상·폼, flex/grid, pseudo selector, CSS 변수·함수, em/vh/% 및 복잡한 shorthand는 지원하지 않는다. 글꼴은 기존 Pretendard 렌더러를 사용하며 CSS font-family/weight를 전부 재현하지 않는다. anchor fragment는 상단 이동으로 처리하고, 상대 링크는 기본 디렉터리 연결 방식이다. 완전한 URL 표준 정규화나 redirect 자동 추적은 없다.

HTML 64KiB 미만, DOM 768개, CSS 규칙 128개, text run 4096개, DOM 중첩 32단계, 외부 stylesheet 최대 4개/각 32KiB 미만으로 제한한다. 문서와 디스플레이 목록은 정적 버퍼에 보관한다. 폰트 렌더링은 하나의 WebTask에서 수행하고 네트워크 대기 중에는 렌더 lock을 잡지 않는다.

## 검증

- make 전체 빌드·링크, 신규 파일 Werror 문법 검사, clang-format, diff 공백 검사, clang 정적 분석 통과.
- 실제 production Kernel.c로 16 CPU KVM 부팅. 1280x720 초기 화면과 2560x1440 최종 화면을 캡처해 시각 확인.
- CERN 본문과 링크를 표시. 첫 hypermedia 링크 클릭으로 WhatIs.html 1149바이트/55노드 문서를 받아 표시하고 Back으로 원래 문서 복귀.
- QMP의 실제 가상 키보드 입력으로 Ctrl+L 주소 편집, Enter 요청, PageDown/Home 스크롤 확인.
- 외부 example.com의 style 태그 2개 CSS 규칙 적용 경로 확인. 지원하지 않는 CSS 함수/단위는 그대로 재현하지 않는다.
- 테스트용 loopback HTTP 서버의 HTML을 QEMU gateway 경유로 받고, 별도 /theme.css 요청으로 4개 규칙을 적용. 배경·제목 색·글자 크기·패딩·최대 폭의 실제 QHD 캡처 확인. 이 페이지는 CSS 검증 fixture이며 기본 외부 사이트와 구분한다.
- 실제 Web.c 소스를 사용하는 호스트 ASan/UBSan 검사에서 DOM, inline/external CSS, specificity, 링크, entity, 키 매핑과 생성 입력 10000개에 대한 파싱/레이아웃 경계 검사 통과.

테스트 하네스는 Tests/Web/Host.c에 있다. Tests는 기존 gitignore에 포함되므로 ZIP에도 보관했다. GUI를 쓰는 사용자의 QEMU 프로세스와 원본 이미지·firmware는 조작하지 않았으며, 검증은 별도 부팅 디렉터리·VARS 복사본으로 수행했다. 커밋·스테이징하지 않았다.

최종 확인 화면은 LineOS-Web.png, CSS fixture 화면은 LineOS-Web-ExternalCSS.png다.
