# MyWinApps Setup Script
# Implements interactive Fluent UI installer for MyWinApps

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# 1. Require Administrator Elevation
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    # Re-run as Administrator
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

# 2. Form Setup
$form = New-Object System.Windows.Forms.Form
$form.Text = "MyWinApps - Installer"
$form.Size = New-Object System.Drawing.Size(560, 480)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.BackColor = System.Drawing.Color::FromArgb(32, 20, 31, 39) # Fluent-style dark background

# Apply Font
$fontTitle = New-Object System.Drawing.Font("Segoe UI", 16, [System.Drawing.FontStyle]::Bold)
$fontLabel = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Regular)
$fontBtn = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)

# Title Label
$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = "MyWinApps Installer"
$lblTitle.Location = New-Object System.Drawing.Point(30, 25)
$lblTitle.Size = New-Object System.Drawing.Size(500, 35)
$lblTitle.Font = $fontTitle
$lblTitle.ForeColor = System.Drawing.Color::White
$form.Controls.Add($lblTitle)

# Description Label
$lblDesc = New-Object System.Windows.Forms.Label
$lblDesc.Text = "Choose where to install MyWinApps and select which package managers to set up."
$lblDesc.Location = New-Object System.Drawing.Point(30, 65)
$lblDesc.Size = New-Object System.Drawing.Size(500, 20)
$lblDesc.Font = $fontLabel
$lblDesc.ForeColor = System.Drawing.Color::FromArgb(255, 160, 174, 186)
$form.Controls.Add($lblDesc)

# --- Path Selection ---
$lblPath = New-Object System.Windows.Forms.Label
$lblPath.Text = "Install Location:"
$lblPath.Location = New-Object System.Drawing.Point(30, 105)
$lblPath.Size = New-Object System.Drawing.Size(500, 20)
$lblPath.Font = $fontLabel
$lblPath.ForeColor = System.Drawing.Color::White
$form.Controls.Add($lblPath)

$txtPath = New-Object System.Windows.Forms.TextBox
$txtPath.Text = Join-Path $env:ProgramFiles "MyWinApps"
$txtPath.Location = New-Object System.Drawing.Point(30, 128)
$txtPath.Size = New-Object System.Drawing.Size(390, 25)
$txtPath.BackColor = System.Drawing.Color::FromArgb(255, 32, 31, 31)
$txtPath.ForeColor = System.Drawing.Color::White
$txtPath.BorderStyle = "FixedSingle"
$form.Controls.Add($txtPath)

$btnBrowse = New-Object System.Windows.Forms.Button
$btnBrowse.Text = "Browse..."
$btnBrowse.Location = New-Object System.Drawing.Point(430, 127)
$btnBrowse.Size = New-Object System.Drawing.Size(90, 25)
$btnBrowse.FlatStyle = "Flat"
$btnBrowse.BackColor = System.Drawing.Color::FromArgb(255, 47, 47, 46)
$btnBrowse.ForeColor = System.Drawing.Color::White
$btnBrowse.FlatAppearance.BorderSize = 0
$btnBrowse.Click += {
    $browser = New-Object System.Windows.Forms.FolderBrowserDialog
    $browser.SelectedPath = $txtPath.Text
    if ($browser.ShowDialog() -eq "OK") {
        $txtPath.Text = Join-Path $browser.SelectedPath "MyWinApps"
    }
}
$form.Controls.Add($btnBrowse)

# --- Checkboxes ---
$grpOptions = New-Object System.Windows.Forms.GroupBox
$grpOptions.Text = "Installation Options"
$grpOptions.Location = New-Object System.Drawing.Point(30, 175)
$grpOptions.Size = New-Object System.Drawing.Size(490, 170)
$grpOptions.ForeColor = System.Drawing.Color::White
$grpOptions.Font = $fontLabel

# Checkbox: Desktop Shortcut
$chkDesktop = New-Object System.Windows.Forms.CheckBox
$chkDesktop.Text = "Create Desktop Shortcut"
$chkDesktop.Location = New-Object System.Drawing.Point(20, 30)
$chkDesktop.Size = New-Object System.Drawing.Size(450, 25)
$chkDesktop.Checked = $true
$grpOptions.Controls.Add($chkDesktop)

# Checkbox: Start Menu Shortcut
$chkStartMenu = New-Object System.Windows.Forms.CheckBox
$chkStartMenu.Text = "Create Start Menu Shortcut"
$chkStartMenu.Location = New-Object System.Drawing.Point(20, 55)
$chkStartMenu.Size = New-Object System.Drawing.Size(450, 25)
$chkStartMenu.Checked = $true
$grpOptions.Controls.Add($chkStartMenu)

