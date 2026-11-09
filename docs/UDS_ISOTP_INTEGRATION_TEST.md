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

## 6. Limitations and Future Work

- Testing used QEMU and virtual CAN (`vcan0`), not physical CAN hardware.
- DID data is stored in RAM and is not persistent across application or
  system restarts.
- The SecurityAccess algorithm is educational and is not production-grade.
- Physical ECU testing and persistence testing remain future work.
