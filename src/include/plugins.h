#include "main.h"
#if RHO_IS_POSIX
#ifndef RHO_PLUGINS_H
#define RHO_PLUGINS_H

#include "object.h"

#define RHO_PLUGIN_PATH_ENV "RHO_PLUGINS_PATH"

typedef struct {
	RhoValue module;
} RhoPlugin;

typedef void (*RhoPluginInitFunc)(RhoPlugin *plugin);

void rho_set_plugin_path(const char *path);
int rho_reload_plugins(void);

#endif /* RHO_PLUGINS_H */
#endif /* RHO_IS_POSIX */
