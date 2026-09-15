#ifndef CMD_H
#define CMD_H

#include <stddef.h>

#define CMD_INPUT_BUFFER_LENGTH (12U * 1024U)
#define CMD_RESPONSE_BUFFER_LENGTH (12U * 1024U)

const char *CmdProcess(char *line);

#endif
