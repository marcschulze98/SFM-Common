/// @file
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

/**
 * @brief This struct contains a normal C-string (char*), its length (in bytes)
 * and its capacity
 */
struct string
{
	char* data;
	///should inlude NULL if possible
	uint32_t length;
	uint32_t capacity;
};

/**
 * @brief This struct contains an array of generic pointers, its length (in bytes)
 * and its capacity and a mutex for shared access
 */
struct dynamic_array
{
	///Note: not used by functions, you have to lock this manually
	pthread_mutex_t mutex;
	void** data;
	size_t length;
	size_t capacity;
};

/**
 * @brief This struct contains an error code and the actual return value of the function
 */
struct return_info
{
	bool error_occured;
	///compatible with strerr()
	int error_code;
	int return_code;
};

/**
 * @brief Writes @a c into @a target at @a position
 * 
 * Writes the char @a c at @a position.
 * If @a target doesn't contain @a position, it is automatically expanded
 * @param Target string to write into
 * @param c Char to write into @a target
 * @param position Replace character at position with @p c
 */
void realloc_write(struct string* target, char c, uint32_t position);
/**
 * @brief Appends @a source to @a target
 * 
 * Writes the characters in @a source to string beginning from position @a target->length,
 * so it breaks if @a target is already NULL-terminated
 * @param target Append to this string
 * @param source Valid C-string
 */
void string_append(struct string* target, char* source);
/**
 * @brief Copies @a source bit for bit into @a target
 * 
 * Copies all the fields of @a source to @a target and copies the string in @a source->data
 * to @a target->data. The struct @a target should already be malloc'd, but
 * @a target->data will be malloc'd by this function.
 * @note @a target->data should be free'd if it is malloc'd before this function
 * is called, otherwise the pointer to that data is lost.
 * @param target Will be bit-identical to @a source
 * @param source Copy fields and string from this string
 */
void string_copy(struct string* target, struct string* source);
/**
 * @brief Resets the size of @a stringbuffer to @a buffer_size if it is larger
 * @param stringbuffer String to resize
 * @param buffer_size Maximum size of stringbuffer after resize
 */
void reset_string(struct string* stringbuffer, uint32_t buffer_size);
void printBits(size_t size, void* ptr);
/**
 * @brief Resizes @a target to fit @a size chars
 * @param target String to resize
 * @param size @a target should be able to hold this many bytes
 */
void adjust_string_size(struct string* target ,uint32_t size);
/**
 * @brief Swaps the endianess of the 16 bit @a byte
 */
void swap_endianess_16(uint16_t* byte);
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

