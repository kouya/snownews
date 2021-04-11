Snownews
========

Snownews is a command-line RSS feed reader, originally written by
[Oliver Feiler](https://github.com/kouya).
It is designed to be simple and lightweight, and integrates well with
other command-line tools, for both generating and filtering the feeds
it reads.

Features
--------

* Runs on Linux, BSD, OS X (Darwin), Solaris and probably many more Unices. Yes, even works under Cygwin.
* Fast and very resource friendly.
* Downloads feeds using libcurl to support a variety of URL types.
* Uses local cache for minimal network traffic.
* A help menu available throughout the program.
* Few dependencies on external libraries; ncurses, libcurl, openssl, and libxml2.
* Import feature for OPML subscription lists.
* Fully customizable key bindings of all program functions.
* Type Ahead Find for quick and easy navigation.
* Color support.
* Feed categories and many other useful features!

Building Snownews
-----------------

You will need the following:

- GCC compiler 5+
- ncurses 5.0+
- libcurl
- libxml2
- openssl 1.1+
- gettext

Once you have these dependencies installed:

```sh
./configure --prefix=/usr
make install
```

`configure --help` will list other options that you may find interesting.

Localization
------------

Snownews and its documentation is currently available in the following languages:

* Belarusian Latin
* Chinese, Traditional
* Chinese, Simplified
* Dutch
* English
* French
* German
* Italian
* Japanese
* Korean
* Polish
* Portuguese, Brazilian
* Russian
* Slovenian
* Spanish
* Swedish

If you want to create a new translation or update an exisiting one, send a patch or a pull request on github.

License
-------

Snownews is licensed under the GNU General Public License, version 3 only.
For more details, see the text of the license in `LICENSE.md`.
