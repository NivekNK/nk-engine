param(
    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$BuildType = "Debug"
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = if ($env:NK_BUILD_DIR) {
    $env:NK_BUILD_DIR
} else {
    Join-Path $projectRoot "out/build/Win32-$BuildType"
}

if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    cmake --preset "Win32-$BuildType"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed."
    }
}

cmake --build $buildDir --target verify_shader_assets
if ($LASTEXITCODE -ne 0) {
    throw "Slang shader compilation failed."
}
