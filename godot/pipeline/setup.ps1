# Fireteam: Skirmish — Blender setup for the asset pipeline (optional)
# Finds or installs Blender 4.2 LTS to a user-owned folder, verifies it runs
# headless and exports GLB, and records the path in pipeline/config.local.json
# (gitignored — machine-specific). Works in a fresh terminal; no admin required.
# Only needed to REGENERATE assets; the committed GLBs already work in Godot.
#
#   powershell -ExecutionPolicy Bypass -File pipeline\setup.ps1
#
# Override the install location with $env:FIRETEAM_BLENDER (full path to
# blender.exe) if Blender 4.2 is already installed somewhere else.

$ErrorActionPreference = "Stop"
$PermRoot = Join-Path $env:USERPROFILE "Tools\Blender"
$PermExe  = Join-Path $PermRoot "blender-4.2.23-windows-x64\blender.exe"
if ($env:FIRETEAM_BLENDER) { $PermExe = $env:FIRETEAM_BLENDER }
$PipeDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$Config   = Join-Path $PipeDir "config.local.json"
$Sha      = "82e791475779a7342424a480bdde9a20b43710da9264c60346125aa16cd910cb"

function Write-Config($blender) {
  $godot = ""
  if ($env:FIRETEAM_GODOT) { $godot = $env:FIRETEAM_GODOT }
  elseif (Test-Path $Config) { try { $godot = (Get-Content $Config -Raw | ConvertFrom-Json).godot } catch {} }
  @{ blender = $blender; blender_version = "4.2.23 LTS"; godot = $godot;
     notes = "Local machine paths (gitignored). Set 'godot' to your Godot 4.7 console exe for the QA scripts." } |
    ConvertTo-Json | Set-Content -Encoding utf8 $Config
  Write-Host "Wrote $Config"
}

# 1. Already installed permanently?
if (Test-Path $PermExe) {
  Write-Host "Blender already at: $PermExe"
} else {
  New-Item -ItemType Directory -Force $PermRoot | Out-Null
  # 2. Reuse an already-extracted copy or a verified zip if present
  $foundExe = Get-ChildItem -Recurse -Filter "blender.exe" "$env:TEMP","$env:USERPROFILE\Downloads" -ErrorAction SilentlyContinue |
              Where-Object { $_.FullName -match "4\.2\.23" } | Select-Object -First 1
  $foundZip = Get-ChildItem -Recurse -Filter "*.zip" "$env:TEMP","$env:USERPROFILE\Downloads" -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -match "blender" } | Select-Object -First 1
  if ($foundExe) {
    $src = Split-Path -Parent $foundExe.FullName
    Write-Host "Copying existing Blender from $src ..."
    robocopy $src (Join-Path $PermRoot "blender-4.2.23-windows-x64") /E /NFL /NDL /NJH /NJS /NP /MT:8 | Out-Null
  } elseif ($foundZip) {
    Write-Host "Extracting $($foundZip.FullName) ..."
    Expand-Archive -Path $foundZip.FullName -DestinationPath $PermRoot -Force
  } else {
    # 3. Download official portable build and verify sha256
    $zip = Join-Path $env:TEMP "blender-4.2.23.zip"
    Write-Host "Downloading official Blender 4.2.23 (~365 MB)..."
    Invoke-WebRequest -Uri "https://download.blender.org/release/Blender4.2/blender-4.2.23-windows-x64.zip" -OutFile $zip
    $h = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
    if ($h -ne $Sha) { throw "SHA256 mismatch: got $h" }
    Write-Host "Checksum OK. Extracting..."
    Expand-Archive -Path $zip -DestinationPath $PermRoot -Force
  }
}

if (-not (Test-Path $PermExe)) { throw "Blender not found at $PermExe after setup." }

# 4. Verify headless run + GLB export
Write-Host "`nVerifying Blender..."
$verify = Join-Path $PipeDir "verify_blender.py"
$out = Join-Path $env:TEMP "fireteam_bltest"
& $PermExe --background --factory-startup --python $verify -- $out 2>&1 |
  Select-String -Pattern "BLENDER_VERSION|BPY_IMPORT|PY_EXEC|GLB_EXPORT|OBJ_EXPORT|SELFTEST_DONE"
if ($LASTEXITCODE -ne 0) { throw "Blender self-test failed (exit $LASTEXITCODE)" }

Write-Config $PermExe
Write-Host "`nSetup complete. Blender: $PermExe"
