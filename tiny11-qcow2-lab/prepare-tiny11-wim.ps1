# Split official ISO download from Tiny11 Builder and provide regular progress.
# Builds from original Microsoft evaluation media, not redistributed.
param([Parameter(Mandatory=$true)][ValidateSet('Download','Build')][string]$Stage)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:RUNNER_TEMP 'tiny11-lab'
$logs=Join-Path $env:GITHUB_WORKSPACE 'tiny11-qcow2-logs'
New-Item -ItemType Directory -Force -Path $root,$logs | Out-Null
$iso=Join-Path $root 'official-win11.iso'
$expected='BC3F24086EBADC94489066B5AD78089E2CF5C3491E90E790BB81A2B199C10E38'

if($Stage -eq 'Download') {
    $url='https://go.microsoft.com/fwlink/?LinkId=2382600&clcid=0x409&country=us&culture=en-us'
    Write-Host 'STAGE 1: Download Microsoft Windows 11 Enterprise evaluation 26H2 en-US x64'
    & curl.exe --fail --location --retry 2 --connect-timeout 20 --max-time 1200 --progress-bar --output $iso $url
    if($LASTEXITCODE -ne 0){throw "ISO download failed, curl exit $LASTEXITCODE"}
    $bytes=(Get-Item $iso).Length
    Write-Host "Official ISO bytes: $bytes"
    if($bytes -ne 8225329152){throw "Unexpected Microsoft ISO size: $bytes"}
    $hash=(Get-FileHash $iso -Algorithm SHA256).Hash
    Write-Host "Official ISO SHA256: $hash"
    if($hash -ne $expected){throw 'Official ISO SHA256 does not match published hash'}
    "Official Microsoft SHA256: $hash" | Set-Content (Join-Path $logs 'official-sha256.txt')
    Write-Host "STAGE 1 PASS: SHA256 checked, bytes=$bytes"
    exit 0
}

