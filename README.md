# AutoDiag Linux

**A Yocto-based embedded Linux platform for automotive diagnostics using SocketCAN, ISO-TP, and UDS.**

AutoDiag Linux is a custom embedded Linux distribution built with the Yocto Project (Scarthgap) and KAS. It targets QEMU x86-64 and provides a platform for developing and testing automotive Electronic Control Unit (ECU) diagnostic software.

The project includes a modular C-based UDS ECU simulator, Linux SocketCAN and ISO-TP integration, diagnostic session and security management, DTC handling, and persistent storage for writable diagnostic data identifiers.

> **Project scope:** This is a development and validation platform. ECU behavior and diagnostic communication have been tested in QEMU; physical vehicle and ECU hardware validation is not claimed.

## Features

- Custom Yocto Scarthgap distribution for `qemux86-64`.
- Linux SocketCAN and virtual CAN (`vcan`) support.
- Linux kernel ISO-TP transport.
- Modular UDS ECU server written in C.
- Diagnostic session management and ECU reset handling.
- SecurityAccess implementation.
- Diagnostic trouble code (DTC) management.
- Read and write DataByIdentifier services.
- Persistent storage for writable calibration data.
- systemd-managed ECU and CAN setup services.
- Automated UDS dispatcher and DID persistence tests.


## System architecture

The platform consists of three main layers:

- **Linux kernel:** SocketCAN, virtual CAN (vcan), and ISO-TP transport.
- **UDS ECU server:** Receives diagnostic requests and dispatches them to modular service handlers.
- **Diagnostic tester:** Communicates with the ECU over ISO-TP.

The ECU application contains dedicated components for UDS dispatching, diagnostic sessions, security, DTC management, and DID storage.

## Technology stack

| Component | Technology |
|---|---|
| Build system | Yocto Project |
| Yocto release | Scarthgap |
| Build orchestration | KAS |
| Target machine | QEMU x86-64 |
| Distribution | `autodiag` |
| Image | `core-image-minimal` |
| CAN interface | SocketCAN / vcan |
| Transport protocol | Linux ISO-TP |
| Diagnostic protocol | UDS |
| Application language | C |
| Init and service management | systemd |
| Development environment | WSL2, Ubuntu, Docker |


## UDS implementation

The ECU server uses a modular C architecture. Incoming diagnostic requests are received through Linux ISO-TP sockets and passed to a UDS dispatcher, which routes requests to their corresponding service handlers.

### Supported UDS services

| SID | Service | Function |
|---|---|---|
| `0x10` | DiagnosticSessionControl | Switch diagnostic sessions |
| `0x11` | ECUReset | Handle ECU reset requests |
| `0x14` | ClearDiagnosticInformation | Clear diagnostic trouble codes |
| `0x19` | ReadDTCInformation | Read diagnostic trouble codes |
| `0x22` | ReadDataByIdentifier | Read ECU data identifiers |
| `0x27` | SecurityAccess | Handle diagnostic security access |
| `0x28` | CommunicationControl | Control diagnostic communication |
| `0x2E` | WriteDataByIdentifier | Write supported ECU data identifiers |
| `0x3E` | TesterPresent | Keep diagnostic sessions alive |
| `0x85` | ControlDTCSetting | Control DTC setting |

### Data identifiers

- **DID `F190` — VIN:** A simulated vehicle identification number exposed through ReadDataByIdentifier (`0x22`).
- **DID `F1A1` — Calibration data:** A writable 16-byte calibration value handled through ReadDataByIdentifier (`0x22`) and WriteDataByIdentifier (`0x2E`).


### Persistent calibration storage

The writable calibration DID `F1A1` is stored at:

`/var/lib/autodiag-ecu/calibration-f1a1.bin`

The persistence implementation includes:

- Loading calibration data during ECU initialization.
- Creating a default value when the storage file is missing.
- Validation of the expected 16-byte data length.
- Rejection of truncated or oversized storage files.
- Atomic file replacement using a temporary file and rename.
- File and parent-directory synchronization.
- Preservation of in-memory calibration data when persistent storage fails.

The ECU systemd service uses a dedicated state directory with restrictive permissions.

If a UDS write cannot be persisted, the ECU returns negative response code `0x72` (General Programming Failure). The in-memory calibration value remains unchanged.


## Automated tests

Run the host-side UDS and persistence test suite from the repository root using:

    ./tests/run-uds-tests.sh

The test runner compiles the C tests with GCC using -Wall -Wextra -Werror -std=c11.

The suite covers:

