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
} // end of extern "C"


#include "header_version.hpp"
#include "integers.hpp"

#define LIBDAR_URL_VERSION "http://dar.linux.free.fr/pre-release/doc/Notes.html#Dar_version_naming"

using namespace std;

namespace libdar
{
    static const U_I HEADER_CRC_SIZE = 2; //< CRC width (deprecated, now only used when reading old archives)

    header_version::header_version()
    {
	algo_zip = none;
	cmd_line = "";
	initial_offset = 0;
	sym = crypto_none;
	crypted_key = NULL;
	ref_layout = NULL;
    }

    void header_version::read(generic_file & f, user_interaction & dialog, bool lax_mode)
    {
	crc *ctrl = NULL;
	char tmp;
        unsigned char flag;

	f.reset_crc(HEADER_CRC_SIZE);
	try
	{
	    edition.read(f);
	}
	catch(Egeneric & e)
	{
	    if(lax_mode)
	    {
		string answ;
		U_I equivalent;
		bool ok = false;

		dialog.warning(gettext("LAX MODE: Failed to read the archive header's format version."));
		do
		{
		    answ = dialog.get_string(tools_printf(gettext("LAX MODE: Please provide the archive format: You can use the table at %s to find the archive format depending on the release version, (for example if this archive has been created using dar release 2.3.4 to 2.3.7 answer \"6\" without the quotes here): "), LIBDAR_URL_VERSION), true);
		    if(tools_my_atoi(answ.c_str(), equivalent))
			edition = equivalent;
		    else
		    {
			dialog.pause(tools_printf(gettext("LAX MODE: \"%S\" is not a valid archive format"), &answ));
			continue;
		    }

		    try
		    {
			dialog.pause(tools_printf(gettext("LAX MODE: Using archive format \"%d\"?"), equivalent));
			ok = true;
		    }
		    catch(Euser_abort & e)
		    {
			ok = false;
		    }
		}
		while(!ok);
	    }
	    else
		throw;
	}

	if(f.read(&tmp, 1) == 1) // compression algo
	{
	    bool ok = false;
	    do
	    {
		try
		{
		    algo_zip = char2compression(tmp);
		    ok = true;
		}
		catch(Erange & e)
		{
		    if(!lax_mode)
			throw;

		    string answ = dialog.get_string(gettext("LAX MODE: Unknown compression algorithm used, assuming data corruption occurred. Please help me, answering with one of the following words \"none\", \"gzip\", \"bzip2\" or \"lzo\" at the next prompt:"), true);
		    if(answ == gettext("none"))
			tmp = compression2char(none);
		    else if(answ == gettext("gzip"))
			tmp = compression2char(gzip);
		    else if(answ == gettext("bzip2"))
			tmp = compression2char(bzip2);
		    else if(answ == gettext("lzo"))
			tmp = compression2char(lzo);
		}
	    }
	    while(!ok);
	}
	else
	    throw Erange("header_version::read", gettext("Reached End of File while reading archive header_version data structure"));

	tools_read_string(f, cmd_line);

	if(edition > 1)
	{
	    if(f.read((char *)&flag, 1) != 1)
		throw Erange("header_version::read", gettext("Reached End of File while reading archive header_version data structure"));
		// even in lax mode, because reading further is vain
	}
	else
	    flag = 0; // flag has been at edition 2

	if((flag & FLAG_INITIAL_OFFSET) != 0)
	{
	    initial_offset.read(f);
	}
	else
	    initial_offset = 0;

	if((flag & FLAG_SCRAMBLED) != 0)
	{
	    ciphered = true;
	    if(edition >= 9)
	    {
		if(f.read(&tmp, sizeof(tmp)) != 1)
		    throw Erange("header_version::read", gettext("Reached End of File while reading archive header_version data structure"));

		try
		{
		    sym = char_2_crypto_algo(tmp);
		}
		catch(Erange & e)
		{
		    if(!lax_mode)
			throw;
		    dialog.printf("Unknown crypto algorithm used in archive, ignoring that field and simply assuming the archive has been encrypted, if not done you will need to specify the crypto algorithm to use in order to read this archive");
		    sym = crypto_none;
		}
	    }
	    else
		    // unknown ciphering algorithm used (old archive format) or no encryption
		    // not coherent with flag which has the FLAG_SCRAMBLED bit set
		    // but that this way we record that the crypto algo has
		    // to be provided by the user
		sym = crypto_none;
	}
	else
	{
	    ciphered = false;
	    sym = crypto_none; // no crypto used, coherent with flag
	}

	has_tape_marks = (flag & FLAG_SEQUENCE_MARK) != 0;

	if(edition < 8 && has_tape_marks)
	{
	    if(lax_mode)
		has_tape_marks = false; // Escape sequence marks appeared at revision 08
	    else
		throw Erange("header_version::read", gettext("Corruption met while reading header_version data structure"));
	}

	if(crypted_key != NULL)
	{
	    delete crypted_key;
	    crypted_key = NULL;
	}

	if((flag & FLAG_HAS_CRYPTED_KEY) != 0)
	{
	    infinint key_size = f;

	    crypted_key = new (get_pool()) memory_file();
	    if(crypted_key == NULL)
		throw Ememory("header_version::read");
	    if(f.copy_to(*crypted_key, key_size) != key_size)
		throw Erange("header_version::read", gettext("Missing data for encrypted symmetrical key"));
	}

	if((flag & FLAG_HAS_REF_SLICING) != 0)
	{
	    try
	    {
		if(ref_layout == NULL)
		    ref_layout = new (get_pool()) slice_layout();
		if(ref_layout == NULL)
		    throw Ememory("header_version::read");
		ref_layout->read(f);
	    }
	    catch(Egeneric & e)
	    {
		if(lax_mode)
		{
		    dialog.warning(gettext("Error met while reading archive of reference slicing layout, ignoring this field and continuing"));
		    clear_slice_layout();
		}
		else
		    throw;
	    }
	}
	else
	    clear_slice_layout();


	ctrl = f.get_crc();
	if(ctrl == NULL)
	    throw SRC_BUG;

	try
	{
	    if(edition == empty_archive_version())
	    {
		if(lax_mode)
		    dialog.warning(gettext("Consistency check failed for archive header"));
		else
		    throw Erange("header_version::read", gettext("Consistency check failed for archive header"));
	    }

	    if(edition > 7)
	    {
		crc *coh = create_crc_from_file(f, get_pool());

		if(coh == NULL)
		    throw SRC_BUG;
		try
		{
		    if(typeid(*coh) != typeid(*ctrl))
		    {
			if(coh->get_size() != ctrl->get_size())
			    throw SRC_BUG;
			else
			    throw SRC_BUG; // both case lead to a bug, but we need to know which one is met
		    }

		    if(*coh != *ctrl)
		    {
			if(lax_mode)
			    dialog.warning(gettext("Consistency check failed for archive header"));
			else
			    throw Erange("header_version::read", gettext("Consistency check failed for archive header"));
		    }
		}
		catch(...)
		{
		    if(coh != NULL)
			delete coh;
		    throw;
		}
		if(coh != NULL)
		    delete coh;
	    }
	    if(initial_offset == 0)
		initial_offset = f.get_position();
	}
	catch(...)
	{
	    if(ctrl != NULL)
		delete ctrl;
	    throw;
	}

	if(ctrl != NULL)
	    delete ctrl;
    }

