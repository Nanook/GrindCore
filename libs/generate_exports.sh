#!/bin/sh
# Usage: generate_exports.sh <mode> <input> <output>
#   mode: "unix" or "osx"
#   input: entrypoints.c
#   output: export file to generate

MODE="$1"
INFILE="$2"
OUTFILE="$3"

echo "MODE=$MODE INFILE=$INFILE OUTFILE=$OUTFILE" >&2
echo "DIRNAME=$(dirname "$OUTFILE")" >&2

mkdir -p "$(dirname "$OUTFILE")"

if [ "$MODE" = "unix" ]; then
    echo "{" > "$OUTFILE"
    echo "  global:" >> "$OUTFILE"
    awk '/DllImportEntry\(/ {
        line = $0
        sub(/^.*DllImportEntry\(/, "", line)
        sub(/\).*$/, "", line)
        print "    " line ";"
    }' "$INFILE" >> "$OUTFILE" 
    echo "  local:" >> "$OUTFILE"
    echo "    *;" >> "$OUTFILE"
    echo "};" >> "$OUTFILE"
elif [ "$MODE" = "osx" ]; then
    awk '/DllImportEntry\(/ {
        line = $0
        sub(/^.*DllImportEntry\(/, "", line)
        sub(/\).*$/, "", line)
        print "_" line
    }' "$INFILE" > "$OUTFILE"
else
    echo "Unknown mode: $MODE"
    exit 1
fi