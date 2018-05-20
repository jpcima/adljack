#!/bin/sh -e
set -x

for locale in "fr"; do
    mkdir -p po/"$locale"
    xgettext -k_ -L C++ -c -s -o po/"$locale"/adljack.pot sources/*.{h,cc}
    if test -f po/"$locale"/adljack.po; then
        msgmerge -U po/"$locale"/adljack.po po/"$locale"/adljack.pot
    else
        msginit -i po/"$locale"/adljack.pot -l "$locale" -o po/"$locale"/adljack.po
    fi
done
