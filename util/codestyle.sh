#!/bin/sh
# NOTE: You can supply parameters as $@ to util/cpp_indent.py

set -euv
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

error() {
	local parent_lineno="$1"
	local message="$2"
	local code="${3:-1}"
	if [[ -n "$message" ]] ; then
		echo "Error on or near line ${parent_lineno}: ${message}; exiting with status ${code}"
		else
		echo "Error on or near line ${parent_lineno}; exiting with status ${code}"
	fi
	exit "${code}"
}
trap 'error ${LINENO} "" ""' ERR

header_files="$script_dir"/../src/*/*.h
cpp_files="$script_dir"/../src/*/*.cpp

header_files+=" "$(find "$script_dir"/../games -name '*.h')
cpp_files+=" "$(find "$script_dir"/../games -name '*.cpp')

header_files+=" "$(find "$script_dir"/../builtin -name '*.h')
cpp_files+=" "$(find "$script_dir"/../builtin -name '*.cpp')

header_files+=" "$(find "$script_dir"/../3rdparty/c55lib -name '*.h')
cpp_files+=" "$(find "$script_dir"/../3rdparty/c55lib -name '*.cpp')

lua_files=""$(find "$script_dir"/../extensions -name '*.lua')
lua_files+=" "$(find "$script_dir"/../client -name '*.lua')
lua_files+=" "$(find "$script_dir"/../games -name '*.lua')

cmake_files="$script_dir"/../CMakeLists.txt

# Allow files to disable this script by the special directive
# 'codestyle:disable'
function filter_files() {
	local files=$@
	for f in $files; do
		if ! grep -lq 'codestyle:disable' "$f"; then
			echo "$f"
		fi
	done
}
header_files=$(filter_files $header_files)
cpp_files=$(filter_files $cpp_files)
lua_files=$(filter_files $lua_files)
cmake_files=$(filter_files $cmake_files)

echo "header_files: $header_files"
echo "cpp_files: $cpp_files"
echo "lua_files: $lua_files"
echo "cmake_files: $cmake_files"

# Fix all that uncrustify is capable of
uncrustify_base="$script_dir/../util/uncrustify.cfg"
uncrustify_add_header="$script_dir/../util/uncrustify.header.cfg"
uncrustify_add_cpp="$script_dir/../util/uncrustify.cpp.cfg"

uncrustify_header="/tmp/buildat.uncrustify.header.cfg"
uncrustify_cpp="/tmp/buildat.uncrustify.cpp.cfg"
cat "$uncrustify_base" "$uncrustify_add_header" > "$uncrustify_header"
cat "$uncrustify_base" "$uncrustify_add_cpp" > "$uncrustify_cpp"

# Process these one at a time for easier debugging when something goes wrong
for f in $header_files; do
	uncrustify -c "$uncrustify_header" --no-backup $f
done
for f in $cpp_files; do
	uncrustify -c "$uncrustify_cpp" --no-backup $f
done

# Why the hell does uncrustify do stuff like "while(1) ;"?
sed -i -e 's/)[\t ]\+;$/);/' $header_files $cpp_files

# Fix some random things (what the fuck uncrustify?)
# typedef T1*T2 -> typedef T1* T2
sed -i -e 's/typedef \(.*\)\*\(.*\)/typedef \1* \2/g' $header_files $cpp_files

# Remove trailing whitespace
#sed -i -e 's/[\t ]*$//' $header_files $cpp_files $lua_files $cmake_files

# Indent and do some other things that uncrustify isn't capable of doing properly
python "$script_dir/cpp_indent.py" -i $@ $header_files
python "$script_dir/cpp_indent.py" -i -b $@ $cpp_files

# Fix or add Vim modeline magic
sed -i '/^\/\/ vim: set /d' $header_files $cpp_files
for f in $header_files $cpp_files; do
	echo '// vim: set noet ts=4 sw=4:' >> $f
done
sed -i '/^-- vim: set /d' $lua_files
for f in $lua_files; do
	echo '-- vim: set noet ts=4 sw=4:' >> $f
done
sed -i '/^# vim: set /d' $cmake_files
for f in $cmake_files; do
	echo '# vim: set noet ts=4 sw=4:' >> $f
done

# Format CMake too
sed -i -e 's/\(^[ \t]*\)\([A-Za-z_]\+\)[ \t]*(/\1\L\2(/' $cmake_files
sed -i -e 's/([\t ]\+\([A-Za-z_${)]\)/(\1/g' $cmake_files
sed -i -e 's/\([A-Za-z_}(]\)[\t ]\+)/\1)/g' $cmake_files

