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
// $Id: sar.hpp,v 1.16.2.2 2009/04/07 08:45:29 edrusb Rel $
//
/*********************************************************************/

#ifndef SAR_HPP
#define SAR_HPP

#include "../my_config.h"

#include <string>
#include "infinint.hpp"
#include "generic_file.hpp"
#include "header.hpp"
#include "path.hpp"
#include "integers.hpp"

namespace libdar
{
	// contextual is defined in generic_file module
    class sar : public contextual
    {
    public:
        sar(user_interaction & dialog,
	    const std::string & base_name,
	    const std::string & extension,
	    const path & dir,
	    const std::string & execute = "");
        sar(user_interaction  & dialog,
	    const std::string & base_name,
	    const std::string & extension,
	    const infinint & file_size,
	    const infinint & first_file_size,
 	    bool x_warn_overwirte,
	    bool x_allow_overwrite,
	    const infinint & pause,
	    const path & dir,
	    const std::string & execute = "");
        ~sar();

            // inherited from generic_file
        bool skip(const infinint &pos);
        bool skip_to_eof();
        bool skip_relative(S_I x);
        infinint get_position();

            // informational routines
        infinint get_sub_file_size() const { return size; };
        infinint get_first_sub_file_size() const { return first_size; };
        bool get_total_file_number(infinint &num) const { num = of_last_file_num; return of_last_file_known; };
        bool get_last_file_size(infinint &num) const { num = of_last_file_size; return of_last_file_known; };

	    // inherited methods from contextual
        void set_info_status(const std::string & s) { status = s; };
        std::string get_info_status() const { return status; };

	    // disable execution of user command when destroying the current object
	void disable_natural_destruction() { natural_destruction = false; };

	    // enable back execution of user command when destroying the current object
	void enable_natural_destruction() { natural_destruction = true; };

    protected :
        S_I inherited_read(char *a, size_t sz);
        S_I inherited_write(const char *a, size_t sz);

    private :
        path archive_dir;
        std::string base, ext;
        std::string hook;
        infinint size;
        infinint first_size;
        infinint first_file_offset;
        infinint file_offset;
        std::string status;

        bool natural_destruction;

            // these following variables are modified by open_file
            // else the are used only for reading
        infinint of_current;
        infinint of_max_seen;
        bool of_last_file_known;
        infinint of_last_file_num;
        infinint of_last_file_size;
        label of_internal_name;
        fichier *of_fd;
        char of_flag;
        bool initial;

            // these are the option flags
        bool opt_warn_overwrite;
        bool opt_allow_overwrite;

	    //
        infinint pause;

        bool skip_forward(U_I x);
        bool skip_backward(U_I x);
        void close_file();
        void open_readonly(const char *fic, const infinint &num);
        void open_writeonly(const char *fic, const infinint &num);
        void open_file_init();
        void open_file(infinint num);
        void set_offset(infinint offset);
        void open_last_file();
        header make_write_header(const infinint &num, char flag);

            // hook to attach a command to execute after each slice
        std::string hook_substitute(const std::string & path, const std::string & basename, const std::string & num, const std::string & ext, const std::string & context);
        void hook_execute(const infinint &num);
    };


    class trivial_sar : public generic_file
    {
    public:
        trivial_sar(user_interaction & dialog, generic_file *ref); // trivial_sar own the argument
        ~trivial_sar() { if(reference != NULL) delete reference; };

        bool skip(const infinint & pos) { return reference->skip(pos + offset); };
        bool skip_to_eof() { return reference->skip_to_eof(); };
        bool skip_relative(S_I x);
        infinint get_position();

    protected:
        S_I inherited_read(char *a, size_t size) { return reference->read(a, size); };
        S_I inherited_write(const char *a, size_t size) { return reference->write(a, size); };

    private:
        generic_file *reference;
        infinint offset;
    };

    extern std::string sar_make_filename(std::string base_name, infinint num, std::string ext);

} // end of namespace

#endif