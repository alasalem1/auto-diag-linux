import os
import shutil
import tempfile
from pathlib import Path
import pexpect
BUILD = Path(os.environ.get("BUILDDIR", "/work/build"))
DEPLOY = BUILD / "tmp-glibc/deploy/images/qemux86-64"
QEMU = BUILD / "tmp-glibc/sysroots-components/x86_64/qemu-system-native/usr/bin/qemu-system-x86_64"
KERNEL = DEPLOY / "bzImage"
ROOTFS = DEPLOY / "core-image-minimal-qemux86-64.rootfs.ext4"

def require_file(path):
    if not path.is_file():
        raise RuntimeError("Required file not found: " + str(path))

def run_guest_command(child, command, marker, timeout=30):
    child.sendline(command + "; rc=$?; echo " + marker + "=$rc")
    child.expect(marker + r"=(\d+)", timeout=timeout)
    result = int(child.match.group(1))
    output = child.before.decode("utf-8", errors="replace")
    print(output, end="")
    if result != 0:
        raise RuntimeError(command + " failed with exit code " + str(result))
    return output

def main():
    require_file(QEMU)
    require_file(KERNEL)
    require_file(ROOTFS)
    print("QEMU, kernel, and root filesystem found.")
    env = os.environ.copy()
    native_libs = [
        str(BUILD / "tmp-glibc/sysroots-components/x86_64/dtc-native/usr/lib"),
        str(BUILD / "tmp-glibc/sysroots-components/x86_64/libslirp-native/usr/lib"),
    ]
    env["LD_LIBRARY_PATH"] = ":".join(native_libs + [env.get("LD_LIBRARY_PATH", "")])
    with tempfile.TemporaryDirectory(prefix="autodiag-qemu-") as temp_dir:
        test_disk = Path(temp_dir) / "rootfs.ext4"
        print("Creating independent QEMU test disk...")
        shutil.copyfile(ROOTFS, test_disk)
        args = [
            "-cpu", "IvyBridge",
            "-machine", "q35,i8042=off",
            "-m", "256",
            "-smp", "4",
            "-kernel", str(KERNEL),
            "-drive", "if=none,id=hd,file=" + str(test_disk) + ",format=raw",
            "-device", "virtio-scsi-pci,id=scsi",
            "-device", "scsi-hd,drive=hd",
            "-object", "rng-random,filename=/dev/urandom,id=rng0",
            "-device", "virtio-rng-pci,rng=rng0",
            "-append",
            "root=/dev/sda rw console=ttyS0 oprofile.timer=1 tsc=reliable no_timer_check rcupdate.rcu_expedited=1 swiotlb=0",
            "-nic", "none",
            "-display", "none",
            "-monitor", "none",
            "-serial", "stdio",
            "-no-reboot",
        ]
        print("Starting QEMU...")
        child = pexpect.spawn(str(QEMU), args, env=env, timeout=180)
        try:
            child.expect("qemux86-64 login:", timeout=180)
            print("QEMU guest booted. Logging in as root...")
            child.sendline("root")
            child.expect(r"root@qemux86-64:~#", timeout=30)
            print("Guest login successful.")
            uds_output = run_guest_command(child, "/usr/bin/autodiag-isotp-test", "UDS_TEST", timeout=20)
            if "TEST PASS" not in uds_output:
                raise RuntimeError("UDS integration test did not report TEST PASS")
            print("UDS integration test passed.")
            session_output = run_guest_command(child, "/usr/bin/autodiag-session-test", "SESSION_TEST", timeout=60)
            if "ALL SESSION, SECURITY ACCESS AND PROTECTED DID WRITE/READBACK TESTS PASSED" not in session_output:
                raise RuntimeError("Session/security/protected DID integration test did not report success")
            print("Session, security access, and protected DID write/readback integration test passed.")
        finally:
            child.terminate(force=True)

if __name__ == "__main__":
    main()
