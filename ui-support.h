/* Snownews - A lightweight console RSS newsreader
 * $Id: ui-support.h 1159 2005-09-13 19:51:13Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * ui-support.h
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

#ifndef UI_SUPPORT_H
#define UI_SUPPORT_H

typedef enum {
	NORMAL,
	INVERSE
} clear_line;

void InitCurses (void);
void UIStatus (const char * text, int delay, int warning);
void SwapPointers (struct feed * one, struct feed * two);
void SnowSort (void);
void UISupportDrawBox (int x1, int y1, int x2, int y2);
void UISupportDrawHeader (const char * headerstring);
void UISupportURLJump (const char * url);
void SmartFeedsUpdate (void);
void SmartFeedNewitems (struct feed * smart_feed);
int SmartFeedExists (const char * smartfeed);
void DrawProgressBar (int numobjects, int titlestrlen);
void displayErrorLog (void);
void clearLine (int line, clear_line how);

#endif
