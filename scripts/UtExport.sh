#!/bin/bash

# -----------------------------------------------------------------------------
# Script: UtExport.sh
# Description: Automates the export of assets from Unreal Tournament 2004 maps
#              using the UModel tool.
#
# Usage:
#   ./UtExport.sh <MapName> [MeshFormat] [TextureFormat]
#
# Arguments:
#   $1 : MapName      - Name of the map to export (e.g., DM-Rankin)
#   $2 : MeshFormat   - Mesh export format (gltf, psk, etc.) [default: gltf]
#   $3 : TextureFormat- Texture export format (png, tga, dds) [default: png]
# -----------------------------------------------------------------------------

# ------------------------------
# CONFIGURATION
# ------------------------------
UT_PATH="/home/ac/.local/share/Steam/steamapps/common/Unreal Tournament 2004"
EXPORT_PATH="/home/ac/UT2004_Exports"
UMODEL_PATH="$UT_PATH/umodel"

# ------------------------------
# INTERACTIVE MENU / ARGUMENTS
# ------------------------------
if [ -z "$1" ]; then
    echo "========================================"
    echo "      UT2004 Asset Export Tool"
    echo "========================================"

    # Map name
    read -p "Enter Map Name (e.g., DM-Rankin): " MAPNAME
    if [ -z "$MAPNAME" ]; then
        echo "Error: Map Name is required."
        exit 1
    fi

    # Mesh format
    echo ""
    echo "Select Mesh Export Format:"
    echo "1) gltf (Default)"
    echo "2) psk"
    read -p "Enter choice [1-2]: " MESH_CHOICE
    case "$MESH_CHOICE" in
        2) MESH_FORMAT="psk" ;;
        *) MESH_FORMAT="gltf" ;;
    esac

    # Texture format
    echo ""
    echo "Select Texture Format:"
    echo "1) PNG (Default)"
    echo "2) TGA"
    read -p "Enter choice [1-2]: " TEX_CHOICE
    case "$TEX_CHOICE" in
        2) TEXTURE_FORMAT="tga" ;;
        *) TEXTURE_FORMAT="png" ;;
    esac
else
    MAPNAME="$1"
    MESH_FORMAT="${2:-gltf}"
    TEXTURE_FORMAT="${3:-png}"
fi

echo ""
echo "Exporting map: $MAPNAME"
echo "Mesh format: $MESH_FORMAT"
echo "Texture format: $TEXTURE_FORMAT"
echo "Output path: $EXPORT_PATH"
echo "========================================"

# ------------------------------
# ENSURE EXPORT DIRECTORY EXISTS
# ------------------------------
mkdir -p "$EXPORT_PATH"

# ------------------------------
# EXECUTE UMODEL
# ------------------------------
"$UMODEL_PATH" \
  -game=ut2 \
  -path="$UT_PATH" \
  -export \
  -"$MESH_FORMAT" \
  -"$TEXTURE_FORMAT" \
  -out="$EXPORT_PATH" \
  "$UT_PATH/Maps/$MAPNAME.ut2"

# ------------------------------
# FINISHED
# ------------------------------
echo ""
echo "Export complete!"
echo "Check exported assets in: $EXPORT_PATH/$MAPNAME"