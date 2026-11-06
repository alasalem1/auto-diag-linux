#!/usr/bin/env bash

set -e

# ==========================================================
# AutoDiag Linux - Docker/KAS script
# Target: QEMU x86-64
# ==========================================================

# Project root directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Python virtual environment
VENV_DIR="${PROJECT_DIR}/yocto-venv"


# ==========================================================
# 1. Prepare the Python environment
# ==========================================================
do_prepare_env() {
    # Check whether the virtual environment exists
    if [ -d "${VENV_DIR}" ]; then
        # Verify that it is a Python virtual environment
        if [ ! -f "${VENV_DIR}/bin/activate" ]; then
            echo "[x] Directory exists but is not a valid Python venv"
            return 1
        fi
    else
        echo "[+] Creating Python virtual environment"

        python3 -m venv "${VENV_DIR}" || {
            echo "[x] Failed to create the venv"
            return 1
        }
    fi

    # Activate the virtual environment
    echo "[+] Activating the Python venv"

    source "${VENV_DIR}/bin/activate" || {
        echo "[x] Failed to activate the venv"
        return 1
    }

    # Install KAS if needed
    if ! command -v kas-container >/dev/null 2>&1; then
        echo "[+] Installing KAS"

        pip install kas || {
            echo "[x] Failed to install KAS"
            return 1
        }
    fi

    echo "[+] Python environment is ready"
}


# ==========================================================
# 2. Check Docker and KAS availability
# ==========================================================
do_check_tools() {
    if ! command -v docker >/dev/null 2>&1; then
        echo "[x] Docker is not installed or not available in PATH"
        return 1
    fi

    if ! docker info >/dev/null 2>&1; then
        echo "[x] Docker is not running or access is denied"
        return 1
    fi

    if ! command -v kas-container >/dev/null 2>&1; then
        echo "[x] kas-container command is not available"
        return 1
    fi

    echo "[+] Docker and kas-container are ready"
}


# ==========================================================
# 3. Check out repositories
# ==========================================================
do_kas_checkout() {
    local yml="$1"

    echo "[+] Checking out repositories using ${yml}"

    kas-container checkout "${yml}"
}


# ==========================================================
# 4. Open an interactive KAS shell
# ==========================================================
do_kas_shell() {
    local yml="$1"

    echo "[+] Opening KAS shell using ${yml}"

    kas-container shell "${yml}"
}


# ==========================================================
# 5. Build the Yocto image
# ==========================================================
do_kas_build() {
    local yml="$1"

    echo "[+] Building Yocto image using ${yml}"

    kas-container build "${yml}"
}


# ==========================================================
# 6. Main function
# ==========================================================
main() {
    # Require exactly two arguments
    if [ "$#" -ne 2 ]; then
        echo "Usage: $0 checkout|shell|build <path/to/yml>"
        return 1
    fi

    local action="$1"
    local yml="$2"

    # Resolve the YAML path relative to the project root
    if [[ "${yml}" != /* ]]; then
        yml="${PROJECT_DIR}/${yml}"
    fi

    # Verify that the YAML configuration exists
    if [ ! -f "${yml}" ]; then
        echo "[x] Configuration file does not exist: ${yml}"
        return 1
    fi

    # Prepare the environment and check required tools
    do_prepare_env || return 1
    do_check_tools || return 1

    # Execute the requested action
    case "${action}" in
        checkout)
            do_kas_checkout "${yml}"
            ;;
        shell)
            do_kas_shell "${yml}"
            ;;
        build)
            do_kas_build "${yml}"
            ;;
        *)
            echo "[x] Unknown action: ${action}"
            echo "Usage: $0 checkout|shell|build <path/to/yml>"
            return 1
            ;;
    esac
}


# ==========================================================
# 7. Start the script
# ==========================================================
main "$@"
