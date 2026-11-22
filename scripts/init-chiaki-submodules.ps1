# Fetches the pinned chiaki-ng submodule and only the nested submodules its
# protocol library needs (chiaki-ng also carries Android, Switch and Steam Deck
# code that Lynx Play never builds).
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$root = Split-Path -Parent $PSScriptRoot
$chiaki = Join-Path $root 'third_party/chiaki-ng'
$librarySubmodules = @(
    'third-party/nanopb',
    'third-party/jerasure',
    'third-party/gf-complete',
    'third-party/curl'
)

git -C $root submodule update --init --depth 1 third_party/chiaki-ng
git -C $chiaki submodule update --init --depth 1 @librarySubmodules
