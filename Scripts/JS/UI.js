// Scripts/JS/UI.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const Kit = require("@kljj04/cli-kit");
const Readline = require("node:readline");
const {performance} = require("node:perf_hooks");
const Color = Boolean(process.stdout.isTTY && !process.env.NO_COLOR && process.env.TERM !== "dumb");
const Terminal = Boolean(process.stdout.isTTY && process.env.TERM !== "dumb");
let Spinner = null;
let LiveActive = false;
let Completed = 0;
let Total = 1;
let Started = 0;
let Open = false;
function Width()
{
    return Math.max(12, Math.min(96, (process.stdout.columns || 88) - 1));
}
function Fit(Message, Limit = Width() - 4)
{
    let Text = "";
    let Used = 0;
    for (const Character of String(Message).replace(/[\x00-\x1f\x7f]/g, " "))
    {
        const Code = Character.codePointAt(0);
        const Size = Code >= 0x1100 && (Code <= 0x115f || Code >= 0x2e80 && Code <= 0xa4cf || Code >= 0xac00 && Code <= 0xd7a3 || Code >= 0xf900 && Code <= 0xff60 || Code >= 0x1f300) ? 2 : 1;
        if (Used + Size > Limit)
        {
            const Characters = Array.from(Text);
            if (Used === Limit) Characters.pop();
            return Characters.join("") + "…";
        }
        Text += Character;
        Used += Size;
    }
    return Text + " ".repeat(Math.max(0, Limit - Used));
}
function StopSpinner()
{
    if (Spinner) Spinner.stop();
    Spinner = null;
}
function ClearLive()
{
    StopSpinner();
    if (LiveActive)
    {
        Readline.cursorTo(process.stdout, 0);
        Readline.clearLine(process.stdout, 0);
        LiveActive = false;
    }
}
function Print(Message, RGB = [80, 215, 240])
{
    ClearLive();
    if (Color) Kit.color(...RGB).context(Message);
    else console.log(Message);
}
function Row(Message, RGB = [175, 185, 205])
{
    Print(Message, RGB);
}
function DrawLive(Message, RGB)
{
    const Text = Fit(Message, Width()).trimEnd();
    Readline.cursorTo(process.stdout, 0);
    Readline.clearLine(process.stdout, 0);
    process.stdout.write(Color ? `\x1b[38;2;${RGB.join(";")}m${Text}\x1b[0m` : Text);
    LiveActive = true;
}
function Live(Message, RGB = [80, 215, 240])
{
    if (!Terminal) return Print(Message, RGB);
    ClearLive();
    DrawLive(Message, RGB);
}
function Gradient(Message)
{
    const Characters = Array.from(Message);
    const Result = Characters.map((Character, Index) =>
    {
        const T = Index / Math.max(1, Characters.length - 1);
        const Lightness = 0.82 - 0.15 * T;
        const Chroma = 0.14 + 0.08 * T;
        const Hue = (210 + 90 * T) * Math.PI / 180;
        const A = Chroma * Math.cos(Hue);
        const B = Chroma * Math.sin(Hue);
        const L = (Lightness + 0.3963377774 * A + 0.2158037573 * B) ** 3;
        const M = (Lightness - 0.1055613458 * A - 0.0638541728 * B) ** 3;
        const S = (Lightness - 0.0894841775 * A - 1.2914855480 * B) ** 3;
        const Channels = [4.0767416621 * L - 3.3077115913 * M + 0.2309699292 * S, -1.2684380046 * L + 2.6097574011 * M - 0.3413193965 * S, -0.0041960863 * L - 0.7034186147 * M + 1.7076147010 * S];
        const RGB = Channels.map(Value => Math.round(255 * Math.max(0, Math.min(1, Value <= 0.0031308 ? 12.92 * Value : 1.055 * Math.max(0, Value) ** (1 / 2.4) - 0.055))));
        return `\x1b[38;2;${RGB.join(";")}m${Character}`;
    });
    return Result.join("") + "\x1b[0m";
}
function Banner()
{
    const Text = "LineOS Builder 3.0.0";
    if (Color) console.log(Gradient(Text));
    else console.log(Text);
}
function SetTotal(Count)
{
    Total = Count;
    Completed = 0;
}
function Start(Names)
{
    SetTotal(Names.length);
    Started = performance.now();
    Open = true;
    Banner();

}
async function Stage(Name, Action)
{
    Print("─".repeat(Width()), [110, 125, 160]);
    const Labels = {Build: "Building LineOS", Format: "Formatting sources", Clean: "Cleaning build files", Image: "Updating disk image", QEMU: "Running QEMU"};
    const Label = Labels[Name] || Name;
    Print(`${Label}...`, [80, 215, 240]);
    const StartTime = performance.now();
    try
    {
        const Result = await Action();
        Completed++;
        Row(`${Label}... Done (${((performance.now() - StartTime) / 1000).toFixed(2)}s)`, [100, 220, 170]);
        return Result;
    }
    finally { ClearLive(); }
}
function Info(Message)
{
    Row(Message);
}
function Success(Message)
{
    Row(Message, [100, 220, 170]);
}
function Error(Message)
{
    Row(`E: ${Message}`, [255, 100, 110]);
}
function StartSpinner(Message)
{
    ClearLive();
    if (!Terminal) return Live(Message);
    const Frames = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"];
    let Index = 0;
    const Draw = () => DrawLive(`${Frames[Index++ % Frames.length]}  ${Message}`, [80, 215, 240]);
    Draw();
    const Timer = setInterval(Draw, 80);
    Spinner = {stop: () => clearInterval(Timer)};
}
function FileProgress(Count, Current, File, RGB)
{
    const Percent = Math.floor(100 * Math.min(Current, Count) / Math.max(1, Count));
    if (!Color || Width() < 44) return Live(`[${String(Percent).padStart(3)}%] [${Count}/${Current}] ${File}`, RGB);
    ClearLive();
    Kit.gradient([0, 220, 255], [180, 100, 255]).style("progress", "override", "fixedgradient").context(Percent);
    const BarWidth = 34 + String(Percent).length;
    const Remaining = Width() - BarWidth;
    if (Percent === 100)
    {
        Readline.moveCursor(process.stdout, 0, -1);
        Readline.cursorTo(process.stdout, BarWidth);
    }
    if (Remaining > 2) process.stdout.write(Fit(` [${Count}/${Current}] ${File}`, Remaining).trimEnd());
    if (Percent === 100) process.stdout.write("\n");
    else LiveActive = true;
}
function BuildLog(Message, FileTotal = null, FileCurrent = null)
{
    const File = Message.trim().match(/^\[(FORMAT|CLEAN|COPY) (\d+)\/(\d+)\] (.*)$/);
    if (File) return FileProgress(Number(File[2]), Number(File[3]), File[4], File[1] === "FORMAT" ? [185, 125, 245] : [235, 180, 100]);
    const Match = Message.trim().match(/^\[(CC|AS|LD|\*)\]\s*(.*)$/);
    if (!Match)
    {
        const RGB = /error:|fatal:|Error/.test(Message) ? [255, 100, 110] : /warning:/.test(Message) ? [245, 195, 100] : [175, 185, 205];
        Print(Message, RGB);
        return;
    }
    const Colors = {CC: [70, 215, 240], AS: [185, 125, 245], LD: [235, 180, 100], "*": [100, 220, 170]};
    if (FileTotal === null) Row(`${Match[1].padEnd(4)} ${Match[2]}`, Colors[Match[1]]);
    else FileProgress(FileTotal, FileCurrent, Match[2], Colors[Match[1]]);
}
function Suspend()
{
    ClearLive();
}
function Resume() {}
function Finish(Failed)
{
    ClearLive();
    if (!Open) return;
    Row(`${Completed}/${Total} completed, ${Failed ? "stopped with errors" : "finished"} in ${((performance.now() - Started) / 1000).toFixed(2)}s.`, Failed ? [255, 100, 110] : [100, 220, 170]);
    Open = false;
}
process.on("exit", ClearLive);
module.exports = {Banner, Stage, Info, Success, Error, BuildLog, StartSpinner, StopSpinner, SetTotal, ClearLive, Start, Finish, Suspend, Resume};
