# Apply Tiny11 install.wim offline into a dynamic VHDX, then zstd-compress into qcow2.
# QEMU software TCG can boot the resulting disk after an OVMF firmware is supplied.
$ErrorActionPreference='Stop'
$root=Join-Path $env:RUNNER_TEMP 'tiny11-lab'
$report=Join-Path $env:GITHUB_WORKSPACE 'tiny11-qcow2-logs'
$image=Join-Path $root 'tiny11-install.wim'
$vhdx=Join-Path $root 'tiny11.vhdx'
$qcow=Join-Path $root 'tiny11-zstd.qcow2'
if(-not(Test-Path $image)){throw 'Tiny11 modified WIM was not generated'}
if(Get-Volume -DriveLetter W -ErrorAction SilentlyContinue){throw 'Drive W: occupied'}
if(Get-Volume -DriveLetter Y -ErrorAction SilentlyContinue){throw 'Drive Y: occupied'}
$qemuImg='C:\Program Files\qemu\qemu-img.exe'
if(-not(Test-Path $qemuImg)){
  & choco install qemu --version=2026.8.11 --yes --no-progress --limit-output
  if($LASTEXITCODE -ne 0){throw "Chocolatey QEMU install failure $LASTEXITCODE"}
}
if(-not(Test-Path $qemuImg)){throw 'qemu-img not installed'}
& $qemuImg --version
$attached=$false
try {
  Write-Host 'Applying already-customized Tiny11 install.wim directly; installer ISO not required'
  Get-WindowsImage -ImagePath $image | Format-Table | Out-String | Set-Content (Join-Path $report 'tiny11-wim-info.txt')
  $commands=@(
    ('CREATE VDISK FILE="{0}" MAXIMUM=65536 TYPE=EXPANDABLE' -f $vhdx),
    ('SELECT VDISK FILE="{0}"' -f $vhdx),
    'attach vdisk',
    'convert gpt',
    'create partition efi size=260',
    'format quick fs=fat32 label="EFI"',
    'assign letter=Y',
    'create partition msr size=16',
    'create partition primary',
    'format quick fs=ntfs label="Windows"',
    'assign letter=W',
    'exit'
  )
  $cmdFile=Join-Path $root 'create-vhdx.dps'
  $commands | Set-Content $cmdFile -Encoding ASCII
  Write-Host 'DiskPart input:'; Get-Content $cmdFile | Out-Host
  & diskpart.exe /s $cmdFile | Tee-Object -FilePath (Join-Path $report 'diskpart-create.txt') | Out-Host
  if($LASTEXITCODE -ne 0){throw "Diskpart failed: $LASTEXITCODE"}
  $attached=$true
  if(-not(Test-Path 'W:\')){throw 'Windows volume W: not present'}
  if(-not(Test-Path 'Y:\')){throw 'EFI volume Y: not present'}
  Write-Host 'Applying Tiny11 Windows image to VHDX without graphical installer'
  & dism.exe /Apply-Image "/ImageFile:$image" /Index:1 /ApplyDir:W:\ /CheckIntegrity |
    Tee-Object -FilePath (Join-Path $report 'apply-image.txt') | Out-Host
  if($LASTEXITCODE -ne 0){throw "DISM Apply-Image failed: $LASTEXITCODE"}
  # SetupComplete is expected to run as SYSTEM when initial Windows setup finalizes.
  # QEMU host provides TINY11_PROBE.TXT on its FAT virtual exchange disk.
  $hooks='W:\Windows\Setup\Scripts'
  New-Item -ItemType Directory -Force -Path $hooks | Out-Null
  @'
@echo off
echo SETUP_COMPLETE_STARTED > %SystemRoot%\Temp\tiny11_setup_complete.txt
for %%D in (D E F G H I J K L M N O P Q R S T U V X Y Z) do (
  if exist %%D:\TINY11_PROBE.TXT (
    echo TINY11_READY>%%D:\TINY11_READY.TXT
    ver >>%%D:\TINY11_READY.TXT
    echo MACHINE=%COMPUTERNAME%>>%%D:\TINY11_READY.TXT
  )
)
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $hooks 'SetupComplete.cmd') -Encoding ASCII
  Write-Host 'Created Windows guest-side boot marker hook (SetupComplete)'
  & bcdboot.exe W:\Windows /s Y: /f UEFI /l en-US | Out-Host
  if($LASTEXITCODE -ne 0){throw "BCDBoot failed: $LASTEXITCODE"}
  (Get-Item 'W:\Windows\System32\ntoskrnl.exe').VersionInfo.FileVersion |
    Set-Content (Join-Path $report 'ntoskrnl-version.txt')
  Write-Host 'UEFI Windows boot partition constructed'
} finally {
  if($attached){
    $detach=Join-Path $root 'detach-vhdx.dps'
    @( ('SELECT VDISK FILE="{0}"' -f $vhdx), 'DETACH VDISK', 'EXIT' ) |
      Set-Content $detach -Encoding ASCII
    & diskpart.exe /s $detach | Out-Host
  }
}
Write-Host 'Converting Windows VHDX to compressed QCOW2 with real zstd compressed clusters'
& $qemuImg convert -p -c -f vhdx -O qcow2 -o 'compat=1.1,compression_type=zstd' $vhdx $qcow
if($LASTEXITCODE -ne 0){throw "qemu-img zstd convert failed: $LASTEXITCODE"}
Write-Host 'Verifying compressed Windows disk is byte-identical to source VHDX'
& $qemuImg compare -f vhdx -F qcow2 $vhdx $qcow
if($LASTEXITCODE -ne 0){throw "VHDX and compressed QCOW2 differ"}
& $qemuImg info --output=json $qcow |
  Set-Content (Join-Path $report 'tiny11-qcow2-info.json')
if($LASTEXITCODE -ne 0){throw 'qemu-img info failed'}
& $qemuImg map --output=json $qcow |
  Set-Content (Join-Path $report 'tiny11-qcow2-map.json')
if($LASTEXITCODE -ne 0){throw 'qemu-img map failed'}
$info=(Get-Content (Join-Path $report 'tiny11-qcow2-info.json') -Raw | ConvertFrom-Json)
$maps=(Get-Content (Join-Path $report 'tiny11-qcow2-map.json') -Raw | ConvertFrom-Json)
$compressed=@($maps | Where-Object { $_.compressed -eq $true }).Count
if($compressed -le 0){throw "QCOW2 metadata reports 0 compressed extents"}
if(($info | ConvertTo-Json -Depth 10) -notmatch 'zstd'){throw 'QCOW2 not configured for zstd'}
$before=(Get-Item $vhdx).Length
$after=(Get-Item $qcow).Length
@(
 "VHDX physical size: $before bytes",
 "QCOW2 physical size: $after bytes",
 "Compressed map extents: $compressed",
 "Virtual size: 65536 MiB",
 "QCOW2 compression_type: zstd"
) | Set-Content (Join-Path $report 'compression-result.txt')
Get-Content (Join-Path $report 'compression-result.txt') | Out-Host
Write-Host "PASS: actual compressed Tiny11 QCOW2 built; deliberately not uploaded to public GitHub."
