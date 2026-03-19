#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  deploy.sh  —  Install Kinetic onto Termux / any POSIX system
#
#  Usage:
#    ./deploy.sh                    Install to ~/bin/  (default)
#    ./deploy.sh /usr/local/bin     Install to custom prefix
#    ./deploy.sh --termux           Install to Termux prefix (auto-detected)
#
#  What it does:
#    1. Copies kinetic binary to <prefix>/kinetic
#    2. Copies assets/ to <prefix>/assets/   (aapt2-aarch64 lives here)
#    3. Adds <prefix> to PATH in ~/.bashrc / ~/.bash_profile if needed
#
#  The kinetic binary MUST be run from a directory where assets/ is beside it,
#  OR the assets/ folder must be in the same directory as the binary in PATH.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$SCRIPT_DIR/dist/kinetic"
ASSETS_DIR="$SCRIPT_DIR/dist/assets"

# ── Determine install prefix ──────────────────────────────────────────────────
PREFIX=""
TERMUX=false

for arg in "$@"; do
    case "$arg" in
        --termux)
            TERMUX=true
            PREFIX="/data/data/com.termux/files/usr/bin"
            ;;
        --*)
            echo "Unknown option: $arg"
            exit 1
            ;;
        *)
            PREFIX="$arg"
            ;;
    esac
done

if [[ -z "$PREFIX" ]]; then
    # Auto-detect Termux
    if [[ -d "/data/data/com.termux/files/usr/bin" ]]; then
        PREFIX="/data/data/com.termux/files/usr/bin"
        TERMUX=true
    else
        PREFIX="$HOME/bin"
    fi
fi

# ── Validate source files ─────────────────────────────────────────────────────
if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: kinetic binary not found at $BINARY"
    echo "       Build it first:  cmake -B build && cmake --build build -j\$(nproc)"
    exit 1
fi
if [[ ! -d "$ASSETS_DIR" ]]; then
    echo "ERROR: assets/ directory not found at $ASSETS_DIR"
    echo "       The bundled aapt2-aarch64 is required for aarch64 devices."
    exit 1
fi

# ── Install ───────────────────────────────────────────────────────────────────
echo "⚡ Installing Kinetic"
echo "   Binary  : $BINARY  →  $PREFIX/kinetic"
echo "   Assets  : $ASSETS_DIR/  →  $PREFIX/assets/"
echo ""

mkdir -p "$PREFIX"

# Copy binary
cp "$BINARY" "$PREFIX/kinetic"
chmod 755 "$PREFIX/kinetic"

# Copy assets/ directory (aapt2-aarch64 lives here)
mkdir -p "$PREFIX/assets"
cp "$ASSETS_DIR/aapt2-aarch64" "$PREFIX/assets/aapt2-aarch64"
chmod 755 "$PREFIX/assets/aapt2-aarch64"

echo "✓  kinetic  → $PREFIX/kinetic"
echo "✓  aapt2    → $PREFIX/assets/aapt2-aarch64"

# ── PATH check ────────────────────────────────────────────────────────────────
if ! echo "$PATH" | grep -q "$PREFIX"; then
    echo ""
    echo "NOTE: $PREFIX is not in your PATH."

    if [[ "$TERMUX" == "true" ]]; then
        echo "      Termux usually adds /data/data/com.termux/files/usr/bin automatically."
        echo "      If kinetic is not found, run:"
        echo "        export PATH=\"\$PATH:$PREFIX\""
    else
        RC="$HOME/.bashrc"
        [[ -f "$HOME/.bash_profile" ]] && RC="$HOME/.bash_profile"
        echo "      Adding to $RC ..."
        echo "export PATH=\"\$PATH:$PREFIX\"" >> "$RC"
        echo "      Done. Run:  source $RC"
    fi
fi

# ── Verify ────────────────────────────────────────────────────────────────────
echo ""
"$PREFIX/kinetic" --version && echo "" && echo "✓  Kinetic installed successfully." || true

echo ""
echo "Deployment layout:"
echo "  $PREFIX/"
echo "  ├── kinetic"
echo "  └── assets/"
echo "      └── aapt2-aarch64   (bundled native aapt2 for aarch64 devices)"
echo ""
echo "Usage from your project root:"
echo "  kinetic --build"
