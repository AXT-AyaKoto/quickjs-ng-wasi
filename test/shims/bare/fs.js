const fs = {
    marker: "shims/bare/fs.js",
    readFileSync() {
        return "";
    },
};

export default fs;
