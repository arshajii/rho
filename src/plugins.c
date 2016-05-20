#include "main.h"
#if RHO_IS_POSIX

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <assert.h>
#include "object.h"
#include "module.h"
#include "vm.h"
#include "util.h"
#include "plugins.h"

#define PATH_MAX 4096

static const char *plugin_path = NULL;

void rho_set_plugin_path(const char *path)
{
	plugin_path = path;
}

static bool is_plugin(struct dirent *ent)
{
	const char *name = ent->d_name;
	const size_t name_len = strlen(name);
	return name_len > 3 && strcmp(name + name_len - 3, ".so") == 0;
}

int rho_reload_plugins(void)
{
	if (plugin_path == NULL) {
		return 1;
	}

	DIR *dir = opendir(plugin_path);

	if (!dir) {
		return 1;
	}

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (is_plugin(ent)) {
			char full_name[PATH_MAX + 1];
			snprintf(full_name, PATH_MAX + 1, "%s/%s", plugin_path, ent->d_name);
			void *handle = dlopen(full_name, RTLD_NOW);

			if (!handle) {
				fprintf(stderr, RHO_WARNING_HEADER "%s(): could not open %s: %s\n", __func__, ent->d_name, dlerror());
				continue;
			}

#define STR_MAX_LEN 50
			char init_func_name[STR_MAX_LEN];
			size_t init_func_name_len =
			        snprintf(init_func_name, STR_MAX_LEN, "init_%s", ent->d_name);
			assert(init_func_name_len > 0);

			char *dot = strchr(init_func_name, '.');

			assert(dot);
			*dot = '\0';

			if (init_func_name_len > STR_MAX_LEN) {
				init_func_name_len = STR_MAX_LEN;
			}
#undef STR_MAX_LEN

			RhoPluginInitFunc init_func = (RhoPluginInitFunc)(intptr_t)dlsym(handle, init_func_name);

			if (!init_func) {
				fprintf(stderr, RHO_WARNING_HEADER "%s(): could not init %s: %s\n", __func__, ent->d_name, dlerror());
				dlclose(handle);
				continue;
			}

			RhoPlugin plugin;
			init_func(&plugin);
			RhoModule *module = rho_objvalue(&plugin.module);
			rho_vm_register_module(module);
		}
	}

	closedir(dir);
	return 0;
}

#endif /* RHO_IS_POSIX */
