#!/usr/bin/env bash
# Run from a fresh output directory; CLIENT is a clean, user-owned 1.12.1 install.
# EXTRACTORS must contain binaries built from the same core as the server.
set -euo pipefail
: "${CLIENT:?Set CLIENT to the clean WoW install (containing Data)}"
: "${EXTRACTORS:?Set EXTRACTORS to the matching compiled extraction tools}"
threads=${THREADS:-8}
for directory in maps vmaps mmaps Buildings 5875; do
    if [ -e "$directory" ]; then
        echo "Use a fresh output directory: $directory already exists" >&2
        exit 1
    fi
done
"$EXTRACTORS/MapExtractor" -i "$CLIENT" -o "$PWD" -e 7 --silent
mkdir -p 5875
mv dbc 5875/dbc
"$EXTRACTORS/VMapExtractor" -d "$CLIENT/Data" --silent
"$EXTRACTORS/VMapAssembler" --silent
"$EXTRACTORS/MoveMapGenerator" --threads "$threads" --silent \
    --configInputPath "$EXTRACTORS/config.json" --offMeshInput "$EXTRACTORS/offmesh.txt"
