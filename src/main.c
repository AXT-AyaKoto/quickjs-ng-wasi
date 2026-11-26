#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "quickjs.h"
#include "quickjs-libc.h"

/* Wasm linear-memory stack is 128 MiB, set with
   -Wl,-z,stack-size=134217728. Stay under that so deep JS recursion raises
   RangeError before the engine hits a wasm memory trap. */
#define QJS_MAX_STACK_SIZE ((size_t)125829120) /* 120 MiB */

static void usage(void)
{
    fputs("usage: qjs.wasm [--shim-dir <dir>] <entry.js>\n", stderr);
}

static void trim_trailing_slashes(char *path)
{
    size_t n = strlen(path);

    while (n > 1 && path[n - 1] == '/')
        path[--n] = '\0';
}

static char *shim_node_module(JSContext *ctx, const char *shim_dir,
                              const char *name)
{
    size_t dir_len = strlen(shim_dir);
    size_t name_len = strlen(name);
    char *out;

    out = js_malloc(ctx, dir_len + 6 + name_len + 3 + 1);
    if (!out)
        return NULL;
    memcpy(out, shim_dir, dir_len);
    memcpy(out + dir_len, "/node/", 6);
    memcpy(out + dir_len + 6, name, name_len);
    memcpy(out + dir_len + 6 + name_len, ".js", 4);
    return out;
}

/* Same '.' / '..' resolution as quickjs.c js_default_module_normalize_name.
   That helper is static, so the command embeds the same rules for relative
   and absolute specifiers. */
static char *default_module_normalize(JSContext *ctx, const char *base_name,
                                      const char *name)
{
    char *filename, *p;
    const char *r;
    int cap, len, n;

    if (name[0] != '.')
        return js_strdup(ctx, name);
    if (!base_name)
        base_name = "";

    r = strrchr(base_name, '/');
    if (r)
        len = (int)(r - base_name);
    else
        len = 0;

    cap = len + (int)strlen(name) + 1 + 1;
    filename = js_malloc(ctx, (size_t)cap);
    if (!filename)
        return NULL;
    memcpy(filename, base_name, (size_t)len);
    filename[len] = '\0';

    r = name;
    for (;;) {
        if (r[0] == '.' && r[1] == '/') {
            r += 2;
        } else if (r[0] == '.' && r[1] == '.' && r[2] == '/') {
            if (filename[0] == '\0')
                break;
            p = strrchr(filename, '/');
            if (!p)
                p = filename;
            else
                p++;
            if (!strcmp(p, ".") || !strcmp(p, ".."))
                break;
            if (p > filename)
                p--;
            *p = '\0';
            r += 3;
        } else {
            break;
        }
    }
    if (filename[0] != '\0') {
        n = (int)strlen(filename);
        if (n + 1 < cap) {
            filename[n] = '/';
            filename[n + 1] = '\0';
        }
    }
    n = (int)strlen(filename);
    if (n + (int)strlen(r) + 1 <= cap)
        memcpy(filename + n, r, strlen(r) + 1);
    return filename;
}

static char *module_normalize(JSContext *ctx, const char *base_name,
                              const char *name, void *opaque)
{
    const char *shim_dir = opaque;

    if (!strcmp(name, "qjs:std") || !strcmp(name, "qjs:os"))
        return js_strdup(ctx, name);
    if (!strncmp(name, "node:", 5))
        return shim_node_module(ctx, shim_dir, name + 5);
    if (name[0] != '.' && name[0] != '/')
        return shim_node_module(ctx, shim_dir, name);
    return default_module_normalize(ctx, base_name, name);
}

static int eval_module(JSContext *ctx, const char *filename, bool is_main)
{
    uint8_t *buf;
    size_t len;
    JSValue val;

    buf = js_load_file(ctx, &len, filename);
    if (!buf) {
        perror(filename);
        return -1;
    }

    val = JS_Eval(ctx, (const char *)buf, len, filename,
                  JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    js_free(ctx, buf);
    if (!JS_IsException(val)) {
        if (js_module_set_import_meta(ctx, val, false, is_main) < 0) {
            js_std_dump_error(ctx);
            JS_FreeValue(ctx, val);
            return -1;
        }
        val = JS_EvalFunction(ctx, val);
    }
    val = js_std_await(ctx, val);
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        JS_FreeValue(ctx, val);
        return -1;
    }
    JS_FreeValue(ctx, val);
    return 0;
}

static int parse_args(int argc, char **argv, char **shim_dir, const char **entry)
{
    const char *dir = "/shims";
    const char *file = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shim-dir")) {
            if (i + 1 >= argc) {
                usage();
                return -1;
            }
            dir = argv[++i];
            continue;
        }
        if (argv[i][0] == '-') {
            usage();
            return -1;
        }
        if (file) {
            usage();
            return -1;
        }
        file = argv[i];
    }
    if (!file) {
        usage();
        return -1;
    }

    *shim_dir = strdup(dir);
    if (!*shim_dir) {
        perror("strdup");
        return -1;
    }
    trim_trailing_slashes(*shim_dir);
    *entry = file;
    return 0;
}

int main(int argc, char **argv)
{
    char *shim_dir = NULL;
    const char *entry = NULL;
    char preload[4096];
    JSRuntime *rt = NULL;
    JSContext *ctx = NULL;
    int n;
    int status = 1;

    if (parse_args(argc, argv, &shim_dir, &entry))
        return 1;

    n = snprintf(preload, sizeof(preload), "%s/preload.js", shim_dir);
    if (n < 0 || (size_t)n >= sizeof(preload)) {
        fprintf(stderr, "qjs: shim directory path is too long\n");
        goto done;
    }

    rt = JS_NewRuntime();
    if (!rt) {
        fprintf(stderr, "qjs: cannot allocate JS runtime\n");
        goto done;
    }
    JS_SetMaxStackSize(rt, QJS_MAX_STACK_SIZE);
    js_std_init_handlers(rt);
    ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "qjs: cannot allocate JS context\n");
        goto done;
    }

    if (!js_init_module_std(ctx, "qjs:std") || !js_init_module_os(ctx, "qjs:os")) {
        fprintf(stderr, "qjs: cannot register built-in modules\n");
        goto done;
    }

    /* js_module_loader's signature includes import attributes, so the
       attributes-aware setter is the one that can call it directly.
       The normalizer argument is the same hook as JS_SetModuleLoaderFunc. */
    JS_SetModuleLoaderFunc2(rt, module_normalize, js_module_loader,
                            js_module_check_attributes, shim_dir);
    JS_SetHostPromiseRejectionTracker(rt, js_std_promise_rejection_tracker, NULL);

    if (access(preload, R_OK) == 0) {
        if (eval_module(ctx, preload, false))
            goto done;
    }
    if (eval_module(ctx, entry, true))
        goto done;
    if (js_std_loop(ctx)) {
        js_std_dump_error(ctx);
        goto done;
    }
    status = 0;

done:
    if (rt)
        js_std_free_handlers(rt);
    if (ctx)
        JS_FreeContext(ctx);
    if (rt)
        JS_FreeRuntime(rt);
    free(shim_dir);
    return status;
}
