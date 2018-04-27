// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
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

#include <ncurses.h>

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#include "config.h"
#include "support.h"

extern struct keybindings keybindings;
extern struct color color;

/* Snowflake for the xmas about box. */
struct snowflake {
	int x;
	int y;
	int oldx;
	int oldy;
	char oldchar;
	char oldchar2;
	int visible;		/* Don't draw flakes over text. */
	int vspeed;			/* Vertical speed of the flake */
	int hspeed;			/* Horizontal speed */
	struct snowflake * next;
	struct snowflake * prev;
};

/* Santa Hunta */
typedef struct shot {
	int x;
	int y;
	int fired;
} shot;
typedef struct santa {
	int x;
	int y;
	int height;
	int length;
	int speed;
	int anim;
	char *gfx;
	char *gfx_line2;
	char *altgfx;
	char *altgfx_line2;
} santa;
typedef struct scoreDisplay {
	int score;
	int x;
	int y;
	int rounds;
} scoreDisplay;

/* This draws the colored parts of the christmas tree.
   Called everytime during SnowFall to avoid to much duplicate code.
   Use sparsly as snowflakes will vanish behind this elements. */
void ChristmasTree (void) {
	attron (COLOR_PAIR(13));
	mvaddch (6, 13, '&');
	mvaddch (16, 5, '&');
	attroff (COLOR_PAIR(13));
	attron (COLOR_PAIR(10));
	mvaddch (9, 10, '&');
	mvaddch (19, 12, '&');
	attroff (COLOR_PAIR(10));
	attron (COLOR_PAIR(15));
	mvaddch (12, 14, '&');
	mvaddch (18, 6, '&');
	attroff (COLOR_PAIR(15));
	attron (COLOR_PAIR(14));
	mvaddch (16, 15, '&');
	mvaddch (13, 10, '&');
	attroff (COLOR_PAIR(14));
}

/* This function sometimes deletes characters. Maybe oldchar doesn't
   get saved correctly everytime... But who cares!? :P */
