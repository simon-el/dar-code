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
// $Id: database_header.cpp,v 1.14.2.1 2005/03/13 20:07:49 edrusb Rel $
//
/*********************************************************************/

#include "../my_config.h"

extern "C"
{
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
} // end extern "C"

#include "database_header.hpp"
#include "compressor.hpp"
#include "tools.hpp"
#include "user_interaction.hpp"
#include "integers.hpp"
#include "cygwin_adapt.hpp"

using namespace libdar;

static unsigned char database_version = 1;

#define HEADER_OPTION_NONE 0x00

generic_file *database_header_create(user_interaction & dialog, const string & filename, bool overwrite)
{
    char *ptr = tools_str2charptr(filename);
    generic_file *ret = NULL;

    try
    {
        struct stat buf;
        S_I fd;
        database_header h;
        compressor *comp;

        if(stat(ptr, &buf) >= 0 && !overwrite)
            throw Erange("database_header_create", gettext("Cannot create database, file exists"));
        fd = ::open(ptr, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666);
        if(fd < 0)
            throw Erange("database_header_create", tools_printf(gettext("Cannot create database %S : %s"), &filename, strerror(errno)));
        ret = new fichier(dialog, fd);
        if(ret == NULL)
        {
            close(fd);
            throw Ememory("database_header_create");
        }

        h.version = database_version;
        h.options = HEADER_OPTION_NONE;
        h.write(*ret);

        comp = new compressor(dialog, gzip, ret); // upon success, ret is owned by compr
        if(comp == NULL)
            throw Ememory("database_header_create");
        else
            ret = comp;
    }
    catch(...)
    {
        delete [] ptr;
        if(ret != NULL)
            delete ret;
        throw;
    }
    delete [] ptr;

    return ret;
}

generic_file *database_header_open(user_interaction & dialog, const string & filename)
{
    char *ptr = tools_str2charptr(filename);
    generic_file *ret = NULL;

    try
    {
        database_header h;
        compressor *comp;

	try
	{
	    ret = new fichier(dialog, ptr, gf_read_only);
	}
	catch(Erange & e)
	{
	    throw Erange("database_header_open", tools_printf(gettext("Error reading database %S : "), &filename) + e.get_message());
	}
        if(ret == NULL)
            throw Ememory("database_header_open");
        h.read(*ret);
        if(h.version != database_version)
            dialog.pause(gettext("The format version of this database is too high for that software version, try reading anyway ? "));
        if(h.options != HEADER_OPTION_NONE)
            throw Erange("database_header_open", gettext("Unknown header option in database, aborting\n"));

        comp = new compressor(dialog, gzip, ret);
        if(comp == NULL)
            throw Ememory("database_header_open");
        else
            ret = comp;
    }
    catch(...)
    {
        delete [] ptr;
        if(ret != NULL)
            delete ret;
        throw;
    }

    delete [] ptr;
    return ret;
}

static void dummy_call(char *x)
{
    static char id[]="$Id: database_header.cpp,v 1.14.2.1 2005/03/13 20:07:49 edrusb Rel $";
    dummy_call(id);
}