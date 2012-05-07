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
// $Id: dar_slave.cpp,v 1.5 2002/10/31 21:02:35 edrusb Rel $
//
/*********************************************************************/
//
#include <string>
#include <string.h>
#include <iostream.h>
#include <getopt.h>
#include "user_interaction.hpp"
#include "zapette.hpp"
#include "sar.hpp"
#include "path.hpp"
#include "tuyau.hpp"
#include "erreurs.hpp"
#include "tools.hpp"
#include "dar_suite.hpp"
#include "integers.hpp"

#define DAR_SLAVE_VERSION "1.1.0"

static bool command_line(S_I argc, char *argv[], path * &chemin, string & filename, 
			 string &input_pipe, string &output_pipe, string & execute);
static void show_usage(const char *command);
static void show_version(const char *command);
static S_I little_main(S_I argc, char *argv[]);

S_I main(S_I argc, char *argv[])
{
    return dar_suite_global(argc, argv, &little_main);
}

static S_I little_main(S_I argc, char *argv[])
{
    path *chemin = NULL;
    string filename;
    string input_pipe;
    string output_pipe;
    string execute;

    if(command_line(argc, argv, chemin, filename, input_pipe, output_pipe, execute))
    {
	tuyau *input = NULL;
	tuyau *output = NULL;
	sar *source = NULL;
	try
	{
	    source = new sar(filename, EXTENSION, SAR_OPT_DONT_ERASE, *chemin, execute);
	    if(source == NULL)
		throw Ememory("little_main");

	    tools_open_pipes(input_pipe, output_pipe, input, output);

	    slave_zapette zap = slave_zapette(input, output, source);
	    input = output = NULL; // now managed by zap;
	    source = NULL;  // now managed by zap;
	    
	    try
	    {
		zap.action();
	    }
	    catch(Erange &e)
	    {
		user_interaction_warning(e.get_message());
		throw Edata(e.get_message());
	    }
	}
	catch(...)
	{
	    delete chemin;
	    if(input != NULL)
		delete input;
	    if(output != NULL)
	       delete output;
	    if(source != NULL)
		delete source;
	    throw;
	}
	delete chemin;
	if(input != NULL)
	    delete input;
	if(output != NULL)
	    delete output;
	if(source != NULL)
	    delete source;
	
	return EXIT_OK;
    }
    else
	return EXIT_SYNTAX;
}

static bool command_line(S_I argc,char *argv[], path * &chemin, string & filename, 
			 string &input_pipe, string &output_pipe, string & execute)
{
    S_I lu;
    execute = "";

    if(argc < 1)
    {
	user_interaction_warning("cannot read arguments on command line, aborting");
	return false;
    }

    while((lu = getopt(argc, argv, "i:o:hVE:")) != EOF)
    {
	switch(lu)
	{
	case 'i':
	    if(optarg == NULL)
		throw Erange("get_args", "missing argument to -i");
	    if(input_pipe == "")
		input_pipe = optarg;
	    else
		user_interaction_warning("only one -i option is allowed, considering the first one");
	    break;
	case 'o':
	    if(optarg == NULL)
		throw Erange("get_args", "missing argument to -o");
	    if(output_pipe == "")
		output_pipe = optarg;
	    else
		user_interaction_warning("only one -o option is allowed, considering the first one");
	    break;
	case 'h':
	    show_usage(argv[0]);
	    return false;
	case 'V':
	    show_version(argv[0]);
	    return false;
	case 'E':
	    if(optarg == NULL)
		throw Erange("get_args", "missing argument to -E");
	    if(execute == "")
		execute = optarg;
	    else
		user_interaction_warning("only one -E option is allowed, ignoring other instances");
	    break;
	case ':':
	    throw Erange("get_args", string("missing parameter to option ") + char(optopt));
	case '?':
	    user_interaction_warning(string("ignoring unknown option ") + char(optopt)); 
	    break;
	default:
	    user_interaction_warning(string("ignoring unknown option ") + char(lu)); 
	}
    }
	
    if(optind + 1 > argc)
    {
	user_interaction_warning("missing archive basename, see -h option for help");
	return false;
    }
    
    if(optind + 1 < argc)
    {
	user_interaction_warning("too many argument on command line, see -h option for help");
	return false;
    }
    
    tools_split_path_basename(argv[optind], chemin, filename);
    return true;
}

static void dummy_call(char *x)
{
    static char id[]="$Id: dar_slave.cpp,v 1.5 2002/10/31 21:02:35 edrusb Rel $";
    dummy_call(id);
}

static void show_usage(const char *command)
{
    ostream & out = user_interaction_stream(); // for readability

    char *cmd = tools_extract_basename(command);
    try
    {
	out << endl;
	out << "usage : " << endl;
	out << "  command1 | " << cmd << " [options] [<path>/]basename | command2" << endl;
	out  << "  " << cmd << "[options] [-i input_pipe] [-o output_pipe] [<path>/]basename" << endl;
	out << "  " << cmd << " -h" << endl;
	out << "  " << cmd << " -V" << endl << endl;
#include "dar_slave.usage"
    }
    catch(...)
    {
	delete cmd;
	throw;
    }
    delete cmd;
}
    
static void show_version(const char *command)
{
    ostream & out = user_interaction_stream(); // for readability

    char *cmd = tools_extract_basename(command);
    try
    {
	out << endl << " " << cmd << " version " << DAR_SLAVE_VERSION << ", Copyright (C) 2002 Denis Corbin" << endl << endl;
	out << " " << __DATE__ << " with " << CC_NAT << " version " << __VERSION__ << endl;
	out << " " << cmd << " is part of the Disk ARchive suite (DAR " << dar_suite_version() << ")" << endl;
	out << " " << cmd << " comes with ABSOLUTELY NO WARRANTY; for details" << endl;
	out << " type `dar -W'.  This is free software, and you are welcome" << endl;
	out << " to redistribute it under certain conditions; type `dar -L | more'" << endl;
	out << " for details." << endl << endl;
    }
    catch(...)
    {
	delete cmd;
	throw;
    }
    delete cmd;
}

