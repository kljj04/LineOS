// Scripts/JS/Process.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const {spawn} = require("node:child_process");
let      Cancelled = null;
let      Active = null;
function Cancel(Signal)
{
    Cancelled = Signal;
    if (Active && !Active.Protected)
        Active.Group ? process.kill(-Active.pid, Signal) : Active.kill(Signal);
}
process.on("SIGINT", () => Cancel("SIGINT"));
process.on("SIGTERM", () => Cancel("SIGTERM"));
function Run(Command, Args, Options = {})
{
    return new Promise((Resolve, Reject) => {
        if (Cancelled && !Options.Cleanup)
            return Reject(new Error(`Interrupted (${Cancelled})`));
        const CaptureErrors = Boolean(Options.Capture && process.stdout.isTTY);
        const Child = spawn(Command, Args, {cwd: Options.Cwd, detached: process.platform !== "win32" && !Options.Interactive && Command !== "sudo", stdio: Options.OnLine ? ["inherit", "pipe", "pipe"] : Options.Capture ? ["ignore", "pipe", CaptureErrors ? "pipe" : "inherit"] : "inherit"});
        Child.Group = process.platform !== "win32" && !Options.Interactive && Command !== "sudo";
        Child.Protected = Boolean(Options.Protected || Options.Cleanup);
        Active = Child;
        let Output = "";
        if (Options.OnLine)
        {
            const Readline = require("node:readline");
            for (const Stream of [Child.stdout, Child.stderr]) Readline.createInterface({input: Stream, crlfDelay: Infinity}).on("line", Options.OnLine);
        }
        if (CaptureErrors) require("node:readline").createInterface({input: Child.stderr, crlfDelay: Infinity}).on("line", Message => require("./UI").Info(Message));
        if (Options.Capture)
            Child.stdout.on("data", Data => { Output += Data; });
        Child.on("error", Reject);
        Child.on("close", (Code, Signal) => {
            if (Active === Child)
                Active = null;
            if (Code !== 0 || (Cancelled && !Options.Cleanup && !Options.Protected))
                Reject(new Error(`${Command} failed (${Signal || Code})`));
            else
                Resolve(Output.trim());
        });
    });
}
module.exports = {Run};
