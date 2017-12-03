#define _POSIX_C_SOURCE 200809L
#define DEFAULT_BUFFER_LENGTH 128
#define DEFAULT_NAME_LENGTH 32
#define POLL_TIMEOUT 200
#define MAX_WRITE_SIZE 1000000000
#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)

#include <sys/socket.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <poll.h>
#include <stdatomic.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <assert.h>

struct string
{
	char* data;
	uint32_t length;	//may inlude NULL
	uint32_t capacity;
};

struct dynamic_array
{
	pthread_mutex_t mutex;
	void** data;
	size_t length;
	size_t capacity;
};

struct return_info
{
	bool error_occured;
	int error_code;
	int return_code;
};

void realloc_write(struct string* target, char c, uint32_t position);
void string_append(struct string* target, char* source);
void string_copy(struct string* target, struct string* source);
void reset_string(struct string* stringbuffer, uint32_t buffer_size);
void printBits(size_t size, void* ptr);
void adjust_string_size(struct string* target ,uint32_t size);
void swap_endianess_16(uint16_t * byte);
void convert_string(struct string* source);
void dynamic_array_push(struct dynamic_array* array, void* item);
void dynamic_array_adjust(struct dynamic_array* array, size_t size);
void dynamic_array_remove(struct dynamic_array* array, size_t position);
void destroy_dynamic_array(struct dynamic_array* array);
void* dynamic_array_at(const struct dynamic_array* array, size_t position);
struct string new_string(uint32_t initial_size);
struct dynamic_array* new_dynamic_array(void);
struct return_info send_string(const struct string* message, int socket_fd);
bool valid_message_format(const struct string* message, bool is_extended_format);
struct return_info realloc_read(struct string* target, unsigned short bytes_to_read, int socket_fd, uint32_t offset);
struct return_info get_message(struct string* message, int socket_fd);

