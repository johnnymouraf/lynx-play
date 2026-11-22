# Downloads the pinned NVIDIA NGX SDK pieces the build needs: the DLSS Super
# Resolution and DLSS Frame Generation headers, the loader import library for
# /MD and the two runtime DLLs that ship next to the executable. Nothing here
# is committed.
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$root = Split-Path -Parent $PSScriptRoot
$tag = 'v310.9.16'
$base = "https://raw.githubusercontent.com/NVIDIA/DLSS/$tag"
$target = Join-Path $root '.ngx'

$headers = @(
    'nvsdk_ngx.h',
    'nvsdk_ngx_defs.h',
    'nvsdk_ngx_params.h',
    'nvsdk_ngx_helpers.h',
    'nvsdk_ngx_helpers_d3d.h',
    'nvsdk_ngx_helpers_cuda.h',
    'nvsdk_ngx_loader.h',
    'nvsdk_ngx_defs_dlssg.h',
    'nvsdk_ngx_params_dlssg.h',
    'nvsdk_ngx_helpers_dlssg_d3d.h'
)

New-Item -ItemType Directory -Force (Join-Path $target 'include') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $target 'lib') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $target 'bin') | Out-Null

foreach ($header in $headers) {
    Invoke-WebRequest "$base/include/$header" -OutFile (Join-Path $target "include/$header")
}

# nvsdk_ngx_d.lib is the /MD variant; the _s one is for /MT.
Invoke-WebRequest "$base/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib" `
    -OutFile (Join-Path $target 'lib/nvsdk_ngx_d.lib')

# The release DLLs keep NVIDIA's Authenticode signature; the dev builds carry a
# debug overlay and must not be distributed.
$dll = Join-Path $target 'bin/nvngx_dlss.dll'
Invoke-WebRequest "$base/lib/Windows_x86_64/rel/nvngx_dlss.dll" -OutFile $dll

$expectedSize = 58956912
$size = (Get-Item $dll).Length

if ($size -ne $expectedSize) {
    throw "nvngx_dlss.dll is $size bytes, expected $expectedSize"
}

$dlssg = Join-Path $target 'bin/nvngx_dlssg.dll'
Invoke-WebRequest "$base/lib/Windows_x86_64/rel/nvngx_dlssg.dll" -OutFile $dlssg

if (-not (Test-Path $dlssg) -or (Get-Item $dlssg).Length -eq 0) {
    throw "nvngx_dlssg.dll wasn't downloaded"
}

Invoke-WebRequest "$base/LICENSE.txt" -OutFile (Join-Path $target 'LICENSE.txt')

Write-Output "NGX $tag is in $target"
