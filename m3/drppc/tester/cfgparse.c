/*
 * cfgparse.c
 *
 * Configuration file parser.
 *
 * The parser is line oriented. Every line must have this format:
 *
 *      LHS = ARG0[, ARG2, ARG3, ...]
 *
 * At least one argument must be present. When an argument is not present,
 * a NULL pointer is returned. Strings are read with leading and trailing
 * whitespace stripped, but everything in between is preserved. Quotes do not
 * have any special meaning.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drppc.h"
#include "cfgparse.h"

/******************************************************************/
/* Configuration                                                  */
/*                                                                */
/* NOTE: This should probably be moved to a header file.          */
/******************************************************************/

#define BUFFER_SIZE 256 // defines the size of the line buffer

/******************************************************************/
/* Local Data                                                     */
/******************************************************************/

static CHAR buffer[BUFFER_SIZE];    // line buffer
static FILE *fp;                    // file

static INT  line_num;               // current line number

static CHAR data[BUFFER_SIZE];      // extracted data

/******************************************************************/
/* File Manipulation                                              */
/******************************************************************/

/*
 * get_line():
 *
 * Reads a line. Returns non-zero if an error occured or EOF was reached.
 */

static BOOL get_line(void)
{
    if (fgets(buffer, BUFFER_SIZE, fp) != buffer)
        return 1;   // error or EOF
    return 0;
}

/******************************************************************/
/* Parsing                                                        */
/******************************************************************/

/*
 * strip_comments():
 *
 * Removes comments and maintains line_num.
 */

static void strip_comments(void)
{
    INT i;

    for (i = 0; i < BUFFER_SIZE && buffer[i] != '\0'; i++)
    {
        if (buffer[i] == ';')   // end the line the first line comment
            buffer[i] = '\0';
        if (buffer[i] == '\n')  // newline, increment line number
            ++line_num;
    }
}

/*
 * test_line():
 *
 * Returns true if the line has any characters that are not whitespace.
 */

static BOOL test_line(void)
{
    INT     i;

    for (i = 0; i < BUFFER_SIZE && buffer[i] != '\0'; i++)
    {
        if (!isspace(buffer[i]))
            return 1;   // found something valid
    }

    return 0;           // nothing here
}

/*
 * seek_data():
 *
 * Seeks the first non-whitespace data in the supplied zero-terminated buffer
 * and returns a pointer to it or NULL if nothing was found.
 */

static CHAR *seek_data(CHAR *ptr)
{
    while (*ptr != '\0')
    {
        if (!isspace(*ptr))
            return ptr;
        ++ptr;
    }

    return NULL;
}

/*
 * seek_char():
 *
 * Seeks through the zero-terminated buffer until the requested character is
 * founded and returns a pointer to it. If not found, returns NULL.
 */

static CHAR *seek_char(CHAR *ptr, CHAR c)
{
    while (*ptr != '\0')
    {
        if (*ptr == c)
            return ptr;
        ++ptr;
    }

    return NULL;
}

/*
 * seek_rhs():
 *
 * Seeks to the first character after the = sign. Returns NULL if the = sign
 * is not present.
 */

static CHAR *seek_rhs(void)
{
    CHAR    *ptr;

    ptr = seek_char(buffer, '=');
    if (ptr == NULL)
        return NULL;
    ++ptr;

    return ptr;
}

/*
 * seek_arg():
 *
 * Finds an argument by searching for the RHS and then counting commas. It
 * returns with the pointer pointing to the first valid character of the
 * argument. If this is a comma or \0, then the argument is null. The caller
 * must check for this. NULL means the argument does not exist.
 */

static CHAR *seek_arg(INT arg_num)
{
    CHAR    *ptr;
    INT     i;

    /*
     * Seek to the = and then to the first valid character after that
     */

    ptr = seek_rhs();
    if (ptr == NULL)
        return NULL;
    ptr = seek_data(ptr);
    if (ptr == NULL)    // nothing
        return NULL;    // no arguments exist

    /*
     * Seek to whichever argument number we're looking for by counting commas
     */

    for (i = 0; i < arg_num; i++)
    {
        ptr = seek_char(ptr, ',');
        if (ptr == NULL)        // not found
            return NULL;
        ++ptr;                  // skip over current comma to find next
    }

    ptr = seek_data(ptr);       // ignore leading whitespace
    return ptr;
}

