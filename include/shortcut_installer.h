#pragma once

/* Must match titleId in assets/param.json. */
#define SHORTCUT_TITLE_ID "WKAL00001"

/* Installs or updates the WebKit Autoloader homescreen app when needed. */
/* Returns 1 if installed/updated, 0 if skipped, or -1 on failure. */
int shortcut_install_if_needed(void);
