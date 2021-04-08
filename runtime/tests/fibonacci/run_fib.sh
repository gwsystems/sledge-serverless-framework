#!/bin/sh

ITERS=$3

# before running this benchmark,
# copy fibonacci to fibonacci_native.out

testeach() {
	tmp_cnt=${ITERS}
	exe_relpath=$1

	echo "${exe_relpath} ($2) for ${tmp_cnt}"

	while [ ${tmp_cnt} -gt 0 ]; do
		bench=$(echo $2 | $exe_relpath 2> /dev/null)
		tmp_cnt=$((tmp_cnt - 1))
		echo "$bench"
	done

	echo "Done!"
}

MAXNUM=$2

tmp1_cnt=${MAXNUM}

while [ ${tmp1_cnt} -gt 28 ]; do
	testeach ./fibonacci_$1.out ${tmp1_cnt}
	tmp1_cnt=$((tmp1_cnt - 1))
done

echo "All done!"
