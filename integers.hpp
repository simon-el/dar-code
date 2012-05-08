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
// to contact the author : dar.linux@free.fr
/*********************************************************************/
// $Id: integers.hpp,v 1.3 2003/02/11 22:01:53 edrusb Rel $
//
/*********************************************************************/
//

#ifndef INTEGERS_HPP
#define INTEGERS_HPP

#ifndef OS_BITS

#include <stdint.h>
#define U_16 uint16_t
#define U_32 uint32_t
#define U_64 uint64_t
#define U_I  unsigned int
#define S_16 int16_t
#define S_32 int32_t
#define S_64 int64_t
#define S_I signed int

#else  //  OS_BITS not defined
#if OS_BITS == 32

#define U_16 unsigned short
#define U_32 unsigned long
#define U_64 unsigned long long
#define U_I unsigned int 
#define S_16 signed short
#define S_32 signed long
#define S_64 signed long long
#define S_I signed int

#else
#if "OS_BITS" == 64

#define U_16 unsigned short
#define U_32 unsigned int
#define U_64 unsigned long long
#define U_I  unsigned int
#define S_16 signed short
#define S_32 signed int
#define S_64 signed long long
#define S_I signed int

#else
#error // unknown OS_BITS value
    // the previous line will not compile, this is the expected behaviour

#endif // OS_BITS == 64
#endif // OS_BITS == 32
#endif // OS_BITS not defined

#endif // ifndef



