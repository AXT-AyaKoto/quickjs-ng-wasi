import fs from "node:fs";
import fs2 from "fs";

if (globalThis.__preload_seen !== "preload")
    throw new Error("preload.js did not run");

if (!fs || fs.marker !== "shims/node/fs.js")
    throw new Error("node:fs did not resolve to /shims/node/fs.js");
if (!fs2 || fs2.marker !== "shims/bare/fs.js")
    throw new Error("fs did not resolve to /shims/bare/fs.js");

console.log("smoke ok");

try {
    process.exit(0);
} catch (e) {
    console.error("process.exit was caught");
    throw e;
}

throw new Error("process.exit returned");
