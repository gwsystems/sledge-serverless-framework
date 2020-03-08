#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <sandbox.h>
#include <module.h>
#include <util.h>
#include <jsmn.h>
#include <http.h>

#define UTIL_MOD_LINE_MAX 1024

/**
 * Removes leading and trailing spaces from a string
 * @param str source string
 * @return string without leading or training spaces
 **/
static char *
util_remove_spaces(char *str)
{
	int i = 0;
	while (isspace(*str)) str++;
	i = strlen(str);
	while (isspace(str[i - 1])) str[i - 1] = '\0';

	return str;
}


/**
 * Parses a JSON file and allocates one or more new modules
 * @param file_name The path of the JSON file
 * @return RC 0 on Success. -1 on Error
 */
int
util_parse_modules_file_json(char *file_name)
{
	// Use stat to get file attributes and make sure file is there and OK
	struct stat stat_buffer;
	memset(&stat_buffer, 0, sizeof(struct stat));
	if (stat(file_name, &stat_buffer) < 0) {
		perror("stat");
		return -1;
	}

	// Open the file
	FILE *module_file = fopen(file_name, "r");
	if (!module_file) {
		perror("fopen");
		return -1;
	}

	// Initialize a Buffer, Read the file into the buffer, and then check that the buffer size equals the file size
	char *file_buffer = malloc(stat_buffer.st_size);
	memset(file_buffer, 0, stat_buffer.st_size);
	int total_chars_read = fread(file_buffer, sizeof(char), stat_buffer.st_size, module_file);
	debuglog("size read: %d content: %s\n", total_chars_read, file_buffer);
	if (total_chars_read != stat_buffer.st_size) {
		perror("fread");
		return -1;
	}

	// Close the file
	fclose(module_file);

	// Initialize the Jasmine Parser and an array to hold the tokens
	jsmn_parser module_parser;
	jsmn_init(&module_parser);
	jsmntok_t tokens[MOD_MAX * JSON_ELE_MAX];

	// Use Jasmine to parse the JSON
	int total_tokens = jsmn_parse(&module_parser, file_buffer, strlen(file_buffer), tokens,
	                              sizeof(tokens) / sizeof(tokens[0]));
	if (total_tokens < 0) {
		debuglog("jsmn_parse: invalid JSON?\n");
		return -1;
	}

	int module_count = 0;
	for (int i = 0; i < total_tokens; i++) {
		assert(tokens[i].type == JSMN_OBJECT);

		char  module_name[MOD_NAME_MAX] = { 0 };
		char  module_path[MOD_PATH_MAX] = { 0 };
		char *request_headers           = (char *)malloc(HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
		memset(request_headers, 0, HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
		char *reponse_headers = (char *)malloc(HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
		memset(reponse_headers, 0, HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
		i32  request_size                                = 0;
		i32  response_size                               = 0;
		i32  argument_count                              = 0;
		u32  port                                        = 0;
		i32  is_active                                   = 0;
		i32  request_count                               = 0;
		i32  response_count                              = 0;
		int  j                                           = 1;
		int  ntoks                                       = 2 * tokens[i].size;
		char request_content_type[HTTP_HEADERVAL_MAXSZ]  = { 0 };
		char response_content_type[HTTP_HEADERVAL_MAXSZ] = { 0 };

		for (; j < ntoks;) {
			int  ntks     = 1;
			char key[32]  = { 0 };
			char val[256] = { 0 };

			sprintf(val, "%.*s", tokens[j + i + 1].end - tokens[j + i + 1].start,
			        file_buffer + tokens[j + i + 1].start);
			sprintf(key, "%.*s", tokens[j + i].end - tokens[j + i].start,
			        file_buffer + tokens[j + i].start);
			if (strcmp(key, "name") == 0) {
				strcpy(module_name, val);
			} else if (strcmp(key, "path") == 0) {
				strcpy(module_path, val);
			} else if (strcmp(key, "port") == 0) {
				port = atoi(val);
			} else if (strcmp(key, "argsize") == 0) {
				argument_count = atoi(val);
			} else if (strcmp(key, "active") == 0) {
				is_active = (strcmp(val, "yes") == 0);
			} else if (strcmp(key, "http-req-headers") == 0) {
				assert(tokens[i + j + 1].type == JSMN_ARRAY);
				assert(tokens[i + j + 1].size <= HTTP_HEADERS_MAX);

				request_count = tokens[i + j + 1].size;
				ntks += request_count;
				ntoks += request_count;
				for (int k = 1; k <= tokens[i + j + 1].size; k++) {
					jsmntok_t *g = &tokens[i + j + k + 1];
					char *     r = request_headers + ((k - 1) * HTTP_HEADER_MAXSZ);
					assert(g->end - g->start < HTTP_HEADER_MAXSZ);
					strncpy(r, file_buffer + g->start, g->end - g->start);
				}
			} else if (strcmp(key, "http-resp-headers") == 0) {
				assert(tokens[i + j + 1].type == JSMN_ARRAY);
				assert(tokens[i + j + 1].size <= HTTP_HEADERS_MAX);

				response_count = tokens[i + j + 1].size;
				ntks += response_count;
				ntoks += response_count;
				for (int k = 1; k <= tokens[i + j + 1].size; k++) {
					jsmntok_t *g = &tokens[i + j + k + 1];
					char *     r = reponse_headers + ((k - 1) * HTTP_HEADER_MAXSZ);
					assert(g->end - g->start < HTTP_HEADER_MAXSZ);
					strncpy(r, file_buffer + g->start, g->end - g->start);
				}
			} else if (strcmp(key, "http-req-size") == 0) {
				request_size = atoi(val);
			} else if (strcmp(key, "http-resp-size") == 0) {
				response_size = atoi(val);
			} else if (strcmp(key, "http-req-content-type") == 0) {
				strcpy(request_content_type, val);
			} else if (strcmp(key, "http-resp-content-type") == 0) {
				strcpy(response_content_type, val);
			} else {
				debuglog("Invalid (%s,%s)\n", key, val);
			}
			j += ntks;
		}
		i += ntoks;
		// do not load if it is not active
		if (is_active == 0) continue;

		// Allocate a module based on the values from the JSON
		struct module *module = module__new(module_name, module_path, argument_count, 0, 0, 0, port,
		                                     request_size, response_size);
		assert(module);
		module__set_http_info(module, request_count, request_headers, request_content_type, response_count,
		                 reponse_headers, response_content_type);
		module_count++;
		free(request_headers);
		free(reponse_headers);
	}

	free(file_buffer);
	assert(module_count);
	debuglog("Loaded %d module%s!\n", module_count, module_count > 1 ? "s" : "");

	return 0;
}