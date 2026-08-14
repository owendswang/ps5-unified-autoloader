/*
 * PS5 homescreen shortcut installer.
 *
 * Kept in sync with ps5-webkit-autoloader/src/app_installer.c: compare the
 * installed assets, overwrite them only when needed, and register the title
 * without uninstalling it first.
 */

#include "shortcut_installer.h"
#include "notification.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <ps5/kernel.h>

#define INCASSET(name, file)                                                   \
    __asm__(".section .rodata\n"                                               \
            ".global " #name "\n"                                             \
            ".global " #name "_end\n"                                         \
            ".global " #name "_size\n"                                        \
            ".align 16\n" #name ":\n"                                         \
            ".incbin \"" file "\"\n" #name "_end:\n"                        \
            #name "_size:\n"                                                   \
            ".quad " #name "_end - " #name "\n"                              \
            ".previous\n");                                                    \
    extern const uint8_t name[];                                               \
    extern const size_t name##_size

INCASSET(shortcut_param_json, "assets/param.json");
INCASSET(shortcut_icon0_png, "assets/icon0.png");

int sceAppInstUtilInitialize(void);
int sceAppInstUtilTerminate(void);
int sceAppInstUtilAppInstallAll(void *);

_Static_assert(sizeof(SHORTCUT_TITLE_ID) <= 16,
               "SHORTCUT_TITLE_ID too long for path buffers");

static int install_file(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file)
        return -1;

    if (fwrite(data, size, 1, file) != 1) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int install_app(const char *title_id, const char *dir) {
    int (*install_title_dir)(const char *, const char *, void *) = NULL;
    const char *nid = "Wudg3Xe3heE";
    uint32_t handle;

    if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &handle) == 0)
        install_title_dir = (void *)kernel_dynlib_resolve(-1, handle, nid);

    if (install_title_dir)
        return install_title_dir(title_id, dir, NULL);

    return sceAppInstUtilAppInstallAll(NULL);
}

static int needs_update(const char *path, const uint8_t *expected_data,
                        size_t expected_size) {
    struct stat st;
    if (stat(path, &st) != 0)
        return 1;
    if ((size_t)st.st_size != expected_size)
        return 1;

    FILE *file = fopen(path, "rb");
    if (!file)
        return 1;

    uint8_t *buffer = malloc(expected_size);
    if (!buffer) {
        fclose(file);
        return 1;
    }

    if (fread(buffer, 1, expected_size, file) != expected_size) {
        free(buffer);
        fclose(file);
        return 1;
    }
    fclose(file);

    int mismatch = memcmp(buffer, expected_data, expected_size);
    free(buffer);
    return mismatch != 0;
}

int shortcut_install_if_needed(void) {
    const char *title_id = SHORTCUT_TITLE_ID;
    char app_dir[256];
    char sce_sys_dir[256];
    char param_path[256];
    char icon_path[256];

    snprintf(app_dir, sizeof(app_dir), "/user/app/%s", title_id);
    snprintf(sce_sys_dir, sizeof(sce_sys_dir), "/user/app/%s/sce_sys",
             title_id);
    snprintf(param_path, sizeof(param_path), "%s/param.json", sce_sys_dir);
    snprintf(icon_path, sizeof(icon_path), "%s/icon0.png", sce_sys_dir);

    int update_needed = 0;
    struct stat st;
    if (stat(app_dir, &st) != 0) {
        update_needed = 1;
    } else {
        if (needs_update(param_path, shortcut_param_json,
                         shortcut_param_json_size))
            update_needed = 1;
        if (needs_update(icon_path, shortcut_icon0_png,
                         shortcut_icon0_png_size))
            update_needed = 1;
    }

    if (!update_needed) {
        printf("[shortcut] %s is already up to date\n", title_id);
        return 0;
    }

    if (stat(app_dir, &st) == 0) {
        printf("[shortcut] Updating existing app launcher %s\n", title_id);
        autoloader_notify("Updating WebKit Autoloader App...");
    } else {
        printf("[shortcut] Installing app launcher %s\n", title_id);
        autoloader_notify("Installing WebKit Autoloader App...");
    }

    int error = sceAppInstUtilInitialize();
    if (error != 0) {
        printf("[shortcut] sceAppInstUtilInitialize: 0x%08X\n", error);
        return -1;
    }

    if (mkdir(app_dir, 0755) != 0 && errno != EEXIST) {
        printf("[shortcut] Failed to create %s (errno=%d)\n", app_dir, errno);
        sceAppInstUtilTerminate();
        return -1;
    }
    if (mkdir(sce_sys_dir, 0755) != 0 && errno != EEXIST) {
        printf("[shortcut] Failed to create %s (errno=%d)\n", sce_sys_dir,
               errno);
        sceAppInstUtilTerminate();
        return -1;
    }

    if (install_file(param_path, shortcut_param_json,
                     shortcut_param_json_size) != 0) {
        printf("[shortcut] Failed to install param.json\n");
        sceAppInstUtilTerminate();
        return -1;
    }
    if (install_file(icon_path, shortcut_icon0_png,
                     shortcut_icon0_png_size) != 0) {
        printf("[shortcut] Failed to install icon0.png\n");
        sceAppInstUtilTerminate();
        return -1;
    }

    error = install_app(title_id, "/user/app/");
    if (error != 0) {
        printf("[shortcut] install_app: 0x%08X\n", error);
        sceAppInstUtilTerminate();
        return -1;
    }

    printf("[shortcut] App launcher installed successfully\n");
    autoloader_notify("WebKit Autoloader App Ready!");
    sceAppInstUtilTerminate();
    return 0;
}
