[README.md]

<p align="center">
  <a href="https://github.com/whisprer-s-webspinners/frogKlan">
    <img src="https://img.shields.io/github/stars/whisprer-s-webspinners/frogKlan?style=for-the-badge" alt="GitHub stars" />
  </a>
  <a href="https://github.com/whisprer-s-webspinners/frogKlan/issues">
    <img src="https://img.shields.io/github/issues/whisprer-s-webspinners/frogKlan?style=for-the-badge" alt="GitHub issues" />
  </a>
  <a href="https://github.com/whisprer-s-webspinners/frogKlan/fork">
    <img src="https://img.shields.io/github/forks/whisprer-s-webspinners/frogKlan?style=for-the-badge" alt="GitHub forks" />
  </a>
</p>

# frogKlan
# FrogKlan SIP QA (signaling-only)

A tiny, dependency-free SIP signaling QA tool:
- SIP OPTIONS probe (availability + RTT)
- Optional SIP REGISTER with Digest auth (401 challenge handling)
- Writes JSON report to per-OS app data directory

## Build (all OS)

### Configure
mkdir -p build
cd build
cmake ..

### Build
cmake --build . --config Release

The output binary will be:
- build/frogklan (Linux/macOS)
- build/Release/frogklan.exe (Visual Studio generator)

## Run

OPTIONS probe:
./frogklan qa --host sip.example.com --from sip:qa@ex.com --to sip:qa@ex.com

REGISTER probe (Digest):
./frogklan qa --host sip.example.com --from sip:qa@ex.com --to sip:qa@ex.com \
  --register --aor sip:1001@example.com --contact sip:1001@sip.example.com \
  --user 1001 --pass secret --expires 120
