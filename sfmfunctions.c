#include "SFM.h"


void realloc_write(struct string* target, char c, uint32_t position)	//write char into target->data, increase buffer-size if necessary
{
	adjust_string_size(target, position);
	target->data[position] = c;
}

void string_append(struct string* target, char* source)	//appends source to
{
	uint32_t length = (uint32_t)strlen(source);
	adjust_string_size(target, target->length+length);
	memcpy(target->data+target->length, source, length);
	target->length += length;
}

void string_copy(struct string* target, struct string* source)	//appends source to
{
	memcpy(target, source, sizeof(*target));
	target->data = malloc(target->capacity);
	memcpy(target->data, source->data, target->length);
}

void reset_string(struct string* stringbuffer, uint32_t buffer_size) //reset stringbuffer to buffer_size
{
	stringbuffer->length = 0;
	if(stringbuffer->capacity != buffer_size)
	{
		stringbuffer->data = realloc(stringbuffer->data, buffer_size);
		stringbuffer->capacity = buffer_size;
	}
}

struct string new_string(uint32_t initial_size) //creates a new string pointer
{
	return (struct string){ .data = malloc(initial_size), .length = 0, .capacity = initial_size };
}

void convert_string(struct string* source)		//write length in reversed endianess in the first two bytes
{
	uint32_t length = source->length;
	adjust_string_size(source, length+2);
	char* tmp = malloc(length);

	memcpy(tmp, source->data, length);

	if(!IS_BIG_ENDIAN)
	{
		source->data[0] = (char)(length >> 8);
		source->data[1] = (char)(length >> 0);
	} else {
		source->data[1] = (char)(length >> 8);
		source->data[0] = (char)(length >> 0);
	}

	memcpy(source->data+2, tmp, length);
	free(tmp);
	source->length += 2;
}

void printBits(size_t size, void* ptr)	//only for debugging
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    size_t i;
    int j;

    for (i=0; i < size;i++)
    {
        for (j=7;j>=0;j--)
        {
            byte = (unsigned char)((b[i] >> j) & 1);
            printf("%u", byte);
        }
        puts("\n");
    }
    puts("");
}

struct return_info get_message(struct string* message, int socket_fd) //read the first two bytes for length of incoming string, then run realloc_read to read that amount, return false if nothing read
{
	struct pollfd socket_ready = { .fd = socket_fd, .events = POLLIN, .revents = 0};
	struct return_info returning = { .error_occured = false, .error_code = 0, .return_code = false};
	uint16_t bytes_to_read = 0;
	uint32_t offset = 0;
	message->length = 0;

	while(poll(&socket_ready, 1, POLL_TIMEOUT)) //poll socket input and check for interrupt
	{
		returning.return_code = true;
		long read_return;
		if((read_return = read(socket_fd, &bytes_to_read, 2)) != 2)
		{
			if(read_return == -1)
			{
				returning.error_occured = true;
				returning.error_code = errno;
				break;
			}
			returning.error_occured = true;
			returning.error_code = -1; //indicate wrong amount read
			break;
		}

		if(!IS_BIG_ENDIAN)
			swap_endianess_16(&bytes_to_read);

		if(bytes_to_read == 0)
		{
			returning.error_occured = true;
			returning.error_code = -2; //indicate missing following message
			break;
		}
		struct return_info realloc_read_return = realloc_read(message, bytes_to_read, socket_fd, offset);

		if(realloc_read_return.error_occured)
		{
			returning.error_occured = true;
			returning.error_code = realloc_read_return.error_code;
			break;
		}

		if(realloc_read_return.return_code)	//message.data is a null terminated string / if realloc_read returns true, then message continues
		{
			offset += (uint16_t)(bytes_to_read-2);
			if(offset > MAX_WRITE_SIZE)
			{
				returning.error_occured = true;
				returning.error_code = -3; //indicate message too long
				break;
			}
			continue;
		} else {
			break;
		}
	}
	return returning;
}

struct return_info realloc_read(struct string* target, unsigned short bytes_to_read, int socket_fd, uint32_t offset)
{
	//read message into buffer, return true if message continues
	long bytes_read = 0;
	struct return_info returning = { .error_occured = false, .error_code = 0, .return_code = false};

	adjust_string_size(target, bytes_to_read+offset);

	if((bytes_read = read(socket_fd, (target->data+offset), bytes_to_read)) == -1)
	{
		returning.error_occured = true;
		returning.error_code = errno;
	} else if(bytes_read != bytes_to_read) {
		returning.error_occured = true;
		returning.error_code = -1;  //indicate wrong amount read
	}

