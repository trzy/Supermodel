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

typedef struct
{
	IR_OP	op;

	/* Operands, modifier, dflow vectors, and other stuff. */

} IR;

extern INT IR_Init (DRPPC_CFG *);
extern void IR_Shutdown (void);

extern INT IR_BeginBB (void);
extern INT IR_EndBB (IR **);

#endif // INCLUDED_IR_H
