// The original MegaLZ Speccy packer, Z80 depacker and packed file
// format  (C) fyrex^mhm.
// 
// Advanced C MegaLZ packer  (C) lvd^mhm.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "megalz.h"

ULONG mode;	// mode of working:

UBYTE * indata;		// input data here
UBYTE * outdata;	// output data here

ULONG datapos;

// buffer for outting output stream to file
UBYTE oubuf[OUBUFSIZE];

ULONG inpos;
ULONG inlen;		// length of input file

UBYTE dbuf[DBSIZE];
ULONG dbpos;		// current position in buffer (wrappable)

UBYTE bitstream;
ULONG bitcount;

// hashes for every 3byte string in file: hash[i] is hash for indata[i-2],indata[i-1] and indata[i]
UBYTE * hash;	// hash algorithm: (byte[0]>>>2) ^ (byte[1]>>>1) ^ byte[2]

// position of byte in output buffer where bits are written
ULONG ob_bitpos;

// current free position in output buffer
ULONG ob_freepos;

// num of bits in 'bit'-byte (pointed to by ob_bitpos)
ULONG ob_bits;

// two-byte lookup entry table
struct tb_chain *tb_entry[65536];	//array of ptrs to tb_chain chains or NULL if match not exists
// array index i=FirstByte*256+SecondByte (65536 entries total)
// at every position in input file current tb_entry points to the
// displacements from current pos: -1 .. -4352
struct tb_chain * tb_free;	// linked list of freed tb_chain entries - NULL at the beginning

struct tb_bunch * tb_bunches;	// all allocated bunches as linked list

struct lzcode codes[256];	// for the current byte contain all possible codes from .length=1 to .length=255,
// stopflag - .length=1, .disp=0 (also means OUTBYTE)
ULONG codepos;	// current position in codes[] array

struct packinfo * pdata;	// array of packinfo elements for every byte of input file
// (dynamically allocated)

ULONG init_hash(void)
{
	hash = (UBYTE*) malloc(inlen* sizeof(UBYTE));

	return (hash != NULL) ? 1 : 0;
}

void free_hash(void)
{
	if (hash) free(hash);
}

void make_hash(void)
{
	// makes hash for every byte of input file
	// for position i, hash is from bytes at[i-2], [i-1], [i]
	// first and second bytes does not have valid hash values

	UBYTE curr, prev, prev2;
	ULONG i;

	prev = curr = 0;

	for (i = 0; i < inlen; i++)
	{
		prev2 = (UBYTE)((prev >> 1) | (prev << 7));
		prev = (UBYTE)((curr >> 1) | (curr << 7));
		curr = indata[i];

		hash[i] = (UBYTE)(prev2 ^ prev ^ curr);
	}
}

ULONG emit_init(void)
{
	// inits output stream emitter
#ifdef DEBUG
	ULONG i;

	for(i = 0; i < OUBUFSIZE; i++)
		oubuf[i] = 0;
#endif

	oubuf[0] = indata[0];	// copy first byte 'as-is'
	ob_bitpos = 1;
	ob_freepos = 2;
	ob_bits = 0;

	return 1;
}

void emit_bits(ULONG bits, ULONG bitsnum)
{
	ULONG i, shifter;

	shifter = bits;

	for (i = 0; i < bitsnum; i++)
	{
		if (ob_bits == 8)
		{
			ob_bitpos = ob_freepos;
			ob_freepos++;
			ob_bits = 0;
		}

		oubuf[ob_bitpos] = (UBYTE)((oubuf[ob_bitpos] << 1) | (UBYTE)(shifter >> 31));

		ob_bits++;

		shifter <<= 1;
	}
}


