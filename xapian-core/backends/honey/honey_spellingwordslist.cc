/** @file honey_spellingwordslist.cc
 * @brief Iterator for the spelling correction words in a honey database.
 */
/* Copyright (C) 2004,2005,2006,2007,2008,2009,2017 Olly Betts
 * Copyright (C) 2007 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#include <config.h>

#include "honey_spellingwordslist.h"

#include "xapian/error.h"
#include "xapian/types.h"

#include "debuglog.h"
#include "honey_database.h"
#include "pack.h"
#include "stringutils.h"

using namespace std;

HoneySpellingWordsList::~HoneySpellingWordsList()
{
    LOGCALL_DTOR(DB, "HoneySpellingWordsList");
    delete cursor;
}

Xapian::termcount
HoneySpellingWordsList::get_approx_size() const
{
    // This is an over-estimate, but we only use this value to build a balanced
    // or-tree, and it'll do a decent enough job for that.
    return database->spelling_table.get_entry_count();
}

string
HoneySpellingWordsList::get_termname() const
{
    LOGCALL(DB, string, "HoneySpellingWordsList::get_termname", NO_ARGS);
    Assert(cursor);
    Assert(!at_end());
    Assert(!cursor->current_key.empty());
    Assert(cursor->current_key[0] == 'W');
    RETURN(cursor->current_key.substr(1));
}

Xapian::doccount
HoneySpellingWordsList::get_termfreq() const
{
    LOGCALL(DB, Xapian::doccount, "HoneySpellingWordsList::get_termfreq", NO_ARGS);
    Assert(cursor);
    Assert(!at_end());
    Assert(!cursor->current_key.empty());
    Assert(cursor->current_key[0] == 'W');
    cursor->read_tag();

    Xapian::termcount freq;
    const char *p = cursor->current_tag.data();
    if (!unpack_uint_last(&p, p + cursor->current_tag.size(), &freq)) {
	throw Xapian::DatabaseCorruptError("Bad spelling word freq");
    }
    RETURN(freq);
}

Xapian::termcount
HoneySpellingWordsList::get_collection_freq() const
{
    throw Xapian::InvalidOperationError("HoneySpellingWordsList::"
					"get_collection_freq() "
					"not meaningful");
}

TermList *
HoneySpellingWordsList::next()
{
    LOGCALL(DB, TermList *, "HoneySpellingWordsList::next", NO_ARGS);
    Assert(!at_end());

    cursor->next();
    if (cursor->after_end() || !startswith(cursor->current_key, 'W')) {
	// We've reached the end of the prefixed terms.
	delete cursor;
	cursor = NULL;
    }

    RETURN(NULL);
}

TermList *
HoneySpellingWordsList::skip_to(const string &tname)
{
    LOGCALL(DB, TermList *, "HoneySpellingWordsList::skip_to", tname);
    Assert(!at_end());

    if (!cursor->find_entry_ge("W" + tname)) {
	// The exact term we asked for isn't there, so check if the next
	// term after it also has a W prefix.
	if (cursor->after_end() || !startswith(cursor->current_key, 'W')) {
	    // We've reached the end of the prefixed terms.
	    delete cursor;
	    cursor = NULL;
	}
    }
    RETURN(NULL);
}

bool
HoneySpellingWordsList::at_end() const
{
    LOGCALL(DB, bool, "HoneySpellingWordsList::at_end", NO_ARGS);
    RETURN(cursor == NULL);
}
