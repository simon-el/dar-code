/*********************************************************************/
// dar - disk archive - a backup/restoration program
// Copyright (C) 2002 Denis Corbin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// to contact the author : dar.linux@free.fr
/*********************************************************************/
// $Id: infinint.cpp,v 1.17 2002/03/18 11:00:54 denis Rel $
//
/*********************************************************************/

#include <unistd.h>
#include "infinint.hpp"
#include "erreurs.hpp"
#include "generic_file.hpp"

typedef unsigned char bitfield[8];

static void swap_bytes(unsigned char &a, unsigned char &b) throw();
static void swap_bytes(unsigned char *a, unsigned int size) throw();
static void expand_byte(unsigned char a, bitfield &bit) throw();
static void contract_byte(const bitfield &b, unsigned char & a) throw(Erange);

infinint::endian infinint::used_endian = not_initialized;

void infinint::read_from_file(int fd) throw(Erange, Ememory, Ebug)
{
    fichier f = dup(fd);
    read_from_file(f);
}

void infinint::dump(int fd) const throw(Einfinint, Ememory, Erange, Ebug)
{
    fichier f = dup(fd);
    dump(f);
}

void infinint::read_from_file(generic_file & x) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    unsigned char a;
    bool fin = false, known_size = false;
    infinint skip = 0;
    infinint size = 0;
    storage::iterator it;
    int lu;
    
    while(!fin)
    {
	lu = x.read((char *)&a, 1);

	if(lu <= 0)
	{
	    if(known_size)
	    {
		delete field;
		field = NULL;
	    }
	    throw Erange("infinint::read_from_file(generic_file)", "reached end of file before all data could be read");
	}
	
	if(! known_size)
	    if(a == 0)
		skip++;
	    else
	    {
		bitfield bf;
		unsigned int pos = 0;

		expand_byte(a, bf);
		for(int i = 0; i < 8; i++)
		    pos = pos + bf[i];
		if(pos != 1)
		    throw Erange("infinint::read_from_file(generic_file)", "badly formed infinint or not supported format"); // more than 1 bit is set to 1

		pos = 0;
		while(bf[pos] == 0)
		    pos++;
		pos += 1; // bf starts at zero, or bit zero means 1 TG of length

		size = (skip * 8 + pos)*TG;
		detruit();
		field = new storage(size);

		if(field != NULL)
		{		
		    it = field->begin();
		    known_size = true;
		}
		else
		    throw Ememory("infinint::read_from_file(generic_file)");
	    }
	else /* known_size */
	{
	    *(it++) = a;
	    if(it == field->end())
		fin = true;
	}
    }
    reduce();
    E_END("infinint::read_from_file", "generic_file");
}

 
void infinint::dump(generic_file & x) const throw(Einfinint, Ememory, Erange, Ebug)
{
    E_BEGIN;
    int ecrit;
    storage::iterator it;
    infinint width;
    unsigned int pos;
    unsigned char last_width;
    unsigned int justification;
    
    if(! is_valid())
	throw SRC_BUG;

    width = field->size();    // this is the informational field size in byte
    justification = width % TG; // which may not be a multiple of TG bytes
    width /= TG;              // this is the width en TG, thus the number of bit that must have the preamble
    if(justification != 0)    // in case we need to add some bytes to have a width multiple of TG
	width++;              // we need then one more group to have a width multiple of TG

    pos = (width-1) % 8;      // this is the position of the bit set to 1 in the last byte of the preamble
    last_width = 0x80 >> pos; // here we generate the last byte of the preamble
    width /= 8;               // width is now the number of byte that must have the preamble
    if(pos == 7)              // this is when the preambule length is not a multiple of byte
	width--;              // as the last byte is added separately we reduce the count by one
	// in the other cases, the division makes some rest and adding the last byte separately makes the correct count
    

	// now we write the preamble except the last byte. All theses are zeros.

    pos = 0;
    unsigned char u = 0x00;
    width.unstack(pos);
    do
    {
	while(pos-- > 0)
	    if(x.write((char *)(&u), 1) < 1)
		throw Erange("infinint::dump(generic_file)", "can't write data to file");
	pos = 0;
	width.unstack(pos);
    }
    while(pos > 0);

	// now we write the last byte of the preambule, which as only one bit set

    if(x.write((char *)&last_width, 1) < 1)
	throw Erange("infinint::dump(generic_file)", "can't write data to file");

	// we need now to write some justification byte to have an informational field multiple of TG

    if(justification != 0)
    {
	justification = TG - justification;
	while(justification-- > 0)
	    if(x.write((char *)(&u), 1) < 1)
		throw Erange("infinint::dump(generic_file)", "can't write data to file");
    }

	// now we continue dumping the informational bytes :

    it = field->begin();    
    while(it != field->end())
    {
	ecrit = x.write((char *)(&(*(it++))), 1);
	if(ecrit <= 0)
	    throw Erange("infinint::dump(generic_file)", "can't write data to file");
    }
    E_END("infinint::dump", "generic_file");
}

