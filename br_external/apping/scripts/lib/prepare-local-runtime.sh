#!/usr/bin/env bash

APPING_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APPING_BUILD_DIR="${APPING_BUILD_DIR:-${APPING_ROOT_DIR}/build}"
APPING_LOG_DIR="${APPING_ROOT_DIR}/logs"
APPING_PULSE_RUNTIME_DIR="${HOME}/.cache/apping/pulse-runtime"
APPING_SIM_RUNTIME_DIR="${APPING_ROOT_DIR}/config/simulation/generated"

export APPING_ROOT_DIR
export APPING_BUILD_DIR
export APPING_LOG_DIR
export APPING_PULSE_RUNTIME_DIR
export APPING_SIM_RUNTIME_DIR

mkdir -p "${APPING_LOG_DIR}" "${APPING_PULSE_RUNTIME_DIR}" "${APPING_SIM_RUNTIME_DIR}"
chmod 700 "${APPING_PULSE_RUNTIME_DIR}"

export ROC_SEND="${APPING_ROOT_DIR}/roc-toolkit-opus-master/bin/x86_64-pc-linux-gnu/roc-send"
export ROC_RECV="${APPING_ROOT_DIR}/roc-toolkit-opus-master/bin/x86_64-pc-linux-gnu/roc-recv"

