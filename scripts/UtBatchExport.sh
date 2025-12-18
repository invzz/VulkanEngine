#!/bin/bash

# -----------------------------------------------------------------------------
# Script: UtExportBatchByType.sh
# Description: Batch export all assets from Unreal Tournament 2004 packages,
#              organizing exports by file type. Handles spaces in paths.
#
# Usage:
#   ./UtExportBatchByType.sh [MeshFormat] [TextureFormat]
#
# Arguments:
#   $1 : Mesh export format (gltf, psk, etc.) [default: gltf]
#   $2 : Texture export format (png, tga, dds) [default: png]
# -----------------------------------------------------------------------------

# ------------------------------
# CONFIGURATION
# ------------------------------
UT_PATH="blenderUnreal Tournament 2004"
EXPORT_PATH="/home/ac/UT2004_Exports"
UMODEL_PATH="$UT_PATH/umodel"

MESH_FORMAT="${1:-gltf}"
TEXTURE_FORMAT="${2:-png}"

PACKAGE_EXTENSIONS=("u" "uax" "ucl" "ukx" "upl" "usx" "ut2" "utx" "uvx")

echo "========================================"
echo "Batch exporting UT2004 assets by type..."
echo "Mesh format: $MESH_FORMAT"
echo "Texture format: $TEXTURE_FORMAT"
echo "Export path: $EXPORT_PATH"
echo "========================================"

# ------------------------------
# ENSURE EXPORT DIRECTORY EXISTS
# ------------------------------
mkdir -p "$EXPORT_PATH"

# ------------------------------
# EXPORT LOOP
# ------------------------------
for ext in "${PACKAGE_EXTENSIONS[@]}"; do
    echo ""
    echo "Processing *.$ext packages..."

    find "$UT_PATH" -type f -iname "*.$ext" | while IFS= read -r pkg; do
        # Relative path from UT_PATH
        REL_PATH="${pkg#$UT_PATH/}"

        # Output directory: preserve folder hierarchy, grouped by extension
        OUTPUT_DIR="$EXPORT_PATH/$ext/$(dirname "$REL_PATH")"
        mkdir -p "$OUTPUT_DIR"

        echo "Exporting package: $REL_PATH"
        "$UMODEL_PATH" \
            -game=ut2 \
            -path="$UT_PATH" \
            -export \
            -"$MESH_FORMAT" \
            -"$TEXTURE_FORMAT" \
            -out="$OUTPUT_DIR" \
            "$pkg"
    done
done

# ------------------------------
# FINISHED
# ------------------------------
echo ""
echo "Batch export complete!"
echo "Check exported assets in: $EXPORT_PATH"