/******************************************************************/
/* Public Interface                                               */
/******************************************************************/

/*
 * BOOL cfgparser_get_rhs_number(UINT *result, INT arg_num);
 *
 * Extracts a number from the given argument. The number is written to result.
 * Negative and floating point numbers are not supported (invalid number data
 * will be returned but the function will still be successful.)
 *
 * Parameters:
 *      result  = Pointer to write number to.
 *      arg_num = Argument number (starting at 0.)
 *
 * Returns:
 *      Non-zero if the argument doesn't exist or no text at all was
 *      specified (a null argument.)
 */

BOOL cfgparser_get_rhs_number(UINT *result, INT arg_num)
{
    CHAR    *start_ptr;

    *result = 0;

    /*
     * Find the argument and make sure it's not NULL.
     */

    start_ptr = seek_arg(arg_num);
    if (start_ptr == NULL)                          // arg doesn't even exist
        return 1;
    if (*start_ptr == ',' || *start_ptr == '\0')    // another comma or nothing -- a null arg
        return 1;                                   // nothing was specified

    /*
     * Interpret the number
     */

    if (start_ptr[0] == '0' && (start_ptr[1] == 'x' || start_ptr[1] == 'x'))
    {
        start_ptr += 2;                 // move to the number
        while (isxdigit(*start_ptr))    // fetch hex number
        {
            *result <<= 4;

            if (isdigit(*start_ptr))
                *result |= (*start_ptr - '0');
            else
            {
                if (isupper(*start_ptr))
                    *result |= ((*start_ptr - 'A') + 10);
                else
                    *result |= ((*start_ptr - 'a') + 10);
            }

            ++start_ptr;
        }
    }
    else
    {
        while (isdigit(*start_ptr))     // fetch decimal number
        {
            *result *= 10;
            *result += (*start_ptr - '0');
            ++start_ptr;
        }
    }

    return 0;
}

/*
 * CHAR *cfgparser_get_rhs_string(INT arg_num);
 *
 * Returns a pointer to the string representation of the specified argument
 * number. Subsequent parser calls will destroy what is in the buffer.
 *
 * Parameters:
 *      arg_num = Argument number (starting at 0.)
 *
 * Returns:
 *      A pointer to a null-terminated string containing the argument or NULL
 *      if a parse error occured (badly formed line or argument is not
 *      present.)
 */

CHAR *cfgparser_get_rhs_string(INT arg_num)
{
    CHAR    *start_ptr, *end_ptr;

    /*
     * Find the argument and ensure it is not a null arg
     */

    start_ptr = seek_arg(arg_num);
    if (start_ptr == NULL)
        return NULL;
    if (*start_ptr == ',' || *start_ptr == '\0')    // another comma or nothing -- a null arg
        return NULL;

    /*
     * Seek to next comma or end of string manually; then, go backwards until
     * first valid character is found -- this will mark the end of the string
     * argument to extract.
     */

    for (end_ptr = start_ptr; *end_ptr != '\0' && *end_ptr != ','; end_ptr++);

    while (end_ptr > start_ptr)
    {
        --end_ptr;
        if ((*end_ptr != '\0') && (!isspace(*end_ptr)))   // we hit something
            break;
    }

    /*
     * If we hit a comma scanning backwards, that means there's no valid data
     */

    if (*end_ptr == ',')
        return NULL;

    /*
     * Copy the argument to a temporary buffer and return that.
     */

    memcpy(data, start_ptr, (end_ptr - start_ptr + 1) * sizeof(CHAR));
    data[end_ptr - start_ptr + 1] = '\0';
    return data;
}

/*
 * CHAR *cfgparser_get_rhs(void);
 *
 * Returns a pointer to the entire string representation of the RHS. Leading
 * and trailing whitespace is stripped off. Subsequent parser calls will
 * destroy what is in the buffer returned here.
 *
 * Returns:
 *      A pointer to a null-terminated string containing the RHS of the line
 *      or NULL if a parse error occured (badly formed line or null argument.)
 */