if(-not(Test-Path $iso)){throw "Stage 1 ISO missing: $iso"}
$hash=(Get-FileHash $iso -Algorithm SHA256).Hash
if($hash -ne $expected){throw "Original ISO changed, SHA256 mismatch"}
Write-Host 'STAGE 2: mount Microsoft ISO and invoke pinned upstream Tiny11 Builder'
Mount-DiskImage -ImagePath $iso -ErrorAction Stop | Out-Null
try {
    $letter=(Get-DiskImage -ImagePath $iso|Get-Volume|Where-Object DriveLetter|Select-Object -First 1 -ExpandProperty DriveLetter)
    if(-not $letter){throw 'Could not find mounted ISO drive letter'}
    $drive=[string]$letter+':'
    $wim=Join-Path $drive 'sources\install.wim'
    if(-not(Test-Path $wim)){throw 'Official ISO missing sources/install.wim'}
    $images=@(Get-WindowsImage -ImagePath $wim -ErrorAction Stop)
    $images|Select-Object ImageIndex,ImageName|Format-Table|Out-Host
    if(-not($images.ImageIndex -contains 1)){throw "WIM image 1 is unavailable"}
    $tinyDir=Join-Path $root 'tiny11builder'
    New-Item -ItemType Directory -Force -Path $tinyDir | Out-Null
    $upstreamCommit='00e7d8a151a39ccffccab4a267bb81fb3756a01d'
    $upstream='https://raw.githubusercontent.com/ntdevlabs/tiny11builder/'+$upstreamCommit+'/'
    $maker=Join-Path $tinyDir 'tiny11maker.ps1'
    Invoke-WebRequest ($upstream+'tiny11maker.ps1') -OutFile $maker -ErrorAction Stop
    Invoke-WebRequest ($upstream+'autounattend.xml') -OutFile (Join-Path $tinyDir 'autounattend.xml') -ErrorAction Stop
    $body=Get-Content $maker -Raw
    foreach($needle in @('Read-Host "Please enter the image index"','Read-Host "Press Enter to continue"')){
        if(-not $body.Contains($needle)){throw "Unexpected upstream script version; missing: $needle"}
    }
    $body=$body.Replace('Read-Host "Please enter the image index"','1')
    $body=$body.Replace('Read-Host "Please enter the drive letter for the Windows 11 image"','$ISO')
    $body=$body.Replace('Read-Host "Press Enter to continue"','"noninteractive"')
    # WIM-only path: we need a bootable Windows disk, not a second installer ISO.
    # Tiny11 upstream has completed all DISM editing before "Creating ISO image...".
    if(-not $body.Contains('Write-Output "Creating ISO image..."')){throw 'Upstream ISO generation boundary not found'}
    $body=$body.Replace('Write-Output "Creating ISO image..."', @'
Copy-Item -LiteralPath "$ScratchDisk\tiny11\sources\install.wim" -Destination "$PSScriptRoot\tiny11-install.wim" -Force
Write-Output "Tiny11 WIM-only build completed (no installer ISO required)."
exit 0
'@)

    # Replace an infinite retry loop with a deterministic one-time check.
    $body=$body.Replace('while ($ImagesIndex -notcontains $index)', 'if ($ImagesIndex -notcontains $index)')
    $stray=[regex]::Matches($body,'Read-Host')
    if($stray.Count -gt 1){throw "Unexpected interactive Read-Host sites remain: $($stray.Count)"}
    [System.IO.File]::WriteAllText($maker,$body,[System.Text.UTF8Encoding]::new($false))
    $out=Join-Path $logs 'maker-stdout.txt'
    $err=Join-Path $logs 'maker-stderr.txt'
    Write-Host "Tiny11 pinned upstream=$upstreamCommit, scratch=D:\, source=$letter, index=1"
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$maker,'-ISO',([string]$letter),'-SCRATCH','D')
    $p=Start-Process -FilePath 'powershell.exe' -ArgumentList $args -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    $t0=Get-Date
    $lastCount=0
    while($true){
        Start-Sleep -Seconds 30
        $p.Refresh()
        $elapsed=[int]((Get-Date)-$t0).TotalMinutes
        $last=@()
        if(Test-Path $out){$last=@(Get-Content $out -Tail 14 -ErrorAction SilentlyContinue)}
        $size=(Get-Item $out -ErrorAction SilentlyContinue).Length
        Write-Host ("TINY11 HEARTBEAT elapsed={0}m stdout_bytes={1} running={2}" -f $elapsed,$size,(-not $p.HasExited))
        $last|ForEach-Object {Write-Host $_}
        if($p.HasExited){break}
        if($elapsed -ge 54){
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
            throw 'Tiny11 WIM Builder exceeded bounded 54-minute build window'
        }
    }
    Write-Host "Tiny11 Builder exit code: $($p.ExitCode)"
    if(Test-Path $err){Get-Content $err -Tail 40 | Out-Host}
    if($p.ExitCode -ne 0){throw "Tiny11 Builder failed with exit code $($p.ExitCode)"}
    $tinyWIM=Join-Path $tinyDir 'tiny11-install.wim'
    if(-not(Test-Path $tinyWIM)){throw 'Tiny11 Builder completed without modified WIM'}
    if((Get-Item $tinyWIM).Length -lt 800MB){throw 'Tiny11 WIM suspiciously small'}
    Move-Item $tinyWIM (Join-Path $root 'tiny11-install.wim') -Force
    $h=(Get-FileHash (Join-Path $root 'tiny11-install.wim') -Algorithm SHA256).Hash
    "Tiny11 modified WIM SHA256: $h" | Set-Content (Join-Path $logs 'tiny11-wim-sha256.txt')
    Write-Host "STAGE 2 PASS: Tiny11 WIM-only bytes=$((Get-Item (Join-Path $root 'tiny11-install.wim')).Length), SHA256=$h"
} finally {
    Dismount-DiskImage -ImagePath $iso -ErrorAction SilentlyContinue | Out-Null
}