ULONG emit_code(struct lzinfo *lz)
{
	// emits given lz code to the output stream

	// write bits
	emit_bits(lz->bits, lz->bitsnum);

	// write byte, if any
	if (lz->byte != 0xFFFFFFFF)
	{
		oubuf[ob_freepos++] = (UBYTE)(lz->byte & 0x000000FF);
	}

	// flush to disk if needed
	if (ob_freepos > (OUBUFSIZE - 8))
	{
		// write up to current bit position
		memcpy(outdata + datapos, oubuf, ob_bitpos);
		datapos += ob_bitpos;

		// move remaining part of buffer to the beginning
		memcpy(oubuf, &oubuf[ob_bitpos], OUBUFSIZE - ob_bitpos);

		// update pointers
		ob_freepos -= ob_bitpos;
		ob_bitpos = 0;
	}

	return 1;
}

ULONG emit_finish(void)
{
	// finishes output stream outting (flushes everyth, writes stopping bitcode)

	// write stopping bitcode
	emit_bits(0x60100000, 12);

	// fill up last unfinished 'bit'-byte
	while (ob_bits < 8)
	{
		oubuf[ob_bitpos] <<= 1;
		ob_bits++;
	}
	
	// write remaining part of buffer
	memcpy(outdata + datapos, oubuf, ob_freepos);
	datapos += ob_freepos;

	return 1;
}

//----------------------------------------------------------------

void init_twobyters(void)
{
	// init pointers tb_free, tb_bunches and tb_entry array

	ULONG i;

	tb_free = NULL;	// init linked list of free tb_chain elements

	tb_bunches = NULL;	// no bunches already allocated

	for (i = 0; i < 0x10000; i++)	// init array of 2-byte match pointers
	{
		tb_entry[i] = NULL;
	}
}

//----------------------------------------------------------------

void free_twobyters(void)
{
	// free all twobyter-related memory

	struct tb_bunch * tbtmp;

	while (tb_bunches)
	{
		tbtmp = tb_bunches;
		tb_bunches = tb_bunches->next;
		free(tbtmp);
	}
}

//----------------------------------------------------------------

struct tb_chain* get_free_twobyter(void)
{
	// gets from tb_free new free twobyter or return NULL if no free twobyters
	struct tb_chain * newtb;

	if (tb_free)
	{
		newtb = tb_free;
		tb_free = tb_free->next;

		return newtb;
	}
	else
	{
		return NULL;
	}
}

//----------------------------------------------------------------

void cutoff_twobyte_chain(ULONG index, ULONG curpos)
{
	// cuts off given chain so that there are no older than 4352 bytes back elements

	struct tb_chain *curr, *prev;

	curr = tb_entry[index];
	if (!curr) return;

	// see if we should delete elements after first element in the given chain
	prev = curr;
	curr = curr->next;

	while (curr)
	{
		if ((curpos - (curr->pos)) > 4352)
		{
			prev->next = curr->next;	// remove from chain

			curr->next = tb_free;	// insert into free chain
			tb_free = curr;

			curr = prev->next;	// step to the next element in chain
		}
		else
		{
			prev = curr;
			curr = curr->next;
		}
	}

	// delete first element in chain if needed
	curr = tb_entry[index];
	if ((curpos - (curr->pos)) > 4352)
	{
		tb_entry[index] = curr->next;
		curr->next = tb_free;
		tb_free = curr;
	}
}

//----------------------------------------------------------------

ULONG add_bunch_of_twobyters(void)
{
	// adds a bunch of twobyters to the free list

	ULONG i;
	struct tb_bunch * newbunch;

	// alloc new bunch
	newbunch = (struct tb_bunch *) malloc(sizeof(struct tb_bunch));
	if (newbunch == NULL) return 0;

	// link every twobyter into one list
	for (i = 0; i < (BUNCHSIZE - 1); i++)
	{
		newbunch->bunch[i].next = &(newbunch->bunch[i + 1]);
	}

	// add this list to the free list
	newbunch->bunch[BUNCHSIZE - 1].next = tb_free;
	tb_free = &(newbunch->bunch[0]);

	// add bunch to bunches list
	newbunch->next = tb_bunches;
	tb_bunches = newbunch;

	return 1;
}