infinint & infinint::operator += (const infinint & arg) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    if(! is_valid() || ! arg.is_valid())
	throw SRC_BUG;

	// enlarge field to be able to receive the result of the operation
    make_at_least_as_wider_as(arg);

	// now processing the operation

    storage::iterator it_a = arg.field->rbegin();
    storage::iterator it_res = field->rbegin();
    unsigned int retenue = 0, somme;

    while(it_res != field->rend())
    {
	somme = *it_res;
	if(it_a != arg.field->rend())
	    somme += *(it_a--);
	somme += retenue;

	retenue = 0;

	while(somme >= 256)
	{
	    retenue++;
	    somme -= 256;
	}

	*(it_res--) = somme;
    }

    if(retenue != 0)
    {
	field->insert_null_bytes_at_iterator(field->begin(), 1); 
	(*field)[0] = retenue;
    }

    reduce();

    return *this;
    E_END("infinint::operator +=", "");
}

infinint & infinint::operator -= (const infinint & arg) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    if(! is_valid() || ! arg.is_valid())
	throw SRC_BUG;

    if(*this < arg)
	throw Erange("infinint::operator", "substracting a infinint greater than the first, infinint can't be negative");

	// now processing the operation

    storage::iterator it_a = arg.field->rbegin();
    storage::iterator it_res = field->rbegin();
    unsigned int retenue = 0;
    signed int somme;

    while(it_res != field->rend())
    {
	somme = *it_res;
	if(it_a != arg.field->rend())
	    somme -= *(it_a--);
	somme -= retenue;
	retenue = 0;

	while(somme < 0)
	{
	    somme += 256;
	    retenue++;
	}
	
	*(it_res--) = somme;
    }

    reduce();
    
    return *this;
    E_END("infinint::operator -=", "");
}

infinint & infinint::operator *= (unsigned char arg) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    if(!is_valid())
	throw SRC_BUG;

    storage::iterator it = field->rbegin();
    unsigned int produit, retenue = 0;

    while(it != field->rend())
    {
	produit = (*it) * arg + retenue;
	retenue = 0;
	while(produit >= 256)
	{
	    retenue++;
	    produit -= 256;
	}
	*(it--) = produit;
    }

    if(retenue != 0)
    {
	field->insert_null_bytes_at_iterator(field->begin(), 1);
	(*field)[0] = retenue;
    }

    reduce();
    
    return *this;
    E_END("infinint::operator *=", "unsigned char");
}

infinint & infinint::operator *= (const infinint & arg) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    infinint ret = 0;

    if(!is_valid() || !arg.is_valid())
	throw SRC_BUG;

    storage::iterator it_t = field->begin();

    while(it_t != field->end())
    {
	ret <<= 8; // shift by one byte;
	ret += arg * (*(it_t++));
    } 

    ret.reduce();
    *this = ret;

    return *this;
    E_END("infinint::operator *=", "infinint");
}

infinint & infinint::operator >>= (unsigned long bit) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    if(! is_valid())
	throw SRC_BUG;

    unsigned long byte = bit/8;
    storage::iterator it = field->rbegin() - byte + 1;
    bitfield bf;
    unsigned char mask, r1 = 0, r2 = 0;
    int shift_retenue;

    bit = bit % 8;
    shift_retenue = 8 - bit;
    if(byte >= field->size())
	*this = 0;
    else
    {
	field->remove_bytes_at_iterator(it, byte);
    
	if(bit != 0)
	{
	    for(register unsigned int i = 0; i < 8; i++)
		bf[i] = i < 8 - bit ? 0 : 1;
	    contract_byte(bf, mask);
	    
	    it = field->begin();
	    while(it != field->end())
	    {
		r1 = *it & mask;
		r1 <<= shift_retenue;
		*it >>= bit;
		*it |= r2;
		r2 = r1;
		it++;
	    }
	    reduce();
	}
    }

    return *this;
    E_END("infinint::operator >>=", "unsigned long");
}

