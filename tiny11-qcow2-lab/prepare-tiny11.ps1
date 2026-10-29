# Build Tiny11 on Windows hosted runner using Microsoft official evaluation ISO.
# Windows 11 Enterprise 26H2 en-US x64 SHA256 published by Microsoft, Sep 2026.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:RUNNER_TEMP 'tiny11-lab'
$report=Join-Path $env:GITHUB_WORKSPACE 'tiny11-qcow2-logs'
New-Item -ItemType Directory -Force -Path $root,$report | Out-Null
$iso=Join-Path $root 'official-win11.iso'
$uri='https://go.microsoft.com/fwlink/?LinkId=2382600&clcid=0x409&country=us&culture=en-us'
$expected='BC3F24086EBADC94489066B5AD78089E2CF5C3491E90E790BB81A2B199C10E38'
Get-PSDrive -PSProvider FileSystem | Select-Object Name,Used,Free | Format-Table
Write-Host "Downloading official Windows 11 Enterprise eval 26H2"
& curl.exe --fail --location --retry 3 --output $iso $uri
if($LASTEXITCODE -ne 0){throw 'Official ISO download failed'}
$hash=(Get-FileHash $iso -Algorithm SHA256).Hash
if($hash -ne $expected){throw "ISO SHA256 mismatch: $hash"}
"Official Microsoft ISO SHA256: $hash" | Set-Content (Join-Path $report 'official-sha256.txt')
Write-Host "ISO SHA256 confirmed: $hash"
Mount-DiskImage -ImagePath $iso | Out-Null
try {
 $letter=(Get-DiskImage -ImagePath $iso | Get-Volume | Where-Object DriveLetter | Select-Object -First 1 -ExpandProperty DriveLetter)
 if(-not $letter){throw 'ISO drive letter unavailable'}
 $drive=([string]$letter)+':'
 $wim=Join-Path $drive 'sources\install.wim'
 if(-not(Test-Path $wim)){throw 'Official ISO install.wim missing'}
 Get-WindowsImage -ImagePath $wim | Format-Table | Out-String | Set-Content (Join-Path $report 'official-wim-info.txt')
 $tinyDir=Join-Path $root 'tiny11builder'
 New-Item -ItemType Directory -Force -Path $tinyDir | Out-Null
 $raw='https://raw.githubusercontent.com/ntdevlabs/tiny11builder/tiny11WinUI/'
 $maker=Join-Path $tinyDir 'tiny11maker.ps1'
 Invoke-WebRequest -Uri ($raw+'tiny11maker.ps1') -OutFile $maker
 Invoke-WebRequest -Uri ($raw+'autounattend.xml') -OutFile (Join-Path $tinyDir 'autounattend.xml')
 $body=Get-Content $maker -Raw
 if(-not $body.Contains('Read-Host "Please enter the image index"')){throw 'Unknown Tiny11 script version'}
 $body=$body.Replace('Read-Host "Please enter the image index"','1')
 $body=$body.Replace('Read-Host "Press Enter to continue"','"noninteractive"')
 [System.IO.File]::WriteAllText($maker,$body,[System.Text.UTF8Encoding]::new($false))
 Write-Host "Run Tiny11 maker against Microsoft Enterprise eval (index 1)"
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $maker -ISO $letter 2>&1 |
   Tee-Object -FilePath (Join-Path $report 'tiny11-build.log') | Out-Host
 if($LASTEXITCODE -ne 0){throw "Tiny11 maker returned $LASTEXITCODE"}
 $tinyISO=Join-Path $tinyDir 'tiny11.iso'
 if(-not(Test-Path $tinyISO)){throw 'Tiny11 ISO missing after build'}
 if((Get-Item $tinyISO).Length -lt 800MB){throw 'Tiny11 ISO unexpectedly small'}
 Move-Item $tinyISO (Join-Path $root 'tiny11.iso') -Force
 (Get-FileHash (Join-Path $root 'tiny11.iso') -Algorithm SHA256).Hash |
   Set-Content (Join-Path $report 'tiny11-iso-sha256.txt')
 Write-Host 'Tiny11 ISO built; no proprietary ISO will be uploaded by CI'
}
finally { Dismount-DiskImage -ImagePath $iso -ErrorAction SilentlyContinue | Out-Null }
