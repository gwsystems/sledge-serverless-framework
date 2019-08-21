#ifndef SFRT_UTIL_H
#define SFRT_UTIL_H

#include <sandbox.h>
#include <module.h>

/* perhaps move it to module.h or sandbox.h? */
struct sandbox *util_parse_sandbox_string_custom(struct module *m, char *str);
struct sandbox *util_parse_sandbox_string_json(struct module *m, char *str);
int util_parse_modules_file_json(char *filename);
int util_parse_modules_file_custom(char *filename);

#endif /* SFRT_UTIL_H */
