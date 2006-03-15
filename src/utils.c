/***************************************************************************
 *  Pinfo is a ncurses based lynx style info documentation browser
 *
 *  Copyright (C) 1999  Przemek Borys <pborys@dione.ids.pl>
 *  Copyright (C) 2005  Bas Zoetekouw <bas@debian.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 *  USA
 ***************************************************************************/

#include "common_includes.h"

RCSID("$Id$")

#include <regex.h>
#include <ctype.h>
#include <sys/select.h>

#ifdef USE_WCHAR
  #include <wchar.h>
#endif

char *safe_user = "nobody";
char *safe_group = "nogroup";

#ifndef HAVE_CURS_SET
void
curs_set(int a)
{
}
#endif

#ifdef ___DONT_USE_REGEXP_SEARCH___
char *pinfo_re_pattern = 0;
#else
int pinfo_re_offset = -1;
#endif

#ifdef HAS_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <term.h>
/* HAS_READLINE */
#endif


/*
 * the bellow define enables malloc/realloc/free logging to stderr.
 * They start to log their argument values.
 *
 * #define ___DEBUG___
 *
 */

#ifdef ___DEBUG___
unsigned long malloc_addr[1000];
unsigned long msizes[1000];
long addrescount = 0;
/* ___DEBUG___ */
#endif


int curses_open = 0;

int shell_cursor = 1;

void
xfree(void *ptr)
{
#ifdef ___DEBUG___
	int i, j;
	int flag = 0;
	unsigned long msize = 0;
	for (i = 0; i < addrescount; i++)
		msize += msizes[i];
	fprintf(stderr, "Size: %lu, count: %ld, freeing %lu\n", msize, addrescount,(unsigned long) ptr);
	for (i = 0; i < addrescount; i++)
		if (malloc_addr[i] ==(unsigned long) ptr)
		{
			flag = 1;
			for (j = i + 1; j < addrescount; j++)
			{
				malloc_addr[j - 1] = malloc_addr[j];
				msizes[j - 1] = msizes[j];
			}
			addrescount--;
			break;
		}
	if (flag == 0)
	{
		fprintf(stderr, "ERROR!!!\n");
		getchar();
	}
/* ___DEBUG___ */
#endif
	free(ptr);
}

void *
xmalloc(size_t size)
{
	register void *value = malloc(size);
#ifdef ___DEBUG___
	unsigned long msize = 0;
	int i;
/* ___DEBUG___ */
#endif
	if (value == 0)
	{
		closeprogram();
		printf(_("Virtual memory exhausted\n"));
		exit(1);
	}
#ifdef ___DEBUG___
	for (i = 0; i < addrescount; i++)
		msize += msizes[i];
	fprintf(stderr, "Size %lu, count: %ld, allocated %lu\n", msize, addrescount,(unsigned long) value);
	malloc_addr[addrescount] =(unsigned long) value;
	msizes[addrescount] = size;
	if (addrescount < 1000)
		addrescount++;
	else
	{
		fprintf(stderr, "trace buffer exhausted\n");
	}
/* ___DEBUG___ */
#endif
	memset(value, 0, size);
	return value;
}