apping_detect_graphical_runtime_dir() {
  if [[ -n "${XDG_RUNTIME_DIR:-}" && -d "${XDG_RUNTIME_DIR}" ]]; then
    if [[ "$(id -u)" -ne 0 || -S "${XDG_RUNTIME_DIR}/pulse/native" || -S "${XDG_RUNTIME_DIR}/wayland-0" ]]; then
      echo "${XDG_RUNTIME_DIR}"
      return
    fi
  fi

  local runtime
  for runtime in /run/user/*; do
    [[ -d "${runtime}" ]] || continue
    if [[ "$(basename "${runtime}")" == "$(id -u)" ]]; then
      continue
    fi
    if [[ -S "${runtime}/pulse/native" || -S "${runtime}/wayland-0" ]]; then
      echo "${runtime}"
      return
    fi
  done

  local current_runtime="/run/user/$(id -u)"
  if [[ -d "${current_runtime}" ]]; then
    echo "${current_runtime}"
    return
  fi

  echo "${current_runtime}"
}

APPING_SESSION_RUNTIME_DIR="$(apping_detect_graphical_runtime_dir)"
APPING_SESSION_UID="$(basename "${APPING_SESSION_RUNTIME_DIR}")"
if [[ ! "${APPING_SESSION_UID}" =~ ^[0-9]+$ ]]; then
  APPING_SESSION_UID="$(id -u)"
fi
export APPING_SESSION_RUNTIME_DIR
export APPING_SESSION_UID

APPING_SESSION_DBUS="unix:path=${APPING_SESSION_RUNTIME_DIR}/bus"
APPING_SESSION_PULSE_SERVER="unix:${APPING_SESSION_RUNTIME_DIR}/pulse/native"
APPING_PRIVATE_PULSE_SERVER="unix:${APPING_PULSE_RUNTIME_DIR}/pulse/native"

apping_pactl_with() {
  local runtime_dir="$1"
  local pulse_server="$2"
  local dbus_bus="$3"
  shift 3
  XDG_RUNTIME_DIR="${runtime_dir}" \
  DBUS_SESSION_BUS_ADDRESS="${dbus_bus}" \
  PULSE_SERVER="${pulse_server}" \
  pactl "$@"
}

APPING_AUDIO_BACKEND="private"
ACTIVE_RUNTIME_DIR="${APPING_PULSE_RUNTIME_DIR}"
ACTIVE_DBUS_BUS="${APPING_SESSION_DBUS}"
export PULSE_SERVER="${APPING_PRIVATE_PULSE_SERVER}"

if [[ -S "${APPING_SESSION_RUNTIME_DIR}/pulse/native" ]] \
  && apping_pactl_with "${APPING_SESSION_RUNTIME_DIR}" \
                       "${APPING_SESSION_PULSE_SERVER}" \
                       "${APPING_SESSION_DBUS}" \
                       info >/dev/null 2>&1; then
  APPING_AUDIO_BACKEND="session"
  ACTIVE_RUNTIME_DIR="${APPING_SESSION_RUNTIME_DIR}"
  ACTIVE_DBUS_BUS="${APPING_SESSION_DBUS}"
  export PULSE_SERVER="${APPING_SESSION_PULSE_SERVER}"
else
  if ! apping_pactl_with "${APPING_PULSE_RUNTIME_DIR}" \
                         "${APPING_PRIVATE_PULSE_SERVER}" \
                         "${ACTIVE_DBUS_BUS}" \
                         info >/dev/null 2>&1; then
    rm -rf "${APPING_PULSE_RUNTIME_DIR}/pulse"
    mkdir -p "${APPING_PULSE_RUNTIME_DIR}"
    XDG_RUNTIME_DIR="${APPING_PULSE_RUNTIME_DIR}" \
    pulseaudio --daemonize=yes --exit-idle-time=-1 \
      --load='module-null-sink sink_name=apping_null sink_properties=device.description=AppingNull' \
      >/dev/null 2>&1 || true
  fi
fi
APPING_ACTIVE_RUNTIME_DIR="${ACTIVE_RUNTIME_DIR}"
export APPING_ACTIVE_RUNTIME_DIR

apping_pactl() {
  apping_pactl_with "${ACTIVE_RUNTIME_DIR}" "${PULSE_SERVER}" "${ACTIVE_DBUS_BUS}" "$@"
}

if ! apping_pactl list short sinks | awk '{print $2}' | grep -qx 'apping_null'; then
  apping_pactl load-module \
    module-null-sink \
    sink_name=apping_null \
    sink_properties=device.description=AppingNull >/dev/null 2>&1 || true
fi

DEFAULT_SINK="$(
  apping_pactl info 2>/dev/null \
    | sed -n 's/^Default Sink: //p' \
    | head -n 1
)"
FIRST_REAL_SINK="$(
  apping_pactl list short sinks 2>/dev/null \
    | awk '$2 != "apping_null" && $2 != "auto_null" { print $2; exit }'
)"
if [[ -z "${DEFAULT_SINK}" || "${DEFAULT_SINK}" == "auto_null" || "${DEFAULT_SINK}" == "apping_null" ]]; then
  DEFAULT_SINK="${FIRST_REAL_SINK:-apping_null}"
fi
if [[ -z "${DEFAULT_SINK}" ]]; then
  DEFAULT_SINK="apping_null"
fi
export PULSE_SINK="${DEFAULT_SINK}"

DEFAULT_SOURCE="$(
  apping_pactl info 2>/dev/null \
    | sed -n 's/^Default Source: //p' \
    | head -n 1
)"
FIRST_INPUT_SOURCE="$(
  apping_pactl list short sources 2>/dev/null \
    | awk '$2 !~ /\.monitor$/ { print $2; exit }'
)"
if [[ -z "${DEFAULT_SOURCE}" || "${DEFAULT_SOURCE}" =~ \.monitor$ ]]; then
  DEFAULT_SOURCE="${FIRST_INPUT_SOURCE:-}"
fi
if [[ -n "${DEFAULT_SOURCE}" ]]; then
  export PULSE_SOURCE="${DEFAULT_SOURCE}"
else
  unset PULSE_SOURCE
fi

export APPING_AUDIO_BACKEND
export APPING_ROC_MULTICAST_IFACE="${APPING_ROC_MULTICAST_IFACE:-127.0.0.1}"

if command -v ip >/dev/null 2>&1; then
  if [[ "$(id -u)" -eq 0 ]]; then
    ip route replace 239.42.0.10/32 dev lo src 127.0.0.1 >/dev/null 2>&1 || true
  elif command -v sudo >/dev/null 2>&1; then
    sudo -n ip route replace 239.42.0.10/32 dev lo src 127.0.0.1 >/dev/null 2>&1 || true
  fi
fi

BASE_CONFIGS=(
  "${APPING_ROOT_DIR}/config/simulation/base-alpha.json"
  "${APPING_ROOT_DIR}/config/simulation/base-beta.json"
  "${APPING_ROOT_DIR}/config/simulation/base-gamma.json"
  "${APPING_ROOT_DIR}/config/simulation/base-delta.json"
  "${APPING_ROOT_DIR}/config/simulation/base-epsilon.json"
)
PORTABLE_SOURCE_CONFIG="${APPING_ROOT_DIR}/config/simulation/portable-console.json"

base_index=0
for config in "${BASE_CONFIGS[@]}"; do
  runtime_config="${APPING_SIM_RUNTIME_DIR}/$(basename "${config}")"
  if [[ "${APPING_SIM_SINGLE_SPEAKER:-0}" == "1" && "${base_index}" -ne 0 ]]; then
    base_audio_output_uri="pulse://apping_null"
  else
    base_audio_output_uri="pulse://default"
  fi
  python3 - "${config}" "${runtime_config}" "${base_audio_output_uri}" <<'PY'
import json
import pathlib
import sys

source_path = pathlib.Path(sys.argv[1])
target_path = pathlib.Path(sys.argv[2])
audio_output_uri = sys.argv[3]

data = json.loads(source_path.read_text())
data["audio_output_uri"] = audio_output_uri
target_path.write_text(json.dumps(data, indent=2) + "\n")
PY
  base_index=$((base_index + 1))
done

export APPING_BASE_DIR="${APPING_SIM_RUNTIME_DIR}"
export APPING_BASE_ALPHA_CONFIG="${APPING_SIM_RUNTIME_DIR}/base-alpha.json"
export APPING_BASE_BETA_CONFIG="${APPING_SIM_RUNTIME_DIR}/base-beta.json"
export APPING_BASE_GAMMA_CONFIG="${APPING_SIM_RUNTIME_DIR}/base-gamma.json"
export APPING_BASE_DELTA_CONFIG="${APPING_SIM_RUNTIME_DIR}/base-delta.json"
export APPING_BASE_EPSILON_CONFIG="${APPING_SIM_RUNTIME_DIR}/base-epsilon.json"

export APPING_PORTABLE_CONFIG="${APPING_SIM_RUNTIME_DIR}/portable-console.json"
python3 - "${PORTABLE_SOURCE_CONFIG}" "${APPING_PORTABLE_CONFIG}" "${DEFAULT_SOURCE}" <<'PY'
import json
import pathlib
import sys

source_path = pathlib.Path(sys.argv[1])
target_path = pathlib.Path(sys.argv[2])
default_source = sys.argv[3]

data = json.loads(source_path.read_text())
data["capture_input_uri"] = f"pulse://{default_source}" if default_source else ""
target_path.write_text(json.dumps(data, indent=2) + "\n")
PY

export APPING_LOCAL_RUNTIME_PREPARED=1