# Checkbox: Install Winget
$chkWinget = New-Object System.Windows.Forms.CheckBox
$chkWinget.Text = "Install/Upgrade Winget package manager"
$chkWinget.Location = New-Object System.Drawing.Point(20, 80)
$chkWinget.Size = New-Object System.Drawing.Size(450, 25)
$chkWinget.Checked = $true
$grpOptions.Controls.Add($chkWinget)

# Checkbox: Install Scoop
$chkScoop = New-Object System.Windows.Forms.CheckBox
$chkScoop.Text = "Install Scoop package manager"
$chkScoop.Location = New-Object System.Drawing.Point(20, 105)
$chkScoop.Size = New-Object System.Drawing.Size(450, 25)
$grpOptions.Controls.Add($chkScoop)

# Checkbox: Install Chocolatey
$chkChoco = New-Object System.Windows.Forms.CheckBox
$chkChoco.Text = "Install Chocolatey package manager"
$chkChoco.Location = New-Object System.Drawing.Point(20, 130)
$chkChoco.Size = New-Object System.Drawing.Size(450, 25)
$grpOptions.Controls.Add($chkChoco)

$form.Controls.Add($grpOptions)

# --- Progress Bar & Status ---
$progressBar = New-Object System.Windows.Forms.ProgressBar
$progressBar.Location = New-Object System.Drawing.Point(30, 360)
$progressBar.Size = New-Object System.Drawing.Size(490, 15)
$progressBar.Visible = $false
$form.Controls.Add($progressBar)

$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.Text = ""
$lblStatus.Location = New-Object System.Drawing.Point(30, 380)
$lblStatus.Size = New-Object System.Drawing.Size(490, 20)
$lblStatus.Font = $fontLabel
$lblStatus.ForeColor = System.Drawing.Color::FromArgb(255, 0, 120, 212)
$lblStatus.Visible = $false
$form.Controls.Add($lblStatus)

# --- Install & Cancel Buttons ---
$btnCancel = New-Object System.Windows.Forms.Button
$btnCancel.Text = "Cancel"
$btnCancel.Location = New-Object System.Drawing.Point(430, 400)
$btnCancel.Size = New-Object System.Drawing.Size(90, 30)
$btnCancel.FlatStyle = "Flat"
$btnCancel.BackColor = System.Drawing.Color::FromArgb(255, 47, 47, 46)
$btnCancel.ForeColor = System.Drawing.Color::White
$btnCancel.Font = $fontBtn
$btnCancel.FlatAppearance.BorderSize = 0
$btnCancel.Click += { $form.Close() }
$form.Controls.Add($btnCancel)

