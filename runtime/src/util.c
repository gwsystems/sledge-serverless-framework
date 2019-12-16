#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <sandbox.h>
#include <module.h>
#include <util.h>
#include <jsmn.h>

#define UTIL_MOD_LINE_MAX 1024

static char *
util_remove_spaces(char *str)
{
	int i = 0;
	while (isspace(*str)) str++;
	i = strlen(str);
	while (isspace(str[i-1])) str[i-1]='\0';

	return str;
}


int
util_parse_modules_file_json(char *filename)
{
	struct stat sb;
	memset(&sb, 0, sizeof(struct stat));
	if (stat(filename, &sb) < 0) {
		perror("stat");
		return -1;
	}

	FILE *mf = fopen(filename, "r");
	if (!mf) {
		perror("fopen");
		return -1;
	}

	char *filebuf = malloc(sb.st_size);
	memset(filebuf, 0, sb.st_size);
	int ret = fread(filebuf, sizeof(char), sb.st_size, mf);
	debuglog("size read: %d content: %s\n", ret, filebuf);
	if (ret != sb.st_size) {
		perror("fread");
		return -1;
	}
	fclose(mf);

	jsmn_parser modp;
	jsmn_init(&modp);
	jsmntok_t toks[MOD_MAX * JSON_ELE_MAX];

	int r = jsmn_parse(&modp, filebuf, strlen(filebuf), toks, sizeof(toks) / sizeof(toks[0]));
	if (r < 0) {
		debuglog("jsmn_parse: invalid JSON?\n");
		return -1;
	}

	int nmods = 0;
	for (int i = 0; i < r; i++) {
		assert(toks[i].type == JSMN_OBJECT);

		char mname[MOD_NAME_MAX] = { 0 };
		char mpath[MOD_PATH_MAX] = { 0 };
		i32 nargs = 0;
		u32 port = 0;
		i32 isactive = 0;
		for (int j = 1; j < (toks[i].size  * 2); j+=2) {
			char val[256] = { 0 }, key[32] = { 0 };

			sprintf(val, "%.*s", toks[j + i + 1].end - toks[j + i + 1].start, filebuf + toks[j + i + 1].start);
			sprintf(key, "%.*s", toks[j + i].end - toks[j + i].start, filebuf + toks[j + i].start);
			if (strcmp(key, "name") == 0) {
				strcpy(mname, val);
			} else if (strcmp(key, "path") == 0) {
				strcpy(mpath, val);
			} else if (strcmp(key, "port") == 0) {
				port = atoi(val);
			} else if (strcmp(key, "argsize") == 0) {
				nargs = atoi(val);
			} else if (strcmp(key, "active") == 0) {
				isactive = (strcmp(val, "yes") == 0);
			} else {
				debuglog("Invalid (%s,%s)\n", key, val);
			}
		}
		i += (toks[i].size * 2);
		// do not load if it is not active
		if (isactive == 0) continue;

		struct module *m = module_alloc(mname, mpath, nargs, 0, 0, 0, port, 0, 0);
		assert(m);
		nmods++;
	}

	free(filebuf);
	assert(nmods);
	debuglog("Loaded %d module%s!\n", nmods, nmods > 1 ? "s" : "");

	return 0;
}

/*
 * TEST data file should contain:
 * module_name:<arg1,arg2,arg3...>
 * and if your arg has to contain a ',', woops i can't deal with that for now!
 * if the first character in a line is ";", then the line is ignored!
 */
int
parse_sandbox_file_custom(char *filename)
{
	FILE *mf = fopen(filename, "r");
	char buff[UTIL_MOD_LINE_MAX] = { 0 };
	int total_boxes = 0;

	if (!mf) {
		perror("fopen");

		return -1;
	}

	while (fgets(buff, UTIL_MOD_LINE_MAX, mf) != NULL) {
		char mname[MOD_NAME_MAX] = { 0 };
		char *tok = NULL, *src = buff;
		struct module *mod = NULL;
		struct sandbox *sb = NULL;
		char *args = NULL;

		src = util_remove_spaces(src);
		if (src[0] == ';') goto next;

		if ((tok = strtok_r(src, ":", &src))) {
			int ntoks = 0;
			strncpy(mname, tok, MOD_NAME_MAX);

			mod = module_find_by_name(mname);
			assert(mod);
			if (mod->nargs > 0) {
				args = (char *)malloc(mod->nargs * MOD_ARG_MAX_SZ);
				assert(args);

				while ((tok = strtok_r(src, ",", &src))) {
					strncpy(args + (ntoks * MOD_ARG_MAX_SZ), tok, MOD_ARG_MAX_SZ);
					ntoks++;

					assert(ntoks < MOD_MAX_ARGS);
				}
			}
		} else {
			assert(0);
		}

		sb = sandbox_alloc(mod, args, 0, NULL);
		assert(sb);
		total_boxes++;

next:
		memset(buff, 0, UTIL_MOD_LINE_MAX);
	}

	assert(total_boxes);
	debuglog("Instantiated %d sandbox%s!\n", total_boxes, total_boxes > 1 ? "es" : "");

	return 0;
}


