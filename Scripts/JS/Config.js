// Scripts/JS/Config.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const FS = require("node:fs");
const Path = require("node:path");
const TOML = require("@iarna/toml");

const BaseDir = Path.resolve(__dirname, "../..");
const ConfigFile = Path.join(BaseDir, "Configs", "QEMUSettings.toml");

function LoadConfig()
{
    const Raw = FS.readFileSync(ConfigFile, "utf8");
    const Settings = TOML.parse(Raw);

    const ResolvePath = (Value) => Path.resolve(BaseDir, Value);

    return {
        Version : "3.0.0",
        Platform : process.platform,
        BaseDir,

        Build : {Makefile : ResolvePath("Makefiles/Makefile")},

        Image : {
            File : ResolvePath(Settings.Disk.Image),
            Size : Settings.Disk.Size,
            EFISizeMiB : Settings.Disk.EFISizeMiB,
            MountDir : "/tmp/lineos_img_mount",

            BootFile : ResolvePath("LineOS/EFI/BOOT/BOOTX64.EFI"),
            KernelFile : ResolvePath("LineOS/KERNEL/LINEOS_KERNEL.ELF")

        },

        QEMU : {
            Binary : "qemu-system-x86_64",

            Machine : Settings.Machine.Type,
            Accelerator : Settings.Machine.Acceleration,

            CPU : Settings.CPU.Model,
            CPUCount : Settings.CPU.Count,
            CPUFlags : Settings.CPU.Flags,

            RAM : Settings.Memory.Size,

            Graphics : {Display : process.env.LINEOS_DISPLAY || Settings.Graphics.Display, Resolution : Settings.Graphics.Resolution, Device : Settings.Graphics.Device, HostMemory : Settings.Graphics.HostMemory},

            Network : Settings.Network,
            Input : Settings.Input,
            RTC : Settings.RTC,

            Firmware : {Code : ResolvePath("UEFI/OVMF_CODE.fd"), Vars : ResolvePath("UEFI/OVMF_VARS.fd")},

            Debug : {Flags : Settings.Debug.Flags, LogFile : ResolvePath("Logs/QEMU.log"), DebugConFile : ResolvePath("Logs/DebugCon.log"), NoReboot : Settings.Debug.NoReboot, NoShutdown : Settings.Debug.NoShutdown}
        }
    };
}

module.exports = LoadConfig;
