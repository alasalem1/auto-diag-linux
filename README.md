# AutoDiag Linux

**A Yocto-based embedded Linux platform for automotive diagnostics using SocketCAN, ISO-TP, UDS, and a Qt 6 graphical interface.**

AutoDiag Linux is a custom embedded Linux distribution built with the Yocto Project (Scarthgap) and KAS. It targets QEMU x86-64 and provides a platform for developing and testing automotive Electronic Control Unit (ECU) diagnostic software.

The project includes a modular C-based UDS ECU simulator, Linux SocketCAN and ISO-TP integration, diagnostic session and security management, DTC handling, persistent storage for writable diagnostic data identifiers, and a Qt 6 graphical diagnostic tester.

> **Project scope:** This is a development and validation platform. ECU behavior and diagnostic communication have been tested in QEMU; physical vehicle and ECU hardware validation is not claimed.

## Features

- Custom Yocto Scarthgap distribution for `qemux86-64`.
- KAS-based build orchestration.
- Linux SocketCAN and virtual CAN (`vcan`) support.
- Linux kernel ISO-TP transport.
- Modular C-based UDS ECU simulator.
- Qt 6 graphical diagnostic interface.
- UDS diagnostic request transmission and ECU response display.
- Seven UDS Quick Actions in the graphical interface.
- Automatic TesterPresent transmission every 2 seconds while connected.
- Diagnostic session management and ECU reset handling.
- SecurityAccess implementation.
- Diagnostic trouble code (DTC) management.
- Read and write DataByIdentifier services.
- Persistent storage for writable diagnostic data identifiers.
- systemd-managed ECU and CAN setup services.
- Automated UDS dispatcher and DID persistence tests.
- Automated QEMU integration testing.

## System architecture

The platform consists of four main components:

- **Yocto embedded Linux:** Custom Linux distribution built with Yocto Scarthgap and KAS.
- **Linux kernel:** SocketCAN, virtual CAN (`vcan`), and ISO-TP transport.
- **UDS ECU server:** Receives diagnostic requests and dispatches them to modular service handlers.
- **Diagnostic tester:** Qt 6 graphical application that communicates with the ECU over ISO-TP.

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
| ECU application | C |
| Graphical interface | Qt 6 Widgets / C++ |
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
- **DID `F1A0` — Protected calibration identifier:** A security-protected data identifier used to demonstrate access control.

### Diagnostic sessions and security

The ECU implements diagnostic session management and SecurityAccess.

The diagnostic workflow includes:

1. Entering the Extended Diagnostic Session using `10 03`.
2. Requesting a security seed using `27 01`.
3. Sending the corresponding demo key using `27 02`.
4. Accessing protected diagnostic data after successful authentication.

The graphical interface provides Quick Actions for these diagnostic operations.

### Automatic TesterPresent

The Qt diagnostic application implements automatic TesterPresent transmission.

- UDS request: `3E 80`.
- Transmission interval: 2 seconds.
- Starts automatically when the CAN connection is established.
- Stops when the CAN connection is disconnected.

The `0x80` subfunction bit requests suppression of the positive response.

This mechanism periodically refreshes the ECU's S3 session timeout and helps maintain the active diagnostic session during longer diagnostic operations.

## Qt 6 diagnostic GUI

AutoDiag Linux includes a Qt 6 Widgets graphical diagnostic tester.

### Main features

- CAN interface selection.
- Connection and disconnection management.
- UDS diagnostic request input using hexadecimal bytes.
- ECU response display.
- Diagnostic activity log.
- Quick Actions for common UDS services.
- Automatic TesterPresent while connected.

### UDS Quick Actions

| Action | UDS request |
|---|---|
| Default Session | `10 01` |
| Extended Session | `10 03` |
| Request Security Seed | `27 01` |
| Unlock (Demo Key) | `27 02 B7 F7 C8 09` |
| Read VIN (F190) | `22 F1 90` |
| Read Protected DID (F1A0) | `22 F1 A0` |
| Read Calibration (F1A1) | `22 F1 A1` |

The GUI uses Linux ISO-TP sockets to exchange diagnostic messages with the ECU simulator.

## Persistent calibration storage

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

Run the host-side UDS and persistence test suite from the repository root:

```bash
./tests/run-uds-tests.sh
```

The test runner compiles the C tests with GCC using `-Wall -Wextra -Werror -std=c11`.

The suite covers:

- UDS request dispatching and service responses.
- Valid and invalid diagnostic requests.
- Supported and unsupported data identifiers.
- Calibration persistence and reload after initialization.
- Invalid calibration data lengths.
- Truncated and oversized storage files.
- Persistent write failures.
- Preservation of in-memory data after failed writes.
- UDS negative response `0x72` when persistent storage fails.

