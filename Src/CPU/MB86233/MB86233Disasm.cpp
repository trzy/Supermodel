// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/devices/cpu/mb86233/mb86233d.cpp).

#include "MB86233Disasm.h"

#include <cstdarg>
#include <cstdio>
#include <sstream>

// Replacement for MAME's util::stream_format (printf style only)
static void sfmt(std::ostream &stream, const char *fmt, ...)
{
	char buf[256];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);
	stream << buf;
}

/*

    Main register bank:
        0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
    0  b0  b1  x0  x1  x2  i0  i1  i2  sp pag vsm dmc  c0  c1  pc   -
    1   a  ah  al   b  bh  bl   c  ch  cl   d  dh  dl   p  ph  pl sft

    Second register bank:
         0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
    2 <register file>
    3 sio0  si1  pio pioa  rpc    -    -    -  pad  mod  ear   st mask  tim   cx   dx

    In the 86233/86234 the C, page, serial, parallel, timer registers
    don't seem to exist.  The register file is turned into i/o ports.
    Of the second bank part '3', only "mask" is actually used.


    Address encoding (adx uses x0/b0, ady uses x1/b1):
      0 0aaa aaaa  $a
      0 1aaa aaaa  $a(xn)
      1 0aaa aaaa  $a(xn+)
      1 100b bbbb  (bxn+$b)
      1 101b bbbb  (xn+$b)
      1 110b bbbb  [bxn+$b]
      1 111b bbbb  [xn+$b]



    3322 2222 2222 1111 1111 1100 0000 0000
    1098 7654 3210 9876 5432 1098 7654 3210

    0000 00aa aaa0 0?yy yyyy yyyx xxxx xxxx lab adx, ady (e)
    0000 00aa aaa0 11yy yyyy yyyx xxxx xxxx lab adx, ady + 0x200
    0000 00aa aaa1 00yy yyyy yyyx xxxx xxxx lab adx + 0x200, ady

    0001 11aa aaa0 0?yy yyyy yyyx xxxx xxxx mov adx, ady (e)  (the ? is not a + 0x200 select, cf. VR copro code)
    0001 11aa aaa0 10yy yyyy yyyx xxxx xxxx mov adx (e), ady
    0001 11aa aaa0 11yy yyyy yyyx xxxx xxxx mov adx, ady + 0x200
    0001 11aa aaa1 00yy yyyy yyyx xxxx xxxx mov adx + 0x200, ady
    0001 11aa aaa1 01yy yyyy yyyx xxxx xxxx mov adx (o), ady

    0001 11aa aaa1 1100 0rrr rrry yyyy yyyy mov r, ady
    0001 11aa aaa1 1100 1rrr rrry yyyy yyyy mov r, ady (e)
    0001 11aa aaa1 1101 0rrr rrry yyyy yyyy mov ady + 0x200, r
    0001 11aa aaa1 1101 1rrr rrry yyyy yyyy mov ady, r
    0001 11aa aaa1 1110 0rrr rrry yyyy yyyy mov ady (e), r
    0001 11aa aaa1 1110 1rrr rrrx xxxx xxxx mov adx (o), r
    0001 11aa aaa1 1111 0rrr rrr. ..ss ssss mov s, r

    0011 011. .... 101. vvvv vvvv vvvv vvvv stmh (bit0 = fp, bit 1-2 = rounding mode, rest unused)

    0011 10rr vvvv vvvv vvvv vvvv vvvv vvvv lipl/lia/lib/lid #v

    0011 11.a aaaa 000. rrrr rrrr rrrr rrrr clr0 <reglist>
    0011 11.a aaaa 001. ffff ffff ffff ffff clr1 flags
    0011 11.a aaaa 010. 0... .... nnnn nnnn rep #n (0 = 0x100)
    0011 11.a aaaa 010. 1... .... ..11 0100 rep rpc
    0011 11.a aaaa 011. ffff ffff ffff ffff set flags

    01rr rrrr vvvv vvvv vvvv vvvv vvvv vvvv ldi #v, r (actual v size varies 3-24 depending on register)

    1i11 11.c cccc 0000 aaaa aaaa aaaa aaaa brif cond #a
    1i11 11.c cccc 0010 .0.. ...x xxxx xxxx brul cond (adx)
    1i11 11.c cccc 0010 .1.. .... ...r rrrr brul cond r
    1i11 11.c cccc 0100 aaaa aaaa aaaa aaaa bsif cond #a
    1i11 11.c cccc 0110 .0.. ...x xxxx xxxx bsul cond (adx)
    1i11 11.c cccc 0110 .1.. .... ...r rrrr bsul cond r
    1i11 11.c cccc 1010 .... .... .... .... rtif cond
    1i11 11.c cccc 1100 .rrr rrrx xxxx xxxx ldif cond adx, r
    1011 11.1 0110 1110 .... .... .... .... iret

    Top 3 bits = instruction group, except when it isn't.


    Conditions are 5 bits, but only a subset is known, see condition()

    Alu can theorically be on C or D registers, but since C doesn't
    exist, we don't know how C access is encoded.  The operation
    itself is 5-bits for the "full" alu range.

 */