void Snowfall (int christmastree) {
	struct snowflake *cur;
	struct snowflake *first = NULL;
	struct snowflake *new;
	struct snowflake *curnext;
	/* The idea behind wind:
	   Determine windtimer and wait that many rounds before we initially
	   select any windspeed. If we've waited enough rounds (windtimer==0)
	   select a windspeed and windtimer (which should be much less than
	   between the storms). Use (slightly randomly offset) windspeed to
	   blow snowflakes around (to left or right). If windtimer drops to 0
	   again select new windtimer, wait, repeat. */
	int windspeed = 0;
	int windtimer = 0;
	int wind = 1;			/* 1: wind active, 0: inactive 
							   set to 1 here so the first loop run clears it. */
	int newflake = 0;		/* Time until a new flake appears. */

	/* Set ncurses halfdelay mode. */
	halfdelay (3);

	/* White snowflakes! */
	/* Doesn't work on white terminal background... obviously.
	attron (COLOR_PAIR(16));
	attron (WA_BOLD); */

	while (1) {
		/* Set up the storm. */
		if (windtimer == 0) {
			if (wind) {
				/* Entering silence */
				windtimer = 10 + ((float)rand() / (float)RAND_MAX * 50);
				wind = 0;
				windspeed = 0;
			} else {
				/* Entering storm. */
				windtimer = 10 + ((float)rand() / (float)RAND_MAX * 20);
				wind = 1;
				windspeed = (1+(float)rand() / (float)RAND_MAX * 11)-6;
			}
		}
		/*
		mvaddstr(2,1,     "                              ");
		if (wind)
			mvprintw (2,1,"Windspeed: %d; rounds left: %d",windspeed,windtimer);
		else
			mvprintw (2,1,"No wind; rounds left: %d",windtimer);
		*/

		/* Add new snowflakes. */
		if (newflake == 0) {
			/* Add new flake to pointer chain with random x offset. */
			new = malloc (sizeof (struct snowflake));
			new->y = 0;
			new->x = (float)rand() / (float)RAND_MAX * COLS;
			new->oldx = new->x;
			new->oldy = new->y;
			new->visible = 1;
			new->oldchar = new->oldchar2 = ' ';
			new->vspeed = 1+(float)rand() / (float)RAND_MAX * 2;
			new->hspeed = (1+(float)rand() / (float)RAND_MAX * 7)-4;

			/* Add our new snowflake to the pointer chain. */
			new->next = NULL;
			if (first == NULL) {
				new->prev = NULL;
				first = new;
			} else {
				new->prev = first;
				while (new->prev->next != NULL)
					new->prev = new->prev->next;
				new->prev->next = new;
			}

			/* Set new counter until next snowflake. */
			newflake = 1+(float)rand() / (float)RAND_MAX * 2;
			/*
			mvaddstr (1,1,"      ");
			mvprintw (1,1,"New flake in %d rounds.", newflake);
			*/
		}

		for (cur = first; cur != NULL; cur = curnext) {
			curnext = cur->next;
			/* Draw every snowflake at its coordinates to the screen. */
			if (cur->visible) {
				/* Only draw if y<=LINES. This makes some snow lie on bottom of screen. */
				if (cur->y <= LINES) {
					/* Restore old char with proper color.
					   See also ChristmasTree() above. */
					if (((cur->oldx < 25) && (cur->oldy > 5)) ||
						(cur->oldx < 20))
						attron (COLOR_PAIR(11));
					if ((cur->oldx < 14) && (cur->oldx > 9) && (cur->oldy < 24) && (cur->oldy > 20)) {
						attroff (COLOR_PAIR(11));
						attron (COLOR_PAIR(12));
					}
					mvaddch (cur->oldy, cur->oldx, cur->oldchar2);
					if (((cur->oldx < 25) && (cur->oldy > 5)) ||
						(cur->oldx < 20))
						attroff (COLOR_PAIR(11));
					if ((cur->oldx < 14) && (cur->oldx > 9) && (cur->oldy < 24) && (cur->oldy > 20)) {
						attroff (COLOR_PAIR(12));
					}

					if (christmastree)
						ChristmasTree();
				}
				mvaddch (cur->y, cur->x, '*');
				cur->oldx = cur->x;
				cur->oldy = cur->y;
			}
			/* Set new hspeed for flake */
			cur->hspeed = (1+(float)rand() / (float)RAND_MAX * 7)-4;

			/* Advance every flake downwards by a random amount and to
			   the left or right.
			   Check if the next position would obscure a character on the screen
			   and set visible to 0 in this case. Clear visible flag as needed. */
			cur->y += cur->vspeed;
			if (wind)
				cur->hspeed += windspeed;
			cur->x += cur->hspeed;

			if (cur->y > LINES) {
				if (cur == first) {
					first = first->next;
					first->prev = NULL;
				} else if (cur->next == NULL) {
					cur->prev->next = NULL;
				} else {
					cur->prev->next = cur->next;
					cur->next->prev = cur->prev;
				}
				free (cur);
				continue;
			}

			/* Only draw if we're still inside the window. */
			if (cur->y <= LINES) {
				/*
				mvaddstr (3,1,"                ");
				mvprintw (3,1,"Flake hspeed: %d",cur->hspeed);
				*/
				cur->oldchar2 = cur->oldchar;
				/* Reset to ' ' if we accidently set it to *. */
				if (cur->oldchar2 == '*')
					cur->oldchar2 = ' ';
				if ((cur->x <= COLS) && (cur->y <= LINES))
					cur->oldchar = mvinch (cur->y, cur->x);
				else
					cur->oldchar = ' ';
			}
		}

		windtimer--;
		newflake--;

		refresh();

		/* Leave loop if anykey(tm) was pressed. */
		if (getch() != ERR)
			break;
	}
	/* Leave halfdelay mode. */
	cbreak();
}

