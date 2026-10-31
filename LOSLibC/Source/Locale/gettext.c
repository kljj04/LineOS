// LOSLibC/Source/Locale/gettext.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <libintl.h>
#include <locale.h>
#include <string.h>

typedef struct
{
    CONST char *English;
    CONST char *Korean;
} LOCALE_MESSAGE;

STATIC CONST LOCALE_MESSAGE Messages[] = {
    {"\nTasks: ", "\n태스크: "},
    {"  -     Read standard input", "  -     표준 입력 읽기"},
    {"  -1  Print one entry per line", "  -1  한 줄에 한 항목 표시"},
    {"  -A  Show hidden entries except . and ..", "  -A  .과 ..을 제외한 숨김 항목 표시"},
    {"  -F  Append / to directories and @ to symlinks", "  -F  디렉터리에 /, 심볼릭 링크에 @ 표시"},
    {"  -a  Show all entries", "  -a  모든 항목 표시"},
    {"  -l  Use a long listing format", "  -l  자세한 목록 표시"},
    {"  -p  Show elapsed time only", "  -p  경과 시간만 표시"},
    {"  -s  Show boot time in the configured timezone", "  -s  설정된 시간대의 부팅 시각 표시"},
    {"  FILE  Read and print a file", "  파일  파일을 읽어 출력"},
    {"  PID    UID  CPU   CPU% STATE   TYPE NAME", "  PID    UID  CPU   CPU% 상태    유형 이름"},
    {"  [PAUSED]", "  [일시정지]"},
    {"  adduser USERNAME  Create user account", "  adduser 사용자명  사용자 계정 생성"},
    {"  deluser USERNAME  Delete user account", "  deluser 사용자명  사용자 계정 삭제"},
    {"  exit              Return to Login", "  exit              로그인으로 돌아가기"},
    {"  help              Show available commands", "  help              사용 가능한 명령 표시"},
    {"  page ", "  페이지 "},
    {"  returned ", "  조회 "},
    {"  sort: CPU\n", "  정렬: CPU\n"},
    {"  sort: PID\n", "  정렬: PID\n"},
    {"  status            Show recovery status", "  status            복구 상태 표시"},
    {"  timezone set <TIMEZONE>", "  timezone set <시간대>"},
    {" MiB managed  |  up ", " MiB 관리  |  가동 "},
    {" cores\n     ", " 코어\n     "},
    {" idle tasks (excluded)\n\n", " 유휴 태스크 (제외)\n\n"},
    {" s\n", " 초\n"},
    {"% idle time  |  ", "% 유휴 시간  |  "},
    {"0 seconds", "0초"},
    {"AddUser: deriving password hash...", "AddUser: 암호 해시를 생성하고 있습니다..."},
    {"AddUser: invalid username", "AddUser: 잘못된 사용자명입니다"},
    {"AddUser: password must contain 8 to 127 printable characters", "AddUser: 암호는 출력 가능한 문자 8~127자로 구성해야 합니다"},
    {"AddUser: passwords do not match or confirmation is invalid", "AddUser: 암호가 일치하지 않거나 확인 입력이 잘못되었습니다"},
    {"AddUser: secure random source or password hashing unavailable", "AddUser: 안전한 난수나 암호 해시를 사용할 수 없습니다"},
    {"Available commands:", "사용 가능한 명령:"},
    {"Click me", "눌러 보세요"},
    {"Confirm password: ", "암호 확인: "},
    {"DST:      Disabled", "일광 절약 시간: 사용 안 함"},
    {"Drag any edge or corner to resize.", "변이나 모서리를 끌어 크기를 바꾸세요."},
    {"Drag the lower-right corner to resize.", "오른쪽 아래를 끌어 크기를 바꾸세요."},
    {"Drag the title bar to move.", "제목 표시줄을 끌어 이동하세요."},
    {"Exit", "종료"},
    {"Failed to create stress supervisor.\n", "stress: 관리 프로세스를 생성하지 못했습니다.\n"},
    {"Hello, LineOS!", "안녕하세요, LineOS!"},
    {"Init: Login execve failed", "Init: 로그인을 실행할 수 없습니다"},
    {"Init: Login fork failed", "Init: 로그인 프로세스를 생성할 수 없습니다"},
    {"Init: Terminal execve failed", "Init: 터미널을 실행할 수 없습니다"},
    {"Init: Terminal fork failed", "Init: 터미널 프로세스를 생성할 수 없습니다"},
    {"Init: asset publication failed: %s\n", "Init: 자산을 게시할 수 없습니다: %s\n"},
    {"Init: cannot open font: %s\n", "Init: 글꼴을 열 수 없습니다: %s\n"},
    {"Init: empty font file: %s\n", "Init: 빈 글꼴 파일입니다: %s\n"},
    {"Init: font exceeds buffer capacity: %s\n", "Init: 글꼴이 버퍼 용량을 초과했습니다: %s\n"},
    {"Init: font read failed: %s\n", "Init: 글꼴을 읽을 수 없습니다: %s\n"},
    {"Init: terminal master open failed", "Init: 터미널 마스터를 열 수 없습니다"},
    {"LineOS Login", "LineOS 로그인"},
    {"LineOS User Rescue", "LineOS 사용자 복구"},
    {"Locale set: %s\n", "로켈 설정: %s\n"},
    {"Login: UserRescue execve failed", "Login: 복구 환경을 실행할 수 없습니다"},
    {"Login: UserRescue fork failed", "Login: 복구 프로세스를 생성할 수 없습니다"},
    {"Login: account database unavailable or invalid; entering UserRescue", "Login: 계정 데이터베이스를 사용할 수 없어 복구 환경으로 진입합니다"},
    {"Login: cannot enter home directory", "Login: 홈 디렉터리로 이동할 수 없습니다"},
    {"Login: cannot prepare home directory", "Login: 홈 디렉터리를 준비할 수 없습니다"},
    {"Login: failed to establish user credentials", "Login: 사용자 권한을 설정할 수 없습니다"},
    {"Login: incorrect username or password", "Login: 사용자명이나 암호가 잘못되었습니다"},
    {"Login: invalid username", "Login: 잘못된 사용자명입니다"},
    {"Login: shell execve failed", "Login: 쉘을 실행할 수 없습니다"},
    {"Login: shell fork failed", "Login: 쉘 프로세스를 생성할 수 없습니다"},
    {"Login: waitpid failed", "Login: 자식 프로세스를 기다릴 수 없습니다"},
    {"Name:     %s\n", "이름:   %s\n"},
    {"New password for %s: ", "%s의 새 암호: "},
    {"Offset:   UTC%c%02u:%02u\n", "오프셋: UTC%c%02u:%02u\n"},
    {"Password: ", "암호: "},
    {"Shell + GUI", "쉘 + GUI"},
    {"Stress test started in background.\n", "스트레스 테스트를 백그라운드에서 시작했습니다.\n"},
    {"Terminal: cannot publish terminal size", "Terminal: 터미널 크기를 설정할 수 없습니다"},
    {"Terminal: font initialization failed", "Terminal: 글꼴 초기화에 실패했습니다"},
    {"Terminal: framebuffer initialization failed", "Terminal: 프레임버퍼 초기화에 실패했습니다"},
    {"Terminal: framebuffer mapping failed", "Terminal: 프레임버퍼 매핑에 실패했습니다"},
    {"Terminal: initialization failed", "Terminal: 초기화에 실패했습니다"},
    {"TestGUI: cannot acquire font", "TestGUI: 글꼴을 가져올 수 없습니다"},
    {"TestGUI: cannot initialize font", "TestGUI: 글꼴을 초기화할 수 없습니다"},
    {"TestGUI: cannot open devices", "TestGUI: 장치를 열 수 없습니다"},
    {"TestGUI: cannot query framebuffer", "TestGUI: 프레임버퍼 정보를 조회할 수 없습니다"},
    {"TestGUI: display is busy", "TestGUI: 화면이 사용 중입니다"},
    {"TestGUI: unsupported framebuffer", "TestGUI: 지원하지 않는 프레임버퍼입니다"},
    {"Timezone changed: %s -> %s\n", "시간대 변경: %s -> %s\n"},
    {"Timezone set: %s\n", "시간대 설정: %s\n"},
    {"Timezone: %s\n", "시간대: %s\n"},
    {"Type 'help' for available commands.", "사용 가능한 명령을 보려면 'help'를 입력하십시오."},
    {"Usage:", "사용법:"},
    {"Usage: adduser USERNAME", "사용법: adduser 사용자명"},
    {"Usage: btop", "사용법: btop"},
    {"Usage: cat [FILE ...]", "사용법: cat [파일 ...]"},
    {"Usage: date", "사용법: date"},
    {"Usage: deluser USERNAME", "사용법: deluser 사용자명"},
    {"Usage: locale [-a | set <LOCALE> | --help]", "사용법: locale [-a | set <로켈> | --help]"},
    {"Usage: ls [-aAlF1] [PATH ...]", "사용법: ls [-aAlF1] [경로 ...]"},
    {"Usage: sudo COMMAND [ARGUMENT ...] | sudo -k", "사용법: sudo 명령어 [인자 ...] | sudo -k"},
    {"Usage: timezone [-a | set <TIMEZONE> | --help]", "사용법: timezone [-a | set <시간대> | --help]"},
    {"Usage: touch [-c|--no-create] [--] FILE...", "사용법: touch [-c|--no-create] [--] 파일..."},
    {"Usage: uptime [-p|--pretty] [-s|--since]", "사용법: uptime [-p|--pretty] [-s|--since]"},
    {"UserRescue: Login execve failed", "UserRescue: 로그인을 실행할 수 없습니다"},
    {"UserRescue: Login fork failed", "UserRescue: 로그인 프로세스를 생성할 수 없습니다"},
    {"UserRescue: recovery environment active", "UserRescue: 복구 환경이 활성화되었습니다"},
    {"UserRescue: returning to Login", "UserRescue: 로그인으로 돌아갑니다"},
    {"UserRescue: unknown command", "UserRescue: 알 수 없는 명령입니다"},
    {"UserRescue: use adduser root to initialize a missing account database", "UserRescue: 계정 데이터베이스가 없으면 adduser root로 초기화하십시오"},
    {"Username: ", "사용자명: "},
    {"Workers: %u | Duration: %u seconds\n", "작업자: %u | 실행 시간: %u초\n"},
    {"[sudo] password for %s: ", "[sudo] %s의 암호: "},
    {"account database I/O failed", "계정 데이터베이스 입출력에 실패했습니다"},
    {"account database limit reached", "계정 데이터베이스 한도에 도달했습니다"},
    {"account database updated", "계정 데이터베이스를 갱신했습니다"},
    {"account updated; lock cleanup required; do not repeat the update", "계정을 갱신했습니다. 잠금 정리가 필요합니다. 갱신을 반복하지 마십시오"},
    {"block", "대기"},
    {"btop: cannot open terminal", "btop: 터미널을 열 수 없습니다"},
    {"btop: monitor read or output failed", "btop: 상태 조회 또는 출력에 실패했습니다"},
    {"btop: terminal must be at least 64 columns by 20 rows", "btop: 터미널은 최소 64열 20행이어야 합니다"},
    {"cannot acquire account database lock", "계정 데이터베이스 잠금을 얻을 수 없습니다"},
    {"cannot open account database", "계정 데이터베이스를 열 수 없습니다"},
    {"cat: missing file operand", "cat: 파일을 지정해야 합니다"},
    {"date: cannot format current time", "date: 현재 시각을 표시할 수 없습니다"},
    {"day", "일"},
    {"exit", "종료"},
    {"hour", "시간"},
    {"initialize root first", "먼저 root 계정을 생성해야 합니다"},
    {"invalid account database or password encoding; no recovery overwrite performed", "계정 데이터베이스나 암호 인코딩이 잘못되었습니다. 덮어쓰지 않았습니다"},
    {"invalid username", "잘못된 사용자명입니다"},
    {"kern ", "커널 "},
    {"killed", "중단"},
    {"locale: invalid locale environment", "locale: 잘못된 로켈 환경변수입니다"},
    {"locale: unsupported locale '%s'\n", "locale: 지원하지 않는 로켈 '%s'\n"},
    {"ls: invalid directory data in '%s'\n", "ls: '%s': 잘못된 디렉터리 데이터\n"},
    {"ls: invalid directory entry in '%s'\n", "ls: '%s': 잘못된 디렉터리 항목\n"},
    {"ls: invalid option -- '%c'\n", "ls: 잘못된 옵션 -- '%c'\n"},
    {"ls: path too long: '%s'\n", "ls: '%s': 경로가 너무 깁니다\n"},
    {"lwm demo", "lwm 데모"},
    {"minute", "분"},
    {"n: new window", "n: 새 창"},
    {"q quit | space pause | s sort | n/p tasks | c cores", "q 종료 | space 정지 | s 정렬 | n/p 태스크 | c 코어"},
    {"q quit, space pause, s CPU/PID sort, n/p task pages, c core pages", "q 종료, space 일시정지, s CPU/PID 정렬, n/p 태스크 페이지, c 코어 페이지"},
    {"ready", "준비"},
    {"root account is protected", "root 계정은 보호되어 있습니다"},
    {"run", "실행"},
    {"second", "초"},
    {"sleep", "수면"},
    {"sudo: %s: command not found\n", "sudo: %s: 명령을 찾을 수 없습니다\n"},
    {"sudo: authentication failed", "sudo: 인증에 실패했습니다"},
    {"sudo: cannot establish root credentials", "sudo: 관리자 권한을 설정할 수 없습니다"},
    {"sudo: cannot identify calling user", "sudo: 호출한 사용자를 확인할 수 없습니다"},
    {"sudo: cd: cannot change directory", "sudo: cd: 디렉터리를 변경할 수 없습니다"},
    {"sudo: cd: path too long", "sudo: cd: 경로가 너무 깁니다"},
    {"sudo: cd: too many arguments", "sudo: cd: 인자가 너무 많습니다"},
    {"sudo: executable must be owned by root with mode 4755", "sudo: 실행 파일의 소유자는 root이고 권한은 4755여야 합니다"},
    {"sudo: execve failed", "sudo: 프로그램 실행에 실패했습니다"},
    {"sudo: user is not allowed by /etc/sudoers", "sudo: /etc/sudoers에서 허용되지 않은 사용자입니다"},
    {"timezone: cannot read current timezone", "timezone: 현재 시간대를 읽을 수 없습니다"},
    {"timezone: unknown timezone '%s'\n", "timezone: 알 수 없는 시간대 '%s'\n"},
    {"total %lu\n", "합계 %lu\n"},
    {"touch: missing file operand", "touch: 파일을 지정해야 합니다"},
    {"touch: unknown option: %s\n", "touch: 알 수 없는 옵션: %s\n"},
    {"unknown account database error", "알 수 없는 계정 데이터베이스 오류"},
    {"up ", "가동 시간 "},
    {"update failed; lock or temporary file cleanup may be required", "갱신에 실패했습니다. 잠금이나 임시 파일 정리가 필요할 수 있습니다"},
    {"uptime: --pretty and --since cannot be combined", "uptime: --pretty와 --since는 함께 사용할 수 없습니다"},
    {"uptime: cannot convert time", "uptime: 시각을 변환할 수 없습니다"},
    {"uptime: cannot read current time", "uptime: 현재 시각을 읽을 수 없습니다"},
    {"uptime: cannot read monotonic time", "uptime: 단조 시각을 읽을 수 없습니다"},
    {"uptime: cannot read system information", "uptime: 시스템 정보를 읽을 수 없습니다"},
    {"uptime: unknown argument: %s\n", "uptime: 알 수 없는 인자: %s\n"},
    {"user ", "유저 "},
    {"user already exists", "사용자가 이미 있습니다"},
    {"user not found", "사용자를 찾을 수 없습니다"},
};

char *gettext(const char *Message)
{
    if (Message == NULL)
    {
        return NULL;
    }

    if (strcmp(setlocale(LC_MESSAGES, NULL), "ko_KR.UTF-8") != 0)
    {
        return (char *) Message;
    }

    UINTN First = 0;
    UINTN Last = sizeof(Messages) / sizeof(Messages[0]);

    while (First < Last)
    {
        UINTN Middle = First + (Last - First) / 2;
        int   Order = strcmp(Message, Messages[Middle].English);

        if (Order == 0)
        {
            return (char *) Messages[Middle].Korean;
        }

        if (Order < 0)
        {
            Last = Middle;
        }
        else
        {
            First = Middle + 1;
        }
    }

    return (char *) Message;
}