void *
xrealloc(void *ptr, size_t size)
{
#ifdef ___DEBUG___
	int i, j, flag = 0;
	register void *value;
	unsigned long msize = 0;
	for (i = 0; i < addrescount; i++)
		msize += msizes[i];
	fprintf(stderr, "Size: %lu, count: %ld, reallocating %lu to ", msize, addrescount,(unsigned long) ptr);
	for (i = 0; i < addrescount; i++)
		if (malloc_addr[i] ==(unsigned long) ptr)
		{
			flag = 1;
			for (j = i + 1; j < addrescount; j++)
			{
				malloc_addr[j - 1] = malloc_addr[j];
				msizes[j - 1] = msizes[j];
			}
			addrescount--;
			break;
		}
	if (flag == 0)
	{
		fprintf(stderr, "ERROR!!!\n");
		getchar();
	}
	value = realloc(ptr, size);
#else
	register void *value = realloc(ptr, size + 1024);
/* ___DEBUG___ */
#endif
	if (value == 0)
	{
		closeprogram();
		printf(_("Virtual memory exhausted\n"));
		exit(1);
	}
#ifdef ___DEBUG___
	fprintf(stderr, "%lu, with size %lu\n",(unsigned long) value,(unsigned long) size);
	malloc_addr[addrescount] =(unsigned long) value;
	msizes[addrescount] = size;
	if (addrescount < 1000)
		addrescount++;
	else
	{
		fprintf(stderr, "trace buffer exhausted\n");
	}
/* ___DEBUG___ */
#endif
	return value;
}

inline void
initlocale()
{
#ifdef ___DEBUG___
	int i;
	for (i = 0; i < 1000; i++)
		malloc_addr[i] = 0;
/* ___DEBUG___ */
#endif
	sbrk(100000);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
}

void
mymvhline(int y, int x, char ch, int len)
{
	int i;
	for (i = 0; i < len; i++)
		mvaddch(y, x + i, ch);
}

void
checkfilename(char *filename)
{
	if ((strchr(filename, '<')) ||
			(strchr(filename, '>')) ||
			(strchr(filename, '|')) ||
			(strchr(filename, '(')) ||
			(strchr(filename, ')')) ||
			(strchr(filename, '!')) ||
			(strchr(filename, '`')) ||
			(strchr(filename, '&')) ||
			(strchr(filename, ';')))
	{
		printf(_("Illegal characters in filename!\n*** %s\n"), filename);
		exit(1);
	}
}

#ifdef HAS_READLINE
/* custom function that readline will use to display text */
void
my_rl_display()
{
	/* go to the bottom line, empty it, and print the prompt and buffer */
	attrset(bottomline);
	mymvhline(maxy - 1, 0, ' ', maxx);
	move(maxy-1,0);
	printw("%s%s", rl_prompt, rl_line_buffer);
	refresh();
}
#endif

char *
getstring(char *prompt)
{
	char *buf;

#ifdef HAS_READLINE

	curs_set(1);
	move(maxy - 1, 0);
	refresh();

	rl_readline_name = PACKAGE;
	
	/* set display function for readline to my_rl_display and call readline */
	rl_redisplay_function = my_rl_display;
	buf = readline(prompt);
	if (buf && *buf) 
		add_history(buf);
	
	curs_set(0);

#else

	move(maxy - 1, 0);
	buf = readlinewrapper(prompt);

#endif

	return buf;
}

void
init_curses()
{
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	/*  meta(stdscr, TRUE); */
	initcolors();
	shell_cursor = curs_set(0);
#ifdef CURSES_MOUSE
	mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED, NULL);
#endif
	curses_open = 1;
}


void
closeprogram()
{
	if (curses_open)
		myendwin();
	if (ClearScreenAtExit)
		system("clear");
	else
		printf("\n");
	if (tmpfilename1)
	{
		unlink(tmpfilename1);
		xfree(tmpfilename1);
	}
	if (tmpfilename2)
	{
		unlink(tmpfilename2);
		xfree(tmpfilename2);
	}
}

int
gettagtablepos_search_internal(char *node, int left, int right)
{
	/* left+(right-left)/2 */
	int thispos = left +((right - left) >> 1);
	int compare_result = compare_tag_table_string(tag_table[thispos].nodename, node);
	if (compare_result == 0)
		return thispos;
	else
	{
		if (left == right)
			return -1;
		if (compare_result > 0)
		{
			if (thispos > left)
				return gettagtablepos_search_internal(node, left, thispos - 1);
			else
				return -1;
		}
		else if (compare_result < 0)
		{
			if (thispos < right)
				return gettagtablepos_search_internal(node, thispos + 1, right);
			else
				return -1;
		}
	}
	return -1;
}

