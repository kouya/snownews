Snownews
========

_Please note that I can no longer maintain this software. It is provided as is, if you want to fork it, but I will not be able to add any more things to it, nor merge patches you submit._

What it is
------------

Snownews is a command-line RSS feed reader. It is designed to be simple and
lightweight, and integrates well with other command-line tools, for both
generating and filtering the feeds it reads. Snownews runs on almost anything
Unix (and will even build with Cygwin).

Features
--------

* Runs on Linux, BSD, OS X (Darwin), Solaris and probably many more Unices. Yes, even works under Cygwin.
* Fast and very resource friendly.
* Builtin HTTP client will follow server redirects and update feed URLs that point to permanent redirects (301) automatically.
* Understands "Not-Modified" (304) server replies and handles gzip compression.
* Uses local cache for minimal network traffic.
* Supports HTTP proxy.
* Supports HTTP authentication (basic and digest methods).
* Supports cookies.
* A help menu available throughout the program.
* Few dependencies on external libraries; ncurses and libxml2.
* Import feature for OPML subscription lists.
* Fully customizable key bindings of all program functions.
* Type Ahead Find for quick and easy navigation.
* Color support.
* Extensible via plugins.
* Feed categories and many other useful features!


Building Snownews
-----------------

You will need the following:

- GCC compiler 5+
- ncurses 5.0+
- libxml2
- Perl (for extension scripts)
- gettext (lib and msgfmt tool)

Simply do the following steps:

```
./configure
make
sudo make install
```

By default, this will install Snownews into ``/usr/local``. If you
prefer it to go somewhere else, set the ``./configure --prefix=DIR``
parameter. ``configure --help`` will list other options that you may
find interesting. For example, if you're on Arch, you would do:

```
./configure --prefix=/usr
sudo make install
```

How to use it
---------------

Snownews comes with a complete man page, where you can find all
the details for its use. If you prefer a tutorial, you can find one
[here](https://retro-freedom.nz/tech-101-snownews.html). The man page
is available in English, German, Dutch and French at the moment.

A set of helper scripts, made for Snownews by various contributors, can be
found in the ``contrib`` directory, along with instructions for their use.

Contributing
------------

If you would like to contribute to Snownews, consider fixing one of
our issues, adding a helper script, or writing a localization. If
you are interested in contributing a helper script, please read
[contrib/CONTRIBUTING.md](contrib/CONTRIBUTING.md) for details.

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

Snownews is licensed under the GNU General Public License, version 3 *only*
(SPDX code ``GPL-3.0``). For more details, as well as the text of the license,
please see the ``COPYING`` file.
