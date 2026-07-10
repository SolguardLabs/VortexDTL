#!/usr/bin/env bash
set -euo pipefail

node scripts/check-js.mjs
node --test "tests/node/*.test.js"