//----------------------------------------------------------------

ULONG add_twobyter(UBYTE last, UBYTE curr, ULONG curpos)
{
	// adds new twobyter to the array of chains

	struct tb_chain * newtb;
	ULONG index;

	index = (((ULONG) last) << 8) + ((ULONG) curr);

	newtb = get_free_twobyter();
	if (!newtb)
	{
		// no free elements

		// first try to flush current chain
		cutoff_twobyte_chain(index, curpos);

		newtb = get_free_twobyter();
		if (!newtb)
		{
			// nothing free - allocate new bunch
			if (!add_bunch_of_twobyters())
			{
				return 0;
			}

			newtb = get_free_twobyter();
		}
	}

	newtb->next = tb_entry[index];
	tb_entry[index] = newtb;

	newtb->pos = curpos - 1;	// points to the first byte of given two bytes

	return 1;
}

//----------------------------------------------------------------

void start_lz(void)
{
	// re-init codes[] array for next byte
	codepos = 0;
}

void end_lz(void)
{
	// ends codes[] array filling by setting stopflag
	codes[codepos].disp = 0;
	codes[codepos].length = 1;

	if (codepos >= 256) printf("end_lz(): overflow!\n");
}

void add_lz(LONG disp, ULONG length)
{
	// adds LZ code to the codes[] array
	// position - from which point LZ code starts in input file, (position-curpos) - back displacement

	codes[codepos].length = (UWORD) length;
	codes[codepos].disp = (WORD) disp;
	codepos++;
}

