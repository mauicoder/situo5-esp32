#!/usr/bin/env python3
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_pio_executable():
    pio_path = shutil.which("pio") or shutil.which("platformio")
    if pio_path:
        return pio_path

    home = Path.home()
    candidates = [
        home / ".platformio" / "penv" / "bin" / "pio",
        home / ".platformio" / "penv" / "bin" / "platformio",
        Path("/usr/local/bin/pio"),
        Path("/usr/local/bin/platformio"),
    ]
    for candidate in candidates:
        if candidate.exists() and os.access(candidate, os.X_OK):
            return str(candidate)

    raise RuntimeError("PlatformIO executable (pio) not found.")


def find_boot_app0(env_build_dir):
    local_boot_app0 = env_build_dir / "boot_app0.bin"
    if local_boot_app0.exists():
        return local_boot_app0

    home = Path.home()
    packages_dir = home / ".platformio" / "packages"
    if packages_dir.exists():
        for p in packages_dir.glob("**/boot_app0.bin"):
            if p.is_file():
                return p

    raise FileNotFoundError("boot_app0.bin could not be found.")


def build_environment(env_name, project_dir):
    pio_exec = find_pio_executable()
    print(f"Building PlatformIO environment: {env_name} using {pio_exec}...")
    cmd = [pio_exec, "run", "-e", env_name]
    res = subprocess.run(cmd, cwd=project_dir, check=True)
    if res.returncode != 0:
        raise RuntimeError(f"PlatformIO build failed for environment {env_name}")


def package_flasher(env_name="heltec_wifi_lora_32_V3", project_dir=None):
    if project_dir is None:
        project_dir = Path(__file__).resolve().parent.parent

    build_environment(env_name, project_dir)

    pio_build_dir = project_dir / ".pio" / "build" / env_name
    output_dir = project_dir / "docs" / "firmware" / env_name
    output_dir.mkdir(parents=True, exist_ok=True)

    binary_files = {
        "bootloader.bin": pio_build_dir / "bootloader.bin",
        "partitions.bin": pio_build_dir / "partitions.bin",
        "firmware.bin": pio_build_dir / "firmware.bin",
        "boot_app0.bin": find_boot_app0(pio_build_dir),
    }

    for name, src_path in binary_files.items():
        if not src_path.exists():
            raise FileNotFoundError(f"Required binary {name} not found at {src_path}")
        dest_path = output_dir / name
        shutil.copy2(src_path, dest_path)
        print(f"Copied {src_path} -> {dest_path} ({dest_path.stat().st_size} bytes)")

    manifest_data = {
        "name": f"iown-homecontrol ({env_name})",
        "version": "1.0.0",
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": [
                    {"path": "bootloader.bin", "offset": 0x0000},
                    {"path": "partitions.bin", "offset": 0x8000},
                    {"path": "boot_app0.bin", "offset": 0xE000},
                    {"path": "firmware.bin", "offset": 0x10000},
                ],
            }
        ],
    }

    manifest_path = output_dir / "manifest.json"
    with open(manifest_path, "w") as f:
        json.dump(manifest_data, f, indent=2)
    print(f"Generated manifest at {manifest_path}")


if __name__ == "__main__":
    package_flasher()
