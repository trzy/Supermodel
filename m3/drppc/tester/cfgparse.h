/*
 * cfgparse.h
 *
 * Header for configuration file parser.
 */

#ifndef INCLUDED_CFGPARSE_H
#define INCLUDED_CFGPARSE_H

/*
 * Functions
 */

extern BOOL cfgparser_get_rhs_number(UINT *, INT);
extern CHAR *cfgparser_get_rhs_string(INT);
extern CHAR *cfgparser_get_rhs(void);
extern CHAR *cfgparser_get_lhs(void);
extern BOOL cfgparser_step(void);

extern INT  cfgparser_get_line_number(void);
extern BOOL cfgparser_eof(void);

extern BOOL cfgparser_set_file(const CHAR *);
extern void cfgparser_unset_file(void);

#endif  // INCLUDED_CFGPARSE_H