int
gettagtablepos(char *node)
{
	/* strip spaces from the beginning */
	while (1)
	{
		if ((*node != ' ') &&(*node != '\t'))
			break;
		node++;
	}
	return gettagtablepos_search_internal(node, 1, TagTableEntries);
}

int
pinfo_getch()
{
	int key = getch();
	/* following key will be alt's value */
	if (key == META_KEY)
	{
		key = getch();
		key |= 0x200;
	}
	return key;
}

void
waitforgetch()
{
	fd_set rdfs;
	FD_ZERO(&rdfs);
	FD_SET(0, &rdfs);
	select(1, &rdfs, NULL, NULL, NULL);
}

/* returns 0 on success, 1 on error */
int
pinfo_re_comp(char *name)
{
#ifdef ___DONT_USE_REGEXP_SEARCH___
	if (pinfo_re_pattern)
	{
		free(pinfo_re_pattern);
		pinfo_re_pattern = 0;
	}
	pinfo_re_pattern = strdup(name);
	return 0;
#else
	/* first see if we can compile the regexp */
	regex_t preg;
	if (regcomp(&preg, name, REG_ICASE) != 0)
	{
		/* compilation failed, so return */
		return -1;
	}
	
	/* compilation succeeded */
	/* first make some space in h_regexp[] to store the compiled regexp */
	if (pinfo_re_offset == -1)
	{
		pinfo_re_offset = h_regexp_num;
		if (!h_regexp_num)
			h_regexp = malloc(sizeof(regex_t));
		else
			h_regexp = realloc(h_regexp, sizeof(regex_t) *(h_regexp_num + 1));
	}
	else
	{
		regfree(&h_regexp[pinfo_re_offset]);
	}

	/* then copy the compiled expression into the newly allocated space */
	memcpy(&h_regexp[pinfo_re_offset], &preg, sizeof(preg));

	/* and finally return 0 for success */
	return 0;
#endif
}

int
pinfo_re_exec(char *name)
{
#ifdef ___DONT_USE_REGEXP_SEARCH___
	char *found;
	if (pinfo_re_pattern)
	{
		found = strstr(name, pinfo_re_pattern);
		if (found != NULL)
			return 1;
		else
			return 0;
	}
#else
	regmatch_t pmatch[1];
	return !regexec(&h_regexp[pinfo_re_offset], name, 1, pmatch, 0);
#endif
}

int
yesno(char *prompt, int def)
{
	char *yes = _("yes");
	char *no = _("no");
	int key;

	attrset(bottomline);
	mymvhline(maxy - 1, 0, ' ', maxx);
	move(maxy - 1, 0);
	/* if default answer is yes */
	if (def)
		printw("%s([%c]/%c)", prompt, *yes, *no);
	else
		printw("%s([%c]/%c)", prompt, *no, *yes);
	nodelay(stdscr, FALSE);
	while (1)
	{
		key = getch();
		if (key == ERR)
			return -1;
		if (is_enter_key(key))
			break;
		else
		{
			if (tolower(key) == tolower(*yes))
			{
				def = 1;
				break;
			}
			else
			{
				if (tolower(key) == tolower(*no))
				{
					def = 0;
					break;
				}
				else
					beep();
			}
		}
	}

	nodelay(stdscr, TRUE);
	if (def)
		addstr(yes);
	else
		addstr(no);
	attrset(normal);
	return def;
}

void
myclrtoeol()
{
	int x, y, i;
	getyx(stdscr, y, x);
	for (i = x; i < maxx; i++)
		mvaddch(y, i, ' ');
}

void
copy_stripped_from_regexp(char *src, char *dest)
{
	char *forbidden = "*.\\()[]\n";
	while (strchr(forbidden, *src) == NULL)
	{
		if (*src == 0)
			break;
		*dest = *src;
		src++;
		dest++;
	}
	*dest = 0;
}

