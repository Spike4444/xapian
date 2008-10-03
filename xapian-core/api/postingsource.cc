/** @file postingsource.cc
 * @brief External sources of posting information
 */
/* Copyright (C) 2008 Olly Betts
 * Copyright (C) 2008 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include "xapian/postingsource.h"

#include "autoptr.h"

#include "database.h"
#include "document.h"
#include "xapian/document.h"
#include "xapian/error.h"
#include "xapian/queryparser.h" // For sortable_unserialise

#include "omassert.h"
#include "utils.h"

#include <cfloat>

namespace Xapian {

PostingSource::~PostingSource() { }

Xapian::weight
PostingSource::get_maxweight() const
{
    return 0;
}

Xapian::weight
PostingSource::get_weight() const
{
    return 0;
}

void
PostingSource::skip_to(Xapian::docid did, Xapian::weight min_wt)
{
    while (!at_end() && get_docid() < did) {
	next(min_wt);
    }
}

bool
PostingSource::check(Xapian::docid did, Xapian::weight min_wt)
{
    skip_to(did, min_wt);
    return true;
}

PostingSource *
PostingSource::clone() const
{
    return NULL;
}


std::string
PostingSource::get_description() const
{
    return "Xapian::PostingSource subclass";
}


ValueWeightPostingSource::ValueWeightPostingSource(Xapian::valueno valno_)
	: valno(valno_),
	  specified_max_value(DBL_MAX)
{
}

ValueWeightPostingSource::ValueWeightPostingSource(Xapian::valueno valno_,
						   double max_weight_)
	: valno(valno_),
	  specified_max_value(max_weight_)
{
}

Xapian::doccount
ValueWeightPostingSource::get_termfreq_min() const
{
    return termfreq_min;
}

Xapian::doccount
ValueWeightPostingSource::get_termfreq_est() const
{
    return termfreq_est;
}

Xapian::doccount
ValueWeightPostingSource::get_termfreq_max() const
{
    return termfreq_max;
}

Xapian::weight
ValueWeightPostingSource::get_maxweight() const
{
    return max_value;
}

Xapian::weight
ValueWeightPostingSource::get_weight() const
{
    Assert(!at_end());
    Assert(current_docid != 0);
    return current_value;
}

void
ValueWeightPostingSource::next(Xapian::weight min_wt)
{
    if (min_wt > max_value) {
	current_docid = last_docid + 1;
	return;
    }
    while (current_docid <= last_docid) {
	++current_docid;
	std::string value;

	// Open document lazily so that we don't waste time checking for
	// its existence.

	unsigned int multiplier = db.internal.size();
	Assert(multiplier != 0);
	Xapian::doccount n = (current_docid - 1) % multiplier; // which actual database
	Xapian::docid m = (current_docid - 1) / multiplier + 1; // real docid in that database

	try {
	    AutoPtr<Xapian::Document::Internal> doc;
	    doc = db.internal[n]->open_document(m, true);
	    value = doc->get_value(valno);
	    if (value.empty())
		continue;
	} catch (const Xapian::DocNotFoundError &) {
	    continue;
	}

	current_value = sortable_unserialise(value);
	// Don't check that the value is in the specified range, since this
	// could be a slow loop and isn't required.
	return;
    }
}

void
ValueWeightPostingSource::skip_to(Xapian::docid min_docid,
				  Xapian::weight min_wt)
{
    if (current_docid < min_docid) {
	current_docid = min_docid - 1;
	next(min_wt);
    }
}

bool
ValueWeightPostingSource::check(Xapian::docid min_docid,
				Xapian::weight min_wt)
{
    current_docid = min_docid;
    std::string value;
    try {
	unsigned int multiplier = db.internal.size();
	Assert(multiplier != 0);
	Xapian::doccount n = (current_docid - 1) % multiplier; // which actual database
	Xapian::docid m = (current_docid - 1) / multiplier + 1; // real docid in that database

	AutoPtr<Xapian::Document::Internal> doc;
	doc = db.internal[n]->open_document(m, true);
	value = doc->get_value(valno);
	if (value.empty())
	    return false;
    } catch (const Xapian::DocNotFoundError &) {
	return false;
    }
    current_value = sortable_unserialise(value);
    return (current_value >= min_wt);
}

bool
ValueWeightPostingSource::at_end() const
{
    return current_docid > last_docid;
}

Xapian::docid
ValueWeightPostingSource::get_docid() const
{
    return current_docid;
}

ValueWeightPostingSource *
ValueWeightPostingSource::clone() const
{
    return new ValueWeightPostingSource(valno, specified_max_value);
}

void
ValueWeightPostingSource::reset(const Database & db_)
{
    db = db_;
    current_docid = 0;
    current_value = 0.0;
    last_docid = db.get_lastdocid();
    try {
	termfreq_max = db.get_value_freq(valno);
	termfreq_est = termfreq_max;
	termfreq_min = termfreq_max;
	max_value = std::min(specified_max_value,
			sortable_unserialise(db.get_value_upper_bound(valno)));
    } catch (const Xapian::UnimplementedError &) {
	termfreq_max = db.get_doccount();
	termfreq_est = termfreq_max / 2;
	termfreq_min = 0;
	max_value = specified_max_value;
    }
}

std::string
ValueWeightPostingSource::get_description() const
{
    return "Xapian::ValueWeightPostingSource(valno=" + om_tostring(valno) + ")";
}

}