infinint & infinint::operator >>= (infinint bit) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    if(! is_valid() || ! bit.is_valid())
	throw SRC_BUG;

    unsigned long int delta_bit = 0;
    bit.unstack(delta_bit);

    do 
    {
	*this >>= delta_bit;
	delta_bit = 0;
	bit.unstack(delta_bit);
    }
    while(delta_bit > 0);

    return *this;
    E_END("infinint::operator >>=", "infinint");
}

infinint & infinint::operator <<= (unsigned long bit) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    if(! is_valid())
	throw SRC_BUG;

    unsigned long byte = bit/8;
    storage::iterator it = field->end();
    unsigned int shift_retenue, r1 = 0, r2 = 0;
    bitfield bf;
    unsigned char mask;

    if(*this == 0)
	return *this;

    bit = bit % 8;     // bit gives now the remaining translation after the "byte" translation

    if(bit != 0)
	byte++;        // to prevent the MSB to be lost (in "out of space" ;-) )

	// this is the "byte" translation
    field->insert_null_bytes_at_iterator(it, byte);

    if(bit != 0)
    {
	    // and now the bit translation 
	shift_retenue = 8 - bit;
	it = field->rbegin();
	
	    // the mask for selecting the retenue
	for(register int unsigned i = 0; i < 8; i++)
	    bf[i] = i < bit ? 1 : 0;
	contract_byte(bf, mask);
	
	while(it != field->rend())
	{
	    r1 = (*it) & mask;
	    r1 >>= shift_retenue;
	    *it <<= bit;
	    *it |= r2;
	    r2 = r1;
	    it--;
	}
	reduce();
    }

    return *this;
    E_END("infinint::operator <<=", "unsigned long");
}

infinint & infinint::operator <<= (infinint bit) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    unsigned long int delta_bit = 0;
    bit.unstack(delta_bit);

    do 
    {
	*this <<= delta_bit;
	delta_bit = 0;
	bit.unstack(delta_bit);
    }
    while(delta_bit > 0);

    return *this;
    E_END("infinint::operator <<=", "infinint");
}

template <class T> T infinint::modulo(T arg) const throw(Einfinint, Ememory, Erange, Ebug)
{
    E_BEGIN;
    infinint tmp = *this % infinint(arg);
    T ret = 0;
    unsigned char *debut = (unsigned char *)(&ret);
    unsigned char *ptr = debut + sizeof(T) - 1;
    storage::iterator it = tmp.field->rbegin();

    while(it != tmp.field->rend() && ptr >= debut)
	*(ptr--) = *(it--);

    if(it != tmp.field->rend())
	throw SRC_BUG; // could not put all the data in the returned value !

    if(used_endian == big_endian)
	swap_bytes(debut, sizeof(T));
    
    return ret;   
    E_END("infinint::modulo", "");
}

signed int infinint::difference(const infinint & b) const throw(Erange, Ebug)
{
    E_BEGIN;
    storage::iterator ita;
    storage::iterator itb;
    const infinint & a = *this;

    if(! a.is_valid() || ! b.is_valid())
	throw SRC_BUG;

    if(*a.field < *b.field) // field is shorter for this than for ref and object are always "reduced" 
	return -1;
    else
	if(*a.field > *b.field) 
	    return +1;
	else // *a.field == *b.field 
	{
	    ita = a.field->begin();
	    itb = b.field->begin();
	    
	    while(ita != a.field->end() && itb != b.field->end() && *ita == *itb)
	    {
		ita++;
		itb++;
	    }
	    
	    if(ita == a.field->end() && itb == b.field->end())
		return 0; 
		    
	    if(itb == b.field->end()) 
		return +1; // b can't be greater than a, at most it can be equal to it

	    if(ita == a.field->end())
		return -1;  // because itb != b.field->end();

	    return *ita - *itb;
	}
    E_END("infinint::difference", "");
}


bool infinint::is_valid() const throw()
{
    E_BEGIN;
    return field != NULL;
    E_END("infinint::is_valid", "");
}

