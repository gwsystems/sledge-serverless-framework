# shellcheck shell=bash
# shellcheck disable=SC2034,SC2153,SC2154,SC2155
if [ -n "$__experiment_server_globals_sh__" ]; then return; fi
__experiment_server_globals_sh__=$(date)

# App WASM so files:
declare -r FIBONACCI="fibonacci.wasm.so"
declare -r EKF="gps_ekf.wasm.so"
declare -r CIFAR10="cifar10.wasm.so"
declare -r GOCR="gocr.wasm.so"
declare -r LPD="license_plate_detection.wasm.so"
declare -r RESIZE="resize_image.wasm.so"
declare -r CNN="cnn_face_detection.wasm.so"

# The global configs for the scripts
declare -gr SERVER_LOG_FILE="perf.log"
declare -gr SERVER_HTTP_LOG_FILE="http_perf.log"
declare -gr HEY_OPTS="-disable-compression -disable-keepalive -disable-redirects"

# Sandbox Perf Log Globals:
declare  -ga SANDBOX_METRICS=(total queued uninitialized allocated initialized runnable interrupted preempted running_sys running_user asleep returned complete error)
declare  -gA SANDBOX_METRICS_FIELDS=(
	[total]=6
	[queued]=7
	[uninitialized]=8
	[allocated]=9
	[initialized]=10
	[runnable]=11
	[interrupted]=12
	[preempted]=13
	[running_sys]=14
	[running_user]=15
	[asleep]=16
	[returned]=17
	[complete]=18
	[error]=19
)
declare -gr SANDBOX_TENANT_NAME_FIELD=2
declare -gr SANDBOX_ROUTE_FIELD=3
declare -gr SANDBOX_CPU_FREQ_FIELD=20
declare -gr SANDBOX_RESPONSE_CODE_FIELD=21
declare -gr SANDBOX_GUARANTEE_TYPE_FIELD=22
declare -gr SANDBOX_PAYLOAD_SIZE=23

# HTTP Session Perf Log Globals:
declare  -ga HTTP_METRICS=(http_receive http_sent http_total http_preprocess)
declare  -gA HTTP_METRICS_FIELDS=(
	[http_receive]=6
	[http_sent]=7
	[http_total]=8
	[http_preprocess]=9
)
declare -gr HTTP_TENANT_NAME_FIELD=1
declare -gr HTTP_ROUTE_FIELD=2
declare -gr HTTP_CPU_FREQ_FIELD=10
