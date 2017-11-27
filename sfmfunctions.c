#include "SFM.h"


inline void realloc_write(struct string* target, char c, uint32_t offset)	//write char into target->data, increase buffer-size if necessary
{
	adjust_string_size(target, offset);
	target->data[offset] = c;
}

inline void reset_string(struct string* stringbuffer, uint32_t buffer_size) //reset buffer to buffer_size
{
	stringbuffer->length = 0;
	if(stringbuffer->capacity != buffer_size)
	{
		stringbuffer->data = realloc(stringbuffer->data, buffer_size);
		stringbuffer->capacity = buffer_size;
	}
}
inline struct string new_string(uint32_t initial_size)
{
	return (struct string){ .data = malloc(initial_size), .length = 0, .capacity = initial_size };
}

inline struct linked_list* new_linked_list(void)
{
	struct linked_list* returned_linked_list;
	returned_linked_list = malloc(sizeof(*returned_linked_list));
	returned_linked_list->data = NULL;
	returned_linked_list->next = NULL;
	pthread_mutex_init(&(returned_linked_list->mutex), NULL);
	return returned_linked_list;
}

inline void convert_string(struct string* source)		//write length in reversed endianess in the first two bytes
{
	uint32_t length = source->length;
	adjust_string_size(source, length+2u);
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

inline void printBits(size_t size, void* ptr)	//only for debugging
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

inline struct return_info get_message(struct string* message, int socket_fd) //read the first two bytes for length of incoming string, then run realloc_read to read that amount, return false if nothing read
{
	struct pollfd socket_ready = { .fd = socket_fd, .events = POLLIN, .revents = 0};
	uint16_t bytes_to_read = 0;
	uint32_t offset = 0;
	message->length = 0;
	
	struct return_info returning = { .error_occured = false, .error_code = 0, .return_code = false};
	
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
			}
			returning.error_occured = true;
			returning.error_code = -1;
		}

		if(!IS_BIG_ENDIAN)
		{
			swap_endianess_16(&bytes_to_read);
		}

		if(bytes_to_read == 0)
		{
			returning.error_occured = true;
			returning.error_code = -2;
		}
		struct return_info realloc_read_return = realloc_read(message, bytes_to_read, socket_fd, offset);
		if(realloc_read_return.return_code)	//message.data is a null terminated string / if realloc_read returns true, then message continues
		{
			offset += (uint16_t)(bytes_to_read-2);
			continue;
		} else {
			break;
		}
	}
	return returning;
}

inline struct return_info realloc_read(struct string* target, unsigned short bytes_to_read, int socket_fd, uint32_t offset) 	//if buffer is too small to hold bytes_to_read then double
{																				//the space until space is big enough, null terminate the string
	long bytes_read = 0;
	struct return_info returning = { .error_occured = false, .error_code = 0, .return_code = false};
	
	adjust_string_size(target, bytes_to_read+offset);

	if((bytes_read = read(socket_fd, (target->data+offset), bytes_to_read)) == -1)
	{
		returning.error_occured = true;
		returning.error_code = errno;	
	} else if(bytes_read != bytes_to_read) {
		returning.error_occured = true;
		returning.error_code = -1;
	}

	target->length += (unsigned short)bytes_read;
	target->data[bytes_read-1] = '\0';  //last char has to be nul
	
	if(bytes_read > 1)
	{
		returning.return_code = target->data[bytes_read-2] == '\0' ? true : false; //if the char one before last is nul, then the message is split
	}

	return returning;
}

inline void swap_endianess_16(uint16_t * byte) //swap 16 bytes in endianess
{
	uint16_t swapped = (uint16_t)((*byte>>8) | (*byte<<8));
	*byte = swapped;
}

inline void adjust_string_size(struct string* target ,uint32_t size) //resizes a string to fit the size
{
	while(size > target->capacity)
	{
		if(target->capacity == 0)
		{
			target->capacity = 1;
		}
		if((uint32_t)(target->capacity * 2) < target->capacity)
		{
			target->data = realloc(target->data, UINT32_MAX);
			target->capacity = UINT32_MAX;
		} else {
			target->data = realloc(target->data, target->capacity * 2);
			target->capacity = target->capacity * 2;
		}
	}
}

inline struct return_info send_string(const struct string* message, int socket_fd) 		//takes message that already has length prefix
{
	struct return_info returning = { .error_occured = false, .error_code = 0, .return_code = false};
	
	if(write(socket_fd, message->data, message->length) != message->length) //strlen has to count from offset, then +offset+1 to account for the offset and NUL
	{
		returning.error_occured = true;
		returning.error_code = errno;
	} 
		
	return returning;
}

inline bool valid_message_format(const struct string* message, bool is_extended_format) //checks if message has a valid format
{
	bool source_server_found = false;
	bool source_user_found = false;
	bool timestamp_found = false;
	bool target_server_found = false;
	bool target_user_found = false;
	
	if(message->data[0] == '/')
	{
		return true;
	}

	if(is_extended_format)
	{
		for(uint32_t i = 0; i < message->length; i++)
		{
			if(message->data[i] == '@')
			{
				if(!source_server_found)
				{
					source_server_found = true;
				} else if(source_user_found && !target_server_found) {
					target_server_found = true;
				}
			}
			
			if(message->data[i] == '>')
			{
				timestamp_found = true;
			}
			
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
			{
				return true;
			}
		}
		return false;
	} else {
		for(uint32_t i = 0; i < message->length; i++)
		{
			if(message->data[i] == '@' && !target_server_found)
			{
				target_server_found = true;
			}
			
			if(message->data[i] == ':' && target_server_found && !target_user_found)
			{
				target_user_found = true;
			}
			if(target_user_found)
			{
				return true;
			}
		}	
		return false;
	}
}

inline void destroy_linked_list(struct linked_list* currentitem) //frees the linked list completely
{
	struct linked_list tmp;
	while(currentitem != NULL)
	{
		tmp = *currentitem;
		free(currentitem->data);
		free(currentitem);
		currentitem = tmp.next;
	}
}