    void header_version::write(generic_file &f) const
    {
	crc *ctrl = NULL;
	char tmp;
        unsigned char flag = 0;

	    // preparing the data

	if(initial_offset != 0)
	    flag |= FLAG_INITIAL_OFFSET; // adding it to the flag

	if(crypted_key != NULL)
	    flag |= FLAG_HAS_CRYPTED_KEY;

	if(ref_layout != NULL)
	    flag |= FLAG_HAS_REF_SLICING;

	if(has_tape_marks)
	    flag |= FLAG_SEQUENCE_MARK;

	if(sym != crypto_none)
	    flag |= FLAG_SCRAMBLED;
	    // Note: we cannot set this flag (even if ciphered is true) if we do not know the crypt algo
	    // as since version 9 the presence of this flag implies the existence
	    // of the crypto algorithm in the header/trailer and we will always
	    // write down a header/version of the latest known format (thus greater or
	    // equal to 9).

	    // writing down the data

	f.reset_crc(HEADER_CRC_SIZE);
	edition.dump(f);
	tmp = compression2char(algo_zip);
	f.write(&tmp, sizeof(tmp));
	tools_write_string(f, cmd_line);
	f.write((char *)&flag, sizeof(flag));
	if(initial_offset != 0)
	    initial_offset.dump(f);
	if(sym != crypto_none)
	{
	    tmp = crypto_algo_2_char(sym);
	    f.write(&tmp, sizeof(tmp));
	}

	if(crypted_key != NULL)
	{
	    crypted_key->size().dump(f);
	    crypted_key->skip(0);
	    crypted_key->copy_to(f);
	}

	if(ref_layout != NULL)
	    ref_layout->write(f);

	ctrl = f.get_crc();
	if(ctrl == NULL)
	    throw SRC_BUG;
	try
	{
	    ctrl->dump(f);
	}
	catch(...)
	{
	    if(ctrl != NULL)
		delete ctrl;
	    throw;
	}
	if(ctrl != NULL)
	    delete ctrl;
    }

    void header_version::detruit()
    {
	clear_crypted_key();
	clear_slice_layout();
    }

    void header_version::copy_from(const header_version & ref)
    {
	edition = ref.edition;
	algo_zip = ref.algo_zip;
	cmd_line = ref.cmd_line;
	initial_offset = ref.initial_offset;
	sym = ref.sym;
	if(ref.crypted_key != NULL)
	{
	    crypted_key = new (get_pool()) memory_file(*ref.crypted_key);
	    if(crypted_key == NULL)
		throw Ememory("header_version::copy_from");
	}
	else
	    crypted_key = NULL;
	if(ref.ref_layout != NULL)
	{
	    ref_layout = new (get_pool()) slice_layout(*ref.ref_layout);
	    if(ref_layout == NULL)
		throw Ememory("header_version::copy_from");
	}
	else
	    ref_layout = NULL;
	has_tape_marks = ref.has_tape_marks;
	ciphered = ref.ciphered;
    }



} // end of namespace
