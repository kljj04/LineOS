# LineOS Builder v3.0.0

저장소 루트에서 `npm ci --prefix Scripts`로 의존성을 설치합니다. Node.js 18 이상과 기존 빌드 도구가 필요합니다.

| 명령 | 동작 |
| --- | --- |
| `./Scripts/Run.sh` | 빌드, 기존 이미지에 산출물 복사 및 검증, QEMU 실행 |
| `./Scripts/Run.sh -build` | 증분 빌드만 실행합니다. |
| `./Scripts/Run.sh -Clean` | PascalCase Makefile 타겟 `Clean`을 실행합니다. |
| `./Scripts/Run.sh -Format` | 원본 소스에 clang-format을 적용합니다. |
| `./Scripts/Run.sh -image` | EFI 파티션에 두 산출물만 복사하고 cmp로 검증합니다. |
| `./Scripts/Run.sh -qemu` | 기존 이미지로 QEMU만 실행합니다. |
| `./Scripts/Run.sh --help` | 도움말을 표시합니다. |

옵션은 대소문자를 구분하지 않습니다. Makefile은 `make -f Makefiles/Makefile All` 형태로 직접 실행할 수 있습니다. CC, AS, LD 출력은 JS에서 색을 지정합니다. 터미널에서는 Cyan→Purple OKLCH 배너, cli-kit 스피너와 단계 번호를 표시합니다. Build/Format/Clean의 현재 파일은 `[총 항목 수/현재 항목] 경로`로 같은 줄에서 갱신합니다. Clean은 파일, 심볼릭 링크, 디렉터리를 순서대로 집계합니다. 경고와 오류는 로그에 남깁니다. QEMU 실행 중에는 종료 대기 스피너를 표시합니다. 파이프 출력이나 `NO_COLOR=1`에서는 색과 애니메이션을 생략합니다.

이미지는 자동 생성하거나 포맷하지 않습니다. 이미지가 없으면 실패합니다. 복사하려면 Linux에서 터미널 실행 시 sudo 암호를 입력합니다. 인증과 후속 sudo 명령은 같은 터미널 세션을 유지합니다. 인증 후 작업과 정리 단계는 비대화형으로 실행합니다. 터미널 없는 실행은 사전 인증이 필요합니다. 복사 대상은 `EFI/BOOT/BOOTX64.EFI`와 `KERNEL/LINEOS_KERNEL.ELF`뿐입니다. 기존 루프 연결이 있으면 거부하고, 이번 실행이 할당한 장치와 임시 마운트 경로만 정리합니다. 정리 실패 시 해당 경로와 장치를 오류로 표시합니다. 기존의 지속 마운트 CLI는 제공하지 않습니다.

QEMU 설정은 `Configs/QEMUSettings.toml`에서 읽습니다. 펌웨어 변수는 실행마다 임시 복제하여 저장소의 OVMF_VARS.fd를 보존합니다. 일반 실행은 디스크에 게스트 쓰기를 허용합니다. `LINEOS_DISPLAY=none`으로 화면 없이 실행할 수 있습니다.

`node Scripts/JS/Test.js`는 실제 디스크나 관리자 권한 없이 마운트 실패/정리 경로와 SIGTERM 전달을 검증합니다. 실제 마운트 및 디스크 복사 검증을 대체하지 않습니다.

## CLI 출력

전체 화면 대신 apt처럼 간결한 작업 문구와 진행 상태 줄을 사용하는 CLI입니다. 현재 파일은 한 줄에서 갱신하고 경고와 오류는 로그로 남깁니다. QEMU 종료까지 스피너를 표시합니다. 완료 시 입력 대기 없이 셸로 복귀합니다. 빌드는 기본 순차 실행입니다. 비터미널에서는 갱신 대신 일반 로그를 기록합니다.