$btnInstall = New-Object System.Windows.Forms.Button
$btnInstall.Text = "Install"
$btnInstall.Location = New-Object System.Drawing.Point(325, 400)
$btnInstall.Size = New-Object System.Drawing.Size(95, 30)
$btnInstall.FlatStyle = "Flat"
$btnInstall.BackColor = System.Drawing.Color::FromArgb(255, 0, 120, 212)
$btnInstall.ForeColor = System.Drawing.Color::White
$btnInstall.Font = $fontBtn
$btnInstall.FlatAppearance.BorderSize = 0
$btnInstall.Click += {
    # Disable controls during install
    $btnInstall.Enabled = $false
    $btnBrowse.Enabled = $false
    $txtPath.Enabled = $false
    $grpOptions.Enabled = $false
    $btnCancel.Enabled = $false
    $progressBar.Visible = $true
    $lblStatus.Visible = $true

    $scriptDir = Split-Path $MyInvocation.MyCommand.Path
    # Handle direct script invocation fallback
    if ($scriptDir -eq "") { $scriptDir = Get-Location }
    
    $exeSource = Join-Path $scriptDir "build\Release\MyWinApps.exe"
    if (-not (Test-Path $exeSource)) {
        # Fallback to local script folder
        $exeSource = Join-Path $scriptDir "MyWinApps.exe"
    }

    if (-not (Test-Path $exeSource)) {
        [System.Windows.Forms.MessageBox]::Show("Could not find 'MyWinApps.exe' in '$scriptDir' or builds. Please place 'MyWinApps.exe' in the same folder as this script.", "Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        $form.Close()
        return
    }

    $destFolder = $txtPath.Text

    # Start installation thread
    Start-Job -Name "InstallerTask" -ScriptBlock {
        param($exeSource, $destFolder, $chkDesktopChecked, $chkStartMenuChecked, $chkWingetChecked, $chkScoopChecked, $chkChocoChecked)
        
        $ErrorActionPreference = 'Stop'
        
        # 1. Create target folder
        New-Item -ItemType Directory -Force -Path $destFolder | Out-Null
        $exeDest = Join-Path $destFolder "MyWinApps.exe"
        
        # 2. Copy binary
        Copy-Item -Path $exeSource -Destination $exeDest -Force | Out-Null

        # 3. Create shortcuts
        $WshShell = New-Object -ComObject WScript.Shell
        if ($chkDesktopChecked) {
            $shortcut = $WshShell.CreateShortcut([Join-Path [Environment]::GetFolderPath("Desktop") "MyWinApps.lnk"])
            $shortcut.TargetPath = $exeDest
            $shortcut.WorkingDirectory = $destFolder
            $shortcut.Save()
        }
        if ($chkStartMenuChecked) {
            $startMenuDir = Join-Path [Environment]::GetFolderPath("Programs") "MyWinApps"
            New-Item -ItemType Directory -Force -Path $startMenuDir | Out-Null
            $shortcut = $WshShell.CreateShortcut([Join-Path $startMenuDir "MyWinApps.lnk"])
            $shortcut.TargetPath = $exeDest
            $shortcut.WorkingDirectory = $destFolder
            $shortcut.Save()
        }

        # Setup security protocol
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

        # 4. Optional Tool Installs
        if ($chkWingetChecked) {
            # Probe Winget availability
            $wingetOk = (Get-Command "winget" -ErrorAction SilentlyContinue) -ne $null
            if (-not $wingetOk) {
                # Install Winget
                $tempPath = Join-Path $env:TEMP "winget.msixbundle"
                Invoke-WebRequest -Uri "https://github.com/microsoft/winget-cli/releases/latest/download/Microsoft.DesktopAppInstaller_8wekyb3d8bbwe.msixbundle" -OutFile $tempPath
                Add-AppxPackage -Path $tempPath
            }
        }

        if ($chkScoopChecked) {
            $scoopOk = (Get-Command "scoop" -ErrorAction SilentlyContinue) -ne $null
            if (-not $scoopOk) {
                # Install Scoop
                Invoke-Expression (Invoke-RestMethod -Uri "https://get.scoop.sh")
            }
        }

        if ($chkChocoChecked) {
            $chocoOk = (Get-Command "choco" -ErrorAction SilentlyContinue) -ne $null
            if (-not $chocoOk) {
                # Clean up corrupt directory if any
                if (Test-Path 'C:\ProgramData\chocolatey') {
                    Remove-Item -Path 'C:\ProgramData\chocolatey' -Recurse -Force -ErrorAction SilentlyContinue
                }
                # Install Chocolatey
                Invoke-Expression (New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1')
            }
        }
    } -ArgumentList $exeSource, $destFolder, $chkDesktop.Checked, $chkStartMenu.Checked, $chkWinget.Checked, $chkScoop.Checked, $chkChoco.Checked | Out-Null

    # Progress Animation Timer
    $steps = 0
    $timer = New-Object System.Windows.Forms.Timer
    $timer.Interval = 200
    $timer.Add_Tick({
        $job = Get-Job -Name "InstallerTask"
        if ($job.State -eq "Running") {
            $steps = ($steps + 5) % 100
            $progressBar.Value = $steps
            $lblStatus.Text = "Copying files and configuring environment..."
        } else {
            $timer.Stop()
            $jobData = Receive-Job -Job $job
            Remove-Job -Name "InstallerTask" -Force
            
            $progressBar.Value = 100
            $lblStatus.Text = "Installation completed successfully!"
            
            $exeDest = Join-Path $destFolder "MyWinApps.exe"
            
            # Show completed dialog
            $launch = [System.Windows.Forms.MessageBox]::Show("MyWinApps has been installed successfully!`n`nWould you like to launch the application now?", "Installation Completed", [System.Windows.Forms.MessageBoxButtons]::YesNo, [System.Windows.Forms.MessageBoxIcon]::Information)
            if ($launch -eq "Yes") {
                Start-Process $exeDest
            }
            $form.Close()
        }
    })
    $timer.Start()
}
$form.Controls.Add($btnInstall)

# Render Form
$form.ShowDialog() | Out-Null