void xmasAbout (void) {
	/* Logo */
	mvaddstr (1,  0, "                      _____ _____    ____ _______ _____   ____ _______   _____");
	mvaddstr (2,  0, "                     /  __/ \\    \\  / __ \\\\ |  | \\\\    \\ /  _/ \\ |  | \\ /  __/");
	mvaddstr (3,  0, "                     \\___ \\  \\ |  \\ \\ \\/ / \\     / \\ |  \\\\ __\\  \\     / \\___ \\");
	mvaddstr (4,  0, "                     /____/  /_|__/  \\__/  /__|_/  /_|__/ \\___\\ /__|_/  /____/");
	/* Christmas tree. */
	attron (COLOR_PAIR(11));
	mvaddstr (1,  0, "");
	mvaddstr (2,  0, "");
	mvaddstr (3,  0, "          _\\/_");
	mvaddstr (4,  0, "           /\\");
	mvaddstr (5,  0, "          /  \\");
	mvaddstr (6,  0, "         /   &\\");
	mvaddstr (7,  0, "        /      \\");
	mvaddstr (8,  0, "       /        \\");
	mvaddstr (9,  0, "      /   &      \\");
	mvaddstr (10, 0, "     /            \\");
	mvaddstr (11, 0, "    /              \\");
	mvaddstr (12, 0, "   /   /      &     \\");
	mvaddstr (13, 0, "  /___/   &      \\___\\");
	mvaddstr (14, 0, "     /            \\");
	mvaddstr (15, 0, "    /              \\");
	mvaddstr (16, 0, "   / &         &    \\");
	mvaddstr (17, 0, "  /                  \\");
	mvaddstr (18, 0, " /    &               \\");
	mvaddstr (19, 0, "/____/      &      \\___\\");
	mvaddstr (20, 0, "    /         \\     \\");
	mvaddstr (21, 0, "   /______####_\\_____\\");
	mvaddstr (22, 0, "         |####|");
	mvaddstr (23, 0, "         |####|");
	attroff (COLOR_PAIR(11));
	attron (COLOR_PAIR(12));
	mvaddstr (21, 10, "####");
	mvaddstr (22, 10, "####");
	mvaddstr (23, 10, "####");
	attroff (COLOR_PAIR(12));
	ChristmasTree();
	/* Credits. */
	mvprintw (5, 21, "Version %s", VERSION);
	mvprintw (8, 29, _("Merry Christmas from the Snownews developers."));
	mvaddstr (10, 29, _("Main code"));
	mvaddstr (11, 29, "Oliver Feiler");
	mvaddstr (13, 29, _("Additional code"));
	mvaddstr (14, 29, "Rene Puls");
	mvaddstr (16, 29, _("Translation team"));
	mvaddstr (17, 29, "Oliver Feiler, Frank van der Loo,");
	mvaddstr (18, 29, "Pascal Varet, Simon Isakovic");
	mvaddstr (19, 29, "Fernando J. Pereda, Marco Cova");
	mvaddstr (20, 29, "Cheng-Lung Sung, Dmitry Petukhov");
	mvaddstr (21, 29, "Douglas Campos");

	Snowfall(1);
}

/***************
 * Santa Hunta *
 ***************/
void SHDrawGun (int gun_pos) {
	move (LINES-3, 0);
	clrtoeol();
	move (LINES-2, 0);
	clrtoeol();

	attron (WA_BOLD);
	mvaddstr (LINES-3, gun_pos-3, "___/\\___");
	mvaddstr (LINES-2, gun_pos-3, "|######|");
	attroff (WA_BOLD);
}

void SHDrawStatus (void) {
	attron (WA_BOLD);
	mvprintw (LINES-1, 1, _("Move: cursor or %c/%c; shoot: space; quit: %c"),
	          keybindings.prev, keybindings.next, keybindings.quit);
	move (LINES-1, COLS-1);
	attroff (WA_BOLD);
}

