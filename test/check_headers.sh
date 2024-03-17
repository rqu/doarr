cc="$1 -c -xc"
cxx="$2 -c -xc++ --std=c++20"
rt="$3"

out="/dev/null"

for f in include/doarr/*.hpp
do
	$cxx "$f" -o "$out" &
	$cxx -Iinclude "$f" -o "$out" &
done

for f in runtime/*.hpp
do
	$cxx $rt -Iinclude "$f" -o "$out" &
done

for f in runtime/*.h
do
	$cxx $rt -include stddef.h "$f" -o "$out" &
done

for f in dcc/*.h
do
	$cc -include stddef.h "$f" -o "$out" &
done

wait
