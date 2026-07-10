import { existsSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "../..");
const nativeBinary =
    process.platform === "win32"
        ? resolve(root, "build/vortexdtl.exe")
        : resolve(root, "build/vortexdtl");

export function hasNativeBinary() {
    return existsSync(nativeBinary);
}

export function runVortexCli(args) {
    const command = hasNativeBinary() ? nativeBinary : process.execPath;
    const finalArgs = hasNativeBinary()
        ? args
        : [resolve(root, "scripts/vortex-js.mjs"), ...args];
    const result = spawnSync(command, finalArgs, {
        cwd: root,
        encoding: "utf8",
        stdio: ["ignore", "pipe", "pipe"],
    });
    if (result.status !== 0) {
        const message = result.stderr || result.stdout || "vortex command failed";
        throw new Error(message.trim());
    }
    return result.stdout.trim();
}

export function loadJsonCommand(args) {
    const output = runVortexCli(args);
    return JSON.parse(output);
}
