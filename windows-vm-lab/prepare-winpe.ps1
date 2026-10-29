# Create a test-only Windows PE ISO from official Microsoft Windows ADK.
# Intended for an ephemeral GitHub Actions Windows runner.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$work = Join-Path $env:RUNNER_TEMP 'WinPE_amd64'
$iso = Join-Path $env:RUNNER_TEMP 'winpe-qemu-test.iso'
if (Test-Path $work) { Remove-Item $work -Force -Recurse }
$adk = Join-Path $env:RUNNER_TEMP 'adksetup.exe'
$pe = Join-Path $env:RUNNER_TEMP 'adkwinpesetup.exe'
# Current supported September 2026 ADK for Windows 11 26H2,25H2,24H2.
$adkUrl = 'https://go.microsoft.com/fwlink/?linkid=2289980'
$peUrl = 'https://go.microsoft.com/fwlink/?linkid=2289981'
Write-Host "Download official Microsoft ADK installers"
Invoke-WebRequest -Uri $adkUrl -OutFile $adk -MaximumRedirection 10
Invoke-WebRequest -Uri $peUrl -OutFile $pe -MaximumRedirection 10
foreach ($installer in @(@($adk,'OptionId.DeploymentTools'),@($pe,'OptionId.WindowsPreinstallationEnvironment'))) {
    Write-Host "Install $($installer[0]): $($installer[1])"
    $args = "/quiet /norestart /ceip off /features $($installer[1])"
    $proc = Start-Process -FilePath $installer[0] -ArgumentList $args -Wait -PassThru
    if ($proc.ExitCode -notin @(0,3010)) { throw "Installer failed: $($installer[0]) exit $($proc.ExitCode)" }
}
$base = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Windows Kits\10\Assessment and Deployment Kit'
$envbat = Join-Path $base 'Deployment Tools\DandISetEnv.bat'
$copype = Join-Path $base 'Windows Preinstallation Environment\copype.cmd'
$media = Join-Path $base 'Windows Preinstallation Environment\MakeWinPEMedia.cmd'
foreach ($file in @($envbat,$copype,$media)) {
    if (-not (Test-Path $file)) { throw "Missing ADK file: $file" }
}
Write-Host 'Create official Windows PE image'
& cmd.exe /d /s /c ('call "' + $envbat + '" && call "' + $copype + '" amd64 "' + $work + '"')
if ($LASTEXITCODE -ne 0) { throw 'copype failed' }
$mount = Join-Path $work 'mount'
$image = Join-Path $work 'media\sources\boot.wim'
if (-not (Test-Path $image)) { throw "WinPE boot.wim absent: $image" }
Write-Host "Mount and customize WinPE boot.wim"
Mount-WindowsImage -ImagePath $image -Index 1 -Path $mount | Out-Host
$mounted = $true
try {
    $startup = Join-Path $mount 'Windows\System32\startnet.cmd'
    @'
@echo off
wpeinit
echo WinPE guest initialization started...
for %%D in (A B C D E F G H I J K L M N O P Q R S T U V W Y Z) do (
    if exist %%D:\WINPE_PROBE.TXT (
        echo WINPE_READY> %%D:\WINPE_READY.TXT
        ver >> %%D:\WINPE_READY.TXT
        echo ARCH=%PROCESSOR_ARCHITECTURE% >> %%D:\WINPE_READY.TXT
        echo Detected probe disk %%D:
    )
)
timeout /t 5 /nobreak >nul
wpeutil shutdown
'@ | Set-Content -LiteralPath $startup -Encoding Ascii
    Write-Host "Save Windows PE"
    Dismount-WindowsImage -Path $mount -Save | Out-Host
    $mounted = $false
} finally {
    if ($mounted) { Dismount-WindowsImage -Path $mount -Discard | Out-Host }
}
Write-Host "Create bootable ISO $iso"
& cmd.exe /d /s /c ('call "' + $envbat + '" && call "' + $media + '" /ISO "' + $work + '" "' + $iso + '"')
if ($LASTEXITCODE -ne 0) { throw 'MakeWinPEMedia failed' }
Get-Item $iso | Select-Object FullName,Length | Format-List
Get-FileHash $iso -Algorithm SHA256 | Format-List
