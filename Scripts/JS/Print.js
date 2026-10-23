// Scripts/JS/Print.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const UI = require("./UI");
const Message = process.argv.slice(2).join(" ");
if (process.stdout.isTTY) UI.BuildLog(Message);
else console.log(Message);