CHAR *cfgparser_get_rhs(void)
{
    CHAR    *start_ptr, *end_ptr;

    /*
     * Seek to the =
     */

    start_ptr = seek_rhs();
    if (start_ptr == NULL)
        return NULL;

    /*
     * Seek to first valid character in the right-hand side
    */

    start_ptr = seek_data(start_ptr);
    if (start_ptr == NULL)  // nothing, copy a null string to return buffer
        return NULL;

    /*
     * Find terminating null in buffer (we can't use seek_char()). Then, scan
     * backwards to first valid character. That will be the end of the
     * RHS string.
     */

    for (end_ptr = buffer; *end_ptr != '\0'; end_ptr++) ;
    while (end_ptr > start_ptr)
    {
        if ((*end_ptr != '\0') && !isspace(*end_ptr))   // we hit something
            break;
        --end_ptr;
    }

    /*
     * Copy the RHS to a temporary buffer and return that
     */

    memcpy(data, start_ptr, (end_ptr - start_ptr + 1) * sizeof(CHAR));
    data[end_ptr - start_ptr + 1] = '\0';
    return data;    
}

/*
 * CHAR *cfgparser_get_lhs(void);
 *
 * Returns a pointer to the LHS of the current line. Leading and trailing
 * whitespace is stripped off. Subsequent parser calls will destroy what is in
 * the buffer returned here.
 *
 * NOTE: If the line buffer has not been filled or an error has previously
 * occured, this function's operation is undefined.
 *
 * Returns:
 *      A pointer to a null-terminated string containing the LHS of the line
 *      or NULL if a parse error occured (badly formed line.)
 */

CHAR *cfgparser_get_lhs(void)
{
    CHAR    *start_ptr, *end_ptr;

    if (cfgparser_eof())        // nothing valid in the buffer
        return NULL;

    /*
     * Find the first valid data in the line and the first =. Then, scan
     * backwards from the = to the first non-whitespace character.
     */

    start_ptr = seek_data(buffer);
    end_ptr = seek_char(start_ptr, '=');
    if (start_ptr == end_ptr)   // first thing found was the = sign
        return NULL;
    if (start_ptr == NULL || end_ptr == NULL)
        return NULL;

    do
        --end_ptr;
    while (isspace(*end_ptr) && (end_ptr > start_ptr));
            
    /*
     * Copy the LHS to a temporary buffer and return that
     */

    memcpy(data, start_ptr, (end_ptr - start_ptr + 1) * sizeof(CHAR));
    data[end_ptr - start_ptr + 1] = '\0';
    return data;
}

/*
 * BOOL cfgparser_step(void);
 *
 * Reads in the next valid line. Lines which contain nothing but whitespace
 * are ignored.
 *
 * Returns:
 *      True if EOF was reached or an error occured reading the file.
 */

BOOL cfgparser_step(void)
{
    BOOL    valid = 0;

    do
    {
        if (get_line())
            return 1;
        strip_comments();
        valid = test_line();
    } while (!valid);

    return 0;
}

/*
 * INT cfgparser_get_line_number(void);
 *
 * Obtains the line number for what is currently sitting in the line buffer.
 * If the line number is 0, this indicates that either parsing has not begun
 * (cfgparser_step() hasn't been called) or that the first line is so big
 * that the line buffer only contains part of it (an invalid line.)
 *
 * Returns:
 *      Line number.
 */

INT cfgparser_get_line_number(void)
{
    if (line_num == 0)  // sometimes we're in the middle of a long first line
        return 1;
    return line_num;
}

/*
 * BOOL cfgparser_eof(void);
 *
 * Checks whether the file end has been reached.
 *
 * Returns:
 *      True if the end of the file has been reached.
 */

BOOL cfgparser_eof(void)
{
    return feof(fp);
}

/*
 * void cfgparser_reset(void);
 *
 * Resets the configuration file position to the beginning. If this is called
 * when a file is not loaded, the result is undefined.
 */

void cfgparser_reset(void)
{
    rewind(fp);
    line_num = 0;
}

/*
 * BOOL cfgparser_set_file(const CHAR *fname);
 *
 * Sets the current configuration file to be processed. This causes the file
 * to be opened.
 *
 * Parameters:
 *      fname = File name.
 *
 * Returns:
 *      Non-zero if the file cannot be opened.
 */

BOOL cfgparser_set_file(const CHAR *fname)
{
    line_num = 0;

    fp = fopen(fname, "r");
    if (fp == NULL)
        return 1;

    return 0;
}

/*
 * void cfgparser_unset_file(void);
 *
 * Unsets the current file; i.e., closes it.
 */

void cfgparser_unset_file(void)
{
    fclose(fp);
}
