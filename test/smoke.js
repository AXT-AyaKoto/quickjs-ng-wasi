import fs from "node:fs";
import fs2 from "fs";

if (globalThis.__preload_seen !== "preload")
    throw new Error("preload.js did not run");

if (!fs || fs.marker !== "shims/node/fs.js" || fs !== fs2)
    throw new Error("node:fs and fs did not both resolve to /shims/node/fs.js");

console.log("smoke ok");

try {
    process.exit(0);
} catch (e) {
    console.error("process.exit was caught");
    throw e;
}

throw new Error("process.exit returned");
