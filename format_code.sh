#!/bin/bash
# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
VIOLET='\033[0;35m'
NC='\033[0m' # No Color

# Format C++ code using clang-format
echo -e "[${GREEN} Formatting ${NC}] C++ code..."
find . -name "*.cpp" -o -name "*.hpp" -not -path "./build/*" -not -path "./.xmake/*"| xargs clang-format -i
echo -e "[${GREEN} Formatting ${NC}] Complete."