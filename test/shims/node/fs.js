const fs = {
    marker: "shims/node/fs.js",
    readFileSync() {
        return "";
    },
};

export default fs;
