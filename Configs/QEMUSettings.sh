#!/usr/bin/env bash
# Configs/QEMUSettings.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

QEMUBinary="qemu-system-x86_64"

DiskImage="LineOS/LineOS.img"
DiskSize="8G"
DiskEFISizeMiB=300

FirmwareCode="UEFI/OVMF_CODE.fd"
FirmwareVars="UEFI/OVMF_VARS.fd"

DebugLogDir="Logs"
DebugLogFile="QEMU.log"
DebugConsoleFile="DebugCon.log"
DebugProcessFile="QEMUProcess.log"

Debug="{DebugLogPath}"
DebugConsole="file:{DebugConsolePath}"
DebugGlobal="isa-debugcon.iobase=0xe9"
Serial="none"
Monitor="none"

Machine="q35,accel=kvm"
CPU="host,+tsc-deadline,+invtsc,+rdtscp,+arat,+fsgsbase,+pdpe1gb"
CPUCount="16"
Memory="4G"
RTC="base=utc,clock=host"

DiskDrive="if=none,id=drive0,file={DiskImage},format=raw"
DiskDevice="virtio-blk-pci,drive=drive0,bootindex=0"

GraphicsVGA="none"
GraphicsDevice="virtio-gpu-gl-pci,xres=2560,yres=1440,hostmem=1G"
GraphicsFirmware="name=opt/org.tianocore/GraphicsResolution,string=2560x1440"
GraphicsDisplay="gtk,gl=on"

InputKeyboard="virtio-keyboard-pci"
InputMouse="virtio-tablet-pci"

NetworkBackend="user,id=net0,hostfwd=tcp:192.168.55.192:52222-:52222"
NetworkDevice="virtio-net-pci,netdev=net0,mac=52:54:00:4c:4f:53"

FirmwareCodeDrive="if=pflash,format=raw,readonly=on,file={FirmwareCode}"
FirmwareVarsDrive="if=pflash,format=raw,file={FirmwareVars}"

DebugFlags="int,guest_errors,cpu_reset,unimp"
DebugNoReboot=true
DebugNoShutdown=true
