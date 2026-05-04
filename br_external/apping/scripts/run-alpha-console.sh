#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/lib/prepare-local-runtime.sh"

if [[ ! -x "${APPING_BUILD_DIR}/fixed-base-service" || ! -x "${APPING_BUILD_DIR}/portable-console" ]]; then
  echo "Build missing. Run: cmake -S . -B build && cmake --build build -j" >&2
  exit 1
fi

if [[ ! -x "${ROC_SEND}" || ! -x "${ROC_RECV}" ]]; then
  echo "ROC tools missing. Build roc-toolkit-opus-master first." >&2
  exit 1
fi

if [[ -n "${DISPLAY:-}" ]]; then
  export QT_QPA_PLATFORM="xcb"
elif [[ -n "${WAYLAND_DISPLAY:-}" && -S "${XDG_RUNTIME_DIR:-${APPING_SESSION_RUNTIME_DIR}}/${WAYLAND_DISPLAY}" ]]; then
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-${APPING_SESSION_RUNTIME_DIR}}"
  export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=${XDG_RUNTIME_DIR}/bus}"
  export QT_QPA_PLATFORM="wayland"
elif [[ -S "${APPING_SESSION_RUNTIME_DIR}/wayland-0" ]]; then
  export XDG_RUNTIME_DIR="${APPING_SESSION_RUNTIME_DIR}"
  export WAYLAND_DISPLAY="wayland-0"
  export DISPLAY="${DISPLAY:-:0}"
  export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=${APPING_SESSION_RUNTIME_DIR}/bus}"
  export QT_QPA_PLATFORM="wayland"
else
  export XDG_RUNTIME_DIR="${APPING_PULSE_RUNTIME_DIR}"
  export QT_QPA_PLATFORM="offscreen"
fi

log_file="${APPING_LOG_DIR}/base-alpha.log"
: > "${log_file}"

"${APPING_BUILD_DIR}/fixed-base-service" --config "${APPING_BASE_ALPHA_CONFIG}" >>"${log_file}" 2>&1 &
base_pid=$!

cleanup() {
  kill "${base_pid}" >/dev/null 2>&1 || true
  wait "${base_pid}" >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM

sleep 1

echo "Audio backend: ${APPING_AUDIO_BACKEND}"
echo "Session runtime: ${APPING_SESSION_RUNTIME_DIR}"
echo "PulseAudio runtime: ${APPING_ACTIVE_RUNTIME_DIR}"
if [[ -n "${QT_QPA_PLATFORM:-}" ]]; then
  echo "Qt platform: ${QT_QPA_PLATFORM}"
fi
echo "Pulse server: ${PULSE_SERVER}"
echo "Pulse sink: ${PULSE_SINK}"
if [[ -n "${PULSE_SOURCE:-}" ]]; then
  echo "Pulse source: ${PULSE_SOURCE}"
else
  echo "Pulse source: none detected"
fi
if [[ "${PULSE_SINK}" == "apping_null" ]]; then
  echo "Audio output fallback: null sink"
fi
echo "Base log: ${log_file}"
echo
echo "Launching portable console..."

"${APPING_BUILD_DIR}/portable-console" --portable-config "${APPING_PORTABLE_CONFIG}"
