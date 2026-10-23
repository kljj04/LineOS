// Scripts/JS/Mount.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const FS = require("node:fs");
const OS = require("node:os");
const Path = require("node:path");
const {Run} = require("./Process");
const UI = require("./UI");
async function Authenticate(Interactive = Boolean(process.stdin.isTTY && process.stderr.isTTY))
{
    UI.ClearLive();
    UI.Suspend();
    try
    {
        await Run("sudo", Interactive ? ["-v"] : ["-n", "true"], {Interactive});
    }
    catch (Error)
    {
        throw new Error(Interactive ? `sudo authentication failed: ${Error.message}` : "Image copy requires sudo authentication. Run ./Scripts/Run.sh in a terminal to enter your password.");
    }
    finally { UI.Resume(); }
}
async function WithMount(Config, Action)
{
    if (process.platform !== "linux")
        throw new Error("Image mounting requires Linux.");
    await Authenticate();
    const Existing = await Run("sudo", [ "-n", "losetup", "-j", Config.Image.File ], {Capture : true});
    if (Existing)
        throw new Error("Image is already attached; refusing to reuse another session device.");
    const Directory = FS.mkdtempSync(Path.join(OS.tmpdir(), "lineos-builder-"));
    let   Loop = null;
    let   Mounted = false;
    let   Failure = null;
    try
    {
        Loop = await Run("sudo", [ "-n", "losetup", "--find", "--show", "--partscan", Config.Image.File ], {Capture : true, Protected : true});
        if (!/^\/dev\/loop[0-9]+$/.test(Loop))
            throw new Error("Invalid loop device returned.");
        await Run("sudo", [ "-n", "mount", "-o", "nosuid,nodev,noexec", `${Loop}p1`, Directory ], {Protected : true});
        Mounted = true;
        await Action(Directory);
    }
    catch (Error)
    {
        Failure = Error;
    }
    finally
    {
        try
        {
            if (Mounted)
                await Run("sudo", [ "-n", "umount", Directory ], {Cleanup : true});
            Mounted = false;
            if (Loop)
                await Run("sudo", [ "-n", "losetup", "--detach", Loop ], {Cleanup : true});
            FS.rmdirSync(Directory);
        }
        catch (Error)
        {
            Failure = new Error(`${Failure ? Failure.message + "; " : ""}Cleanup failed: ${Error.message}; inspect ${Directory} / ${Loop}`);
        }
    }
    if (Failure)
        throw Failure;
}
module.exports = {Authenticate, WithMount};
