#!/usr/bin/env bash
# Configs/QEMUSettings.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

MachineType="q35"
MachineAcceleration="kvm"

CPUModel="host"
CPUCount=16
CPUFlags=(
    "+tsc-deadline"
    "+invtsc"
    "+rdtscp"
    "+arat"
    "+fsgsbase"
    "+pdpe1gb"
)

MemorySize="4G"

GraphicsDisplay="gtk"
GraphicsResolution="2560x1440"
GraphicsDevice="virtio-gpu-pci"
GraphicsHostMemory="1G"

NetworkBackend="user"
NetworkMAC="52:54:00:4c:4f:53"

InputKeyboard="virtio-keyboard-pci"
InputMouse="virtio-tablet-pci"

RTCBase="utc"
RTCClock="host"

DiskImage="LineOS/LineOS.img"
DiskSize="8G"
DiskEFISizeMiB=300

DebugFlags=(
    "int"
    "guest_errors"
    "cpu_reset"
    "unimp"
)
DebugNoReboot=true
DebugNoShutdown=true
