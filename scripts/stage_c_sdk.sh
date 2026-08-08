#!/usr/bin/env bash
# Stage a Linux CometTextel C SDK directory (headers + libs + example source).
set -euo pipefail

OutDir=""
BuildDir="build"
RepoRoot=""

usage() {
  echo "Usage: $0 --out <dir> [--build-dir build] [--repo-root <path>]" >&2
  exit 2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OutDir="${2:-}"; shift 2 ;;
    --build-dir) BuildDir="${2:-}"; shift 2 ;;
    --repo-root) RepoRoot="${2:-}"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "Unknown arg: $1" >&2; usage ;;
  esac
done

[[ -n "$OutDir" ]] || usage

if [[ -z "$RepoRoot" ]]; then
  RepoRoot="$(cd "$(dirname "$0")/.." && pwd)"
fi

header="$RepoRoot/include/comettextel/c_api.h"
example="$RepoRoot/examples/c_api_example.c"
sdk_readme="$RepoRoot/sdk/c/README.md"

for path in "$header" "$example" "$sdk_readme"; do
  [[ -f "$path" ]] || { echo "Required path missing: $path" >&2; exit 1; }
done

mkdir -p "$OutDir/lib" "$OutDir/include/comettextel" "$OutDir/examples"

mapfile -t libs < <(find "$BuildDir" -maxdepth 3 -type f \( -name 'libcomettextel.so*' -o -name 'libcomettextel.a' \) | sort)
if [[ ${#libs[@]} -eq 0 ]]; then
  echo "No libcomettextel shared/static libraries under $BuildDir" >&2
  exit 1
fi

cp -a "${libs[@]}" "$OutDir/lib/"
cp -a "$header" "$OutDir/include/comettextel/c_api.h"
cp -a "$example" "$OutDir/examples/c_api_example.c"
cp -a "$sdk_readme" "$OutDir/README.md"
[[ -f "$RepoRoot/LICENSE" ]] && cp -a "$RepoRoot/LICENSE" "$OutDir/LICENSE"

echo "Staged C SDK -> $OutDir"
find "$OutDir" -type f | sed "s|^$OutDir/||"