void infinint::reduce() throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    static unsigned int max_a_time = ~ (unsigned int)(0); // this is the argument type of remove_bytes_at_iterator
    
    unsigned int count = 0;
    storage::iterator it = field->begin();

    do
    {
	while(it != field->end() && (*it) == 0 && count < max_a_time)
	{
	    it++;
	    count++;
	}

	if(it == field->end()) // all zeros
	{
	    if(count == 0) // empty storage;
		field->insert_null_bytes_at_iterator(field->begin(), 1); // field width is at least one byte
	    else
		field->remove_bytes_at_iterator(field->begin(), count - 1);
	}
	else
	{
	    if(count > 0)
		field->remove_bytes_at_iterator(field->begin(), count);
	    count = 0;
	    it = field->begin();
	}
    }
    while(it != field->end() && (*it) == 0);
    E_END("infinint::reduce", "");
}

void infinint::copy_from(const infinint & ref) throw(Ememory, Ebug)
{
    E_BEGIN;
    if(ref.is_valid())
    {
	field = new storage(*(ref.field));
	if(field == NULL)
	    throw Ememory("infinint::copy_from");
    }
    else
	throw SRC_BUG;
    E_END("infinint::copy_from", "");
}

void infinint::detruit() throw(Ebug)
{
    E_BEGIN;
    if(field != NULL)
    {
	delete field;
	field = NULL;
    }
    E_END("infinint::detruit", "");
}

void infinint::make_at_least_as_wider_as(const infinint & ref) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    while(*ref.field > *field)
	field->insert_null_bytes_at_iterator(field->begin(), TG);
    E_END("infinint::make_at_least_as_wider_as", "");
}

template <class T> void infinint::infinint_from(T a) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
    unsigned int size = sizeof(a);

    if(used_endian == not_initialized)
	setup_endian();
    
    field = new storage(size);
    if(field != NULL)
    {
	unsigned char *ptr = (unsigned char *)(&a);
	storage::iterator it;
	unsigned char *rev_ptr = ptr+sizeof(a)-1;

	if(used_endian == big_endian)
	    swap_bytes(ptr, sizeof(a));

	it = field->begin();
	while(it != field->end())
	    *(it++) = 0x00;

	it = field->rbegin();
	while(it != field->rend())
	    *(it--) = *(rev_ptr--);
    }
    else
	throw Ememory("template infinint::infinint_from");

    reduce();
    E_END("infinint::infinint_from", "");
}

static void dummy_call(char *x)
{
    static char id[]="$Id: infinint.cpp,v 1.17 2002/03/18 11:00:54 denis Rel $";
    dummy_call(id);
}

template <class T> void infinint::infinint_unstack_to(T &a) throw(Ememory, Erange, Ebug)
{
    E_BEGIN;
	// T is supposed to be an unsigned "integer" 
	// (ie.: sizeof returns the width of the storage bit field  and no sign bit is present)
	// Note : static here avoids the recalculation of max_T at each call
    static const T max_T = ~ T(0); // negation bit by bit of zero gives max possible value of T (all bits set)
    infinint step = max_T - a;
    
    if(*this < step)
    {
	T transfert = 0;
	unsigned char *debut = (unsigned char *)&transfert;
	unsigned char *ptr = debut + sizeof(transfert) - 1;
	storage::iterator it = field->rbegin();

	while(ptr >= debut && it != field->rend())
	    *(ptr--) = *(it--);
		
	if(used_endian == big_endian)
	    swap_bytes(debut, sizeof(transfert));
	a += transfert;
	*this = 0;
    }
    else
    {
	*this -= step;
	a = max_T;
    }
    E_END("infinint::infinint_unstack_to", "");
}

void infinint::setup_endian()
{
    E_BEGIN;
    unsigned short int u = 1;
    unsigned char *ptr = (unsigned char *)(&u);

    if(ptr[0] == 1)
	used_endian = big_endian;
    else
	used_endian = little_endian;
    E_END("infinint::setup_endian", "");
}

//////////////////////////////////////////////////////
///////////////// local functions ////////////////////
//////////////////////////////////////////////////////

static void swap_bytes(unsigned char &a, unsigned char &b) throw()
{
    E_BEGIN;
    unsigned char c = a;
    a = b;
    b = c;
    E_END("infinint.cpp : swap_bytes", "unsigned char &, unsigned char &");
}

