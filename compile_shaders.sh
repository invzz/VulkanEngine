
#!/bin/bash
# Author: Andres Coronado
# Date: 30/10/2025
#
# This script compiles GLSL shaders to SPIR-V binary format using glslc.
# Usage:
#   bash compile_shaders.sh [-d input_dir] [-o output_dir]
# If no options are provided, defaults are used.
# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
VIOLET='\033[0;35m'
NC='\033[0m' # No Color



# Default folders
SHADER_DIR="assets/shaders"
OUTPUT_DIR="assets/shaders/compiled"

# Parse options
while getopts ":o:d:" opt; do
  case $opt in
    o)
      OUTPUT_DIR="$OPTARG"
      ;;
    d)
      SHADER_DIR="$OPTARG"
      ;;
    *)
      echo -e "${RED}Usage: $0 [-o output_dir] [-d input_dir]${NC}"
      exit 1
      ;;
  esac
done

mkdir -p "$OUTPUT_DIR"

for shader in "$SHADER_DIR"/*.vert; do
    filename=$(basename -- "$shader")
    output_file="$OUTPUT_DIR/${filename%.vert}.vert.spv"
    if glslc "$shader" -o "$output_file"; then
        echo -e "[ ${GREEN}Compiled${NC} ] $shader -> ${VIOLET}$output_file${NC}"
    else
        echo -e "[ ${RED}Failed to compile ${NC}]$shader${NC}"
    fi
done

for shader in "$SHADER_DIR"/*.frag; do
    filename=$(basename -- "$shader")
    output_file="$OUTPUT_DIR/${filename%.frag}.frag.spv"
    if glslc "$shader" -o "$output_file"; then
        echo -e "[ ${GREEN}Compiled${NC} ] $shader -> ${VIOLET}$output_file${NC}"
    else
        echo -e "[ ${RED}Failed to compile ${NC}]$shader${NC}"
    fi
done

for shader in "$SHADER_DIR"/*.comp; do
    filename=$(basename -- "$shader")
    output_file="$OUTPUT_DIR/${filename%.comp}.comp.spv"
    if glslc "$shader" -o "$output_file"; then
        echo -e "[ ${GREEN}Compiled${NC} ] $shader -> ${VIOLET}$output_file${NC}"
    else
        echo -e "[ ${RED}Failed to compile ${NC}]$shader${NC}"
    fi
done

for shader in "$SHADER_DIR"/*.task; do
    filename=$(basename -- "$shader")
    output_file="$OUTPUT_DIR/${filename%.task}.task.spv"
    if glslc "$shader" --target-env=vulkan1.3 -o "$output_file"; then
        echo -e "[ ${GREEN}Compiled${NC} ] $shader -> ${VIOLET}$output_file${NC}"
    else
        echo -e "[ ${RED}Failed to compile ${NC}]$shader${NC}"
    fi
done

for shader in "$SHADER_DIR"/*.mesh; do
    filename=$(basename -- "$shader")
    output_file="$OUTPUT_DIR/${filename%.mesh}.mesh.spv"
    if glslc "$shader" --target-env=vulkan1.3 -o "$output_file"; then
        echo -e "[ ${GREEN}Compiled${NC} ] $shader -> ${VIOLET}$output_file${NC}"
    else
        echo -e "[ ${RED}Failed to compile ${NC}]$shader${NC}"
    fi
done

for shader in "$SHADER_DIR"/*.vert; do
    filename=$(basename -- "$shader")
    output_file="$OUTPUT_DIR/${filename%.vert}.vert.spv"
    if glslc "$shader" -o "$output_file"; then
        echo -e "[ ${GREEN}Compiled${NC} ] $shader -> ${VIOLET}$output_file${NC}"
    else
        echo -e "[ ${RED}Failed to compile ${NC}]$shader${NC}"
    fi
done