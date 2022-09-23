#!/usr/bin/env bash

set -o errexit
set -o pipefail

app=rokuyon.app
contents=$app/Contents

if [[ ! -f rokuyon ]]; then
    echo 'Error: rokuyon binary was not found.'
    echo 'Please run `make` to compile rokuyon before bundling.'
    exit 1
fi

if [[ -d "$app" ]]; then
    rm -rf "$app"
fi

install -dm755 "${contents}"/{MacOS,Resources,Frameworks}
install -sm755 rokuyon "${contents}/MacOS/rokuyon"
install -m644 Info.plist "$contents/Info.plist"

# macOS does not have the -f flag for readlink
abspath() {
    perl -MCwd -le 'print Cwd::abs_path shift' "$1"
}

# Recursively copy dependent libraries to the Frameworks directory
# and fix their load paths
fixup_libs() {
    local libs=($(otool -L "$1" | grep -vE "/System|/usr/lib|:$" | sed -E 's/'$'\t''(.*) \(.*$/\1/'))
    
    for lib in "${libs[@]}"; do
        # Dereference symlinks to get the actual .dylib as binaries' load
        # commands can contain paths to symlinked libraries.
        local abslib="$(abspath "$lib")"
        local base="$(basename "$abslib")"
        local install_path="$contents/Frameworks/$base"

        install_name_tool -change "$lib" "@rpath/$base" "$1"

        if [[ ! -f "$install_path" ]]; then
            install -m644 "$abslib" "$install_path"
            strip -Sx "$install_path"
            fixup_libs "$install_path"
        fi
    done
}

install_name_tool -add_rpath "@executable_path/../Frameworks" $contents/MacOS/rokuyon

fixup_libs $contents/MacOS/rokuyon

codesign --deep -s - rokuyon.app

if [[ $1 == '--dmg' ]]; then
    mkdir build/dmg
    cp -a rokuyon.app build/dmg/
    ln -s /Applications build/dmg/Applications
    hdiutil create -volname rokuyon -srcfolder build/dmg -ov -format UDBZ rokuyon.dmg
    rm -r build/dmg
fi
