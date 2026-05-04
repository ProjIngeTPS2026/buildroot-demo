#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/lib/prepare-local-runtime.sh"

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
elif [[ -S "/tmp/.X11-unix/X0" ]]; then
  export DISPLAY="${DISPLAY:-:0}"
  export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=${APPING_SESSION_RUNTIME_DIR}/bus}"
  export QT_QPA_PLATFORM="xcb"
else
  echo "No graphical session detected. Run this script from a desktop terminal." >&2
  exit 1
fi

if ! command -v qtcreator >/dev/null 2>&1; then
  echo "qtcreator is not installed. Install it first." >&2
  exit 1
fi

cd "${ROOT_DIR}"
setsid -f qtcreator "${ROOT_DIR}/CMakeLists.txt" >/tmp/apping-qtcreator.log 2>&1
echo "Qt Creator launched for ${ROOT_DIR}"
echo "Log: /tmp/apping-qtcreator.log"