ULONG make_lz_info(UBYTE curbyte, struct lzcode *lzcode, struct lzinfo *lzinfo)
{
	// on the given lzcode element, returns total bit length of this code,
	// if given ptr to lzinfo, writes there precise info over lz code
	// if lzcode==NULL or lzcode->length==0 - treated as OUTBYTE type code

	// returns 0 for wrong lz length/displacement combination, does not fill lzinfo

	// curbyte needed only for OUTBYTE lz code, can be given any value otherwise

	ULONG type, length, add_length, len, bits;
	ULONG ptrn;

	// first determine bit length of resulting LZ code
	length = 0;
	if (lzcode == NULL)
	{
		length = 9;	// OUTBYTE
		type = OUTBYTE;
	}
	else if (lzcode->disp == 0)
	{
		length = 9;
		type = OUTBYTE;
	}
	else
	{
		if (lzcode->length == 1)
		{
			if ((lzcode->disp >= (-8)) && (lzcode->disp <= (-1)))
			{
				length = 6;	// LEN1
				type = LEN1;
			}
		}
		else if (lzcode->length == 2)
		{
			if ((lzcode->disp >= (-256)) && (lzcode->disp <= (-1)))
			{
				length = 11;	// LEN2
				type = LEN2;
			}
		}
		else if (lzcode->length == 3)
		{
			if ((lzcode->disp >= (-256)) && (lzcode->disp <= (-1)))
			{
				length = 12;	// LEN3_SHORT
				type = LEN3_SHORT;
			}
			else if ((lzcode->disp >= (-4352)) && (lzcode->disp <= (-257)))
			{
				length = 16;	// LEN3_LONG
				type = LEN3_LONG;
			}
		}
		else if (lzcode->length <= 255)
		{
			if ((lzcode->disp >= (-4352)) && (lzcode->disp <= (-1)))
			{
				if ((lzcode->disp >= (-256)))
				{
					add_length = 12;	// VARLEN_SHORT
					type = VARLEN_SHORT;
				}
				else
				{
					add_length = 16;	// VARLEN_LONG
					type = VARLEN_LONG;
				}

				// calc bits for size
				len = (lzcode->length - 2) >> 1;

				bits = 0;
				while (len)
				{
					bits++;
					len >>= 1;
				}

				length = bits + bits + add_length;	// final length
			}
		}
	}

	// if length==0, error in code!
	if (length == 0) return 0;	// error flag

	// see if we need to fill struct lzinfo
	if (lzinfo)
	{

		// fill already known values
		lzinfo->type = type;

		if (lzcode)
		{
			lzinfo->disp = lzcode->disp;
			lzinfo->length = lzcode->length;
		}
		else
		{
			lzinfo->disp = 0;	// only for OUTBYTE
			lzinfo->length = 1;
		}

		lzinfo->bitsize = length;

		// fill remaining values
		switch (type)
		{
			case OUTBYTE:
				lzinfo->bits = 0x80000000;
				lzinfo->bitsnum = 1;	// one '1' bit

				lzinfo->byte = curbyte;
				break;

			case LEN1:
				lzinfo->bits = ((((ULONG) lzinfo->disp) & 7) << 26);
				lzinfo->bitsnum = 6;

				lzinfo->byte = 0xFFFFFFFF;	// no byte
				break;

			case LEN2:
				lzinfo->bits = 0x20000000;
				lzinfo->bitsnum = 3;

				lzinfo->byte = ((ULONG) lzinfo->disp) & 0x00FF;
				break;

			case LEN3_SHORT:
				lzinfo->bits = 0x40000000;
				lzinfo->bitsnum = 4;

				lzinfo->byte = ((ULONG) lzinfo->disp) & 0x00FF;
				break;

			case LEN3_LONG:
				lzinfo->bits = 0x50000000 | (((((ULONG) lzinfo->disp) + 0x0100) & 0x00000F00) << 16);
				lzinfo->bitsnum = 8;

				lzinfo->byte = ((ULONG) lzinfo->disp) & 0x00FF;
				break;

			case VARLEN_SHORT:
				ptrn = 0xFFFFFFFF >> (32 - bits);
				ptrn = (ptrn &(lzinfo->length - (1 << bits) - 2));
				ptrn = ptrn | (1 << bits);
				ptrn <<= 1;
				ptrn = 0x60000000 | (ptrn << (28 - bits - bits));

				lzinfo->bits = ptrn;
				lzinfo->bitsnum = length - 8;

				lzinfo->byte = ((ULONG) lzinfo->disp) & 0x00FF;
				break;

			case VARLEN_LONG:
				ptrn = 0xFFFFFFFF >> (32 - bits);
				ptrn = (ptrn &(lzinfo->length - (1 << bits) - 2));
				ptrn = ptrn | (1 << bits);
				ptrn = (ptrn << 1) | 1;
				ptrn = (ptrn << 4) | (((((ULONG) lzinfo->disp) + 0x0100) & 0x00000F00) >> 8);
				ptrn = 0x60000000 | (ptrn << (24 - bits - bits));

				lzinfo->bits = ptrn;
				lzinfo->bitsnum = length - 8;

				lzinfo->byte = ((ULONG) lzinfo->disp) & 0x00FF;
				break;
		}
	}

	return length;
}

ULONG stuff_init(void)
{
	//allocate/init different things for packing

	ULONG success;

	success = 1;

	init_twobyters();

	if (!init_hash()) success = 0;

	// allocate packinfo data for input file
	pdata = (struct packinfo *) malloc((inlen + 1) *sizeof(struct packinfo));
	if (!pdata)
	{
		success = 0;
	}

	return success;
}

