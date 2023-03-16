#include <errno.h>
#include "memlogging.h"
#include "panic.h"

__thread struct mem_logging_obj log_obj;

/**
 * Initialize memory log. This should be called in each worker thread.
 * Every worker thread writes log to its own memory.
 * @param log_size the number of bytes of memory to allocate for logging
 * @param file the FILE pointer to an opened file, each worker thread will 
 *	  dump the memory log to this file when sledge exits. The file is 
 *	  defined by an environment variable
 */
void 
mem_log_init2(uint32_t log_size, FILE* file)
{
#ifndef LOG_RUNTIME_MEM_LOG
        return;
#endif
        log_obj.log_size        = log_size;
        log_obj.logger          = (char *)malloc(log_size);
        if (!log_obj.logger) {
                panic("failed to allocate memory for logging\n");
        }

        log_obj.offset          = 0;
        log_obj.fout            = file;
}

/**
 * Initialize memory log. This should be called in each worker thread. 
 * Every worker thread writes log to its own memory.
 * @param log_size the number of bytes of memory to allocate for logging
 * @param file the file to dump the memory log when sledge exits
 */
void 
mem_log_init(uint32_t log_size, char const* file)
{
#ifndef LOG_RUNTIME_MEM_LOG
	return;
#endif
	log_obj.log_size 	= log_size;
	log_obj.logger 		= (char *)malloc(log_size);
	if (!log_obj.logger) {
		panic("failed to allocate memory for logging\n");
	}

	log_obj.offset		= 0;
	log_obj.fout		= fopen(file, "w");
}

/**
 * Prints log to memory
 */
void 
mem_log(char const * fmt, ...)
{
#ifndef LOG_RUNTIME_MEM_LOG	
	return;
#endif
	assert(log_obj.logger);
	if (!log_obj.fout) {
		return;
	}

	va_list va;
	va_start(va, fmt);
	int n = vsnprintf(log_obj.logger + log_obj.offset, log_obj.log_size - log_obj.offset, fmt, va);
	va_end(va);

	if (n < 0) {
		/* Based on the doc of vsnprintf, the write is failed if n is negative */
		panic("failed to write data to memory, return value:%d\n", n);
	} else if (n >= log_obj.log_size - log_obj.offset) {
		/* Memory is full, realloc memory */
		char* old_logger = log_obj.logger;

		while (n >=log_obj.log_size - log_obj.offset) {
			log_obj.logger = (char *)realloc(log_obj.logger, log_obj.log_size * 2);
			if (!log_obj.logger) {
				log_obj.logger = old_logger;
				dump_log_to_file(log_obj);
				panic("failed to realloc memory for logging\n");
			}

			log_obj.log_size = log_obj.log_size * 2;
			va_start(va, fmt);
			n = vsnprintf(log_obj.logger + log_obj.offset, log_obj.log_size - log_obj.offset, fmt, va);
			va_end(va);
		}
		log_obj.offset += n;
	} else {
		/* Write Success */
		log_obj.offset += n;
	}
}

/**
 * Dump log from memory to file. This should be called when a worker thread receives SIGINT signal
 */

void 
dump_log_to_file()
{
#ifndef LOG_RUNTIME_MEM_LOG
	return;
#endif
	if (!log_obj.fout) {
                return;
        }

        assert(log_obj.logger);

	uint32_t write_bytes = 0;
	while(write_bytes != log_obj.offset) {
		int return_bytes = fprintf(log_obj.fout, "%s", log_obj.logger + write_bytes);
		write_bytes += return_bytes;
	}
	fflush(log_obj.fout);
}

