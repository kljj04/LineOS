$BaseDir = (Get-Location).Path
$RAM = "4G"
$LogFile = "qemu.log"
$DebugConFile = "debugcon.log"
$QEMU = "qemu-system-x86_64"
$Accel = "whpx"
$CPU = "max"
$ImgFile = "$BaseDir\LineOS\LineOS.img"
$ImgSize = "8G"
$EFISizeMiB = "300"
$OVMF_CODE = "$BaseDir\uefi\OVMF_CODE.fd"
$OVMF_VARS = "$BaseDir\uefi\OVMF_VARS.fd"
$LogPath = "$BaseDir\logs\$LogFile"
$DebugConPath = "$BaseDir\logs\$DebugConFile"
$RTC = "base=localtime,clock=host"
$Machine = "q35"
$VGA = "none"
$DisplayConfig = "gtk,zoom-to-fit=off"
$GraphicsWidth = "1920"
$GraphicsHeight = "1080"
$GraphicsResolution = "${GraphicsWidth}x${GraphicsHeight}"
$VideoDevice = "virtio-gpu-pci,xres=${GraphicsWidth},yres=${GraphicsHeight}"
$BlkDevice = "virtio-blk-pci,drive=lineos_disk,bootindex=0"
$Network = "none"
$DebugOption = "guest_errors,cpu_reset"

function Test-Administrator
{
    $Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $Principal = New-Object Security.Principal.WindowsPrincipal($Identity)
    return $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Require-Command($Command)
{
    if (-not (Get-Command $Command -ErrorAction SilentlyContinue))
    {
        Write-Host "    [-] $Command not found." -ForegroundColor Red
        return $false
    }

    return $true
}

function Invoke-WSLImageStep
{
    if (-not (Require-Command "wsl.exe"))
    {
        return $false
    }

    $WSLBaseDir = (& wsl.exe wslpath -a "$BaseDir").Trim()
    if ($LastExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($WSLBaseDir))
    {
        Write-Host "    [-] Failed to convert path for WSL." -ForegroundColor Red
        return $false
    }

    $Script = @'
set -e

BaseDir="$1"
ImgFile="${BaseDir}/LineOS/LineOS.img"
ImgSize="8G"
EFISizeMiB="300"
MountDir="/tmp/lineos_img_mount"
BootFile="${BaseDir}/LineOS/EFI/BOOT/BOOTX64.EFI"
KernelFile="${BaseDir}/LineOS/KERNEL/LINEOS_KERNEL.ELF"

RequireCommand() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "    [-] $1 not found."
        return 1
    fi

    return 0
}

DismountImg() {
    if mountpoint -q "$MountDir" 2>/dev/null; then
        sudo umount -l "$MountDir" 2>/dev/null
    fi

    if [ -f "$ImgFile" ]; then
        LoopDevs=$(losetup -j "$ImgFile" | cut -d: -f1)
        for dev in $LoopDevs; do
            sudo losetup -d "$dev" 2>/dev/null
        done
    fi

    sudo rm -rf "$MountDir"
}

CreateImg() {
    local LoopDev

    if [ -f "$ImgFile" ]; then
        return 0
    fi

    RequireCommand qemu-img
    RequireCommand parted
    RequireCommand mkfs.vfat
    RequireCommand mkfs.ext3
    RequireCommand losetup

    mkdir -p "$(dirname "$ImgFile")"

    echo "    [*] Create raw image."
    qemu-img create -f raw "$ImgFile" "$ImgSize" >/dev/null 2>&1

    echo "    [*] Partition image."
    sudo parted -s "$ImgFile" \
        mklabel gpt \
        mkpart EFI fat32 1MiB "$((EFISizeMiB + 1))MiB" \
        set 1 esp on \
        mkpart LineOS "$((EFISizeMiB + 1))MiB" 100% >/dev/null 2>&1

    LoopDev=$(sudo losetup -P -f --show "$ImgFile" 2>/dev/null)
    if [ -z "$LoopDev" ]; then
        echo "    [-] Failed to setup loop device."
        return 1
    fi

    sleep 0.2

    if [ ! -b "${LoopDev}p1" ]; then
        echo "    [-] EFI partition not found on $LoopDev."
        sudo losetup -d "$LoopDev" 2>/dev/null
        return 1
    fi

    if [ ! -b "${LoopDev}p2" ]; then
        echo "    [-] EXT3 partition not found on $LoopDev."
        sudo losetup -d "$LoopDev" 2>/dev/null
        return 1
    fi

    sudo mkfs.vfat -F 32 -n LINEOS_EFI "${LoopDev}p1" >/dev/null 2>&1
    sudo mkfs.ext3 -F -L LINEOS_ROOT "${LoopDev}p2" >/dev/null 2>&1
    sudo losetup -d "$LoopDev" 2>/dev/null

    echo "    [*] Image ready."
}

CopyImg() {
    local LoopDev

    CreateImg

    sudo rm -rf "$MountDir"
    mkdir -p "$MountDir"

    LoopDev=$(sudo losetup -P -f --show "$ImgFile" 2>/dev/null)
    if [ -z "$LoopDev" ]; then
        echo "    [-] Failed to setup loop device."
        return 1
    fi

    sleep 0.2

    sudo mount "${LoopDev}p1" "$MountDir" 2>/dev/null
    if ! mountpoint -q "$MountDir"; then
        echo "    [-] Mount failed on $LoopDev."
        sudo losetup -d "$LoopDev" 2>/dev/null
        return 1
    fi

    echo "    [*] Image mounted."

    sudo mkdir -p "${MountDir}/EFI/BOOT"
    sudo mkdir -p "${MountDir}/KERNEL"

    sudo rm -f "${MountDir}/EFI/BOOT/BOOTX64.EFI" 2>/dev/null
    sudo rm -f "${MountDir}/KERNEL/LINEOS_KERNEL.ELF" 2>/dev/null

    sudo cp -f "$BootFile" "${MountDir}/EFI/BOOT/BOOTX64.EFI"
    sudo cp -f "$KernelFile" "${MountDir}/KERNEL/LINEOS_KERNEL.ELF"

    sync

    echo "    [*] Copy complete."

    sudo umount -l "$MountDir" 2>/dev/null
    sudo losetup -d "$LoopDev" 2>/dev/null
    echo "    [*] Image unmounted."
}

trap 'DismountImg' EXIT
DismountImg
CopyImg
'@

    $Script | & wsl.exe bash -s -- "$WSLBaseDir"
    return ($LastExitCode -eq 0)
}

