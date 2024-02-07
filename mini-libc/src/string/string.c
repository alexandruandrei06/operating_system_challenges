// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>

char *strcpy(char *destination, const char *source)
{
	// pointer used to traverse the destination string
	char *d;

	// iterate through the source string until the null terminator is reached
	for (d = destination; *source != '\0'; source++, d++) {
		// copy each character from source to destination
		*d = *source;
	}
	// copy the null terminator to destination to terminate the string
	*d = *source;

	// return a pointer to the destination string
	return destination;
}

char *strncpy(char *destination, const char *source, size_t len)
{
	// pointer used to traverse the destination string
	char *d;
	// keep track of the number of characters copied
	size_t count = 0;

	// iterate through the source string until len characters are copied or
	// the null terminator is reached
	for (d = destination; count < len && *source != '\0';
		 source++, d++, count++)
		*d = *source;

	// add null terminator to the destination string
	if (count < len)
        destination[count] = '\0';

	// return a pointer to the destination string
	return destination;
}

char *strcat(char *destination, const char *source)
{
	char *d;
	int length = strlen(destination);
	// set d to the end of the destination string
	d = destination + length;

	// iterate through the source string and append each character to destination
	for (; *source != '\0'; d++, source++)
		*d = *source;

	// add a null terminator
	*d = *source;

	return destination;
}

char *strncat(char *destination, const char *source, size_t len)
{
	char *d;
	size_t count = 0;
	int length = strlen(destination);
	// set d to the end of the destination string
	d = destination + length;

	// iterate through the source string, appending characters up to len
	// or the null terminator
	for (; count < len && *source != '\0';
		 count++, d++, source++)
		*d = *source;

	// ensure the destination string is null-terminated
    destination[count + length] = '\0';

	return destination;
}

int strcmp(const char *str1, const char *str2)
{
	// initialize the comparison flag
	int flag = 0;

	// compare characters in both strings until a difference is found or
	// one of the strings ends
	while(flag == 0) {
		if (*str1 == '\0' || *str2 == '\0')
			break;
		if (*str1 < *str2)
			flag = -1;
		if (*str1 > *str2)
			flag = 1;
		str1++;
		str2++;
	}

	// check if one of the strings is longer than the other
	// flag is 1 if str1 is longer than str2 and -1 in the opposite case
	if (flag == 0 && *str1 != '\0')
		flag = 1;
	if (flag == 0 && *str2 != '\0')
		flag = -1;

	return flag;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
	size_t flag = 0, count = 0;

	// compare characters in both strings until a difference is found,
	// one of the strings ends, or 'len' is reached
	while(flag == 0 && count < len) {
		if (*str1 == '\0' || *str2 == '\0')
			break;
		if (*str1 < *str2)
			flag = -1;
		if (*str1 > *str2)
			flag = 1;
		str1++;
		str2++;
		count++;
	}

	return flag;
}

size_t strlen(const char *str)
{
	size_t i = 0;

	for (; *str != '\0'; str++, i++) {}
	return i;
}

char *strchr(const char *str, int c)
{
	while(1) {
		// return a pointer to the location of c in str
		if (*str == c)
			return (char *)str;
		if (*str == '\0')
			return NULL;
		str++;
	}
	// return NULL if c is not found in str
	return NULL;
}

char *strrchr(const char *str, int c)
{
	// pointer to track the last occurrence of c in str
	const char *chr;
	int flag = 0;
	while(*str != '\0') {
		if (*str == c) {
			chr = str;
			// set flag to indicate c was found
			flag = 1;
		}
		str++;
	}
	// if c was found, return a pointer to the last occurrence, NULL otherwise
	if (flag == 1)
		return (char *)chr;
	return NULL;
}

char *strstr(const char *haystack, const char *needle)
{
	int s1_length = strlen(haystack);
	int s2_length = strlen(needle);
	// a copy of haystack
	const char *haystack_copy = haystack;

	// iterate through haystack with an offset of s1_length - s2_length
	for (int i = 0; i < s1_length - s2_length; i++) {
		int i_copy = i;
		// if the current character in haystack matches the first character of needle
		if (haystack[i] == needle[0]) {
			// counter to keep track of matching characters
			int count = 0;
			// compare characters in haystack with needle
			for (int j = 0; j < s2_length; j++) {
				if (haystack[i] == needle[j]) {
					count++;
					i++;
				} else {
					// reset i to the initial position
					i = i_copy;
					break;
				}
			}
			// if count matches the length of needle, needle is found in haystack
			// and return the firs occurrence
			if (count == s2_length)
				return (char *)haystack_copy;
		}
		haystack_copy++;
	}
	return NULL;
}

char *strrstr(const char *haystack, const char *needle)
{
	int s1_length = strlen(haystack);
	int s2_length = strlen(needle);
	int flag = 0;
	const char *haystack_copy = haystack;
	// pointer to track the last occurrence of needle
	const char *haystack_return_point;
	for (int i = 0; i < s1_length - s2_length; i++) {
		int i_copy = i;
		if (haystack[i] == needle[0]) {
			int count = 0;
			for (int j = 0; j < s2_length; j++) {
				if (haystack[i] == needle[j]) {
					count++;
					i++;
				} else {
					i = i_copy;
					break;
				}
			}
			if (count == s2_length) {
				// track the last occurrence of needle
				haystack_return_point = haystack_copy;
				// set the flag to indicate needle was found
				flag = 1;
				i = i_copy;
			}
		}
		haystack_copy++;
	}
	if (flag == 1)
		return (char *)haystack_return_point;
	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num)
{
	// cast destination and source to char pointer for byte-wise copying
	char *char_destination = (char *)destination;
	char *char_source = (char *)source;

	// iterate num times, copying one byte at a time from source to destination
	for (size_t i = 0; i < num; i++)
		char_destination[i] = char_source[i];

	return (void *)destination;
}

void *memmove(void *destination, const void *source, size_t num)
{
	// temporary buffer to store the data
	unsigned char buffer[num];
	// copy num bytes from source to the buffer
	memcpy(buffer, source, num);
	// copy num bytes from the buffer to destination
	memcpy(destination, buffer, num);

	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	char *str1 = (char *)ptr1;
	char *str2 = (char *)ptr2;

	size_t flag = 0, count = 0;
	// compare bytes in str1 and str2 until a difference is found,
	// or num bytes are compared
	while(flag == 0 && count < num) {
		if (*str1 < *str2)
			flag = -1;
		if (*str1 > *str2)
			flag = 1;
		str1++;
		str2++;
		count++;
	}
	// 0 if the two strings are equal, less than 0 if str1 compares less than str2,
	// and greater than 0 if str1 compares greater than str2
	return flag;
}

void *memset(void *source, int value, size_t num)
{
	// cast source to a char pointer for byte-wise filling
	char *string = (char *)source;

	// fill source with num bytes of memory of the specified value
	while (num) {
		*string = (char)value;
		string++;
		num--;
	}
	return source;
}
