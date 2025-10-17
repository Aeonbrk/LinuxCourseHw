#!/usr/bin/env bash

set -euo pipefail

# 必需参数检查
: "${BIN:?missing BIN}"      # 可执行文件路径
: "${DISK:?missing DISK}"    # 压测使用的磁盘文件
: "${DISK_SIZE:?missing DISK_SIZE}"
: "${DURATION:?missing DURATION}"
: "${FILES:?missing FILES}"
: "${THREADS:?missing THREADS}"
: "${WRITE_SIZE:?missing WRITE_SIZE}"
: "${MONITOR:?missing MONITOR}"
: "${WORKSPACE:?missing WORKSPACE}"

echo "Preparing stress disk (${DISK_SIZE}MB)"
rm -f "${DISK}"
trap 'rm -f "${DISK}"' EXIT

"${BIN}" "${DISK}" create "${DISK_SIZE}"
"${BIN}" "${DISK}" format

timestamp=$(date +%Y%m%d_%H%M%S)
workspace_host=${WORKSPACE#/}
if [[ -z "${workspace_host}" ]]; then
  workspace_host="stress"
fi

log_dir="logs/${workspace_host}"
mkdir -p "${log_dir}"
log_file="${log_dir}/stress_${timestamp}.log"

echo "Streaming stress metrics (log: ${log_file})"

set +e
stdbuf -oL "${BIN}" "${DISK}" stress \
  --duration "${DURATION}" \
  --files "${FILES}" \
  --threads "${THREADS}" \
  --write-size "${WRITE_SIZE}" \
  --monitor "${MONITOR}" \
  --workspace "${WORKSPACE}" \
  --cleanup 2>&1 \
  | tee "${log_file}" \
  | stdbuf -oL grep -E --line-buffered '\[Stress\] (Starting|Metrics|Test finished)'
status=${PIPESTATUS[0]}
set -e

if [[ ${status} -ne 0 ]]; then
  echo "Stress workload failed (status ${status}). Full diagnostics: ${log_file}"
  exit ${status}
fi

echo "Stress workload finished successfully. Full log: ${log_file}"
