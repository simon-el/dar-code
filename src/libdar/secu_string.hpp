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

    /// \file secu_string.hpp
    /// \brief this file contains the definition of secu_string class, a std::string like class but allocated in secure memory
    /// \ingroup Private
    ///
    /// secure memory is a allocated memory that is never swapped out (wrote to disk)
    /// the implementation relies on gcrypt_malloc_secure() call (libgcrypt)
    /// rather than relying on mlock()/munlock() posix system call.
    /// as the need for secure string is for strong encryption, there is no much
    /// interest in re-inventing the wheel as the need is dependent on gcrypt availability

#ifndef SECU_STRING_HPP
#define SECU_STRING_HPP

#include "../my_config.h"

#include <string>
#include "integers.hpp"
#include "on_pool.hpp"

namespace libdar
{

	/// \addtogroup Private
	/// @{

	/// class secu_string

	/// it manages the allocation/release of a given size block of memory
	/// and let the user feed it from a filedescriptor
	/// and access to it under with a char* pointer.
	/// if more constructor are needed (espetially toward API), tell me, but
	/// this is a non-sens to build a secure string from another data structure which is
	/// not yet secured, where from the read() method that fetches data directly from a
	/// filedescriptor (read() low-level system call) which might be a tty for example

    class secu_string : public on_pool
    {
    public:
	    /// to know if secure memory is available
	    ///
	    /// it is advised that the user program of class secu_string uses this call before using objects of that class
	    /// and if returned false, advise the user that the necessary support for secure memory
	    /// is not present, that any sensitive data may be swapped out under heavy memory load and thus
	    /// may lead secure password to be wrote to disk in clear.
	static bool is_string_secured();

	    /// constructor 1
	    ///
	    /// create the allocated string in secure memory
	    /// \param[in] size is the amount of secured memory to obtain when creating the object
	secu_string(U_I size = 0) { init(size); };

	    /// constructor 2
	    ///
	    /// create the string from a pointer to a (secure) string or from a portion of it
	secu_string(const char *ptr, U_I size) { init(size); append_at(0, ptr, size); };

	    /// the copy constructor
	secu_string(const secu_string & ref) { copy_from(ref); };

	    /// the assignment operator
	secu_string & operator = (const secu_string & ref) { clean_and_destroy(); copy_from(ref); return *this; };

	bool operator != (const std::string & ref) const { return ! (*this == ref); };
	bool operator != (const secu_string & ref) const { return ! (*this == ref); };
	bool operator == (const std::string &ref) const { return compare_with(ref.c_str(),(U_I)(ref.size())); };
	bool operator == (const secu_string &ref) const { return compare_with(ref.mem, *(ref.string_size)); };

	    /// the destructor (set memory to zero before releasing it)
	~secu_string() { clean_and_destroy(); };

	    /// fill the object with data
	    ///
	    /// set at most size bytes of data directly from the filedescriptor,
	    /// \param[in] fd the filedescriptor to read data from
	    /// \param[in] size is the maximum number of byte read
	    /// \note each call to set clears and reset the string, as well as the allocation size
	void set(int fd, U_I size);

	    /// append some data to the string at a given offset
	    ///
	    /// \param[in] offset defines at which offset in the secu_string will be placed the string to append
	    /// \param[in] ptr is the address of the string to append
	    /// \param[in] size is the number of byte to append
	    /// \note this call does not change the allocation size, (unlike read()), it adds the data pointed by the arguments
	    /// to the object while there is enough place to do so.
	    /// clear_and_resize() must be used first to define enough secure memory to append the expected amount of data
	    /// in one or several call to append.
	void append_at(U_I offset, const char *ptr, U_I size);

	    /// append some data to the string
	void append_at(U_I offset, int fd, U_I size);

	    /// append some data at the end of the string
	void append(const char *ptr, U_I size) { append_at(*string_size, ptr, size); };

	    /// append some data at the end of the string
	void append(int fd, U_I size) { append_at(*string_size, fd, size); };
	    /// shorten the string (do not change the allocated size)
	    /// \param[in] pos is the length of the string to set, it must be smaller or equal to the current size
	void reduce_string_size_to(U_I pos);

	    /// clear the string (set its allocated string to zero)
	void clear() { clean_and_destroy(); init(0); };

	    /// clear and resize the string to the defined allocated size
	    ///
	    /// \param[in] size is the amount of secure memory to allocated
	void clear_and_resize(U_I size) { clean_and_destroy(); init(size); };

	    /// clear and keep same storage space
	void clear_and_not_resize() {  string_size = 0; };

	    /// clear and allocate a given size area filled with random numbers
	void clear_and_randomize(U_I size);

	    /// get access to the secure string

	    /// \return the address of the first byte of the string
	    /// \note check the "size" method to know how much bytes can be read
	    /// \note the string is NULL terminated
	const char*c_str() const { return mem == NULL ? throw SRC_BUG : mem; };

	    /// get the size of the string
	U_I size() const { return *string_size; }; // returns the size of the string

	    /// get the size of the allocated secure space
	U_I max_size() const { return *allocated_size; };

    private:
	U_I *allocated_size;
	char *mem;
	U_I *string_size;

	void init(U_I size);   //< to be used at creation time or after clean_and_destroy() only
	void copy_from(const secu_string & ref); //< to be used at creation time or after clean_and_destroy() only
	bool compare_with(const char *ptr, U_I size) const;
	void clean_and_destroy();
    };

	/// @}

} // end of namespace

#endif
