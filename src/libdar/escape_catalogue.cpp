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

#include "escape_catalogue.hpp"
#include "macro_tools.hpp"
#include "cat_all_entrees.hpp"

using namespace std;

namespace libdar
{

    escape_catalogue::escape_catalogue(user_interaction & dialog,
				       const pile_descriptor & x_pdesc,
				       const datetime & root_last_modif,
				       const label & data_name) : catalogue(dialog, root_last_modif, data_name)
    {
	set_esc_and_stack(x_pdesc);
	x_reading_ver = macro_tools_supported_version;
	x_default_algo = none;
	x_lax = false;
	corres.clear();
	status = ec_completed; // yes, with that constructor, the catalogue contains all known object and entree do not miss any field
	cat_det = NULL;
	min_read_offset = 0;
	depth = 0; // we start at the root... of course
	wait_parent_depth = 0; // to disable this feature

	    // dropping the data_name in the archive
	pdesc.stack->sync_write_above(pdesc.esc); // esc is now up to date
	pdesc.esc->add_mark_at_current_position(escape::seqt_data_name);
	data_name.dump(*pdesc.esc);
    }

    escape_catalogue::escape_catalogue(user_interaction & dialog,
				       const pile_descriptor & x_pdesc,
				       const archive_version & reading_ver,
				       compression default_algo,
				       bool lax) : catalogue(dialog,
							     datetime(0),
							     label_zero)
    {
	set_esc_and_stack(x_pdesc);
	x_reading_ver = reading_ver;
	x_default_algo = default_algo;
	x_lax = lax;
	corres.clear();
	status = ec_init; // with that constructor, the catalogue starts empty and get completed entry by entry each time a one asks for its contents (read() method)
	cat_det = NULL;
	min_read_offset = 0;
	depth = 0; // we start at the root
	wait_parent_depth = 0; // to disable this feature

	    // fetching the value of ref_data_name
	pdesc.stack->flush_read_above(pdesc.esc);
	if(pdesc.esc->skip_to_next_mark(escape::seqt_data_name, false))
	{
		// we are at least at revision "08" of archive format, or this is a collision with data of an old archive or of an archive without escape marks
	    label tmp;

	    try
	    {
		tmp.read(*pdesc.esc);
		set_data_name(tmp);
	    }
	    catch(...)
	    {
		if(!lax)
		    throw Erange("escape_catalogue::escape_catalogue", gettext("incoherent data after escape sequence, cannot read internal data set label"));
		else
		{
		    get_ui().warning("LAX MODE: Could not read the internal data set label, using a fake value, this will probably avoid using isolated catalogue");
		    set_data_name(label_zero);
		}
	    }
	}
	else // skip to expected mark failed
	    if(!lax)
		throw Erange("escape_catalogue::escape_catalogue", gettext("could not find the internal data set label escape sequence"));
	    else
	    {
		contextual *cont_data = NULL;
		pdesc.stack->find_first_from_bottom(cont_data);

		get_ui().warning("LAX MODE: Could not read the internal data set label, using a fake value, this will probably avoid using isolated catalogue");
		if(cont_data == NULL)
		    set_data_name(label_zero);
		else
		    set_data_name(cont_data->get_data_name());
	    }
    }

    const escape_catalogue & escape_catalogue::operator = (const escape_catalogue &ref)
    {
	catalogue *me = this;
	const catalogue *you = &ref;

	destroy();

	    // copying the catalogue part
	*me = *you;

	    // copying the escape_catalogue specific part
	copy_from(ref);

	return *this;
    }

