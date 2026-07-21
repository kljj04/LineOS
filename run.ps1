$BaseDir = Get-Location
$RAM = "4G"
$LogFile = "qemu.log"
$QEMU = "qemu-system-x86_64"
$Accel = "whpx"
$VHD = "$BaseDir\LineOS\LineOS.vhdx"
$VHDDrive = "L:"
$OVMF_CODE = "$BaseDir\uefi\OVMF_CODE.fd"
$OVMF_VARS = "$BaseDir\uefi\OVMF_VARS.fd"
$LogPath = "$BaseDir\logs\$LogFile"
$BootFile = "$BaseDir\LineOS\EFI\BOOT\BOOTX64.EFI"
$KernelFile = "$BaseDir\LineOS\KERNEL\LINEOS_KERNEL.ELF"
$EFIPath = "$VHDDrive\EFI\BOOT"
$KernelPath = "$VHDDrive\KERNEL"
$RTC = "base=localtime,clock=host"
$Machine = "pc"
$VGA = "std"
$GraphicsResolution = "2560x1440"
$Network = "none"
$DebugOption = "guest_errors,cpu_reset"

function Mount-VHD($Silent = $false)
{
    Mount-DiskImage -ImagePath $VHD | Out-Null
    Start-Sleep -Milliseconds 500
    if (-not $Silent)
    {
        Write-Host "    [*] VHD mounted." -ForegroundColor Cyan
    }
}

function Unmount-VHD($Silent = $false)
{
    if (-not (Test-Path $VHD))
    {
        return
    }
    $DiskImage = Get-DiskImage -ImagePath $VHD
    if ($DiskImage.Attached)
    {
        Dismount-DiskImage -ImagePath $VHD | Out-Null
        if (-not $Silent)
        {
            Write-Host "    [*] VHD unmounted." -ForegroundColor Cyan
        }
    }
}

function Copy-VHD
{
    if (-not (Test-Path $VHDDrive))
    {
        Write-Host "    [-] VHD drive not found." -ForegroundColor Red
        return $false
    }
    if (-not (Test-Path $EFIPath))
    {
        New-Item -ItemType Directory -Path $EFIPath -Force | Out-Null
    }
    if (-not (Test-Path $KernelPath))
    {
        New-Item -ItemType Directory -Path $KernelPath -Force | Out-Null
    }
    Copy-Item $BootFile "$EFIPath\BOOTX64.EFI" -Force
    Copy-Item $KernelFile "$KernelPath\LINEOS_KERNEL.ELF" -Force
    Write-Host "    [*] Copy complete." -ForegroundColor Cyan
    return $true
}

function Run-QEMU
{
    if (-not (Test-Path "$BaseDir\LineOS"))
    {
        New-Item -ItemType Directory -Path "$BaseDir\LineOS" -Force | Out-Null
    }
    if (-not (Get-Command $QEMU -ErrorAction SilentlyContinue))
    {
        Write-Host "    [-] QEMU not found." -ForegroundColor Red
        return
    }
    if (-not (Test-Path "$BaseDir\logs"))
    {
        New-Item -ItemType Directory -Path "$BaseDir\logs" -Force | Out-Null
    }
    Write-Host "    [*] QEMU start..." -ForegroundColor Cyan
    & $QEMU `
        -rtc $RTC `
        -accel $accel `
        -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" `
        -drive "if=pflash,format=raw,file=$OVMF_VARS" `
        -net $Network `
        -M $Machine `
        -vga $VGA `
        -fw_cfg "name=opt/org.tianocore/GraphicsResolution,string=$GraphicsResolution" `
        -no-reboot `
        -d $DebugOption `
        -D $LogPath `
        -m $RAM `
        -drive "file=$VHD,format=vhdx" `
        2> $null
    Write-Host "    [*] Done." -ForegroundColor Green
}

Unmount-VHD $true
Write-Host "LineOS Builder v2.4.0" -ForegroundColor Yellow
Write-Host "[*]make:" -ForegroundColor Cyan
make
if ($LastExitCode -ne 0)
{
    Write-Host "    [-] make failed." -ForegroundColor Red
    exit $LastExitCode
}
Write-Host "---End of make---" -ForegroundColor Cyan
Write-Host "[*]vhd:" -ForegroundColor Cyan
Mount-VHD
if (-not (Copy-VHD))
{
    Unmount-VHD $true
    exit 1
}
Unmount-VHD
Write-Host "---End of vhd---" -ForegroundColor Cyan
Write-Host "[*]run:" -ForegroundColor Cyan
Run-QEMU
if (Test-Path $LogPath)
{
    $FaultCheck = Select-String `
        -Path $LogPath `
        -Pattern "cpu_reset", "Triple fault", "triple fault", "Triple Fault" `
        -ErrorAction SilentlyContinue
    if ($FaultCheck)
    {
        Write-Host "    [!!] triple fault." -ForegroundColor Red
        exit 1
    }
}
Write-Host "---End of run---" -ForegroundColor Cyan
Write-Host "[*]Exit." -ForegroundColor Magenta
Mount-VHD $true
