#!/usr/bin/env python3
"""Install checksum-pinned VitaSDK build inputs for app-bundled TLS (Python 3.12+).
The VPK includes the linked libraries; end users do not run this script.
"""
import hashlib
import os
from pathlib import Path
import sys
import tarfile
import tempfile
import urllib.request

PACKAGES = {
    "curl-mbedtls": "58c0ffe0d94bd6933e004a025ecea180e3a42af9671ed9ef2229998e97a0f702",
    "mbedtls": "4a509e9b91e887e974cf58cf39541d121c0f7118d759732179881daa840c8031",
    "zlib": "f39cea16c176f93dc744597113f093db0d48adf23dd25a14ee0d2ec47df17fa6",
    "zstd": "b459390f8f6b6f5c7657d32d2681c82206b636133c4d277916e43270e0eec00b",
}

def main():
    if sys.version_info < (3, 12):
        raise SystemExit("Python 3.12+ is required for safe extraction.")
    sdk = os.environ.get("VITASDK")
    if not sdk:
        raise SystemExit("Set VITASDK to your VitaSDK installation.")
    root = Path(sdk).resolve()
    if not (root / "share/vita.toolchain.cmake").is_file():
        raise SystemExit("VITASDK does not identify a VitaSDK installation.")
    with tempfile.TemporaryDirectory() as temporary:
        verified = []
        for name, digest in PACKAGES.items():
            target = Path(temporary) / (name + ".tar.xz")
            url = "https://github.com/vitasdk/packages/releases/download/master/" + target.name
            print("Fetching", name)
            request = urllib.request.Request(url, headers={"User-Agent": "PSVSD-AM-build"})
            with urllib.request.urlopen(request, timeout=60) as response, target.open("wb") as output:
                total = 0
                while chunk := response.read(65536):
                    total += len(chunk)
                    if total > 100 * 1024 * 1024:
                        raise SystemExit("Dependency archive exceeds size limit.")
                    output.write(chunk)
            if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
                raise SystemExit("Checksum mismatch: " + name + "; refusing installation.")
            verified.append(target)
        destination = root / "arm-vita-eabi"
        for target in verified:
            print("Installing verified", target.name)
            with tarfile.open(target) as archive:
                archive.extractall(destination, filter="data")
    print("Bundled TLS build dependencies installed. Build the VPK normally.")

if __name__ == "__main__":
    main()
