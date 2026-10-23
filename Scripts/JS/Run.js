// Scripts/JS/Run.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const    UI = require("./UI");
function Help()
{
    UI.Banner();
    console.log(`
  Usage: ./Scripts/Run.sh [option]

  (none)       Build → copy artifacts → QEMU
  -build       Build only (incremental)
  -clean       Make Clean
  -format      Make Format (changes source formatting)
  -image       Copy BOOTX64.EFI and LINEOS_KERNEL.ELF
  -qemu        Run QEMU only
  -h, --help   Show help

  Options are case-insensitive, e.g. -Clean.
  Existing disk images are never formatted. Linux + sudo required for copy.
  Compact CLI output; the current file updates on one terminal line.
  NO_COLOR=1 disables colors. Completion returns to the shell.
`);
}
async function Main()
{
    const Args = process.argv.slice(2);
    const Option = Args.length ? Args[0].toLowerCase() : "-all";
    if (Args.length > 1 || !["-all", "-build", "-clean", "-format", "-image", "-qemu", "-h", "--help", "-help"].includes(Option))
        throw new Error("Invalid arguments. Use --help.");
    if ([ "-h", "--help", "-help" ].includes(Option))
        return Help();
    const Config = require("./Config")();
    const Build = require("./Build");
    UI.Start(Option === "-all" ? ["Build", "Image", "QEMU"] : [{"-build": "Build", "-clean": "Clean", "-format": "Format", "-image": "Image", "-qemu": "QEMU"}[Option]]);

        if ([ "-all", "-build" ].includes(Option))
            await UI.Stage("Build", () => Build.Build(Config));
        if (Option === "-clean")
            await UI.Stage("Clean", () => Build.Clean(Config));
        if (Option === "-format")
            await UI.Stage("Format", () => Build.Format(Config));
        if ([ "-all", "-image" ].includes(Option))
            await UI.Stage("Image", () => require("./Image").Copy(Config));
        if ([ "-all", "-qemu" ].includes(Option))
            await UI.Stage("QEMU", () => require("./QEMU").StartQEMU(Config));
}
Main().catch(Error =>
{
    UI.Error(Error.message);
    process.exitCode = 1;
}).then(() => UI.Finish(Boolean(process.exitCode))).catch(Error =>
{
    UI.ClearLive();
    console.error(Error.message);
    process.exitCode = 1;
});
