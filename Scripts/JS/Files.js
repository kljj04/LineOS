// Scripts/JS/Files.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const FS = require("node:fs");
const Path = require("node:path");
const UI = require("./UI");
const {Run} = require("./Process");
const [Mode, ...Args] = process.argv.slice(2);
function Report(Message)
{
    if (process.stdout.isTTY) UI.BuildLog(Message);
    else console.log(Message);
}
function Collect(File, Entries)
{
    let Stat;
    try { Stat = FS.lstatSync(File); }
    catch (Error) { if (Error.code === "ENOENT") return; throw Error; }
    if (Stat.isDirectory()) for (const Name of FS.readdirSync(File)) Collect(Path.join(File, Name), Entries);
    Entries.push({File, Directory: Stat.isDirectory()});
}
async function Main()
{
    if (Mode === "Format")
    {
        const [Tool, ...Files] = Args;
        for (const [Index, File] of Files.entries())
        {
            Report(`[FORMAT ${Files.length}/${Index + 1}] ${File}`);
            await Run(Tool, ["-i", File], {Interactive: true});
        }
    }
    else if (Mode === "Clean")
    {
        const Entries = [];
        for (const File of Args) Collect(File, Entries);
        for (const [Index, Entry] of Entries.entries())
        {
            Report(`[CLEAN ${Entries.length}/${Index + 1}] ${Entry.File}`);
            try
            {
                if (Entry.Directory) FS.rmdirSync(Entry.File);
                else FS.unlinkSync(Entry.File);
            }
            catch (Error) { if (Error.code !== "ENOENT") throw Error; }
        }
    }
    else throw new Error("Expected Format or Clean.");
}
Main().then(() => { if (process.stdout.isTTY) UI.Success(`${Mode} complete`); }).catch(Error => { UI.ClearLive(); console.error(Error.message); process.exitCode = 1; });
