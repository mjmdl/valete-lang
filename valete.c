#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help(const char *program_name) {
	printf("$ %s -h	   // print help (this dialog)\n", program_name);
}

const char *filepath_extension(const char *path) {
	const char *dot = NULL;
	for (const char *p = path; *p != '\0'; ++p) {
		if (*p == '.') {
			dot = p;
		}
	}
	return dot == NULL ? NULL : dot + 1;
}

bool source_filepath(const char *path) {
	const char *ext = filepath_extension(path);
	if (ext == NULL) {
		return false;
	}
	return strcmp(ext, "vlt") == 0;
}

char *read_file(const char *path, size_t *out_opt_length) {
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		printf("ERROR: Could not open %s.\n", path);
		return NULL;
	}

	long length;
	if (fseek(file, 0, SEEK_END) < 0
		|| (length = ftell(file)) < 0
		|| fseek(file, 0, SEEK_SET) < 0) {
		printf("ERROR: Failed to retrieve file info of %s.\n", path);
		fclose(file);
		return NULL;
	}

	char *buffer = malloc(length * sizeof(char));
	assert(buffer != NULL);

	if (fread(buffer, 1, length, file) != length) {
		printf("ERROR: failed to read %zu bytes from file %s.\n", length, path);
		free(buffer);
		fclose(file);
		return NULL;
	}

	fclose(file);
	if (out_opt_length) {
		*out_opt_length = length;
	}
	return buffer;
}

typedef struct Arena {
	uint8_t *begin;
	uint8_t *end;
	uint8_t *cursor;
	struct Arena *next;
} Arena;

Arena *arena_create(size_t length) {
	size_t size = sizeof (Arena) + length;
	Arena *arena = malloc(size);
	assert(arena != NULL);

	arena->begin = (uint8_t *)(arena + 1);
	arena->end = arena->begin + length;
	arena->cursor = arena->begin;
	arena->next = NULL;
	return arena;
}

void arena_destroy(Arena *arena) {
	if (arena->next) {
		free(arena->next);
	}
	free(arena);
}

void *arena_push(Arena *arena, size_t size) {
	size_t spare = arena->end - arena->cursor;
	if (spare >= size) {
		char *result = arena->cursor;
		arena->cursor += size;
		return result;
	}

	if (arena->next == NULL) {
		size_t this_size = arena->end - arena->begin;
		arena->next = arena_create(size > this_size ? size : this_size);
	}

	return arena_push(arena->next, size);
}

typedef struct Strings {
	size_t length;
	size_t count;
	const char **elements;
} Strings;

Strings strings_create(size_t length) {
	assert(length >= 1);
	Strings strings = {
		.length = length,
		.count = 0,
		.elements = malloc(length * sizeof (char *)),
	};
	assert(strings.elements != NULL);
	return strings;
}

void strings_destroy(Strings *strings) {
	free(strings->elements);
	*strings = (Strings){
		.length = 0,
		.count = 0,
		.elements = NULL,
	};
}

size_t strings_add(Strings *strings, const char *element) {
	if (strings->count >= strings->length) {
		strings->length *= 2;
		size_t new_size = strings->length * sizeof (char *);
		strings->elements = realloc(strings->elements, new_size);
		assert(strings->elements != NULL);
	}
	strings->elements[strings->count] = element;
	return ++strings->count;
}

size_t strings_remove(Strings *strings, size_t index) {
	assert(index < strings->count);
	for (size_t i = index; i < (strings->count - 1); ++i) {
		strings->elements[i] = strings->elements[i + 1];
	}
	return --strings->count;
}

size_t strings_find(const Strings *strings, const char *element) {
	for (size_t i = 0; i < strings->count; i++) {
		if (strings->elements[i] == element) {
			return i;
		}
	}
	return SIZE_MAX;
}

typedef struct Commands {
	bool success;
	const char *output_filepath;
	Strings input_filepaths;
} Commands;

Commands commands_parse(int argc, char **argv) {
	Commands commands = {
		.success = false,
		.output_filepath = "out",
		.input_filepaths = strings_create(2),
	};

	if (argc == 1) {
		printf("USAGE: %s -h\n", argv[0]);
		return commands;
	}

	for (size_t i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") == 0) {
			print_help(argv[0]);
			continue;
		}

		if (strcmp(argv[i], "-o") == 0) {
			if (++i == argc) {
				printf("ERROR: Missing output filepath.\n");
				printf("USAGE: %s -o path-to-output\n", argv[0]);
				return commands;
			}

			commands.output_filepath = argv[i];
			continue;
		}

		if (source_filepath(argv[i])) {
			strings_add(&commands.input_filepaths, argv[i]);
			continue;
		}

		printf("ERROR: Unknown argument: '%s'\n", argv[i]);
		printf("Tip: Type '%s -h' for help.\n", argv[0]);
		return commands;
	}
	
	commands.success = true;
	return commands;
}

int main(int argc, char **argv) {
	Commands commands = commands_parse(argc, argv);
	if (!commands.success) {
		strings_destroy(&commands.input_filepaths);
		return EXIT_FAILURE;
	}

	printf("commands.output_filepath = %s\n", commands.output_filepath);
	for (size_t i = 0; i < commands.input_filepaths.count; ++i) {
		printf("commands.input_filepaths.elements[%zu] = %s\n",
			i, commands.input_filepaths.elements[i]);

		size_t length = 0;
		char *data = read_file(commands.input_filepaths.elements[i], &length);
		if (data != NULL) {
			printf("%.*s\n", (int)length, data);
		}
	}
	
	return EXIT_SUCCESS;
}
