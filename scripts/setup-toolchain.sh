#!/bin/bash

echo "SimpleOS Toolchain Setup Script"
echo "==============================="
echo ""
echo "This script will guide you through setting up the x86_64-elf cross-compiler toolchain."
echo ""

# Check OS
OS="Unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
fi

echo "Detected OS: $OS"
echo ""

if [[ "$OS" == "macOS" ]]; then
    echo "For macOS, you can install the toolchain using Homebrew:"
    echo ""
    echo "1. Install Homebrew if not already installed:"
    echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    echo ""
    echo "2. Add the x86_64-elf-gcc tap and install:"
    echo "   brew tap nativeos/i386-elf-toolchain"
    echo "   brew install x86_64-elf-gcc x86_64-elf-binutils"
    echo ""
    echo "Alternative: Build from source (takes longer):"
    echo "   Visit: https://wiki.osdev.org/GCC_Cross-Compiler"
elif [[ "$OS" == "Linux" ]]; then
    echo "For Linux, you have several options:"
    echo ""
    echo "1. Ubuntu/Debian:"
    echo "   sudo apt-get update"
    echo "   sudo apt-get install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo"
    echo ""
    echo "2. Arch Linux:"
    echo "   sudo pacman -S base-devel gmp libmpc mpfr"
    echo ""
    echo "Then build the cross-compiler from source:"
    echo "   Visit: https://wiki.osdev.org/GCC_Cross-Compiler"
else
    echo "For other operating systems, please visit:"
    echo "https://wiki.osdev.org/GCC_Cross-Compiler"
fi

echo ""
echo "After installing the toolchain, verify it works by running:"
echo "   x86_64-elf-gcc --version"
echo ""
echo "You'll also need GRUB tools for creating bootable ISOs:"
echo "   macOS: brew install xorriso"
echo "   Linux: sudo apt-get install grub-pc-bin xorriso"