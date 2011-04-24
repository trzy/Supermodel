/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
 
/*
 * Bus.h
 * 
 * Header file for the CBus abstract base class.
 */

#ifndef INCLUDED_BUS_H
#define INCLUDED_BUS_H


/*
 * CBus:
 *
 * An abstract base class for an address bus. Defines handlers for 8-, 16-,
 * 32-, and 64-bit random access. All accesses are big endian.
 */
class CBus
{
public:
	/*
	 * Read8(addr):
	 * Read16(addr):
	 * Read32(addr):
	 * Read64(addr):
	 *
	 * Read handlers.
	 *
	 * Parameters:
	 *		addr	Address (caller should ensure it is aligned to the correct
	 *				boundary, corresponding to the size).
	 *
	 * Returns:
	 *		Data of the appropriate size (8, 16, 32, or 64 bits).
	 */
	virtual UINT8	Read8(UINT32 addr) = 0;
	virtual UINT16	Read16(UINT32 addr) = 0;
	virtual UINT32	Read32(UINT32 addr) = 0;
	virtual UINT64	Read64(UINT32 addr) = 0;
	
	/*
	 * Write8(addr, data):
	 * Write16(addr, data):
	 * Write32(addr, data):
	 * Write64(addr, data):
	 *
	 * Write handlers.
	 *
	 * Parameters:
	 *		addr	Address (caller should ensure it is aligned to the correct
	 *				boundary, corresponding to the size).
	 *		data	Data to write.
	 */
	virtual void	Write8(UINT32 addr, UINT8 data) = 0;
	virtual void	Write16(UINT32 addr, UINT16 data) = 0;
	virtual void	Write32(UINT32 addr, UINT32 data) = 0;
	virtual void	Write64(UINT32 addr, UINT64 data) = 0;
};


#endif	// INCLUDED_BUS_H