static void swap_bytes(unsigned char *a, unsigned int size) throw()
{
    E_BEGIN;
    if(size <= 1)
	return;
    else
    {
	swap_bytes(a[0], a[size-1]);
	swap_bytes(a+1, size-2); // terminal recursivity
    }
    E_END("infinint.cpp : swap_bytes", "unsigned char *");
}

static void expand_byte(unsigned char a, bitfield &bit) throw()
{
    E_BEGIN;
    unsigned char mask = 0x80;

    for(register int i = 0; i < 8; i++)
    {
        bit[i] = (a & mask) >> 7 - i;
	mask >>= 1;
    }
    E_END("infinint.cpp : expand_byte", "");
}

static void contract_byte(const bitfield &b, unsigned char & a) throw(Erange)
{
    E_BEGIN;
    a = 0;
    
    for(register int i = 0; i < 8; i++)
    {
	a <<= 1;
	if(b[i] > 1)
	    throw Erange("infinint.cpp : contract_byte", "a binary digit is either 0 or 1");
	a += b[i];
    }
    E_END("infinint.cpp : contract_byte", "");
}

///////////////////////////////////////////////////////////////////////
///////////////// friends and not friends of infinint /////////////////
///////////////////////////////////////////////////////////////////////

infinint operator + (const infinint & a, const infinint & b) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint ret = a;
    ret += b;

    return ret;
    E_END("operator +", "infinint");
}

infinint operator - (const infinint & a, const infinint & b) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint ret = a;
    ret -= b;

    return ret;
    E_END("operator -", "infinint");
}

infinint operator * (const infinint & a, const infinint & b) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint ret = a;
    ret *= b;
    
    return ret;
    E_END("operator *", "infinint");
}

infinint operator * (const infinint &a, const unsigned char b) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint ret = a;
    ret *= b;
    
    return ret;
    E_END("operator *", "infinint, unsigned char");
}

infinint operator * (const unsigned char a, const infinint &b) throw(Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint ret = b;
    ret *= a;
    
    return ret;
    E_END("operator *", "unsigned char, infinint");
}

infinint operator / (const infinint & a, const infinint & b) throw(Einfinint, Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint q, r;
    euclide(a, b, q, r);

    return q;
    E_END("operator / ", "infinint");
}

infinint operator % (const infinint & a, const infinint & b) throw(Einfinint, Erange, Ememory, Ebug)
{
    E_BEGIN;
    infinint q, r;
    euclide(a, b, q, r);

    return r;
    E_END("operator %", "infinint");
}

infinint operator >> (const infinint & a, unsigned long bit) throw(Erange, Ememory, Ebug)
{ 
    E_BEGIN;
    infinint ret = a;
    ret >>= bit; 
    return ret; 
    E_END("operator >>", "infinint, unsigned long");
}

infinint operator >> (const infinint & a, const infinint & bit) throw(Erange, Ememory, Ebug)
{ 
    E_BEGIN;
    infinint ret = a;
    ret >>= bit; 
    return ret; 
    E_END("operator >>", "infinint");
}

infinint operator << (const infinint & a, unsigned long bit) throw(Erange, Ememory, Ebug)
{ 
    E_BEGIN;
    infinint ret = a; 
    ret <<= bit; 
    return ret;
    E_END("operator <<", "infinint, unsigned long");
}

infinint operator << (const infinint & a, const infinint & bit) throw(Erange, Ememory, Ebug)
{ 
    E_BEGIN;
    infinint ret = a; 
    ret <<= bit; 
    return ret; 
    E_END("operator <<", "infinint");
}

void euclide(infinint a, const infinint &b, infinint &q, infinint &r) throw(Einfinint, Erange, Ememory, Ebug)
{
    E_BEGIN;
    if(b == 0)
	throw Einfinint("infinint.cpp : euclide", "division by zero"); // division by zero

    if(a < b)
    {
	q = 0;
	r = a;
	return;
    }

    r = b;
    while(*a.field >= *r.field)
	r <<= 8; // one byte

    q = 0;
    while(b < r)
    {
	r >>= 8; // one byte;
	q <<= 8; // one byte;
	while(r <= a)
	{
	    a -= r;
	    q++;
	}
    }

    r = a;
    E_END("euclide", "infinint");
}