char const *const CMB86233Disassembler::regnames[0x40] = {
	"b0", "b1", "x0", "x1", "x2", "i0", "i1", "i2", "sp", "pag", "vsm", "dmc", "c0", "c1", "pc", "-",
	"a", "ah", "al", "b", "bh", "bl", "c", "ch", "cl", "d", "dh", "dl", "p", "ph", "pl", "sft",
	"rf0", "rf1", "rf2", "rf3", "rf4", "rf5", "rf6", "rf7", "rf8", "rf9", "rfa", "rfb", "rfc", "rfd", "rfe", "rff",
	"sio0", "si1", "pio", "pioa", "rpc", "r?35", "r?36", "r?37", "pad", "mod", "ear", "st", "mask", "tim", "cx", "dx"
};

std::string CMB86233Disassembler::condition(unsigned int cond, bool invert)
{
	std::ostringstream stream;

	if(invert)
		stream << '!';

	switch(cond) {
		case 0x00: sfmt(stream, "zrd"); return stream.str();
		case 0x01: sfmt(stream, "ged"); return stream.str();
		case 0x02: sfmt(stream, "led"); return stream.str();
		case 0x0a: sfmt(stream, "gpio0"); return stream.str();
		case 0x0b: sfmt(stream, "gpio1"); return stream.str();
		case 0x0c: sfmt(stream, "gpio2"); return stream.str();
		case 0x10: sfmt(stream, "zc0"); return stream.str();
		case 0x11: sfmt(stream, "zc1"); return stream.str();
		case 0x12: sfmt(stream, "gpio3"); return stream.str();
		case 0x16: sfmt(stream, "alw"); return stream.str();
	}

	sfmt(stream, "cond(%02x)", cond);
	return stream.str();
}

std::string CMB86233Disassembler::regs(UINT32 reg)
{
	return regnames[reg & 0x3f];
}