    void escape_catalogue::pre_add(const cat_entree *ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	if(ceci->pdesc.esc == NULL)
	    throw SRC_BUG;

	ceci->pdesc.stack->sync_write_above(pdesc.esc);
	ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_file);
	ref->dump(pdesc, true);
    }

    void escape_catalogue::pre_add_ea(const cat_entree *ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);
	const cat_mirage *ref_mir = dynamic_cast<const cat_mirage *>(ref);
	const cat_inode *ref_ino = dynamic_cast<const cat_inode *>(ref);

	if(ref_mir != NULL)
	    ref_ino = ref_mir->get_inode();

	if(ref_ino != NULL)
	{
	    if(ref_ino->ea_get_saved_status() == cat_inode::ea_full)
	    {
		if(ceci->pdesc.esc == NULL)
		    throw SRC_BUG;
		else
		{
		    ceci->pdesc.stack->sync_write_above(pdesc.esc);
		    ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_ea);
		}
	    }
		// else, nothing to do.
	}
	    // else, nothing to do.
    }

    void escape_catalogue::pre_add_crc(const cat_entree *ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);
	const cat_mirage *ref_mir = dynamic_cast<const cat_mirage *>(ref);
	const cat_file *ref_file = dynamic_cast<const cat_file *>(ref);

	if(ref_mir != NULL)
	    ref_file = dynamic_cast<const cat_file *>(ref_mir->get_inode());

	if(ref_file != NULL)
	{
	    if(ref_file->get_saved_status() == s_saved)
	    {
		const crc * c = NULL;

		if(ref_file->get_crc(c))
		{
		    if(ceci->pdesc.esc == NULL)
			throw SRC_BUG;

		    ceci->pdesc.stack->sync_write_above(pdesc.esc);
		    ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_file_crc);
		    c->dump(*(ceci->pdesc.esc));
		}
		    // else, the data may come from an old archive format
	    }
		// else, nothing to do.
	}
    }

    void escape_catalogue::pre_add_dirty() const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	if(ceci->pdesc.esc == NULL)
	    throw SRC_BUG;
	ceci->pdesc.stack->sync_write_above(pdesc.esc);
	ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_dirty);
    }


    void escape_catalogue::pre_add_ea_crc(const cat_entree *ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);
	const cat_mirage *ref_mir = dynamic_cast<const cat_mirage *>(ref);
	const cat_inode *ref_ino = dynamic_cast<const cat_inode *>(ref);

	if(ref_mir != NULL)
	    ref_ino = ref_mir->get_inode();

	if(ref_ino != NULL)
	{
	    if(ref_ino->ea_get_saved_status() == cat_inode::ea_full)
	    {
		const crc * c = NULL;

		ref_ino->ea_get_crc(c);

		if(ceci->pdesc.esc == NULL)
		    throw SRC_BUG;
		ceci->pdesc.stack->sync_write_above(pdesc.esc);
		ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_ea_crc);
		c->dump(*(ceci->pdesc.esc));
	    }
		// else, nothing to do.
	}
	    // else, nothing to do.

    }

    void escape_catalogue::pre_add_waste_mark() const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	if(ceci->pdesc.esc == NULL)
	    throw SRC_BUG;
	ceci->pdesc.stack->sync_write_above(pdesc.esc);
	ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_changed);
    }

    void escape_catalogue::pre_add_failed_mark() const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	if(ceci->pdesc.esc == NULL)
	    throw SRC_BUG;
	ceci->pdesc.stack->sync_write_above(pdesc.esc);
	ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_failed_backup);
    }

    void escape_catalogue::pre_add_fsa(const cat_entree *ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);
	const cat_mirage *ref_mir = dynamic_cast<const cat_mirage *>(ref);
	const cat_inode *ref_ino = dynamic_cast<const cat_inode *>(ref);

	if(ref_mir != NULL)
	    ref_ino = ref_mir->get_inode();

	if(ref_ino != NULL)
	{
	    if(ref_ino->fsa_get_saved_status() == cat_inode::fsa_full)
	    {
		if(ceci->pdesc.esc == NULL)
		    throw SRC_BUG;
		else
		{
		    ceci->pdesc.stack->sync_write_above(pdesc.esc);
		    ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_fsa);
		}
	    }
		// else, nothing to do.
	}
	    // else, nothing to do.
    }

    void escape_catalogue::pre_add_fsa_crc(const cat_entree *ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);
	const cat_mirage *ref_mir = dynamic_cast<const cat_mirage *>(ref);
	const cat_inode *ref_ino = dynamic_cast<const cat_inode *>(ref);

	if(ref_mir != NULL)
	    ref_ino = ref_mir->get_inode();

	if(ref_ino != NULL)
	{
	    if(ref_ino->fsa_get_saved_status() == cat_inode::fsa_full)
	    {
		const crc * c = NULL;

		ref_ino->fsa_get_crc(c);

		if(ceci->pdesc.esc == NULL)
		    throw SRC_BUG;
		ceci->pdesc.stack->sync_write_above(pdesc.esc);
		ceci->pdesc.esc->add_mark_at_current_position(escape::seqt_fsa_crc);
		c->dump(*(ceci->pdesc.esc));
	    }
		// else, nothing to do.
	}
	    // else, nothing to do.
    }

    void escape_catalogue::reset_read() const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	ceci->reset_reading_process();
	catalogue::reset_read();
    }

    void escape_catalogue::end_read() const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	ceci->reset_reading_process();
	catalogue::end_read();
    }

    void escape_catalogue::skip_read_to_parent_dir() const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	switch(status)
	{
	case ec_init:
	case ec_eod:
	case ec_detruits:
	    if(cat_det == NULL)
		throw SRC_BUG;
	    cat_det->skip_read_to_parent_dir();
	    break;
	case ec_marks:
	    ceci->wait_parent_depth = depth;
	    break;
	case ec_completed:
	    catalogue::skip_read_to_parent_dir();
	    break;
	default:
	    throw SRC_BUG;
	}
    }

    bool escape_catalogue::read(const cat_entree * & ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);
	const cat_directory *ref_dir = NULL;
	const cat_eod *ref_eod = NULL;
	bool stop = false;

	if(pdesc.esc == NULL)
	    throw SRC_BUG;
	ref = NULL;

	    // if we have already read the whole archive contents (included detruits object),
	    // we do not need inspect the archive again, we instead use the data in memory
	if(status == ec_completed)
	    return catalogue::read(ref);

	pdesc.stack->flush_read_above(pdesc.esc);
	try
	{
	    while(ref == NULL && !stop)
	    {
		switch(status)
		{
		case ec_init:
		    ceci->status = ec_marks;
			// no break;
		case ec_marks:
		    if(min_read_offset > pdesc.esc->get_position())
		    {
			    // for some reason, like datacorruption, bad CRC or missing CRC in hard linked inode
			    // we skipped back to the hard linked inode information (it failed to be restored the first time)
			    // We must not read again and again the same data, so we skip forward to min_read_offset

			ceci->pdesc.esc->skip(min_read_offset);
		    }

		    if(pdesc.esc->skip_to_next_mark(escape::seqt_file, true))
		    {
			ceci->min_read_offset = pdesc.esc->get_position();
			try
			{
			    ref = cat_entree::read(get_ui(),
						   get_pool(),
						   pdesc,
						   x_reading_ver,
						   ceci->access_stats(),
						   ceci->corres,
						   ceci->x_default_algo,
						   false, // lax mode
						   false, // only_detruit
						   true); // always a small read in that context
			    if(pdesc.esc->next_to_read_is_mark(escape::seqt_failed_backup))
			    {
				if(!pdesc.esc->skip_to_next_mark(escape::seqt_failed_backup, false))
				    throw SRC_BUG;
				if(ref != NULL)
				{
				    delete ref;
				    ref = NULL;
				}
				continue; // restarts the while loop
			    }

			    ref_dir = dynamic_cast<const cat_directory *>(ref);
			    if(ref_dir != NULL)
				++(ceci->depth);

			    ref_eod = dynamic_cast<const cat_eod *>(ref);
			    if(ref_eod != NULL)
			    {
				if(depth > 0)
				    --(ceci->depth);
				else
				    if(!x_lax)
					throw Erange("escape_catalogue::read", gettext("Escape sequences used for reading lead the archive to place some files out of the specified root. To overcome this problem, try reading the archive in direct mode (not using sequential reading), try repairing the archive using Parchive if redundancy data has been created or in last resort try using the lax mode"));
				    else // lax mode
				    {
					get_ui().warning(gettext("LAX MODE: Archive directory structure is corrupted, it would lead to place some files out of the specified root directory. Restoring different directory contents at the root not out of it, which will put files of different directories in the specified root directory"));
					if(ref == NULL)
					    throw SRC_BUG;
					delete ref; // this is the CAT_EOD object
					ref = NULL;
					continue; // restarts the while loop
				    }
			    }
			}
			catch(Erange & e)
			{
			    if(!x_lax)
				throw;
			    else
			    {
				get_ui().warning(gettext("LAX MODE: found unknown catalogue entry, assuming data corruption occurred. Skipping to the next entry, this may lead to improper directory structure being restored, if the corrupted data was a directory"));
				ref = NULL;
				continue; // restarts the while loop
			    }
			}

			if(ref == NULL)
			    throw Erange("escape_catalogue::read", gettext("Corrupted entry following an escape mark in the archive"));
			else
			{
			    bool is_eod = ref_eod != NULL;
			    cat_entree *ref_nc = const_cast<cat_entree *>(ref);

			    ceci->add(ref_nc);
			    if(wait_parent_depth > 0) // we must not return this object as it is member of a skipped dir
			    {
				if(depth < wait_parent_depth) // we are back out of the skipped directory
				{
				    if(is_eod)
					ceci->wait_parent_depth = 0;
				    else
					throw SRC_BUG; // we should get out of the directory reading a CAT_EOD !
				}

				ref = NULL; // must not release object except, they are now part of catalogue
				continue;   // ignore this entry and skip to the next one
			    }

			    if(is_eod)
				ref = get_r_eod_address();
			}
		    }
		    else // no more file to read from in-sequence marks
		    {
			if(depth != 0)
			{
			    get_ui().warning(gettext("Uncompleted archive! Assuming it has been interrupted during the backup process. If an error has been reported just above, simply ignore it, this is about the file that was saved at the time of the interruption."));
			    ceci->status = ec_eod;
			    ref = get_r_eod_address();
			    if(ref == NULL)
				throw SRC_BUG;
			    --(ceci->depth);
			}
			else
			{
				// we no more need the hard link correspondance map so we free the memory it uses
			    ceci->corres.clear();
			    ceci->status = ec_detruits;
			    label tmp;
			    tmp.clear();

			    if(pdesc.compr == NULL)
				throw SRC_BUG;
			    if(pdesc.compr->is_compression_suspended())
			    {
				pdesc.compr->resume_compression();
				if(pdesc.compr->get_algo() != none)
				    pdesc.stack->flush_read_above(pdesc.compr);
			    }

				// build the catalogue to get detruit objects
			    if(pdesc.esc->skip_to_next_mark(escape::seqt_catalogue, true))
			    {
				bool only_detruit = !is_empty(); // we read the whole catalogue if we could not find any entry before the catalogue mark
				    // this situation is either an extracted catalogue, or a normal archive which only has detruit objects
				    // in any case, it does not hurt considering the whole catalogue

 				ceci->cat_det = new (get_pool()) catalogue(get_ui(),
									   pdesc,
									   x_reading_ver,
									   x_default_algo,
									   x_lax,  // lax
									   tmp,    // we do not modify the catalogue data_name even in lax mode
									   only_detruit);  // only_detruit

				if(ceci->cat_det == NULL)
				    throw Ememory("escape_catalogue::read");

				cat_det->reset_read();
				if(only_detruit)
				    ceci->status = ec_detruits;
				else
				{
				    ceci->status = ec_completed;
				    ceci->swap_stuff(*(ceci->cat_det));
				    delete ceci->cat_det;
				    ceci->cat_det = NULL;
				}
				stop = false;
				ref = NULL;
			    }
			    else // no more file and no catalogue entry found (interrupted archive)
			    {
				ceci->status = ec_completed;

				if(!x_lax)
				    throw Erange("escape_catalogue::read", gettext("Cannot extract from the internal catalogue the list of files to remove"));
				else
				{
				    get_ui().warning("LAX MODE: Cannot extract from the internal catalogue the list of files to remove, skipping this step");
				    ref = NULL;
				    stop = true;
				}
			    }
			}
		    }
		    break;
		case ec_eod:
		    if(depth > 0)
		    {
			ref = get_r_eod_address();
			if(ref == NULL)
			    throw SRC_BUG;
			--(ceci->depth);
		    }
		    else
			ceci->status = ec_marks;
		    break;
		case ec_detruits:
		    if(cat_det == NULL)
			throw SRC_BUG;
		    if(!cat_det->read(ref))
		    {
			ceci->merge_cat_det();
			ceci->status = ec_completed;
			ref = NULL;
			stop = true;
		    }
		    else
			if(ref == NULL)
			    throw SRC_BUG;
		    break;
		case ec_completed:
		    return catalogue::read(ref);
		default:
		    throw SRC_BUG;
		}
	    }
	}
	catch(...)
	{
	    if(ref != NULL && cat_det == NULL && ref != get_r_eod_address()) // we must not delete objects owned by cat_det
		delete ref;
	    ref = NULL;
	    throw;
	}

	return ref != NULL;
    }

    bool escape_catalogue::read_if_present(std::string *name, const cat_nomme * & ref) const
    {
	escape_catalogue *ceci = const_cast<escape_catalogue *>(this);

	ceci->reset_reading_process();
	return catalogue::read_if_present(name, ref);
    }

    void escape_catalogue::tail_catalogue_to_current_read()
    {
	reset_reading_process();
	catalogue::tail_catalogue_to_current_read();
    }

    void escape_catalogue::set_esc_and_stack(const pile_descriptor & x_pdesc)
    {
	x_pdesc.check(true); // always expecting an escape layer
	pdesc = x_pdesc;
    }

    void escape_catalogue::copy_from(const escape_catalogue & ref)
    {
	pdesc = ref.pdesc;
	x_reading_ver = ref.x_reading_ver;
	x_default_algo = ref.x_default_algo;
	x_lax = ref.x_lax;
	corres = ref.corres;
	status = ref.status;
	if(ref.cat_det == NULL)
	    cat_det = NULL;
	else
	    cat_det = new (get_pool()) catalogue(*ref.cat_det);
	if(cat_det == NULL)
	    throw Ememory("escape_catalogue::copy_from");
	min_read_offset = ref.min_read_offset;
	depth = ref.depth;
	wait_parent_depth = ref.wait_parent_depth;
    }

    void escape_catalogue::destroy()
    {
	if(cat_det != NULL)
	{
	    delete cat_det;
	    cat_det = NULL;
	}
    }

    void escape_catalogue::merge_cat_det()
    {
	if(cat_det != NULL)
	{
	    copy_detruits_from(*cat_det);
	    delete cat_det;
	    cat_det = NULL;
	}
    }

    void escape_catalogue::reset_reading_process()
    {
	switch(status)
	{
	case ec_init:
	    break;
	case ec_marks:
	case ec_eod:
	    get_ui().warning(gettext("Resetting the sequential reading process of the archive contents while it is not finished, will make all data unread so far becoming inaccessible"));
	    corres.clear();
	    status = ec_completed;
	    break;
	case ec_detruits:
	    merge_cat_det();
	    status = ec_completed;
	    break;
	case ec_completed:
	    break;
	default:
	    throw SRC_BUG;
	}
	depth = 0;
	wait_parent_depth = 0;
    }

} // end of namespace
