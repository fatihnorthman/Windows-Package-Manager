# MyWinApps Uninstall Script
#
# Reverse of Setup.ps1. Removes the installed exe, shortcuts, and
# (optionally) the per-user cache directory. Does NOT remove
# winget / scoop / chocolatey themselves — the user can keep
# those for other applications.

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\Program Files\MyWinApps"
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# --- Admin required so we can remove files under Program Files ---
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -InstallDir `"$InstallDir`"" -Verb RunAs
    exit
}

# --- Confirm with the user ---
$result = [System.Windows.Forms.MessageBox]::Show(
    "This will uninstall MyWinApps from:`n$InstallDir`n`nShortcuts and cached data will also be removed.`n`nThe package managers (winget, scoop, chocolatey) will NOT be removed.`n`nContinue?",
    "MyWinApps - Uninstall",
    [System.Windows.Forms.MessageBoxButtons]::YesNo,
    [System.Windows.Forms.MessageBoxIcon]::Warning)

if ($result -ne "Yes") { exit }

$log = @()
function Log($msg) { Write-Host $msg; $script:log += $msg }

Log "Uninstalling MyWinApps from $InstallDir..."

# --- 1. Remove the binary and everything in the install dir ---
if (Test-Path $InstallDir) {
    try {
        Remove-Item -Path $InstallDir -Recurse -Force -ErrorAction Stop
        Log "[OK] Removed $InstallDir"
    } catch {
        Log "[FAIL] Could not fully remove $InstallDir"
        Log "  $($_.Exception.Message)"
    }
} else {
    Log "[skip] $InstallDir not found"
}

# --- 2. Remove shortcuts ---
$WshShell = New-Object -ComObject WScript.Shell
$desktopShortcut = Join-Path [Environment]::GetFolderPath("Desktop") "MyWinApps.lnk"
if (Test-Path $desktopShortcut) {
    try { Remove-Item -Path $desktopShortcut -Force; Log "[OK] Removed desktop shortcut" }
    catch { Log "[FAIL] $($_.Exception.Message)" }
}
$startMenuShortcut = Join-Path (Join-Path [Environment]::GetFolderPath("Programs") "MyWinApps") "MyWinApps.lnk"
if (Test-Path $startMenuShortcut) {
    try { Remove-Item -Path $startMenuShortcut -Force; Log "[OK] Removed Start Menu shortcut" }
    catch { Log "[FAIL] $($_.Exception.Message)" }
}
# Also remove the Start Menu folder if it's empty
$startMenuDir = Join-Path [Environment]::GetFolderPath("Programs") "MyWinApps"
if ((Test-Path $startMenuDir) -and -not (Get-ChildItem -Path $startMenuDir -Force | Select-Object -First 1)) {
    try { Remove-Item -Path $startMenuDir -Force; Log "[OK] Removed empty Start Menu folder" }
    catch { }
}

# --- 3. Remove crash logs / dumps in install dir (best effort) ---
Get-ChildItem -Path $env:LOCALAPPDATA -Filter "PackageManager*.log" -ErrorAction SilentlyContinue | ForEach-Object {
    try { Remove-Item -Path $_.FullName -Force; Log "[OK] Removed $($_.Name)" } catch {}
}
Get-ChildItem -Path $env:LOCALAPPDATA -Filter "*.dmp" -ErrorAction SilentlyContinue | ForEach-Object {
    try { Remove-Item -Path $_.FullName -Force; Log "[OK] Removed $($_.Name)" } catch {}
}

Log ""
Log "Uninstall complete."

# --- Show summary ---
$summary = ($log -join "`n")
[System.Windows.Forms.MessageBox]::Show(
    "Uninstall finished.`n`nDetails:`n$summary",
    "MyWinApps - Uninstall",
    [System.Windows.Forms.MessageBoxButtons]::OK,
    [System.Windows.Forms.MessageBoxIcon]::Information)