void make_LZ_codes(ULONG curpos)
{
	// actual generation of all LZ codes for the given byte @curpos
	// result in codes[256] array (see MegaLZ_lz.h, MegaLZ_lz.c)

	UBYTE curbyte, nextbyte;
	ULONG i;
	ULONG tbi;
	ULONG was_twobyter, last_match;
	ULONG lzlen;

	struct tb_chain * curtb;

	tbi = 0;
	curtb = NULL;

	// init lz codes collecting
	start_lz();

	// find 1-byte code

	curbyte = indata[curpos];

	i = (curpos > 8) ? (curpos - 8) : 0;
	while (i < curpos)
	{
		if (indata[i] == curbyte)
		{
			add_lz((LONG) i - (LONG) curpos, 1);
			break;
		}

		i++;
	}

	// find 2-byte code

	was_twobyter = 0;

	if ((inlen - 1) > curpos)	// if we have enough bytes ahead
	{
		nextbyte = indata[curpos + 1];
		tbi = (((ULONG) curbyte) << 8) + ((ULONG) nextbyte);

		curtb = tb_entry[tbi];	// get twobyter

		if (curtb)
		{
			if ((curpos - curtb->pos) <= 256)
			{
				add_lz((LONG) curtb->pos - (LONG) curpos, 2);
			}

			if ((curpos - curtb->pos) <= 4352)
			{
				was_twobyter = 1;	// there is something for 3 and more bytes finding
			}
			else
			{
				cutoff_twobyte_chain(tbi, curpos);	// flush older twobyters
			}
		}
	}

	// find other codes (len=3..255)

	if (was_twobyter && ((inlen - 2) > curpos))	// was twobyter within -1..-4352 range
	// and at least 3-byter possible
	{
		// some inits

		//curtb=tb_entry[tbi];	// already done in 2-byter search

		last_match = 1;	// was last match (for 2 bytes)

		// loop for all possible lengths
		for (lzlen = 3;
			(lzlen <= 255) && ((inlen - lzlen + 1) > curpos); /*NOTHING!*/)
		{
			if (last_match)
			{
				// there were last match (lzlen-1)

				// compare last bytes of current match
				if (indata[curpos + lzlen - 1] == indata[curtb->pos + lzlen - 1])
				{
					// current match OK
					add_lz((LONG) curtb->pos - (LONG) curpos, lzlen);
					lzlen++;
				}
				else
				{
					// current match failed: - take next (older) twobyter
					MATCH_FAILED: curtb = curtb->next;
					if (!curtb) break;	// stop search if no more twobyters
					if ((curpos - curtb->pos) > 4352)
					{
						// if twobyters out of range
						cutoff_twobyte_chain(tbi, curpos);
						break;
					}
					last_match = 0;	// indicate match fail for next step
				}
			}
			else
			{
				// last match failed so compare all strings

				// first compare hashes of the ends of both strings
				if (hash[curpos + lzlen - 1] == hash[curtb->pos + lzlen - 1])
				{
				 		// if hashes are the same, compare complete strings
					if (!memcmp(&indata[curpos], &indata[curtb->pos], lzlen))
					{
						//match OK
						last_match = 1;
						add_lz((LONG) curtb->pos - (LONG) curpos, lzlen);
						lzlen++;
					}
					else goto MATCH_FAILED;
				}
				else goto MATCH_FAILED;
			}
		}
	}

	// stop collecting codes
	end_lz();
}

void update_price(ULONG curpos, ULONG bitsize, struct lzcode *lz)
{
	// update price for given data
	ULONG curprice;
	ULONG newpos;

	if (lz)
	{
		newpos = curpos + lz->length;
	}
	else
	{
		newpos = curpos + 1;
	}

	curprice = pdata[newpos].price;

	if (curprice > bitsize + pdata[curpos].price)
	{
		pdata[newpos].price = bitsize + pdata[curpos].price;

		if (lz)
		{
			pdata[newpos].best = *lz;
		}
		else
		{
			pdata[newpos].best.length = 1;
			pdata[newpos].best.disp = 0;
		}
	}
}

