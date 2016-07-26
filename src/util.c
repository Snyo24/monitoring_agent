#include "util.h"

#include <time.h>
#include <yaml.h>
#include <zlog.h>

timestamp get_timestamp() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * NANO + now.tv_nsec;
}

void yaml_parser(const char *file, char *result) {
	yaml_parser_t parser;
	yaml_token_t token;
	yaml_parser_initialize(&parser);

	FILE *yaml = fopen(file, "rb");
	yaml_parser_set_input_file(&parser, yaml);

	int status = 0;
	char *target;
	char *scalar;

	do {
		yaml_parser_scan(&parser, &token);
		switch(token.type) {
		/* Stream start/end */
		case YAML_STREAM_START_TOKEN: printf("STREAM START\n"); break;
		case YAML_STREAM_END_TOKEN:   printf("STREAM END\n");   break;
		/* Token types (read before actual token) */
		case YAML_KEY_TOKEN:   printf("(Key token)   "); status = 0; break;
		case YAML_VALUE_TOKEN: printf("(Value token) "); status = 1; break;
		/* Block delimeters */
		case YAML_BLOCK_SEQUENCE_START_TOKEN: printf("Start Block (Sequence)\n"); break;
		case YAML_BLOCK_ENTRY_TOKEN:          printf("Start Block (Entry)\n");    break;
		case YAML_BLOCK_END_TOKEN:            printf("End block\n");              break;
		/* Data */
		case YAML_BLOCK_MAPPING_START_TOKEN:  printf("[Block mapping]\n");            break;
		case YAML_SCALAR_TOKEN:
		scalar = (char *)token.data.scalar.value;
		printf("scalar %s \n", scalar);
		if(status) {
			strcpy(target, (char *)scalar);
		} else {
			if(!strcmp("db_host", scalar))
				target = (result);
			else if(!strcmp(scalar, "db_user"))
				target = (result+50);
			else if(!strcmp(scalar, "db_pass"))
				target = (result+100);
			else if(!strcmp(scalar, "db_name"))
				target = (result+150);
		}
		break;
		/* Others */
		default:
		printf("Got token of type %d\n", token.type);
		}
		if(token.type != YAML_STREAM_END_TOKEN)
			yaml_token_delete(&token);
	} while(token.type != YAML_STREAM_END_TOKEN);

	/* Destroy the Parser object. */
	yaml_parser_delete(&parser);
}