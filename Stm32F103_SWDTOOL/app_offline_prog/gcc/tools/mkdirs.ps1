# Create per-variant object directories for the GCC build.
$root = Split-Path -Parent $PSScriptRoot
$variants = @("boot", "app-cdc", "app-msc", "app-cdc-256k", "app-msc-256k")
foreach ($v in $variants) {
    $p = Join-Path $root ("build\" + $v + "\obj")
    New-Item -ItemType Directory -Force -Path $p | Out-Null
}