ULONG gen_output(void)
{
	ULONG pos;
	struct lzinfo lz;
	
	if (!emit_init())
	{
		printf("gen_output(): emit_init() failed!\n");
		return 0;
	}

	pos = 1;	// first byte copied to output 'as-is', without LZ coding
	
	while (pos < inlen)
	{
		// write best code to the output stream
		if (!make_lz_info(indata[pos], &pdata[pos].best, &lz))
		{
			printf("gen_output(): make_lz_info() failed!\n");
			return 0;
		}

		if (!emit_code(&lz))
		{
			printf("gen_output(): emit_code() failed!\n");
			return 0;
		}
		pos += lz.length;	// step to the next unpacked byte
	}
	
	// sanity check
	if (pos > inlen)
	{
		printf("gen_output(): last position is out of input file!\n");
		return 0;
	}
	
	// just finish all deals
	if (!emit_finish())
	{
		printf("gen_output(): emit_finish() failed!\n");
		return 0;
	}
	
	return 1;
}

void stuff_free(void)
{
	//free different things used during packing

	free_twobyters();

	free_hash();

	if (pdata)
	{
		free(pdata);
	}
}

ULONG pack(void)
{
	// return value
	ULONG retcode = 1;

	ULONG current_pos;	// current position in file

	UBYTE curr_byte, last_byte;	// current byte and last byte (for twobyter)

	struct lzcode curr, tmp;

	ULONG i, bitlen;

	ULONG skip;
	ULONG bestcode;
	LONG curgain, bestgain;

	// allocate mem for arrays/tables/etc.

	if (stuff_init())
	{
		// make hash
		make_hash();
		
		// very first inits
		pdata[0].best.disp = 0;
		pdata[0].best.length = 1;	// 1st byte copied 'as-is'

		curr_byte = indata[0];

		if (mode == PACKMODE_OPTIMAL)
		{
			// !!optimal coding!!

			// initialize prices
			pdata[0].price = 0;
			pdata[1].price = 8;
			for (i = 2; i <= inlen; i++)
			{
				pdata[i].price = 0xffffffff;
			}

			// do every byte
			for (current_pos = 1; current_pos < inlen; current_pos++)
			{
				last_byte = curr_byte;
				curr_byte = indata[current_pos];

				// add current twobyter
				if (!add_twobyter(last_byte, curr_byte, current_pos))
				{
					printf("pack(): add_twobyter() failed!\n");
					retcode = 0;
					goto ERROR;
				}

				// find all LZ codes for current byte
				make_LZ_codes(current_pos);

				// update prices
				i = 0;
				do { 	bitlen = make_lz_info(0x00, &codes[i], NULL);
					if (!bitlen)
					{
						printf("pack(): make_lz_info() failed!\n");
						retcode = 0;
						goto ERROR;
					}
					update_price(current_pos, bitlen, &codes[i]);
				} while (codes[i++].disp);
			}

			// reverse optimal chain
			i = inlen;
			tmp = pdata[i].best;
			while (i > 1)
			{
				curr = tmp;
				i = i - curr.length;
				tmp = pdata[i].best;
				pdata[i].best = curr;
			}
		}
		else if (mode == PACKMODE_GREEDY)
		{
			// greedy coding

			// init chain
			for (i = 0; i < inlen; i++)
			{
				pdata[i].best.disp = 0;
				pdata[i].best.length = 1;
			}

			skip = 0;
			for (current_pos = 1; current_pos < inlen; current_pos++)
			{
				last_byte = curr_byte;
				curr_byte = indata[current_pos];

				// add current twobyter
				if (!add_twobyter(last_byte, curr_byte, current_pos))
				{
					printf("pack(): add_twobyter() failed!\n");
					retcode = 0;
					goto ERROR;
				}

				if (skip)
				{
					skip--;
				}
				else
				{
				 		// find all LZ codes for current byte
					make_LZ_codes(current_pos);

					// find 'best' code on greedy basis
					bestgain = -2;
					bestcode = i = 0;
					do { 	bitlen = make_lz_info(0x00, &codes[i], NULL);
						if (!bitlen)
						{
							printf("pack(): make_lz_info() failed!\n");
							retcode = 0;
							goto ERROR;
						}

						curgain = (LONG)(8 *codes[i].length) - bitlen;

						if (curgain > bestgain)
						{
							bestgain = curgain;
							bestcode = i;
						}
					} while (codes[i++].disp);

					// write found code, skip next (length-1) bytes
					pdata[current_pos].best = codes[bestcode];
					skip = codes[bestcode].length - 1;
				}
			}
		}

		// generate output file
		if (!gen_output())
		{
			printf("pack(): gen_output() failed!\n");
			retcode = 0;
		}
	}
	else
	{
		printf("pack(): stuff_init() failed!\n");
		retcode = 0;
	}

	ERROR:
		stuff_free();

	return retcode;
}