std::string CMB86233Disassembler::memory(UINT32 reg, bool x1, bool bank)
{
	std::ostringstream stream;

	switch(reg & 0x180) {
	case 0x000:
		if(bank)
			sfmt(stream, "$0x%x", 0x200 | (reg & 0x7f));
		else if((reg & 0x7f) < 10)
			sfmt(stream, "$%d", reg & 0x7f);
		else
			sfmt(stream, "$0x%x", reg & 0x7f);
		break;

	case 0x080:
		if(bank || (reg & 0x7f)) {
			if(bank)
				sfmt(stream, "$0x%x", 0x200 | (reg & 0x7f));
			else if((reg & 0x7f) < 10)
				sfmt(stream, "$%d", reg & 0x7f);
			else
				sfmt(stream, "$0x%x", reg & 0x7f);
		}

		stream << '(';

		if(x1)
				sfmt(stream, "x1");
		else
				sfmt(stream, "x0");
		stream << ')';
		break;

	case 0x100:
		if(bank || (reg & 0x7f)) {
			if(bank)
				sfmt(stream, "$0x%x", 0x200 | (reg & 0x7f));
			else if((reg & 0x7f) < 10)
				sfmt(stream, "$%d", reg & 0x7f);
			else
				sfmt(stream, "$0x%x", reg & 0x7f);
		}

		stream << '(';

		if ( x1 )
				sfmt(stream, "x1");
		else
				sfmt(stream, "x0");
		stream << "+)";
		break;

	case 0x180:
		stream << (reg & 0x40 ? "[" : "(");
		if(x1) {
			if(!(reg & 0x20))
				sfmt(stream, "bx1");
			else
				sfmt(stream, "x1");
		} else {
			if(!(reg & 0x20))
				sfmt(stream, "bx0");
			else
				sfmt(stream, "x0");
		}

		if(reg & 0x10) {
			if((0x10 - (reg & 0xf)) < 10)
				sfmt(stream, "-%d", 0x10 - (reg & 0xf));
			else
				sfmt(stream, "-0x%x", 0x10 - (reg & 0xf));

		} else if(reg & 0xf) {
			if((reg & 0xf) < 10)
				sfmt(stream, "+%d", reg & 0xf);
			else
				sfmt(stream, "+0x%x", reg & 0xf);
		}

		stream << (reg & 0x40 ? ']' : ')');

		if(bank)
			stream << "+0x200";

		break;
	}

	return stream.str();
}

std::string CMB86233Disassembler::alu0_func(UINT32 alu)
{
	std::ostringstream stream;

	switch(alu) {
	case 0x00: break;
	case 0x01: sfmt(stream, "andd"); break;
	case 0x02: sfmt(stream, "orad"); break;
	case 0x03: sfmt(stream, "eord"); break;
	case 0x04: sfmt(stream, "notd"); break;
	case 0x05: sfmt(stream, "fcpd"); break;
	case 0x06: sfmt(stream, "fadd"); break;
	case 0x07: sfmt(stream, "fsbd"); break;
	case 0x08: sfmt(stream, "fml"); break;
	case 0x09: sfmt(stream, "fmsd"); break;
	case 0x0a: sfmt(stream, "fmrd"); break;
	case 0x0b: sfmt(stream, "fabd"); break;
	case 0x0c: sfmt(stream, "fsmd"); break;
	case 0x0d: sfmt(stream, "fspd"); break;
	case 0x0e: sfmt(stream, "cxfd"); break;
	case 0x0f: sfmt(stream, "cfxd"); break;
	case 0x10: sfmt(stream, "fdvd"); break;
	case 0x11: sfmt(stream, "fned"); break;
		// 12
	case 0x13: sfmt(stream, "d=b+a"); break;
	case 0x14: sfmt(stream, "d=b-a"); break;
		// 15
	case 0x16: sfmt(stream, "lsrd"); break;
	case 0x17: sfmt(stream, "lsld"); break;
	case 0x18: sfmt(stream, "asrd"); break;
	case 0x19: sfmt(stream, "asld"); break;
	case 0x1a: sfmt(stream, "addd"); break;
	case 0x1b: sfmt(stream, "subd"); break;
		// 1c
		// 1d
		// 1e
		// 1f
	default: sfmt(stream, "alu0_func(%02x)", alu); break;
	}

	return stream.str();
}