function Start-QEMU
{
    if (-not (Test-Path "$BaseDir\LineOS"))
    {
        New-Item -ItemType Directory -Path "$BaseDir\LineOS" -Force | Out-Null
    }

    if (-not (Test-Path "$BaseDir\logs"))
    {
        New-Item -ItemType Directory -Path "$BaseDir\logs" -Force | Out-Null
    }

    if (-not (Require-Command $QEMU))
    {
        return $false
    }

    if (Test-Path $DebugConPath)
    {
        Remove-Item $DebugConPath -Force
    }

    Write-Host "    [*] QEMU start..." -ForegroundColor Cyan
    & $QEMU `
        -rtc $RTC `
        -accel $Accel `
        -cpu $CPU `
        -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" `
        -drive "if=pflash,format=raw,file=$OVMF_VARS" `
        -net $Network `
        -M $Machine `
        -vga $VGA `
        -device "$VideoDevice" `
        -drive "file=$ImgFile,format=raw,id=lineos_disk,if=none" `
        -device "$BlkDevice" `
        -fw_cfg "name=opt/org.tianocore/GraphicsResolution,string=$GraphicsResolution" `
        -no-reboot `
        -d $DebugOption `
        -D $LogPath `
        -debugcon "file:$DebugConPath" `
        -global isa-debugcon.iobase=0xe9 `
        -m $RAM `
        -display $DisplayConfig `
        2> $null

    Write-Host "    [*] Done." -ForegroundColor Green
    return $true
}

if (-not (Test-Administrator))
{
    Write-Host "    [-] Run PowerShell as administrator." -ForegroundColor Red
    exit 1
}

Write-Host "LineOS Builder v2.7.0 (Windows WSL Raw)" -ForegroundColor Yellow

Write-Host "[*] make:" -ForegroundColor Cyan
make
if ($LastExitCode -ne 0)
{
    Write-Host "    [-] make failed." -ForegroundColor Red
    exit $LastExitCode
}
Write-Host "---End of make---" -ForegroundColor Cyan

Write-Host "[*] img:" -ForegroundColor Cyan
if (-not (Invoke-WSLImageStep))
{
    exit 1
}
Write-Host "---End of img---" -ForegroundColor Cyan

Write-Host "[*] run:" -ForegroundColor Cyan
if (-not (Start-QEMU))
{
    exit 1
}

if (Test-Path $LogPath)
{
    $FaultCheck = Select-String `
        -Path $LogPath `
        -Pattern "cpu_reset", "Triple fault", "triple fault", "Triple Fault" `
        -ErrorAction SilentlyContinue
    if ($FaultCheck)
    {
        Write-Host "    [!!] Triple Fault." -ForegroundColor Red
        exit 1
    }
}

Write-Host "---End of run---" -ForegroundColor Cyan
Write-Host "[*] Exit." -ForegroundColor Magenta
