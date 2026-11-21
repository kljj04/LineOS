# QEMU 실행 설정

설정은 `Configs/QEMUSettings.sh`에서 관리한다. 옵션 이름은 Backend에 두고, 설정에는 해당 옵션 뒤에 전달할 값 전체를 둔다.

예: `Debug="{DebugLogPath}"`는 `-D`의 값, `GraphicsDevice="virtio-gpu-gl-pci,xres=2560,yres=1440,hostmem=1G"`는 그래픽 `-device`의 값이다. CPU와 RTC, drive 등도 쉼표로 구분된 값 전체를 직접 편집한다. DebugFlags는 소문자 `-d`의 값이다. Debug, DebugConsole, DebugGlobal, DebugFlags를 빈 문자열로 두면 해당 옵션을 생략한다. DebugNoReboot와 DebugNoShutdown은 인자가 없는 옵션의 활성화 여부다.

네트워크는 `NetworkBackend="user,id=net0"` 또는 `NetworkBackend="bridge,id=net0,br=virbr0"`로 설정한다. NetworkDevice에 NIC 장치와 netdev 연결, MAC 전체를 지정한다. `NetworkBackend="none"`이면 NIC를 생성하지 않고 NetworkDevice를 무시한다. ID를 바꾸면 backend와 device 연결을 함께 수정한다.

DiskImage, DiskSize, DiskEFISizeMiB와 펌웨어 및 로그 경로는 실행 준비에도 사용한다. 상대 경로는 저장소 루트 기준이다. `{DiskImage}`, `{FirmwareCode}`, `{FirmwareVars}`, `{DebugLogPath}`, `{DebugConsolePath}`는 실제 경로로 치환한다. FirmwareVars는 실행 시 임시 복사본이다. drive 경로의 쉼표는 QEMU 규칙에 따라 이스케이프한다. 문자열 eval이나 단어 분리는 사용하지 않는다.

브리지 helper ACL 오류는 `/etc/qemu/bridge.conf`의 허용 브리지, 실제 브리지와 helper 실행 권한 및 배포판 권한 정책을 별도로 확인한다. 스크립트는 ACL, setuid, capability를 자동 변경하지 않는다.

이미지 생성은 raw 기준이다. 사용자 지정 장치·포맷의 지원 여부와 해상도 옵션 등 값 사이의 일관성은 설정 편집 시 확인한다.
