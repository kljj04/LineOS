// Scripts/JS/Build.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const {Run} = require("./Process");
const UI = require("./UI");

async function RunMake(Config, Target) {
    const Args = ["-f", Config.Build.Makefile, "--no-print-directory", "--output-sync=target", Target];
    let Total = 0;
    let Current = 0;
    if (Target === "All") {
        const Plan = await Run("make", ["-n", ...Args], {Cwd: Config.BaseDir, Capture: true});
        Total = (Plan.match(/^node Scripts\/JS\/Print\.js '\[(?:CC|AS|LD)\]/gm) || []).length;
        if (Total === 0) UI.StartSpinner("Up to date");
    }
    const Timer = setTimeout(() => UI.StartSpinner(`Make · ${Target}`), 150);
    try {
        await Run("make", Args, {
            Cwd: Config.BaseDir, OnLine: Message => {
                clearTimeout(Timer);
                if (/^\s*\[(CC|AS|LD)\]/.test(Message)) UI.BuildLog(Message, Total, ++Current);
                else if (!/^\s*\[\*\]/.test(Message)) UI.BuildLog(Message);
            }
        });
    } finally {
        clearTimeout(Timer);
        UI.StopSpinner();
    }
}

module.exports = {
    Build: Config => RunMake(Config, "All"),
    Clean: Config => RunMake(Config, "Clean"),
    Format: Config => RunMake(Config, "Format")
};
