#!/usr/bin/env node
import { mkdirSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const root = process.cwd();
const required = process.argv.includes("--required");
const buildDir = join(root, "build");
const sourceDir = join(root, "src");
const includeDir = join(root, "include");
const cFiles = readdirSync(sourceDir)
    .filter((name) => name.endsWith(".c"))
    .map((name) => join(sourceDir, name));

function canRun(command, args) {
    const result = spawnSync(command, args, {
        cwd: root,
        encoding: "utf8",
        stdio: ["ignore", "pipe", "pipe"],
    });
    return result.status === 0 || result.status === 1;
}

function findCompiler() {
    if (process.env.CC) return { kind: "posix", command: process.env.CC };
    const candidates =
        process.platform === "win32"
            ? [
                  { kind: "msvc", command: "cl", probe: [] },
                  { kind: "posix", command: "clang", probe: ["--version"] },
                  { kind: "posix", command: "gcc", probe: ["--version"] },
                  { kind: "posix", command: "cc", probe: ["--version"] },
              ]
            : [
                  { kind: "posix", command: "cc", probe: ["--version"] },
                  { kind: "posix", command: "clang", probe: ["--version"] },
                  { kind: "posix", command: "gcc", probe: ["--version"] },
              ];
    return candidates.find((candidate) => canRun(candidate.command, candidate.probe));
}

const compiler = findCompiler();
if (!compiler) {
    const message = "no C compiler found; skipping native VortexDTL build";
    if (required) {
        console.error(message);
        process.exit(1);
    }
    console.log(message);
    process.exit(0);
}

mkdirSync(buildDir, { recursive: true });

let command;
let args;
if (compiler.kind === "msvc") {
    command = compiler.command;
    args = [
        "/nologo",
        "/std:c11",
        "/W4",
        `/I${includeDir}`,
        ...cFiles,
        `/Fe:${join(buildDir, "vortexdtl.exe")}`,
    ];
} else {
    command = compiler.command;
    args = [
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-pedantic",
        `-I${includeDir}`,
        ...cFiles,
        "-o",
        resolve(buildDir, process.platform === "win32" ? "vortexdtl.exe" : "vortexdtl"),
    ];
}

console.log(`${command} ${args.join(" ")}`);
const result = spawnSync(command, args, {
    cwd: root,
    encoding: "utf8",
    stdio: "inherit",
});
process.exit(result.status ?? 1);
