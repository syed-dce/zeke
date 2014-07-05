/**
 *******************************************************************************
 * @file    kstring.h
 * @author  Olli Vanhoja
 * @brief   String routines
 *******************************************************************************
 */

/** @addtogroup kstring
 * @{
 */


#pragma once
#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdint.h>

#define _TO_STR(x) #x
#define TO_STR(x) _TO_STR(x)

void * memcpy(void * restrict destination, const void * source, size_t num);
void * memmove(void * destination, const void * source, size_t num);
void * memset(void * ptr, int value, size_t num);

/** @addtogroup strcmp
 * @{
 */

/**
 * Compares the C string str1 to the C string str2.
 * @param str1 String 1.
 * @param str2 String 2.
 * @return A zero value indicates that both strings are equal.
 */
int strcmp(const char * str1, const char * str2);

/**
 * Compares the C string str1 to the C string str2.
 * @param str1 String 1.
 * @param str2 String 2.
 * @param Maximum number of characters to compare.
 * @return A zero value indicates that both strings are equal.
 */
int strncmp(const char * str1, const char * str2, size_t n);

/**
 * @}
 */

/** @addtogroup strcpy strcpy, strncpy, strlcpy
 * @{
 */

/**
 * Copy string.
 * @param destination Pointer to the destination string.
 * @param source Pointer to the source string.
 */
char * strcpy(char * destination, const char * source);

/**
 * Copy characters from string.
 * Copies the first n characters of source to destination.
 * If the end of the source string (which is signaled by a null-character) is
 * found before n characters have been copied, destination is padded with
 * zeros until a total of num characters have been written to it.
 *
 * No null-character is implicitly appended at the end of destination if
 * source is longer than n. Thus, in this case, destination shall not
 * be considered a null terminated C string (reading it as such would overflow).
 * @param dst pointer to the destination array.
 * @param src string to be copied from.
 * @param n is the maximum number of characters to be copied from src.
 * @return dst.
 */
char * strncpy(char * dst, const char * src, size_t n);

/**
 * Copy src to string dst of size siz.
 * At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * @param dst is the destination.
 * @param src is the source C-string.
 * @param siz is the maximum size copied.
 * @return Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char * dst, const char * src, size_t siz);

/**
 * @}
 */

const char * strstr(const char * str1, const char * str2);

/**
 * Get string lenght.
 * @param str null terminated string.
 * @param max Maximum lenght counted.
 */
size_t strlenn(const char * str, size_t max);

/**
 * Concatenate strings.
 * Appends a copy of the src to the dst string.
 * @param dst is the destination array.
 * @param ndst maximum length of dst.
 * @param src is the source string array.
 * @param nsrc maximum number of characters to bo copied from src.
 */
char * strnncat(char * dst, size_t ndst, const char * src, size_t nsrc);

/** @addtogroup strsep strsep
 * Get next token.
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 * @{
 */

char * strsep(char ** stringp, const char * delim);

/**
 * @}
 */

/**
 * Validate null terminated string.
 * Return (1) if the buffer pointed to by kernel pointer 'buffer' and
 * of length 'bufferlen' contains a valid NUL-terminated string.
 * @param buffer
 */
int strvalid(const char * buf, size_t len);

int atoi(const char * str);

/**
 * Convert uint32_t integer to a decimal string.
 * @param str Array in memory where to store the resulting null-terminated
 *            string.
 * @param value Value to be converted to a string.
 * @return number of characters inserted.
 */
int uitoa32(char * str, uint32_t value);

int uitoa64(char * str, uint64_t value);

/**
 * Convert uint32_t integer to a hex string.
 * @param str Array in memory where to store the resulting null-terminated
 *            string.
 * @param value Value to be converted to a string.
 * @return number of characters inserted.
 */
int uitoah32(char * str, uint32_t value);

/**
 * Duplicate a string.
 * @param src is a pointer to the source string.
 * @param max is the maximum length of the src string.
 * @return A kmalloc'd pointer to the copied string.
 */
char * kstrdup(const char * src, size_t max);

/** @addtogroup ksprintf
 * Formated string composer.
 * @{
 */

/**
 * Composes a string by using printf style format string and additional
 * arguments.
 *
 * This function supports following format specifiers:
 *      %d or i Signed decimal integer
 *      %u      Unsigned decimal integer
 *      %x      Unsigned hexadecimal integer
 *      %c      Character
 *      %s      String of characters
 *      %s      Pointer address
 *      %%      Replaced with a single %
 *
 * @param str Pointer to a buffer where the resulting string is stored.
 * @param maxlen Maximum lenght of str. Replacements may cause writing of more
 *               than maxlen characters!
 * @param format String that contains a format string.
 * @param ... Depending on the format string, a sequence of additional
 *            arguments, each containing a value to be used to replace
 *            a format specifier in the format string.
 */
void ksprintf(char * str, size_t maxlen, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

/**
 * @}
 */

char * kstrtok(char * s, const char * delim, char ** lasts);

#endif /* KSTRING_H */

/**
 * @}
 */