// callbacks for depacker() below
ULONG get_byte(void)
{
	// gets byte from input stream

	return (inpos < inlen) ? (ULONG) indata[inpos++] : 0xFFFFFFFF;
}

ULONG put_buffer(ULONG pos, ULONG size)
{
	// writes specified part of buffer to the output file (with wrapping!)
	ULONG size1, size2; //, success;

	if ((pos + size) <= DBSIZE)
	{
		// no wrapping needed
		memcpy(outdata + datapos, &dbuf[pos], size);
		datapos += size;
		return 1;
	}
	else
	{
		// wrapping
		size1 = (DBSIZE - pos);
		size2 = size - size1;

		memcpy(outdata + datapos, &dbuf[pos], size1);
		datapos += size1;
		memcpy(outdata + datapos, &dbuf[0], size2);
		datapos += size2;
		return 1;
	}
}

ULONG get_bits(ULONG numbits)
{
	// gets specified number of bits from bitstream
	// returns them LSB-aligned
	// if error (get_byte() fails) return 0xFFFFFFFF, so function can't get more than 31 bits!
	// if 0 bits required, returns 0

	ULONG count;
	ULONG bits;
	ULONG input;

	bits = 0;
	count = numbits;

	while (count--)
	{
		if (bitcount--)
		{
			bits <<= 1;
			bits |= 0x00000001 & (bitstream >> 7);
			bitstream <<= 1;
		}
		else
		{
			bitcount = 8;
			input = get_byte();
#ifdef ERRORS
			if (input == 0xFFFFFFFF) return 0xFFFFFFFF;
#endif
			bitstream = (UBYTE) input;

			count++;	// repeat loop once more
		}
	}

	return bits;
}

void repeat(LONG disp, ULONG len)
{
	// repeat len bytes with disp displacement (negative)
	// uses dbpos &dbuf

	ULONG i;

	for (i = 0; i < len; i++)
	{
		dbuf[DBMASK & dbpos] = dbuf[DBMASK &(dbpos + disp)];
		dbpos++;
	}
}

LONG get_bigdisp(void)
{
	// fetches 'big' displacement (-1..-4352)
	// returns negative displacement or ZERO if failed either get_bits() or get_byte()

	ULONG bits, byte;

	bits = get_bits(1);
#ifdef ERRORS
	if (bits == 0xFFFFFFFF) return 0;
#endif

	if (bits)
	{
		// longer displacement
		bits = get_bits(4);
#ifdef ERRORS
		if (bits == 0xFFFFFFFF) return 0;
#endif
		byte = get_byte();
#ifdef ERRORS
		if (byte == 0xFFFFFFFF) return 0;
#endif
		return (LONG)(((0xFFFFF000 | (bits << 8)) - 0x0100) | byte);
	}
	else
	{
		// shorter displacement
		byte = get_byte();
#ifdef ERRORS
		if (byte == 0xFFFFFFFF) return 0;
#endif
		return (LONG)(0xFFFFFF00 | byte);
	}
}

