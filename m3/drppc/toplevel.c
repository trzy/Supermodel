/*
 * toplevel.c
 *
 * General purpose source file. It holds a number of basic routines used all
 * through drppc.
 */

#include <stdio.h>
#include <stdarg.h>
#include "types.h"
#include "drppc.h"
#include "toplevel.h"

/*******************************************************************************
 Global Variables
*******************************************************************************/

void	* (* Alloc)(UINT);
void	(* Free)(void *);
void	(* Print)(CHAR *, ...);

/*******************************************************************************
 Interface
*******************************************************************************/

INT SetupBasicServices (DRPPC_CFG * cfg)
{
	if ((Alloc = cfg->Alloc) == NULL || (Free = cfg->Free) == NULL)
		return DRPPC_INVALID_CONFIG;

	if ((Print = cfg->Print) == NULL)
		return DRPPC_INVALID_CONFIG;

	return DRPPC_OKAY;
}
