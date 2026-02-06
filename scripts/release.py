#!/usr/bin/env python3
import argparse
import os
import platform
import subprocess
from pathlib import Path

APP_NAME = "frogklan"
WIN_EXE = "frogklan-win.exe"
LINUX_EXE = "frogklan-linux"
MAC_APP = "frogklan-mac.app"

DIST_WIN = Path("dist/win")
DIST_LINUX = Path("dist/linux")
DIST_MAC = Path("dist/mac")

OUT_DIR = Path("out")

def run(cmd: list[str], *, env: dict | None = None) -> None:
    print(">>", " ".join(cmd))
    subprocess.check_call(cmd, env=env)

def ensure_exists(p: Path, what: str) -> None:
    if not p.exists():
        raise SystemExit(f"[release] missing {what}: {p}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True, help="Semver like 1.2.3")
    ap.add_argument("--target", choices=["windows", "linux", "macos", "all"], default="all")
    args = ap.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    targets = [args.target]
    if args.target == "all":
        sysplat = platform.system().lower()
        if "windows" in sysplat:
            targets = ["windows"]
        elif "darwin" in sysplat:
            targets = ["macos"]
        else:
            targets = ["linux"]

    built = []

    if "windows" in targets:
        ensure_exists(DIST_WIN / WIN_EXE, "Windows exe")
        ensure_exists(Path("installers/nsis/frogklan_installer.nsi"), "NSIS script")
        out_setup = OUT_DIR / f"{APP_NAME}-{args.version}-Setup.exe"
        env = os.environ.copy()
        env["APP_VERSION"] = args.version
        env["APP_NAME"] = APP_NAME
        env["DIST_WIN"] = str(DIST_WIN.resolve())
        env["WIN_EXE"] = WIN_EXE
        env["OUT_SETUP"] = str(out_setup.resolve())
        run(["makensis", "installers/nsis/frogklan_installer.nsi"], env=env)
        ensure_exists(out_setup, "Windows Setup.exe")
        built.append(out_setup)

    if "linux" in targets:
        ensure_exists(DIST_LINUX / LINUX_EXE, "Linux executable")
        ensure_exists(Path("installers/linux/make_appimage.sh"), "AppImage script")
        run(["bash", "installers/linux/make_appimage.sh", APP_NAME, args.version, str(DIST_LINUX), str(OUT_DIR), LINUX_EXE])
        out_img = OUT_DIR / f"{APP_NAME}-{args.version}-x86_64.AppImage"
        ensure_exists(out_img, "AppImage output")
        built.append(out_img)

    if "macos" in targets:
        ensure_exists(DIST_MAC / MAC_APP, "macOS .app bundle")
        ensure_exists(Path("installers/macos/make_pkg_from_app.sh"), "macOS pkg script")
        run(["bash", "installers/macos/make_pkg_from_app.sh", APP_NAME, args.version, str(DIST_MAC), str(OUT_DIR), MAC_APP])
        out_pkg = OUT_DIR / f"{APP_NAME}-{args.version}.pkg"
        ensure_exists(out_pkg, "macOS .pkg output")
        built.append(out_pkg)

    print("\n[release] outputs:")
    for p in built:
        print(" -", p)

if __name__ == "__main__":
    main()
