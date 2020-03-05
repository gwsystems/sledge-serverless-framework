#ifndef SFRT_UTIL_H
#define SFRT_UTIL_H

#include <sandbox.h>
#include <module.h>

/* perhaps move it to module.h or sandbox.h? */
int util_parse_modules_file_json(char *filename);

#endif /* SFRT_UTIL_H */