std::string CMB86233Disassembler::Disassemble(UINT32 opcode)
{
	std::ostringstream stream;

	switch((opcode >> 26) & 0x3f) {
	case 0x00: { // Dual move AB
		UINT32 r1 = opcode & 0x1ff;
		UINT32 r2 = (opcode >> 9) & 0x1ff;
		UINT32 alu = (opcode >> 21) & 0x1f;
		UINT32 op = (opcode >> 18) & 0x7;

		if(alu)
			sfmt(stream, "%s : ", alu0_func(alu).c_str() );

		switch(op) {
		case 0: case 1:
			sfmt(stream, "lab %s, %s (e)", memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;

		case 3:
			sfmt(stream, "lab %s, %s", memory(r1, false, false).c_str(), memory(r2, true, true).c_str());
			break;

		case 4:
			sfmt(stream, "lab %s, %s", memory(r1, false, true).c_str(), memory(r2, true, false).c_str());
			break;

		default:
			sfmt(stream, "lab {%d} %s, %s", op, memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;
		}
		break;
	}

	case 0x07: { // LD/MOV
		UINT32 r1 = opcode & 0x1ff;
		UINT32 r2 = (opcode >> 9) & 0x1ff;
		UINT32 alu = (opcode >> 21) & 0x1f;
		UINT32 op = (opcode >> 18) & 0x7;

		if(alu) {
			sfmt(stream, "%s", alu0_func(alu).c_str());
			if((opcode & 0x001fffff) == 0x1f1e10) {
				// mov a, -  used as nop
				break;
			}
			stream << " : ";
		}

		switch(op) {
		case 0:
			sfmt(stream, "mov {0} %s, %s (e)", memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;

		case 1:
			sfmt(stream, "mov %s, %s (e)", memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;

		case 2:
			sfmt(stream, "mov %s (e), %s", memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;

		case 3:
			sfmt(stream, "mov %s, %s", memory(r1, false, false).c_str(), memory(r2, true, true).c_str());
			break;

		case 4:
			sfmt(stream, "mov %s, %s", memory(r1, false, true).c_str(), memory(r2, true, false).c_str());
			break;

		case 5:
			sfmt(stream, "mov %s (o), %s", memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;

		case 7: {
			switch(r2 >> 6) {
			case 0:
				sfmt(stream, "mov %s, %s", regs(r2 & 0x3f).c_str(), memory(r1, true, false).c_str());
				break;

			case 1:
				sfmt(stream, "mov %s, %s (e)", regs(r2 & 0x3f).c_str(), memory(r1, true, false).c_str());
				break;

			case 2:
				sfmt(stream, "mov %s, %s", memory(r1, true, true).c_str(), regs(r2 & 0x3f).c_str());
				break;

			case 3:
				sfmt(stream, "mov %s, %s", memory(r1, true, false).c_str(), regs(r2 & 0x3f).c_str());
				break;

			case 4:
				sfmt(stream, "mov %s (e), %s", memory(r1, true, false).c_str(), regs(r2 & 0x3f).c_str());
				break;

			case 5:
				sfmt(stream, "mov %s (o), %s", memory(r1, false, false).c_str(), regs(r2 & 0x3f).c_str());
				break;

			case 6:
				if(r1 >> 6)
					sfmt(stream, "mov {r1 %d} %s, %s", r1 >> 6, regs(r1 & 0x3f).c_str(), regs(r2 & 0x3f).c_str());
				else
					sfmt(stream, "mov %s, %s", regs(r1 & 0x3f).c_str(), regs(r2 & 0x3f).c_str());
				break;

			default:
				sfmt(stream, "mov {r2 %d} %s, %s", r2 >> 6, memory(r1, false, false).c_str(), regs(r2 & 0x3f).c_str());
				break;
			}
			break;
		}
		default:
			sfmt(stream, "mov {%d} %s, %s", op, memory(r1, false, false).c_str(), memory(r2, true, false).c_str());
			break;
		}
		break;
	}

	case 0x0d: { // stm/clm
		UINT32 sub2 = (opcode >> 17) & 7;

		switch(sub2) {
		case 5: {
			sfmt(stream, "stmh");
			if(opcode & 0x0001)
				sfmt(stream, " fp");
			static char const *const round_mode[4] = { "rn", "rp", "rm", "rz" };
			sfmt(stream, " %s", round_mode[(opcode >> 1) & 3]);
			break;
		}

		default:
			sfmt(stream, "unk %02x.%x", opcode >> 26, sub2);
			break;
		}
		break;
	}

	case 0x0e: { // Load 24 bit val
		static char const *const inst[4] = { "lipl", "lia", "lib", "lid" };
		sfmt(stream, "%s #0x%x", inst[(opcode >> 24) & 0x3], opcode&0xffffff);
		break;
	}

	case 0x0f: { // rep/clr0/clr1/set
		UINT32 alu = (opcode >> 20) & 0x1f;
		UINT32 sub2 = (opcode >> 17) & 7;

		if(alu)
			sfmt(stream, "%s : ", alu0_func(alu).c_str());

		switch(sub2) {
		case 0: {
			static char const *const rl2[16] = {
				"?0", "?1", "a", "b", "d", "?5", "?6", "?7", "?8", "?9", "?a", "?b", "?c", "?d", "?e", "?f"
			};
			sfmt(stream, "clr0");
			bool first = true;
			for(int i=0; i<16; i++)
				if(opcode & (1<<i)) {
					if(first) {
						first = false;
						sfmt(stream, " %s", rl2[i]);
					} else
						sfmt(stream, ", %s", rl2[i]);
				}
			break;
		}

		case 1:
			sfmt(stream, "clr1 #0x%04x",opcode & 0xffff);
			break;

		case 2:
			if(opcode & 0x8000)
				sfmt(stream, "rep %s", regs(opcode & 0x3f).c_str());
			else if(!(opcode & 0xff))
				sfmt(stream, "rep #0x100");
			else if((opcode & 0xff) < 10)
				sfmt(stream, "rep #%d", opcode & 0xff);
			else
				sfmt(stream, "rep #0x%x", opcode & 0xff);
			break;

		case 3:
			sfmt(stream, "set #0x%04x",opcode & 0xffff);
			break;

		default:
			sfmt(stream, "unk %02x.%x", opcode >> 26, sub2);
			break;
		}
		break;
	}

	case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		sfmt(stream, "ldi #0x%x, %s", opcode & 0xffffff, regnames[(opcode >> 24) & 0x3f]); break;
		break;

	case 0x2f: case 0x3f: {
		UINT32 cond = ( opcode >> 20 ) & 0x1f;
		UINT32 subtype = ( opcode >> 17 ) & 7;
		UINT32 data = opcode & 0xffff;
		bool invert = opcode & 0x40000000;

		switch(subtype) {
		case 0:
			sfmt(stream, "brif %s #0x%x", condition(cond, invert).c_str(), data);
			break;

		case 1:
			sfmt(stream, "brul %s ", condition(cond, invert).c_str());
			if(opcode & 0x4000)
				sfmt(stream, "%s", regs(opcode & 0x1f).c_str());
			else
				sfmt(stream, "(%s)", memory(opcode & 0x1ff, false, false).c_str());
			break;

		case 2:
			sfmt(stream, "bsif %s #0x%x", condition(cond, invert).c_str(), data);
			break;

		case 3:
			sfmt(stream, "bsul %s ", condition(cond, invert).c_str());
			if(opcode & 0x4000)
				sfmt(stream, "%s", regs(opcode & 0x1f).c_str());
			else
				sfmt(stream, "(%s)", memory(opcode & 0x1ff, false, false).c_str());
			break;

		case 5:
			sfmt(stream, "rtif %s", condition(cond, invert).c_str());
			break;

		case 6:
			sfmt(stream, "ldif %s %s, %s", condition(cond, invert).c_str(), memory(data & 0x1ff, false, false).c_str(), regs((data >> 9) & 0x3f).c_str());
			break;

		case 7:
			sfmt(stream, "iret");
			break;

		default:
			sfmt(stream, "unk %02x.%d", opcode >> 26, subtype);
			break;
		}
		break;
	}

	default:
		sfmt(stream, "unk %02x", opcode >> 26);
		break;
	}

	return stream.str();
}
