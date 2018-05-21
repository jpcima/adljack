#!/bin/sh -e
set -x

po_update() {
    if test -f "$1"; then
        msgmerge -U "$1" "$1"t
    else
        msginit -i "$1"t -l "$locale" -o "$1"
    fi
}

for locale in "fr"; do
    mkdir -p po/"$locale"
    xgettext -k_ -L C++ -c -s -o po/"$locale"/adljack.pot sources/*.{h,cc}
    xgettext -k_INST -L C++ -c -o po/"$locale"/adljack_inst.pot sources/insnames.cc
    xgettext -k_PERC -L C++ -c -o po/"$locale"/adljack_perc.pot sources/insnames.cc
    xgettext -k_EX -L C++ -c -o po/"$locale"/adljack_ex.pot sources/insnames.cc
    po_update po/"$locale"/adljack.po
    po_update po/"$locale"/adljack_inst.po
    po_update po/"$locale"/adljack_perc.po
    po_update po/"$locale"/adljack_ex.po
    rm -f po/"$locale"/*.pot
done
