#pragma once

/* Must match titleId in assets/param.json. */
#define SHORTCUT_TITLE_ID "WKAL00001"

/* Installs or updates the WebKit Autoloader homescreen app when needed. */
int shortcut_install_if_needed(void);
