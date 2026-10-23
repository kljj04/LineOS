// Scripts/JS/QEMU.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const FS = require("node:fs");
const Path = require("node:path");
const UI = require("./UI");

function FindFirmware(Primary, Candidates)
{
    for (const File of [Primary, ...Candidates])
    {
        if (FS.existsSync(File))
        {
            return File;
        }
    }

    throw new Error(`OVMF firmware not found: ${Primary}`);
}

function PrepareFirmware(Config)
{
    const Firmware = Config.QEMU.Firmware;

    const Code = FindFirmware(Firmware.Code, [ "/usr/share/OVMF/OVMF_CODE_4M.fd", "/usr/share/OVMF/OVMF_CODE.fd" ]);

    const Vars = FindFirmware(Firmware.Vars, [ "/usr/share/OVMF/OVMF_VARS_4M.fd", "/usr/share/OVMF/OVMF_VARS.fd" ]);

    return {Code, Vars};
}

function BuildArguments(Config, Firmware)
{
    const QEMU = Config.QEMU;
    const Graphics = QEMU.Graphics;
    const Resolution = Graphics.Resolution.split("x");

    if (Resolution.length !== 2 || Resolution.some(Value => !/^\d+$/.test(Value)))
    {
        throw new Error("Invalid graphics resolution.");
    }

    const [Width, Height] = Resolution;

    const Args = [
        "-M",        QEMU.Machine,
        "-accel",    QEMU.Accelerator,
        "-cpu",      [ QEMU.CPU, ...QEMU.CPUFlags ].join(","),
        "-smp",      String(QEMU.CPUCount),
        "-m",        QEMU.RAM,
        "-rtc",      [ `base=${QEMU.RTC.Base}`, `clock=${QEMU.RTC.Clock}` ].join(","),

        "-drive",    `if=pflash,format=raw,readonly=on,file=${Firmware.Code}`,
        "-drive",    `if=pflash,format=raw,file=${Firmware.Vars}`,

        "-drive",    `file=${Config.Image.File},format=raw,id=lineos_disk,if=none`,
        "-device",   "virtio-blk-pci,drive=lineos_disk,bootindex=0",

        "-netdev",   `${QEMU.Network.Backend},id=net0`,
        "-device",   `virtio-net-pci,netdev=net0,mac=${QEMU.Network.MAC}`,

        "-device",   QEMU.Input.Keyboard,
        "-device",   QEMU.Input.Mouse,

        "-vga",      "none",
        "-device",   `${Graphics.Device},xres=${Width},yres=${Height},hostmem=${Graphics.HostMemory}`,
        "-display",  Graphics.Display,

        "-fw_cfg",   `name=opt/org.tianocore/GraphicsResolution,string=${Width}x${Height}`,

        "-d",        QEMU.Debug.Flags.join(","),
        "-D",        QEMU.Debug.LogFile,

        "-debugcon", `file:${QEMU.Debug.DebugConFile}`,
        "-global",   "isa-debugcon.iobase=0xe9"
    ];

    if (QEMU.Debug.NoReboot)
    {
        Args.push("-no-reboot");
    }

    if (QEMU.Debug.NoShutdown)
    {
        Args.push("-no-shutdown");
    }

    return Args;
}

async function StartQEMU(Config)
{
    const {Run} = require("./Process");
    const OS = require("node:os");
    const Directory = FS.mkdtempSync(Path.join(OS.tmpdir(), "lineos-qemu-"));
    try
    {
        const Firmware = PrepareFirmware(Config);
        const Vars = Path.join(Directory, "OVMF_VARS.fd");
        FS.copyFileSync(Firmware.Vars, Vars);
        FS.mkdirSync(Path.dirname(Config.QEMU.Debug.LogFile), {recursive : true});
        UI.StartSpinner("Waiting for QEMU to exit");
        await Run(Config.QEMU.Binary, BuildArguments(Config, {Code : Firmware.Code, Vars}), {Cwd: Config.BaseDir, OnLine: Message =>
        {
            UI.Info(Message);
            UI.StartSpinner("Waiting for QEMU to exit");
        }});
    }
    finally
    {
        UI.ClearLive();
        FS.rmSync(Directory, {recursive : true, force : true});
    }
}
module.exports = {
    PrepareFirmware,
    BuildArguments,
    StartQEMU
};
