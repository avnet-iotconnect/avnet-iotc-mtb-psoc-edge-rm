#!/usr/bin/env bash
# convert-certs.sh
# run inside Git-Bash assuming you have only one downloaded device certificate (*-certificates.zip) in your Downloads directory
# otherwise set ZIP_DIR or OUT_DIR accordingly

###############################
#  CONFIGURATION              #
###############################
ZIP_DIR=${ZIP_DIR:-~/Downloads}
OUT_DIR=${OUT_DIR:-$(dirname -- "$0")/../proj_cm33_ns/certs}   # note: dirname first, then
# ##############################

set -euo pipefail

# Helper: turn a PEM file into the requested C-string literal format
pem_to_c_literal() {
  local in_pem="$1"
  local var_name="$2"
  {
    printf '#define %s \\\n' "$var_name"
    awk '{if(NR>1)print "\\n\"\\"; printf("\"%s",$0)} END{print "\""}' "$in_pem"
  }
}

# 1. locate the zip
zip_file=$(ls -1 "$ZIP_DIR"/*-certificates.zip 2>/dev/null | head -n1)
if [[ -z "$zip_file" ]]; then
    printf 'ERROR: no *-certificates.zip found in %s\n' "$ZIP_DIR" >&2
    exit 1
fi

# 2. make sure output directory exists
mkdir -p "$OUT_DIR"

# 3. unzip only the files we care about into a temp folder
tmp=$(mktemp -d)
trap "rm -rf '$tmp'" EXIT

crt_file=$(zipinfo -1 "$zip_file" | grep -E '^[^/]+\.crt$' | head -n1)
pem_file=$(zipinfo -1 "$zip_file" | grep -E '^[^/]+\.pem$' | head -n1)
[[ -z "$crt_file" || -z "$pem_file" ]] && { echo "ERROR: zip missing .crt or .pem at root"; exit 1; }

unzip -j -o "$zip_file" "$crt_file" "$pem_file" -d "$tmp"

# 4. build the C literals
crt_c_file="$OUT_DIR/$(basename "$crt_file").c"
pem_c_file="$OUT_DIR/$(basename "$pem_file").c"

pem_to_c_literal "$tmp/$crt_file" "IOTCONNECT_DEVICE_CERT"  > "$crt_c_file"
pem_to_c_literal "$tmp/$pem_file" "IOTCONNECT_DEVICE_KEY"   > "$pem_c_file"

echo "Done.  Output files:"
echo "  $crt_c_file"
echo "  $pem_c_file"
