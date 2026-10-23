// Scripts/JS/Test.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const Assert = require("node:assert/strict");
const FS = require("node:fs");
const {spawnSync} = require("node:child_process");
const Process = require("./Process");
const Calls = [];
let Failure = "";
let Existing = "";
Process.Run = async (Command, Args) =>
{
    Calls.push(Args);
    if (Args.includes("--show")) return "/dev/loop999";
    if (Args.includes("-j")) return Existing;
    if (Args.includes(Failure)) throw new Error("Injected failure");
    return "";
};
async function Main()
{
    const {Authenticate, WithMount} = require("./Mount");
    Calls.length = 0;
    await Authenticate(true);
    Assert.deepEqual(Calls[0], ["-v"]);
    for (const Scenario of ["", "mount", "copy", "umount", "--detach"])
    {
        Calls.length = 0;
        Failure = Scenario;
        let Directory;
        const Action = WithMount({Image: {File: "/fake/image"}}, async MountDir =>
        {
            Directory = MountDir;
            if (Scenario === "copy") throw new Error("Copy failed");
        });
        if (Scenario) await Assert.rejects(Action);
        else await Action;
        const MountCall = Calls.find(Args => Args.includes("mount"));
        Directory = MountCall.at(-1);
        if (Scenario === "umount") Assert.ok(!Calls.some(Args => Args.includes("--detach")));
        else Assert.ok(Calls.some(Args => Args.includes("--detach")));
        if (["umount", "--detach"].includes(Scenario)) FS.rmdirSync(Directory);
        else Assert.equal(FS.existsSync(Directory), false);
    }
    Existing = "/dev/loop123: existing session";
    Calls.length = 0;
    await Assert.rejects(WithMount({Image: {File: "/fake/image"}}, async () => {}), /already attached/);
    Assert.ok(!Calls.some(Args => Args.includes("--show") || Args.includes("--detach")));
    const Result = spawnSync(process.execPath, ["-e", `const {Run}=require(${JSON.stringify(require.resolve("./Process"))});setTimeout(()=>process.kill(process.pid,'SIGTERM'),100);Run('sleep',['10']).then(()=>process.exitCode=2).catch(Error=>console.log(Error.message));`], {encoding: "utf8", timeout: 3000});
    Assert.equal(Result.status, 0);
    Assert.match(Result.stdout, /failed/);
    const OS = require("node:os");
    const Path = require("node:path");
    const Directory = FS.mkdtempSync(Path.join(OS.tmpdir(), "lineos-sudo-test-"));
    try
    {
        const Executable = Path.join(Directory, "sudo");
        const State = Path.join(Directory, "session");
        const Helper = `#!${process.execPath}
const FS = require('node:fs');
const Session = require('node:child_process').execFileSync('ps', ['-o', 'sid=', '-p', String(process.pid)], {encoding:'utf8'}).trim();
if (process.argv[2] === '-v') FS.writeFileSync(process.env.LINEOS_TEST_SESSION, Session);
else if (FS.readFileSync(process.env.LINEOS_TEST_SESSION, 'utf8') !== Session) process.exit(1);
if (process.argv.includes('--show')) console.log('/dev/loop999');
`;
        FS.writeFileSync(Executable, Helper, {mode: 0o700});
        const Script = `const {Authenticate,WithMount}=require(${JSON.stringify(require.resolve("./Mount"))});(async()=>{await Authenticate(true);await WithMount({Image:{File:'/fake/image'}},async()=>{});})().catch(Error=>{console.error(Error);process.exitCode=1;});`;
        const Authentication = spawnSync(process.execPath, ["-e", Script], {encoding: "utf8", timeout: 3000, env: {...process.env, PATH: `${Directory}:${process.env.PATH}`, LINEOS_TEST_SESSION: State}});
        Assert.equal(Authentication.status, 0, Authentication.stderr);
    }
    finally { FS.rmSync(Directory, {recursive: true, force: true}); }
    console.log("PASS: mount success, acquisition/copy/cleanup failures, existing-device refusal, SIGTERM forwarding, sudo session continuity");
}
Main().catch(Error => { console.error(Error); process.exitCode = 1; });