ULONG depacker(void)
{
	ULONG i;
	
	ULONG dbflush; // position from which to flush buffer

	ULONG byte;
	ULONG bits;

	LONG disp;

	ULONG finished;

	// init depack buffer with zeros
	for (i = 0; i < DBSIZE; i++) dbuf[i] = 0;

	dbpos = 0;
	dbflush = 0;

	// get first byte of packed file and write to output
	byte = get_byte();
#ifdef ERRORS
	if (byte == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
	dbuf[dbpos++] = (UBYTE) byte;

	// second byte goes to bitstream
	byte = get_byte();
#ifdef ERRORS
	if (byte == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
	bitstream = (UBYTE) byte;
	bitcount = 8;

	// actual depacking loop!
	finished = 0;
	do {
		// get 1st bit - either OUTBYTE (see MegaLZ_lz.h) or beginning of LZ code
		bits = get_bits(1);
#ifdef ERRORS
		if (bits == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif

		if (bits)
		{
			// OUTBYTE
			byte = get_byte();
#ifdef ERRORS
			if (byte == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
			dbuf[(dbpos++) &DBMASK] = (UBYTE) byte;
		}
		else
		{
			// LZ code
			bits = get_bits(2);
#ifdef ERRORS
			if (bits == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif

			switch (bits)
			{
				case 0:	// 000
					bits = get_bits(3);
#ifdef ERRORS
					if (bits == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
					repeat((LONG)(0xFFFFFFF8 | bits), 1);
					break;
				case 1:	// 001
					byte = get_byte();
#ifdef ERRORS
					if (byte == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
					repeat((LONG)(0xFFFFFF00 | byte), 2);
					break;
				case 2:	// 010
					disp = get_bigdisp();
#ifdef ERRORS
					if (!disp) return DEPACKER_NOEOF;
#endif
					repeat(disp, 3);
					break;
				case 3:	// 011
					// extract num of length bits
					i = 0;
					do { 	bits = get_bits(1);
#ifdef ERRORS
						if (bits == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
						i++;
					} while (!bits);

					// check for exit code
					if (i == 9)
					{
						finished = 1;
					}
					else if (i <= 7)
					{
					 			// get length bits itself
						bits = get_bits(i);
#ifdef ERRORS
						if (bits == 0xFFFFFFFF) return DEPACKER_NOEOF;
#endif
						disp = get_bigdisp();
#ifdef ERRORS
						if (!disp) return DEPACKER_NOEOF;
#endif
						repeat(disp, 2 + (1 << i) + bits);
					}
#ifdef ERRORS
					else
					{
						return DEPACKER_ERRCODE;
					}
#endif
					break;
#ifdef ERRORS
				default:
					return DEPACKER_ERRCODE;	// although this should NEVER happen!
#endif
			}
		}
		
		// check if we need flushing buffer
		if ((((dbpos - dbflush) & DBMASK) > (DBSIZE - 257)) || finished)
		{
#ifdef ERRORS
			if (!put_buffer(dbflush & DBMASK, (dbpos - dbflush) & DBMASK))
			{
				return DEPACKER_CANTWRITE;
			}
#else
				put_buffer(dbflush &DBMASK, (dbpos - dbflush) & DBMASK);
#endif
			dbflush = dbpos;
		}
	} while (!finished);

#ifdef ERRORS
	return (get_byte() == 0xFFFFFFFF) ? DEPACKER_OK : DEPACKER_TOOLONG;
#else
	return DEPACKER_OK;
#endif
}

ULONG depack(void)
{
	// main depacking function

	// some inits
	inpos = 0;

	return (depacker() == DEPACKER_OK) ? 1 : 0;
}

unsigned char *megalz_compress(unsigned char *input_data, size_t input_size, ULONG pack_mode, size_t *output_size)
{
	datapos = 0;
	indata = input_data;
	inlen = input_size;
	mode = pack_mode;
	outdata = malloc(input_size);
	
	pack();
	
	*output_size = datapos;
	
	return outdata;
}

void megalz_decompress(unsigned char *input_data, unsigned char *output_data)
{
	datapos = 0;
	indata = input_data;
	outdata = output_data;
	
	depack();
}