void SHDrawScore (int score, int level) {
	int i, len;
	char scorestr[16];
	char levelstr[16];

	attron (WA_REVERSE);
	for (i = 0; i < COLS; i++) {
		mvaddch (0, i, ' ');
	}
	mvaddstr (0, 1, "Santa Hunta!");

	snprintf (scorestr, sizeof(scorestr), _("Score: %d"), score);
	len = strlen (scorestr);
	mvaddstr (0, COLS-1-len, scorestr);

	snprintf (levelstr, sizeof(levelstr), _("Level: %d"), level);
	len = strlen (levelstr);
	mvaddstr (0, COLS/2-len/2, levelstr);

	attroff (WA_REVERSE);
}

void SHClearScreen (void) {
	int i;

	for (i = 0; i < LINES; i++) {
		move (i, 0);
		clrtoeol();
	}
}

void SHDrawProjectile (shot shot) {
	attron (WA_BOLD);
	mvaddstr (shot.y, shot.x, "|");
	attroff (WA_BOLD);
}

void SHDrawSanta (santa santa) {
	int len;
	char *draw_string;

	len = COLS - santa.x;

	if (santa.anim == 0) {
		if (santa.x < 0) {
			draw_string = santa.gfx+abs(santa.x);
			mvaddnstr (santa.y, 0, draw_string, len);
		} else 
			mvaddnstr (santa.y, santa.x, santa.gfx, len);
		if (santa.x < 0) {
			draw_string = santa.gfx_line2+abs(santa.x);
			mvaddnstr (santa.y+1, 0, draw_string, len);
		} else
			mvaddnstr (santa.y+1, santa.x, santa.gfx_line2, len);
	} else {
		if (santa.x < 0) {
			draw_string = santa.altgfx+abs(santa.x);
			mvaddnstr (santa.y, 0, draw_string, len);
		} else
			mvaddnstr (santa.y, santa.x, santa.altgfx, len);
		if (santa.x < 0) {
			draw_string = santa.altgfx_line2+abs(santa.x);
			mvaddnstr (santa.y+1, 0, draw_string, len);
		} else
			mvaddnstr (santa.y+1, santa.x, santa.altgfx_line2, len);
	}

	attron (COLOR_PAIR(10));
	mvaddch (santa.y, santa.x + santa.length - 1, '*');
	attroff (COLOR_PAIR(10));
}

void newSanta (santa * santa, int level) {
	santa->x = -27;
	santa->y = 1+((float)rand() / (float)RAND_MAX * (LINES-7));
	santa->speed = level + ((float)rand() / (float)RAND_MAX * 3);
	santa->anim = 0;
	santa->height = 2;
	santa->length = 27;
	santa->gfx =          "##___  __-+---+/*__-+---+/*";
	santa->gfx_line2 =    "_|__|_)   /\\ /\\     /\\ /\\  "; /* Append 2 spaces! */
	santa->altgfx =       "##___  __-+---+/*__-+---+/*";
	santa->altgfx_line2 = "_|__|_)   /|  |\\    /|  |\\ ";   /* Append 1 space! */
}

int SHHit (shot shot, santa santa) {
	if ((shot.x >= santa.x) && (shot.x <= santa.x + santa.length) &&
		((shot.y == santa.y) || (shot.y == santa.y+1)))
		return 1;
	else
		return 0;
}

int SHAddScore (santa santa) {
	return 100 / santa.y * santa.speed;
}

void SHDrawHitScore (scoreDisplay score) {
	int rand_color;

	rand_color = 10 + ((float)rand() / (float)RAND_MAX * 6);

	attron (WA_BOLD);
	attron (COLOR_PAIR(rand_color));
	mvprintw (score.y, score.x, "%d", score.score);
	attroff (COLOR_PAIR(rand_color));
	attroff (WA_BOLD);
}