- UDS request dispatching and service responses.
- Valid and invalid diagnostic requests.
- Supported and unsupported data identifiers.
- Calibration persistence and reload after initialization.
- Invalid calibration data lengths.
- Truncated and oversized storage files.
- Persistent write failures.
- Preservation of in-memory data after failed writes.
- UDS negative response 0x72 when persistent storage fails.

The persistence-failure regression test verifies that a failed UDS write does not modify the in-memory calibration value or destroy the previously stored calibration.


## Build instructions

### Prerequisites

- Linux environment or WSL2 with Ubuntu.
- Docker installed and running.
- Git.

### Clone the repository

    git clone https://github.com/alasalem1/auto-diag-linux.git
    cd auto-diag-linux

### Build the image

    ./docker/docker.sh build kas/auto-diag-linux.yml

The KAS configuration selects:

- Machine: `qemux86-64`
- Distribution: `autodiag`
- Image: `core-image-minimal`
- Yocto release: Scarthgap

The build produces a bootable QEMU image and associated deployment artifacts.


## Repository structure

    auto-diag-linux/
    ├── docker/                 # Docker build and shell helpers
    ├── docs/                   # Project and integration documentation
    ├── kas/                    # KAS build configurations
    ├── meta-auto-diag/         # Custom Yocto layer
    │   └── recipes-apps/
    │       ├── autodiag-ecu/   # UDS ECU server
    │       ├── autodiag-gui/   # Diagnostic GUI application
    │       └── autodiag-isotp/ # ISO-TP diagnostic test tools
    ├── scripts/                # Development scripts
    ├── tests/                  # Automated tests
    │   └── uds/                # UDS and DID persistence tests
    └── README.md


## Project status

### Implemented

- Yocto Scarthgap distribution built with KAS.
- QEMU x86-64 target configuration.
- SocketCAN virtual CAN interface and ISO-TP integration.
- Modular C-based UDS ECU simulator.
- Ten implemented UDS services.
- Diagnostic sessions and SecurityAccess.
- DTC management.
- VIN reading and writable calibration DID.
- Persistent calibration storage with failure handling.
- Automated UDS dispatcher and persistence tests.
- QEMU validation of diagnostic communication and persistence across service restart and guest reboot.

### Future improvements

- Complete and validate the diagnostic GUI.
- Integrate additional external diagnostic tester tools.
- Validate communication with physical CAN hardware and a real ECU.
- Extend security hardening and performance testing.

## Limitations

AutoDiag Linux is a development and validation platform. The ECU simulator uses simulated diagnostic data and has been validated in QEMU. Physical vehicle testing, production qualification, and automotive certification are outside the current project scope.

## Yocto layer architecture

The project uses a layered Yocto architecture managed by KAS.

| Layer | Responsibility |
|---|---|
| OpenEmbedded Core (`meta`) | Core metadata and `qemux86-64` machine configuration |
| `meta-openembedded` | Additional recipes from `meta-oe` and `meta-python` |
| `meta-qt6` | Qt 6 integration |
| `meta-auto-diag` | Custom AutoDiag distribution and automotive application recipes |

The custom `meta-auto-diag` layer contains:

- `conf/distro/autodiag.conf`: Custom Linux distribution configuration.
- `recipes-apps/`: Automotive diagnostic applications and related recipes.

The project currently uses the upstream `qemux86-64` machine configuration rather than maintaining a separate custom BSP layer.

## Automated QEMU integration test

The project includes an automated QEMU integration test:

`tests/qemu/run-qemu-integration.py`

The test runner uses Python and `pexpect` to:

1. Launch the Yocto-built QEMU image.
2. Wait for the guest Linux system to boot.
3. Log in automatically as root.
4. Execute the UDS diagnostic tester.
5. Verify the VIN response for DID `0xF190`.
6. Confirm that the test reports `TEST PASS`.
7. Terminate QEMU and clean up the temporary disk.

Run from the KAS build environment:

    python3 /work/tests/qemu/run-qemu-integration.py

The test validates the UDS ReadDataByIdentifier (`0x22`) transaction and the simulated VIN:

`AUTODIAG000000001`

## Continuous integration

GitHub Actions is configured in:

`.github/workflows/uds-tests.yml`

The workflow runs on:

- Pushes to `main`.
- Pull requests targeting `main`.
- Manual workflow dispatch.

It compiles and executes the UDS dispatcher and calibration persistence tests, then checks patch formatting.

The GitHub Actions workflow does not perform a complete Yocto image build or QEMU integration test. The QEMU integration test is currently executed locally.
