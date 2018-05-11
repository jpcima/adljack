#!/bin/bash
set -e
set -x

version=$1

if [ -z "$version" ]; then
    echo "Package version not given." 1>&2
    exit 1
fi
if [ -e "work" ]; then
    echo "Delete the work directory first." 1>&2
    exit 1
fi

mkdir work
cd work

git clone .. adljack

cd adljack
git checkout master
git checkout v"$version"
git submodule--helper list | awk '{ print $4 }' | \
while read submodule; do
    git config --file .gitmodules submodule."$submodule".url "../../$submodule"
done
git submodule init
git submodule update

find . -name '.git*' -print0 | xargs -0 rm -rf

cd ..
mv adljack adljack-"$version"

tar cvf adljack-"$version".tar adljack-"$version"
gzip -9 adljack-"$version".tar

mkdir adljack-win32
cd adljack-win32
i686-w64-mingw32-cmake -DCMAKE_BUILD_TYPE=Release -DPREFER_PDCURSES=ON -DENABLE_VIRTUALMIDI=ON ../adljack-"$version"
i686-w64-mingw32-cmake --build .
cpack -G NSIS .
cpack -G ZIP .
zip ../ADLjack-"$version"-win32-installer.zip ADLjack-"$version"-win32.exe
cp ADLjack-"$version"-win32.zip ../ADLjack-"$version"-win32-portable.zip

cd ..

mkdir adljack-win64
cd adljack-win64
x86_64-w64-mingw32-cmake -DCMAKE_BUILD_TYPE=Release -DPREFER_PDCURSES=ON -DENABLE_VIRTUALMIDI=ON ../adljack-"$version"
x86_64-w64-mingw32-cmake --build .
cpack -G NSIS .
cpack -G ZIP .
zip ../ADLjack-"$version"-win64-installer.zip ADLjack-"$version"-win64.exe
cp ADLjack-"$version"-win64.zip ../ADLjack-"$version"-win64-portable.zip