void printFinalScore (int score) {
	int i, j, pos, number_count;
	char *numbers[10][5] = {
		{"_______","|  _  |","| | | |","| |_| |","|_____|"}, /* 0 */
	    {"  ____ "," /   | ","/_/| | ","   | | ","   |_| "}, /* 1 */
	    {"_______","|___  |","| ____|","| |____","|_____|"}, /* 2 */
	    {"_______","|___  |"," ___  |","___|  |","|_____|"}, /* 3 */
	    {"___ ___","| |_| |","|____ |","    | |","    |_|"}, /* 4 */
	    {"_______","|  ___|","|___  |","____| |","|_____|"}, /* 5 */
	    {"    __ ","   / / "," /    \\","|  O  /"," \\___/ "}, /* 6 */
	    {"_______","|___  |","    | |","    | |","    |_|"}, /* 7 */
	    {"_______","|  _  |","| |_| |","| |_| |","|_____|"}, /* 8 */
	    {"  ___  "," /   \\ ","|  O  |"," \\   / ","  \\__\\ "} /* 9 */
	};
	int y;
	int divisor;
	int digit;

	if (score == 0)
		number_count = 1;
	else
		number_count = log10(score) + 1;


	pos = COLS/2 - ((number_count * 8) / 2);

	for (i = 0; i < number_count; i++) {
		y = 12;
		divisor = pow (10, number_count-1-i);
		digit = score / divisor;
		score -= digit * divisor;

		for (j = 0; j < 5; j++) {
			mvaddstr (y, pos + (i * 8), numbers[digit][j]);
			y++;
		}
	}
	refresh();
}

void SHFinalScore (int score) {
	int rand_color;

	attron (WA_BOLD);
	mvaddstr (9, COLS/2-6, "Final score:");
	refresh();
	sleep(1);


	for (;;) {
		if (getch() == 'q')
			break;

		rand_color = 10 + ((float)rand() / (float)RAND_MAX * 6);
		attron (WA_BOLD);
		attron (COLOR_PAIR(rand_color));
		printFinalScore(score);
		attroff (COLOR_PAIR(rand_color));
		attroff (WA_BOLD);

		move (LINES-1, COLS-1);
		refresh();
	}
	attroff (WA_BOLD);
}

void santaHunta (void) {
	int input;
	int score = 0;
	int last_score;
	int gun_pos = COLS/2;
	int count = 0;
	int level = 1;
	struct timeval before;
	struct timeval after;
	int draw_loop = 0;
	long draw_delay = 0;
	shot shot;
	santa santa;
	int targets = 0;
	int hitcount = 0;
	scoreDisplay scoreDisplay;

	shot.fired = 0;
	shot.x = 0;
	shot.y = 0;

	scoreDisplay.rounds = 0;

	/* Set ncurses halfdelay mode.
	 * Max resolution is 1/10sec */
	halfdelay (1);

	for (;;) {
		gettimeofday (&before, NULL);
		input = getch();
		gettimeofday (&after, NULL);

		if (after.tv_sec > before.tv_sec)
			after.tv_usec += 1000000;

		if (!targets) {
			newSanta(&santa, level);
			targets = 1;
		}

		/* Pad drawing resolution to 1/10sec */
		draw_delay += after.tv_usec - before.tv_usec;
		if (draw_delay > 100000) {
			draw_delay = 0;
			draw_loop++;

			SHClearScreen();

			if (targets)
				SHDrawSanta(santa);

			if (santa.anim == 0)
				santa.anim = 1;
			else
				santa.anim = 0;

			if (santa.x >= COLS)
				targets = 0;

			if (shot.fired)
				SHDrawProjectile(shot);

			if (scoreDisplay.rounds > 0)
				SHDrawHitScore(scoreDisplay);

			if (SHHit(shot, santa)) {
				targets = 0;
				hitcount++;
				last_score = SHAddScore(santa);
				score += last_score;

				scoreDisplay.x = shot.x;
				scoreDisplay.y = shot.y;
				scoreDisplay.score = last_score;
				scoreDisplay.rounds = 20;
			}

			santa.x += santa.speed;

			scoreDisplay.rounds--;

			shot.y--;
			if (shot.y == 0)
				shot.fired = 0;

			if (hitcount == 10) {
				hitcount = 0;
				level++;
			}
		}

		count++;

		if ((input == KEY_RIGHT) || (input == keybindings.next)) {
			if (gun_pos < COLS-5)
				gun_pos++;
		} else if ((input == KEY_LEFT) || (input == keybindings.prev)) {
			if (gun_pos > 3)
				gun_pos--;
		} else if (input == ' ') {
			if (!shot.fired) {
				attron (WA_BOLD);
				attron (COLOR_PAIR(12));
				mvaddstr (LINES-4, gun_pos-2, "\\***/");
				attroff (COLOR_PAIR(12));
				attroff (WA_BOLD);

				shot.x = gun_pos;
				shot.y = LINES-4;
				shot.fired = 1;
			}
		} else if (input == keybindings.quit) {
			SHFinalScore(score);
			break;
		}

		SHDrawGun(gun_pos);
		SHDrawScore(score, level);
		SHDrawStatus();

		refresh();
	}

	/* Leave halfdelay mode. */
	cbreak();
}

