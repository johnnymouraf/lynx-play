# Fetches the pinned NVIDIA NVAPI headers. The renderer only includes them;
# the driver's nvapi64.dll is loaded at run time, so no import library is used.
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$root = Split-Path -Parent $PSScriptRoot
git -C $root submodule update --init --depth 1 third_party/nvapi