The persistence-failure regression test verifies that a failed UDS write does not modify the in-memory calibration value or destroy the previously stored calibration.

## Automated QEMU integration test

The project includes an automated QEMU integration test:

`tests/qemu/run-qemu-integration.py`

The test runner uses Python and `pexpect` to:

1. Launch the Yocto-built QEMU image using an independent temporary root filesystem copy.
2. Wait for the guest Linux system to boot and log in automatically as root.
3. Execute `/usr/bin/autodiag-isotp-test` and verify the VIN response for DID `0xF190`.
4. Execute `/usr/bin/autodiag-session-test` and verify diagnostic session transitions and access restrictions.
5. Verify SecurityAccess seed handling, incorrect-key rejection, and successful unlock.
6. Write the 16-byte calibration value `NEW-CAL-DATA-002` to protected DID `0xF1A1` and read it back for exact comparison.
7. Verify protected access is denied again after returning to the Default Session.
8. Terminate QEMU and clean up the temporary disk.

Run from the KAS build environment:

```bash
python3 /work/tests/qemu/run-qemu-integration.py
```

The test validates the UDS ReadDataByIdentifier (`0x22`) transaction and the simulated VIN:

`AUTODIAG000000001`

## Manual GUI validation

The Qt 6 diagnostic interface was launched and tested in the QEMU environment.

The following diagnostic workflow was successfully validated:

| Test | Request | Expected positive response | Result |
|---|---|---|---|
| Extended Diagnostic Session | `10 03` | `50 03 00 32 01 F4` | PASS |
| Request Security Seed | `27 01` | `67 01 12 34 56 78` | PASS |
| Security Access Unlock | `27 02 B7 F7 C8 09` | `67 02` | PASS |
| Read Protected DID | `22 F1 A0` | `62 F1 A0` + calibration identifier | PASS |
| Automatic TesterPresent | `3E 80` | Positive response suppressed | PASS |

The protected DID returned:

`AUTODIAG-CAL-001`

The Extended Session remained accessible after waiting longer than the configured 5-second ECU S3 timeout, with automatic TesterPresent transmissions continuing every 2 seconds.

These results demonstrate the integrated GUI-to-ECU diagnostic workflow in the QEMU virtual CAN environment.

## Build instructions

### Prerequisites

- Linux environment or WSL2 with Ubuntu.
- Docker installed and running.
- Git.

### Clone the repository

```bash
git clone https://github.com/alasalem1/auto-diag-linux.git
cd auto-diag-linux
```

### Build the image

```bash
./docker/docker.sh build kas/auto-diag-linux.yml
```

The KAS configuration selects:

- Machine: `qemux86-64`
- Distribution: `autodiag`
- Image: `core-image-minimal`
- Yocto release: Scarthgap

The build produces a bootable QEMU image and associated deployment artifacts.

## Repository structure

```text
auto-diag-linux/
├── .github/
│   └── workflows/
│       └── uds-tests.yml
├── docker/
│   └── docker.sh
├── docs/
│   └── UDS_ISOTP_INTEGRATION_TEST.md
├── kas/
│   ├── auto-diag-linux.yml
│   └── include/
│       ├── local.yml
│       └── oe.yml
├── meta-auto-diag/
│   ├── conf/
│   │   ├── distro/autodiag.conf
│   │   └── layer.conf
│   ├── recipes-apps/
│   │   ├── autodiag-ecu/
│   │   ├── autodiag-gui/
│   │   └── autodiag-isotp/
│   └── recipes-kernel/
│       └── linux/linux-yocto_%.bbappend
├── tests/
│   ├── qemu/
│   │   └── run-qemu-integration.py
│   ├── uds/
│   │   ├── test_did_manager_persistence.c
│   │   └── test_uds_dispatch.c
│   └── run-uds-tests.sh
├── .gitignore
└── README.md
```

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
- Qt 6 graphical diagnostic interface.
- Seven UDS Quick Actions.
- Automatic TesterPresent transmission every 2 seconds.
- Automated UDS dispatcher and persistence tests.
- Automated QEMU integration tests.
- Manual GUI validation of diagnostic sessions, SecurityAccess, and protected DID reading.

### Future improvements

- Integrate additional external diagnostic tester tools.
- Add support for physical CAN hardware and validate communication with a real ECU.
- Extend security hardening and performance testing.
- Improve diagnostic visualization and response decoding.

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
- `autodiag-gui`: Qt 6 graphical diagnostic tester.
- `autodiag-ecu`: Modular C-based UDS ECU simulator.

The project currently uses the upstream `qemux86-64` machine configuration rather than maintaining a separate custom BSP layer.

## Continuous integration

GitHub Actions is configured in:

`.github/workflows/uds-tests.yml`

The workflow provides automated execution of the UDS test suite.
