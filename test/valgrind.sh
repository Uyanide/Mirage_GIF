#!/bin/bash
script_dir="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
outputfile="$script_dir/valgrind_output.txt"
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=$outputfile \
         "$@"