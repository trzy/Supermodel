/*
 * middle/ir.h
 *
 * Intermediate Representation language definitions and interface.
 */

#ifndef INCLUDED_IR_H
#define INCLUDED_IR_H

typedef enum
{
	IR_NONE_DEFINED_AT_THE_MOMENT = 0,	// :)

} IR_OP;

extern INT IR_Init (DRPPC_CFG *);
extern INT IR_Reset (void);
extern void IR_Shutdown (void);

#endif // INCLUDED_IR_H