	target->length += (unsigned short)bytes_read;
	target->data[bytes_read-1] = '\0';  //last char has to be nul anyways

	if(bytes_read > 1)
		returning.return_code = target->data[bytes_read-2] == '\0' ? true : false; //if the char one before last is nul, then the message is split

	return returning;
}

void swap_endianess_16(uint16_t * byte) //swap 16 bytes in endianess
{
	uint16_t swapped = (uint16_t)((*byte>>8) | (*byte<<8));
	*byte = swapped;
}

void adjust_string_size(struct string* target ,uint32_t size) //resizes a string to fit the size
{
	while(target->capacity < size)
	{
		if(target->capacity == 0)
			target->capacity = 1;
		if((uint32_t)(target->capacity * 2) < target->capacity)
		{
			target->data = realloc(target->data, INT32_MAX);
			target->capacity = INT32_MAX;
		} else {
			target->data = realloc(target->data, target->capacity * 2);
			target->capacity = target->capacity * 2;
		}
	}
}

struct return_info send_string(const struct string* message, int socket_fd) 		//takes message that already has length prefix
{
	struct return_info returning = { .error_occured = false, .error_code = 0, .return_code = false};

	if(write(socket_fd, message->data, message->length) != (ssize_t)message->length)
	{
		returning.error_occured = true;
		returning.error_code = errno;
	}

	return returning;
}

bool valid_message_format(const struct string* message, bool is_extended_format) //checks if message has a valid format TODO: optimize
{
	bool target_server_found = false;
	bool target_user_found = false;

	if(message->data[0] == '/')
		return true;

	if(is_extended_format)
	{
		bool source_server_found = false;
		bool source_user_found = false;
		bool timestamp_found = false;
		for(uint32_t i = 0; i < message->length; i++)
		{
			if(message->data[i] == '@')
			{
				if(!source_server_found)
					source_server_found = true;
				else if(source_user_found && !target_server_found)
					target_server_found = true;
			}

			if(message->data[i] == '>')
				timestamp_found = true;

			if(message->data[i] == ':')
			{
				if(source_server_found && !source_user_found)
				{
					source_user_found = true;
					i += 8;
				} else if(target_server_found && !target_user_found) {
					target_user_found = true;
				}
			}
			if(target_user_found && timestamp_found)
				return true;
		}
		return false;
	} else {
		for(uint32_t i = 0; i < message->length; i++)
		{
			if(message->data[i] == '@' && !target_server_found)
				target_server_found = true;
			if(message->data[i] == ':' && target_server_found && !target_user_found)
				target_user_found = true;
			if(target_user_found)
				return true;
		}
		return false;
	}
}

void dynamic_array_push(struct dynamic_array* array, void* item) //puts item at end of array
{
	dynamic_array_adjust(array, array->length+1);
	array->data[array->length] = item;
	array->length++;
}

void dynamic_array_adjust(struct dynamic_array* array, size_t size) //expands or shrinks array
{
	while(array->capacity < size)
	{
		array->capacity *= 2;
		array->data = realloc(array->data, array->capacity*(sizeof(*array->data)));
	}
	while(array->capacity > (size*2))
	{
		array->capacity /= 2;
		array->data = realloc(array->data, array->capacity*(sizeof(*array->data)));
	}
}

void* dynamic_array_at(const struct dynamic_array* array, size_t position) //returns pointer at position or NULL
{
	if(position >= array->length)
		return NULL;
	else
		return array->data[position];
}

void dynamic_array_remove(struct dynamic_array* array, size_t position) //removes & frees pointer at position and fill missing spot
{
	if(position < array->length)
	{
		free(array->data[position]);
		memmove(array->data+position, array->data+position+1, (array->length-position-1)*sizeof(*array->data));
		array->length--;
	}
}

struct dynamic_array* new_dynamic_array(void) //creates new array with inital capacity of 4
{
	struct dynamic_array* array = malloc(sizeof(*array));
	assert(pthread_mutex_init(&array->mutex, NULL) == 0);
	array->data = malloc(sizeof(*array->data)*4u);
	array->length = 0;
	array->capacity = 4;

	return array;
}

void destroy_dynamic_array(struct dynamic_array* array) //frees all pointers in the array and the array itself
{
	if(array != NULL)
	{
		for(size_t i = 0; i < array->length; i++)
			free(array->data[i]);

		free(array->data);
		pthread_mutex_destroy(&array->mutex);
		free(array);
	}
}
