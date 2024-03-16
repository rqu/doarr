out=`mktemp`
trap "rm '$out'" EXIT

for f in include/doarr/*.hpp
do
	gcc "$@" --std=c++20 "$f" -o "$out" &
	gcc "$@" -Iinclude --std=c++20 "$f" -o "$out" &
done

for f in runtime/*.hpp
do
	gcc "$@" -DINTERNAL_VISIBILITY='' -Iinclude --std=c++20 "$f" -o "$out" &
done

for f in runtime/*.h
do
	gcc "$@" -DINTERNAL_VISIBILITY='' -Iinclude -Dsize_t="unsigned long long" "$f" -o "$out" &
done

wait
