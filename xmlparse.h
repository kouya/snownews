/* Snownews - A lightweight console RSS newsreader
 * $Id: xmlparse.h 92 2004-10-06 10:29:54Z kiza $
 * 
 * Copyright 2003-2004 Rene Puls <rpuls@gmx.net>
 * Written for Snownews.
 *
 * http://kiza.kcore.de/software/snownews/
 *
 * xmlparse.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef XMLPARSE_H
#define XMLPARSE_H

#include <libxml/parser.h>

void parse_rdf10_item(struct feed *feed, xmlDocPtr doc, xmlNodePtr node);
void parse_rdf10_channel(struct feed * feed, xmlDocPtr doc, xmlNodePtr node);
void parse_rdf20_channel(struct feed * feed, xmlDocPtr doc, xmlNodePtr node);
int DeXML (struct feed * cur_ptr);
void print_feed(struct feed * feed);

#endif
