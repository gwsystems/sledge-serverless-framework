# shellcheck shell=bash

source "type_checks.sh" || exit 1

# These utility functions are used to generate percentile tables that summarize distributions of data.
# Each row represents an independent variable, such as a scheduling policy
# The data for each column is provided as a single column of sorted data
# If the data is too course, a percentile might be statistically insignificant. If this is the case,
# The script writes an * to the appropriate cell
#
# Example:
#
# percentiles_table_header "./table.csv"
# for $variant in (fifo_nopreemption fifo_preemption edf_nopreemption edf_preemption); do
#     percentiles_table_row "./${variant}.csv" "./table.csv" "$variant"
# done
#
# See Also:
#   - csv_to_dat - Can transform a table into a *.dat file suitable for gnuplot
#
# References
#   - The AWK Programming Language - https://ia802309.us.archive.org/25/items/pdfy-MgN0H1joIoDVoIC7/The_AWK_Programming_Language.pdf
#   - GAWK: Effective AWK Programming - https://www.gnu.org/software/gawk/manual/gawk.pdf

percentiles_table_header() {
	local table_file="${1:?table_file not set}"
	# Can optionally override "app" in header
	local label_header="${2:-app}"
	echo "${label_header},cnt,min,mean,p50,p90,p99,max" > "$table_file"
}

# columnar_data_file is assumed to be a file containing a single column or sorted data
percentiles_table_row() {
	local -r columnar_data_file="${1:?columnar_data_file not set}"
	check_file columnar_data_file
	local -r table_file="${2:?table_file not set}"
	check_file table_file
	local -r row_label="${3:?row_label not set}"
	local -r format_string="${4:-%1.0f}"

	# Count the number of results
	local -i sample_size
	sample_size=$(wc -l < "$columnar_data_file")

	if ((sample_size == 0)); then
		# We might not have actually run every variant depending on iterations and workload mix
		# Insert a degenerate row if this is the case
		echo "$row_label,0,*,*,*,*,*,*" >> "$table_file"
	else
		awk '
			BEGIN {
				sample_size='"$sample_size"'
				row_label="'"$row_label"'"
				format_string="'"$format_string"'"
				invalid_number_symbol="*"
				sum = 0
				p50_idx = int(sample_size * 0.5)
				p90_idx = int(sample_size * 0.9)
				p99_idx = int(sample_size * 0.99)
				p100_idx = sample_size
			}

			# Empty pattern matches all rows
			             { sum += $0 }
			NR==1 		 { min = sample_size  > 0   ? sprintf(format_string, $0) : invalid_number_symbol }
			NR==p50_idx  { p50 = sample_size >= 3   ? sprintf(format_string, $0) : invalid_number_symbol }
			NR==p90_idx  { p90 = sample_size >= 10  ? sprintf(format_string, $0) : invalid_number_symbol }
			NR==p99_idx  { p99 = sample_size >= 100 ? sprintf(format_string, $0) : invalid_number_symbol }
			NR==p100_idx { p100 = sample_size > 0   ? sprintf(format_string, $0) : invalid_number_symbol }

			END {
				mean = sample_size > 0 ? sprintf(format_string, sum / NR) : invalid_number_symbol
				printf "%s,%d,%s,%s,%s,%s,%s,%s\n", row_label, sample_size, min, mean, p50, p90, p99, p100
			}
		' < "$columnar_data_file" >> "$table_file"
	fi
}
