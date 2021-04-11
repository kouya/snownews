# Snownews

Snownews is a console RSS/Atom feed reader for Linux and other unix
platforms, originally written by [Oliver Feiler](https://github.com/kouya).

## Building

You will need ncurses, libcurl, libxml2, openssl, and gettext, all of which
are likely to be already installed on your distribution. Once you have the
dependencies installed:

```sh
./configure --prefix=/usr
make install
snownews
```

## Using

### Feeds

The main program screen, that is shown right after you start snownews,
lets you add/remove feeds and update them manually. On the right side of
the screen the number of new items is shown for every newsfeed. To add
a feed press 'a' and enter the http or https URL. To delete a listed
feed highlight it with the cursor keys and press 'D'.

If you already use another RSS reader, you can import the feed list into
snownews by exporting it from your old reader in OPML format. snownews
stores its feed list directly in OPML, in `~/.config/snownews/urls.opml`.
You can either copy over it, or merge the 'outline' items from another
list.

Once you have your list of feeds, press 'R' to load them all. 'r' will
reload the selected feed. Select a feed with 'jk' or arrow keys, and press
'Enter' to view it. 'q' to return to feed list. With a feed item selected,
press 'o' to read it in a browser or 'Enter' to read the short description.
Learn other operations by pressing 'h'.

### Browser

The default browser is lynx, but you can change this by pressing 'B' or
by directly editing `~/.config/snownews/browser`. The program replaces
%s with the URL when expanding the string.

### Typeahead

For faster navigation in your feedlist you can use snownews Type Ahead
Find feature. Press the 'Tab' key and the statusline will change into
a text entry field. While you enter the text you want to search for,
highlight will be automatically placed on items as they match. If you
have selected an item just press enter to open the feed. If there are
multiple items matching you can switch between them by pressing 'Tab'. To
quit Type Ahead delete the search text or press 'Ctrl+G'.

### Categories

Snownews uses categories to manage large subscription lists. You can
define as many categories for a feed as you like. You can then apply a
filter in the main menu that will only show feeds that have a matching
category defined. Feeds with a category will have it printed next to
their name in the main menu.

To add or remove a feed from a category, press 'C' while the feed is
highlighted. If you already have defined categories for other feeds you'll
get a list of the existing categories. Just press its number to add the
current feed to this category. To add the feed to a new category, press
'A' and enter the name of the new category. If you want to remove a feed
from a category, press its number in the feed categorization GUI.
You can see all defined categories for a feed in the feed info.

### Keybindings

You can customize the keybindings by editing the file
`~/.config/snownews/keybindings`. The format is "function description:key".
Do not change the string "function description". The single character
behind the colon represents the key the program will associate with the
corresponding function. If you delete a definition or the program cannot
parse the file for some reason the default settings will be used instead.

### Colors

If you prefer to see the world in colors, configure them by editing
`~/.config/snownews/colors`. To globally enable colors in the program,
set enabled to "1". To set a color, use the color key value that is
listed in the comment in that file. You can disable usage for single
items by using the value "-1".

### HTML transformation

Snownews will try to convert HTML content into plain text before
displaying the text. Tags will be stripped alltogether and some
common HTML entities will be translated. By default only the five
entities defined in XML (<>&"') plus a default setting included will be
translated. You can influence this behaviour with the definition file
`~/.config/snownews/html_entities`. See the comments on top of the file
for further details.

### HTTP authentication

To subscribe to a feed that requires authentication, use URL format
`http://username:password@server/feed.rss`. You can use cookies to supply
log in information to a webserver. Put the cookies you want Snownews to
use into the file `~/.config/snownews/cookies`. The file has to be in
standard Netscape cookies.txt file format. Mozilla uses this format for
example. Snownews will automatically send the right cookies to the right
webserver. You can also just place a symlink to your browser's cookie
file, but it is not recommended. If a cookie is expired, Snownews will
print a warning on program start and not use the cookie. If a cookie
is marked as secure (only to be used via an SSL secured connection)
Snownews will also discard the cookie.

### Execurls and filters

Execurls are scripts that produce a valid RSS file by themselves. You can add
one by using the `exec:` prefix to a feed: `exec:wget -q https://feed.com/rss`

Filters convert a resource after it is downloaded. You might subscribe to an
URL that is a webpage or a non-RSS feed. If snownews asks you if you want
to use a filter, because it couldn't parse the resource, enter the location
of your script. You can also add filters to exisiting subscriptions by
highlighting the feed and pressing 'e'.

## Reporting bugs

If you think you found a bug in Snownews, please report it. Anything that
makes the program crash, regardless what you're doing is a bug and needs
to be fixed. Report problems on the
[project](https://github.com/msharov/snownews)
[bugtracker](https://github.com/msharov/snownews/issues).
