Snownews
========

What this is
------------

Snownews is a command-line RSS feed reader. It is designed to be simple and
lightweight, and integrates well with other command-line tools, for both
generating and filtering the feeds it reads. Snownews runs on almost anything
Unix (and will even build with Cygwin).

Building Snownews
-----------------

You will need the following:

- GCC compiler (any recent version will do)
- ncurses 5.0 or higher
- libxml2 (any version)
- openssl 1.0 *only*
- Perl (any recent version will do)

Simply do the following steps:

```
./configure
make
sudo make install
```

By default, this will install Snownews into ``/usr/local``. If you prefer it to
go somewhere else, set the ``PREFIX`` environmental variable before calling
``make``. For example, if you're on Arch, you would do:

```
./configure
PREFIX=/usr make
PREFIX=/usr sudo make install
```

How to use this
---------------

Snownews comes with a complete man page, where you can find all the details for
its use. If you prefer a tutorial, you can find one [here][1]. The man page is
available in English, German, Dutch and French at the moment. 

We also provide a set of helper scripts, made for Snownews by various
contributors. You can find them in the ``contrib`` directory, along with
instructions for their use.

Contributing
------------

If you would like to contribute to Snownews, consider fixing one of our issues,
adding a helper script, or writing a localization. If you are interested in
contributing a helper script, please read ``contrib/CONTRIBUTING.md`` for
details. 

License
-------

Snownews is licensed under the GNU General Public License, version 2 *only*
(SPDX code ``GPL-2.0``). For more details, as well as the text of the license,
please see the ``COPYING`` file.

[1]: https://retro-freedom.nz/tech-101-snownews.html 