struct sandbox *
util_parse_sandbox_string_json(struct module *mod, char *str, const struct sockaddr *addr)
{
	jsmn_parser sp;
	jsmntok_t tk[JSON_ELE_MAX];
	jsmn_init(&sp);

	int r = jsmn_parse(&sp, str, strlen(str), tk, sizeof(tk)/sizeof(tk[0]));
	if (r < 1) {
		debuglog("Failed to parse string:%s\n", str);
		return NULL;
	}

	if (tk[0].type != JSMN_OBJECT) return NULL;

	for (int j = 1; j < r; j++) {
		char key[32] = { 0 };
		sprintf(key, "%.*s", tk[j].end - tk[j].start, str + tk[j].start);
		if (strcmp(key, "module") == 0) {
			char name[32] = { 0 };
			sprintf(name, "%.*s", tk[j + 1].end - tk[j + 1].start, str + tk[j + 1].start);
			if (strcmp(name, mod->name) != 0) return NULL;
			j++;
		} else if (strcmp(key, "args") == 0) {
			if (tk[j + 1].type != JSMN_ARRAY) return NULL;

			char *args = malloc(tk[j + 1].size * MOD_ARG_MAX_SZ);
			assert(args);

			for (int k = 1; k <= tk[j + 1].size; k++) {
				jsmntok_t *g = &tk[j + k + 1];
				strncpy(args + ((k - 1) * MOD_ARG_MAX_SZ), str + g->start, g->end - g->start);
				*(args + ((k - 1) * MOD_ARG_MAX_SZ) + g->end - g->start) = '\0';
			}

			struct sandbox *sb = sandbox_alloc(mod, args, 0, addr);
			assert(sb);

			return sb;
		} else {
			return NULL;
		}
	}

	return NULL;
}

struct sandbox *
util_parse_sandbox_string_custom(struct module *mod, char *str, const struct sockaddr *addr)
{
	char *tok = NULL, *src = str;

	src = util_remove_spaces(src);
	if (src[0] == ';') return NULL;

	if (!(tok = strtok_r(src, ":", &src))) return NULL;

	if (strcmp(mod->name, tok)) return NULL;
	assert(mod->nargs >= 0 && mod->nargs < MOD_MAX_ARGS);

	char *args = (char *)malloc(mod->nargs * MOD_ARG_MAX_SZ);
	assert(args);
	int ntoks = 0;
	while ((tok = strtok_r(src, ",", &src))) {
		strncpy(args + (ntoks * MOD_ARG_MAX_SZ), tok, MOD_ARG_MAX_SZ);
		ntoks++;
		assert(ntoks < MOD_MAX_ARGS);
	}

	struct sandbox *sb = sandbox_alloc(mod, args, 0, addr);
	assert(sb);

	return sb; 
}

/*
 * Each line in the file should be like:
 *
 * module_path:module_name:module_nargs:module_stack_size:module_max_heap_size[:moreargs::argn\n]
 * if the first character in a line is ";", then the line is ignored!
 */
int
util_parse_modules_file_custom(char *filename)
{
	FILE *mf = fopen(filename, "r");
	char buff[UTIL_MOD_LINE_MAX] = { 0 };
	int nmods = 0;

	if (!mf) {
		perror("fopen");

		return -1;
	}

	while (fgets(buff, UTIL_MOD_LINE_MAX, mf) != NULL) {
		char mname[MOD_NAME_MAX] = { 0 };
		char mpath[MOD_PATH_MAX] = { 0 };
		i32 nargs = 0;
		u32 stack_sz = 0;
		u32 max_heap = 0;
		u32 timeout = 0;
		char *tok = NULL, *src = buff;
		u32 port = 0;
		i32 ntoks = 0;

		src = util_remove_spaces(src);
		if (src[0] == ';') goto next;
		while ((tok = strtok_r(src, ":", &src))) {
			switch(ntoks) {
				case MOD_ARG_MODPATH: strncpy(mpath, tok, MOD_PATH_MAX); break;
				case MOD_ARG_MODPORT: port = atoi(tok);
				case MOD_ARG_MODNAME: strncpy(mname, tok, MOD_NAME_MAX); break;
				case MOD_ARG_MODNARGS: nargs = atoi(tok); break;
				default: break;
			}
			ntoks++;
		}
		assert(ntoks >= MOD_ARG_MAX);

		struct module *m = module_alloc(mname, mpath, nargs, 0, 0, 0, port, 0, 0);
		assert(m);
		nmods++;

next:
		memset(buff, 0, UTIL_MOD_LINE_MAX);
	}

	assert(nmods);
	debuglog("Loaded %d module%s!\n", nmods, nmods > 1 ? "s" : "");
	fclose(mf);

	return 0;
}

