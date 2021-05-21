// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2021 Mike Sharov <msharov@users.sourceforge.net>
//
// Snownews is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// Snownews is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Snownews. If not, see http://www.gnu.org/licenses/.

#pragma once
#include "main.h"

void InitCurses (void);
void UIStatus (const char* text, int delay, int warning);
void SwapPointers (struct feed* one, struct feed* two);
void SnowSort (void);
void UISupportDrawBox (unsigned x1, unsigned y1, unsigned x2, unsigned y2);
void UISupportDrawHeader (const char* headerstring);
void UISupportURLJump (const char* url);
void SmartFeedsUpdate (void);
bool SmartFeedExists (const char* smartfeed);
void DrawProgressBar (unsigned numobjects, unsigned titlestrlen);
unsigned utf8_length (const char* s);
void add_utf8 (const char* s);
void addn_utf8 (const char* s, unsigned n);
void mvadd_utf8 (int y, int x, const char* s);
void mvaddn_utf8 (int y, int x, const char* s, unsigned n);
