import * as std from "qjs:std";

function write(file, args) {
    file.puts(args.join(" ") + "\n");
    file.flush();
}

globalThis.console = {
    log(...args) {
        write(std.out, args);
    },
    error(...args) {
        write(std.err, args);
    },
};

globalThis.process = {
    exit(code) {
        std.exit(code === undefined ? 0 : code);
    },
};

globalThis.__preload_seen = "preload";
