# UDS ISO-TP Integration Test

**Date:** 2026-11-09
**Result:** PASS

## 1. Objective

Verify end-to-end communication between the diagnostic tester and the
ECU application using UDS over Linux SocketCAN ISO-TP.

The test covers:
- Diagnostic Session Control (0x10)
- Security Access (0x27)
- Write Data By Identifier (0x2E)
- Read Data By Identifier (0x22)

## 2. Test Environment

| Item | Value |
|---|---|
| Platform | QEMU qemux86-64 |
| CAN interface | vcan0 |
| Tester CAN ID | 0x700 |
| ECU CAN ID | 0x708 |
| ECU application | autodiag-ecu |
| DID | 0xF1A1 |
| DID size | 16 bytes |
| Transport | Linux SocketCAN ISO-TP |

The test was performed inside the QEMU guest using `cansend` and `candump`.

## 3. Test Procedure and Results

### 3.1 Enter Extended Diagnostic Session

Tester request:

`700#0210030000000000`

ECU positive response:

`708#065003003201F4`

UDS response: `50 03 00 32 01 F4`.

### 3.2 Request SecurityAccess Seed

Tester request:

`700#0227010000000000`

ECU response:

`708#06670112345678`

UDS response: `67 01 12 34 56 78`.

### 3.3 Send SecurityAccess Key

Tester request:

`700#062702B7F7C809`

ECU positive response:

`708#026702`

UDS response: `67 02`.

The ECU accepted the key and unlocked the protected DID operation.

### 3.4 Write DID F1A1 (Service 0x2E)

The tester wrote the 16-byte ASCII value:

`NEW-CAL-DATA-001`

ISO-TP First Frame:

`700#10132EF1A14E4557`

Consecutive Frame 1:

`700#212D43414C2D4441`

Consecutive Frame 2:

`700#2254412D30303100`

ECU Flow Control:

`708#300000`

ECU positive UDS response:

`708#036EF1A1`

UDS response: `6E F1 A1`.

### 3.5 Read DID F1A1 (Service 0x22)

Tester request:

`700#0322F1A100000000`

ECU ISO-TP First Frame:

`708#101362F1A14E4557`

Tester Flow Control:

`700#3000000000000000`

ECU Consecutive Frame 1:

`708#212D43414C2D4441`

ECU Consecutive Frame 2:

`708#2254412D303031`

Reassembled UDS response:

`62 F1 A1 4E 45 57 2D 43 41 4C 2D 44 41 54 41 2D 30 30 31`

Decoded value:

`NEW-CAL-DATA-001`

The returned value exactly matches the value written in the preceding
0x2E request.

## 4. ISO-TP Polling Fix

During integration testing, the ISO-TP socket reported `POLLERR` together
with `POLLIN`.

The previous event handler discarded readable data whenever `POLLERR`
was present. The handler was updated to distinguish unavailable-socket
conditions (`POLLHUP` and `POLLNVAL`), inspect pending socket errors using
`getsockopt(SO_ERROR)`, and continue processing when `POLLIN` is present.

The updated implementation was used during the successful live test.

## 5. Conclusion

The QEMU integration test passed. The ECU accepted the 0x2E write request
and returned the exact same 16-byte value through the 0x22 read request.

## 6. Persistent DID Verification

DID F1A1 uses persistent file storage at:

`/var/lib/autodiag-ecu/calibration-f1a1.bin`

The following end-to-end test was completed in QEMU:

1. Entered extended diagnostic session (`0x10 0x03`).
2. Unlocked SecurityAccess (`0x27`) using the test key.
3. Wrote the 16-byte value `NEW-CAL-DATA-001` using WriteDataByIdentifier (`0x2E`).
4. Received positive response `6E F1 A1`.
5. Compared the persistent file byte-for-byte against the expected 16-byte value.
6. Rebooted the complete QEMU Linux guest.
7. Confirmed the calibration file still contained the exact written value.
8. Confirmed the ECU service loaded DID F1A1 from persistent storage at startup.
9. Read DID F1A1 using ReadDataByIdentifier (`0x22`) after reboot.

The final ISO-TP response was:

`62 F1 A1 4E 45 57 2D 43 41 4C 2D 44 41 54 41 2D 30 30 31`

Decoded value: `NEW-CAL-DATA-001`.

**Result: PASS** — the UDS-written calibration value survived a complete Linux reboot and was returned correctly by the ECU.

## 7. Limitations and Future Work

- Testing used QEMU and virtual CAN (`vcan0`), not physical CAN hardware.
- The SecurityAccess algorithm is educational and is not production-grade.
- Physical ECU and hardware CAN validation remain future work.

## 8. Automated QEMU Boot and UDS Test

The automated integration test is implemented in:

`tests/qemu/run-qemu-integration.py`

The Python runner uses `pexpect` to:
1. Check that QEMU, the kernel, and the root filesystem exist.
2. Create an independent temporary root filesystem copy.
3. Start the Yocto-built QEMU image.
4. Wait for the Linux login prompt and log in as root.
5. Execute `/usr/bin/autodiag-isotp-test`.
6. Verify that the output contains `TEST PASS`.
7. Stop QEMU and clean up the temporary disk.

### Automated test environment

| Item | Value |
|---|---|
| Machine | qemux86-64 |
| Emulator | Yocto-native QEMU |
| Test runner | Python 3 + pexpect |
| Diagnostic interface | vcan0 |
| Diagnostic service | ReadDataByIdentifier (0x22) |
| DID | 0xF190 (VIN) |

### Verified result

The automated test successfully booted the guest, logged in as root, and completed a UDS diagnostic exchange.

Expected VIN:

`AUTODIAG000000001`

The ECU returned a positive response (`0x62`) for DID `0xF190`.

**Result: PASS** — the automated QEMU integration test verified the VIN response and reported `UDS integration test passed.`

Run from the KAS build environment:

`python3 /work/tests/qemu/run-qemu-integration.py`