void UIAbout (void) {
	int xpos;

	clear();				/* Get the crap off the screen to make room
							   for our wonderful ASCII logo. :) */

	xpos = COLS/2 - 40;

	if (COLS < 80) {
		mvprintw (0, 0, _("Need at least 80 COLS terminal, sorry!"));
		refresh();
		getch();
		return;
	}

	if (easterEgg()) {
		santaHunta();
	} else {
		/* 80 COLS logo */
		mvaddstr (2,   xpos, "  ________ _______     ____ __________ _______     ______ __________   ________");
		mvaddstr (3, xpos, " /  _____/ \\      \\   /    \\\\  |    | \\\\      \\   /  ___/ \\  |    | \\ / ______/");
		mvaddstr (4, xpos, " \\____  \\   \\   |  \\ /  /\\  \\\\   |    / \\   |  \\ /     /   \\   |    / \\____  \\");
		mvaddstr (5, xpos, " /       \\  /   |   \\\\  \\/  / \\  |   /  /   |   \\\\  ___\\    \\  |   /  /       \\");
		mvaddstr (6, xpos, "/______  / / ___|___/ \\____/  /__|__/  / ___|___/ \\____ \\   /__|__/  /______  /");
		mvaddstr (7, xpos, "       \\/  \\/                          \\/              \\/                   \\/");
		mvprintw (9, COLS/2-(strlen("Version")+strlen(VERSION)+1)/2, "Version %s", VERSION);

		mvaddstr (11, COLS/2-(strlen(_("Brought to you by:")))/2, _("Brought to you by:"));
		mvaddstr (13, COLS/2-(strlen(_("Main code")))/2, _("Main code"));
		mvaddstr (14, COLS/2-6, "Oliver Feiler");
		mvaddstr (16, COLS/2-(strlen(_("Additional code")))/2, _("Additional code"));
		mvaddstr (17, COLS/2-4, "Rene Puls");
		mvaddstr (19, COLS/2-(strlen(_("Translation team")))/2, _("Translation team"));
		mvaddstr (20, COLS/2-31, "Oliver Feiler, Frank van der Loo, Pascal Varet, Simon Isakovic,");
		mvaddstr (21, COLS/2-32, "Fernando J. Pereda, Marco Cova, Cheng-Lung Sung, Dmitry Petukhov");
		mvaddstr (22, COLS/2-26, "Douglas Campos, Ray Iwata, Piotr Ozarowski, Yang Huan");
		mvaddstr (23, COLS/2-15, "Ihar Hrachyshka, Mats Berglund");

		refresh();
		getch();
	}
}
