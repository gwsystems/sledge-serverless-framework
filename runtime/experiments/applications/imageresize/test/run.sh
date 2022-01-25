#!/bin/bash

__run_sh__base_path="$(dirname "$(realpath --logical "${BASH_SOURCE[0]}")")"
__run_sh__bash_libraries_relative_path="../../../bash_libraries"
__run_sh__bash_libraries_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__bash_libraries_relative_path" && pwd)
export PATH="$__run_sh__bash_libraries_absolute_path:$PATH"

__run_sh__project_base_relative_path="../../../../.."
__run_sh__project_base_absolute_path=$(cd "$__run_sh__base_path" && cd "$__run_sh__project_base_relative_path" && pwd)

source csv_to_dat.sh || exit 1
source framework.sh || exit 1
source get_result_count.sh || exit 1
source panic.sh || exit 1
source path_join.sh || exit 1
source validate_dependencies.sh || exit 1

experiment_client() {
	local -r hostname="$1"
	local -r results_directory="$2"

	local -i success_count=0
	local -ri total_count=10

	local tmpfs_dir=/tmp/sledge_imageresize_test/
	[[ -d "$tmpfs_dir" ]] && {
		panic "tmpfs directory exists. Aborting"
		return 1
	}
	mkdir $tmpfs_dir

	for ((i = 0; i < total_count; i++)); do
		ext="$RANDOM"

		if curl -H 'Expect:' -H "Content-Type: image/jpg" --data-binary "@flower.jpg" --output "$tmpfs_dir/result_$ext.jpg" "$hostname:10000" 2> /dev/null 1> /dev/null; then

			pixel_differences="$(compare -identify -metric AE "$tmpfs_dir/result_$ext.jpg" expected_result.jpg null: 2>&1 > /dev/null)"

			if [[ "$pixel_differences" == "0" ]]; then
				success_count=$((success_count + 1))
			else
				{
					echo "FAIL"
					echo "$pixel_differences pixel differences detected"
				} | tee -a "$results_directory/result.txt"
				return 1
			fi
		else
			echo "curl failed with ${?}. See man curl for meaning." | tee -a "$results_directory/result.txt"
		fi
	done

	echo "$success_count / $total_count" | tee -a "$results_directory/result.txt"
	rm -r "$tmpfs_dir"

	return 0
}

validate_dependencies curl compare

# Copy Flower Image if not here
if [[ ! -f "./flower.jpg" ]]; then
	cp "$__run_sh__project_base_absolute_path/applications/sod/bin/flower.jpg" ./flower.jpg
fi

framework_init "$@"
