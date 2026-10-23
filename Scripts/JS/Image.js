// Scripts/JS/Image.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const FS = require("node:fs");
const Path = require("node:path");
const {Run} = require("./Process");
const UI = require("./UI");
const {WithMount} = require("./Mount");
async function Copy(Config)
{
    if (!FS.statSync(Config.Image.File).isFile())
        throw new Error("Disk image must be an existing regular file; automatic formatting is disabled.");
    const Files = [ [ Config.Image.BootFile, "EFI/BOOT/BOOTX64.EFI" ], [ Config.Image.KernelFile, "KERNEL/LINEOS_KERNEL.ELF" ] ];
    for (const [Source] of Files)
        if (!FS.statSync(Source).isFile())
            throw new Error(`Missing artifact: ${Source}`);
    await WithMount(Config, async Directory => {
        for (const [Index, [Source, Relative]] of Files.entries())
        {
            UI.BuildLog(`[COPY ${Files.length}/${Index + 1}] ${Relative}`);
            const Destination = Path.join(Directory, Relative);
            await Run("sudo", [ "-n", "mkdir", "-p", Path.dirname(Destination) ]);
            await Run("sudo", [ "-n", "cp", "--", Source, Destination ]);
            await Run("sudo", [ "-n", "cmp", "--", Source, Destination ]);
        }
        await Run("sudo", [ "-n", "sync", "-f", Directory ]);
    });
}
module.exports = {Copy};
