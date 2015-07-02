/* Snownews - A lightweight console RSS newsreader
 * $Id: filters.h 175 2004-10-18 11:06:10Z kiza $
 * 
 * Copyright 2003 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * filters.h
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

#ifndef FILTERS_H
#define FILTERS_H

int FilterExecURL (struct feed * cur_ptr);
//int FilterPipe (struct feed * cur_ptr);
int FilterPipeNG (struct feed * cur_ptr);

#endif