void
myendwin()
{
	curs_set(shell_cursor);
	endwin();
}

void
handlewinch()
{
	myendwin();
	init_curses();
	doupdate();
	getmaxyx(stdscr, maxy, maxx);
	ungetch(keys.refresh_1);
}

/*
 * this functions checks whether the node header node_header 
 * corresponds to node node_name
 *
 * e.g. the header is something like:
 * File: bash.info,  Node: Introduction,  Next: Defs,  Prev: Top,  Up: Top
 * and we check here if the Node: entry in this header is equal to node_name
 *
 * returns  0 if node_header does not belong to a node with name node_name
 * returns -1 if no checking was done
 * returns  1 if check turned out ok
 */
int
check_node_name( const char * const node_name, const char * const node_header)
{
	/* if either one of node_name or node_header is NULL or a zero 
	 * sized string, we have nothing to check, so return success */
	if ( (node_name==NULL) || (node_header==NULL) 
		|| (strlen(node_name)==0) || (strlen(node_header)==0) )
	{
		return 1;
	}

	size_t header_len = strlen(node_header);
	
	/* copy node_header to a local string which can be mutilated */
	/* don't use strdup here, as xmalloc handles all errors */
	char *header = xmalloc( header_len + 1 );
	strcpy(header, node_header);

	/* search for "Node: foobar," in node_header */
	char *str_start = strstr(header, "Node: ");
	if (str_start==NULL) /* no match */
	{
		return 0;
	}
	/* advance str_start to the start of the node name */
	str_start += strlen("Node: ");
	/* and search for the next comma, tab, or newline */
	char *c = str_start;
	while ( (*c!=',') && (*c!='\t') && (*c!='\n') && (*c!='\0') ) c++;
	*c = '\0';
	
	/* so, now str_start point to a \0-terminated string containing the 
	 * node name from the header.
	 * Let's compare it with the node_name we're looking for */
	int res = strcmp(str_start, node_name);

	/* we're done, so free alloc'ed vars */
	xfree(header);

	/* check result of strcmp() and return */
	if ( res==0 )
	{
		/* match found */
		return 1;
	}
	else
	{
		/* no match */
		return 0;
	}
}


/* 
 * The wcswidth function returns the number of columns needed to represent
 * the  wide-character  string pointed to by s, but at most n wide charac‐
 * ters. If a non-printable wide character occurs among these  characters,
 * -1 is returned.
 */
#if defined(USE_WCHAR) && !defined(HAVE_WCSWIDTH)
int
wcswidth(const wchar_t *wstr, size_t max_len)
{
	int width = 0;
	size_t i;
	size_t len = wcslen(wstr);

	/* never count more than max_len chars */
	if (len>max_len) len=max_len;
			
	for (i=0; i<len; i++)
	{
		if (!iswprint(wstr[i])) return -1;
		width += wcwidth(wstr[i]);
	}

	return width;
}
#endif /* USE_WCHAR && !HAVE_WCSWIDTH */


/* calculcate length of string, handling multibyte strings correctly 
 * returns value <= len
 */
int
width_of_string( const char * const mbs, const int len)
{
	if (len<0) return -1;
	if (len==0) return 0;

	int width;

	/* copy the string to a local buffer, because we only want to 
	 * compare the first len bytes */
	char *str = xmalloc(len+1);
	memcpy(str, mbs, len);
	
#ifdef USE_WCHAR

	/* allocate a widestring */
	wchar_t *wstr = xmalloc( (len+1)*sizeof(wchar_t) );
	
	mbstowcs(wstr, str, len);
	width = wcswidth(wstr, len);

	/* clean up */
	xfree(wstr);
	
#else /* USE_WCHAR */

	width = strlen(str);
		
#endif /* USE_WCHAR */
		
	/* clean up */
	xfree(str);

	return width;
}
