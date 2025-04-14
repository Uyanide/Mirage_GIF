#!/bin/bash

script_dir="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
build_dir=$script_dir/../build
echo "Building in $build_dir"

if [[ "$1" == "--configure" ]]; then
    rm -rf $build_dir
    mkdir $build_dir
    cmake -S $build_dir/.. -B $build_dir
fi

cmake --build $build_dir