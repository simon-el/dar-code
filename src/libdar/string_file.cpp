/*********************************************************************/
// dar - disk archive - a backup/restoration program
// Copyright (C) 2002-2052 Denis Corbin
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
// to contact the author : http://dar.linux.free.fr/email.html
/*********************************************************************/

#include "../my_config.h"

extern "C"
{
#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STRINGS_H
#include <strings.h>
#endif

#if STDC_HEADERS
# include <string.h>
#else
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
    char *strchr (), *strrchr ();
# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif
}

#include "string_file.hpp"

using namespace std;

namespace libdar
{

    bool string_file::skip_relative(S_I x)
    {
	bool ret = true;

	if(is_terminated())
	    throw SRC_BUG;

	if(x > 0)
	{
	    cur += x;
	    if(cur >= len)
	    {
		cur = len;
		ret = false;
	    }
	}
	else
	    if(x < 0)
	    {
		x = -x;
		if(cur > x)
		    cur -= x;
		else
		{
		    cur = 0;
		    ret = false;
		}

	    }

	return ret;
    }

    bool string_file::skip(const infinint & pos)
    {
	bool ret;

	if(is_terminated())
	    throw SRC_BUG;

	if(pos < len)
	{
	    cur = pos;
	    ret=true;
	}
	else
	{
	    cur = len;
	    ret=false;
	}

	return ret;
    }

    U_I string_file::inherited_read(char *a, U_I size)
    {
	infinint avail_inf = len - cur;
	U_I avail = 0;
	U_I ret;
	const char* ptr = data.c_str();
	infinint tmp_sk = cur;
	U_64 tmp_sk_64 = 0;

	if(is_terminated())
	    throw SRC_BUG;

            //
	    // calculating the amount of available character to be returned

	avail_inf.unstack(avail);
	if(avail_inf != 0)
	    throw SRC_BUG;
	ret = avail <= size ? avail : size;

	    //
	    // pointing the iterator to the first char to read

	do
	{
	    tmp_sk.unstack(tmp_sk_64);
	    if(tmp_sk_64 > 0)
	    {
		ptr += tmp_sk_64;
		tmp_sk_64 = 0;
	    }
	}
	while(tmp_sk > 0);

	    //
	    // copying the string

	(void)memcpy(a, ptr, ret);
	cur += ret;
	return ret;
    }



} // end of namespace
