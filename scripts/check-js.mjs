#!/usr/bin/env node
import { readdirSync, statSync } from "node:fs";
import { join, relative } from "node:path";
import { spawnSync } from "node:child_process";

const root = process.cwd();
const candidates = ["scripts", "tests"];
const files = [];

function walk(dir) {
    for (const entry of readdirSync(dir)) {
        const path = join(dir, entry);
        const stat = statSync(path);
        if (stat.isDirectory()) {
            walk(path);
        } else if (/\.(mjs|js)$/.test(entry)) {
            files.push(path);
        }
    }
}

for (const dir of candidates) walk(join(root, dir));

for (const file of files) {
    const result = spawnSync(process.execPath, ["--check", file], {
        cwd: root,
        encoding: "utf8",
        stdio: ["ignore", "pipe", "pipe"],
    });
    if (result.status !== 0) {
        process.stderr.write(result.stderr);
        process.exit(result.status ?? 1);
    }
    console.log(`syntax ok: ${relative(root, file)}`);
}
