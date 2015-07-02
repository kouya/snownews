#!/bin/sh

PREFIX="/usr/local"
LOCALEPATH="/usr/local/share/locale"

if [ ! -d "$PREFIX/bin" ]; then
  mkdir -p $PREFIX/bin;
fi
install snownews $PREFIX/bin
install opml2snow $PREFIX/bin
if [ ! -f "PREFIX/bin/snow2opml" ]; then
	ln -sf $PREFIX/bin/opml2snow $PREFIX/bin/snow2opml;
fi;

echo "Installing manpages ..."
if [ ! -d "$PREFIX/man/man1" ]; then
  mkdir -p $PREFIX/man/man1;
fi
install -m 0644 man/snownews.1 $PREFIX/man/man1

if [ ! -d "$PREFIX/man/de/man1" ]; then
  mkdir -p $PREFIX/man/de/man1;
fi
install -m 0644 man/de/snownews.1 $PREFIX/man/de/man1

if [ ! -d "$PREFIX/man/nl/man1" ]; then
  mkdir -p $PREFIX/man/nl/man1;
fi
install -m 0644 man/nl/snownews.1 $PREFIX/man/nl/man1

if [ ! -d "$PREFIX/man/fr/man1" ]; then
  mkdir -p $PREFIX/man/fr/man1;
fi
install -m 0644 man/fr/snownews.1 $PREFIX/man/fr/man1

if [ ! -d "$PREFIX/man/it/man1" ]; then
  mkdir -p $PREFIX/man/it/man1;
fi
install -m 0644 man/it/snownews.1 $PREFIX/man/it/man1

if [ ! -d "$PREFIX/man/ru_RU.KOI8-R/man1" ]; then
  mkdir -p $PREFIX/man/ru_RU.KOI8-R/man1;
fi
install -m 0644 man/ru_RU.KOI8-R/snownews.1 $PREFIX/man/ru_RU.KOI8-R/man1

echo "Installing translations ..."
if [ ! -d "$LOCALEPATH/de/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/de/LC_MESSAGES/;
fi
install -m 0644 po/de.mo $LOCALEPATH/de/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/es/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/es/LC_MESSAGES/;
fi
install -m 0644 po/es.mo $LOCALEPATH/es/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/fr/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/fr/LC_MESSAGES/;
fi
install -m 0644 po/fr.mo $LOCALEPATH/fr/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/it/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/it/LC_MESSAGES/;
fi
install -m 0644 po/it.mo $LOCALEPATH/it/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/ja/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/ja/LC_MESSAGES/;
fi
install -m 0644 po/ja.mo $LOCALEPATH/ja/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/nl/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/nl/LC_MESSAGES/;
fi
install -m 0644 po/nl.mo $LOCALEPATH/nl/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/pl/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/pl/LC_MESSAGES/;
fi
install -m 0644 po/pl.mo $LOCALEPATH/pl/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/pt_BR/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/pt_BR/LC_MESSAGES/;
fi
install -m 0644 po/pt_BR.mo $LOCALEPATH/pt_BR/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/ru/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/ru/LC_MESSAGES/;
fi
install -m 0644 po/ru.mo $LOCALEPATH/ru/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/sl/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/sl/LC_MESSAGES/;
fi
install -m 0644 po/sl.mo $LOCALEPATH/sl/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/zh_CN/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/zh_CN/LC_MESSAGES/;
fi
install -m 0644 po/zh_CN.mo $LOCALEPATH/zh_CN/LC_MESSAGES/snownews.mo

if [ ! -d "$LOCALEPATH/zh_TW/LC_MESSAGES/" ]; then
    mkdir -p $LOCALEPATH/zh_TW/LC_MESSAGES/;
fi
install -m 0644 po/zh_TW.mo $LOCALEPATH/zh_TW/LC_MESSAGES/snownews.mo
