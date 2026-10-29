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
  # The upstream Tiny11 Builder adds a Sysprep/autounattend.xml intended for
  # installer-ISO mode. Direct DISM /Apply-Image never runs that installer.
  # Remove the conflicting oobeSystem answer file so the one we install
  # below is authoritative (local vmuser, first login and clean shutdown).
  $upstreamAnswer='W:\Windows\System32\Sysprep\autounattend.xml'
  if(Test-Path -LiteralPath $upstreamAnswer){
    Remove-Item -LiteralPath $upstreamAnswer -Force -ErrorAction Stop
    Write-Host 'Removed upstream installer-only autounattend.xml to avoid OOBE collisions'
  }
  # Make a ready-to-boot *normal* Windows installation, not an Audit Mode image.
  # Generate a unique local account credential; never print the secret to CI logs.
  $pwd='VM-'+[Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(18))+'!'
  $cred=Join-Path $root 'ready-credentials.txt'
  @("Windows 11 Enterprise Evaluation (Tiny11)",'Username: vmuser',"Password: $pwd",'This password is needed after the initial automated first login.') |
    Set-Content -LiteralPath $cred -Encoding UTF8
  $unattendDir='W:\Windows\Panther\Unattend'
  New-Item -ItemType Directory -Force -Path $unattendDir | Out-Null
  $answer=Join-Path $unattendDir 'Unattend.xml'
  $xml=@'
<?xml version="1.0" encoding="utf-8"?>
<unattend xmlns="urn:schemas-microsoft-com:unattend">
  <settings pass="oobeSystem">
    <component name="Microsoft-Windows-International-Core" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS">
      <InputLocale>en-US</InputLocale><SystemLocale>en-US</SystemLocale><UILanguage>en-US</UILanguage><UserLocale>en-US</UserLocale>
    </component>
    <component name="Microsoft-Windows-Shell-Setup" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS" xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State">
      <OOBE>
        <HideEULAPage>true</HideEULAPage>
        <HideOnlineAccountScreens>true</HideOnlineAccountScreens>
        <HideWirelessSetupInOOBE>true</HideWirelessSetupInOOBE>
        <ProtectYourPC>3</ProtectYourPC>
      </OOBE>
      <UserAccounts>
        <LocalAccounts>
          <LocalAccount wcm:action="add">
            <Name>vmuser</Name>
            <DisplayName>Sandbox VM</DisplayName>
            <Group>Administrators</Group>
            <Password><Value>REPLACE_LOCAL_PASSWORD</Value><PlainText>true</PlainText></Password>
          </LocalAccount>
        </LocalAccounts>
      </UserAccounts>
      <AutoLogon>
        <Username>vmuser</Username>
        <Enabled>true</Enabled>
        <LogonCount>1</LogonCount>
        <Password><Value>REPLACE_LOCAL_PASSWORD</Value><PlainText>true</PlainText></Password>
      </AutoLogon>
      <FirstLogonCommands>
        <SynchronousCommand wcm:action="add">
          <Order>1</Order>
          <Description>Finalize prepared Tiny11 guest and power off cleanly</Description>
          <CommandLine>cmd.exe /c C:\Windows\Setup\Scripts\Tiny11FirstReady.cmd</CommandLine>
        </SynchronousCommand>
      </FirstLogonCommands>
    </component>
  </settings>
</unattend>
'@
  $xml=$xml.Replace('REPLACE_LOCAL_PASSWORD',$pwd)
  [xml]$xmlValidation=$xml
  if(-not $xmlValidation.unattend){throw 'Invalid generated Windows answer file'}
  [IO.File]::WriteAllText($answer,$xml,[Text.UTF8Encoding]::new($false))
  $scriptDir='W:\Windows\Setup\Scripts'
  New-Item -ItemType Directory -Force -Path $scriptDir | Out-Null
  # Secondary readiness proof: send bounded marker to QEMU emulated COM1.
  # The virtual FAT drive can cache writes; serial proof does not depend on its
  # directory synchronization. Failure of optional COM1 telemetry is nonfatal.
  @'
param([ValidateSet("FirstReady","SecondBoot")][string]$Event)
$ErrorActionPreference="Stop"
$com=$null
try {
  $com=[System.IO.Ports.SerialPort]::new("COM1",115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
  $com.WriteTimeout=3000
  $com.Open()
  if($Event -eq "FirstReady") {
    $com.WriteLine("TINY11_FIRST_LOGON_READY INSTALLED_AND_OOBE_FINISHED")
  } else {
    $com.WriteLine("TINY11_INSTALLED_READY SECOND_BOOT_CONFIRMED")
  }
} catch {
  exit 1
} finally {
  if($com -and $com.IsOpen){ $com.Close() }
}
'@ | Set-Content -LiteralPath (Join-Path $scriptDir 'Tiny11SerialProof.ps1') -Encoding ASCII
  # A startup marker task proves future boots of the *same already-installed* Windows.
  @'
@echo off
setlocal EnableExtensions
if not exist "%SystemRoot%\Temp" mkdir "%SystemRoot%\Temp"
echo TINY11_SECOND_BOOT_STARTED>"%SystemRoot%\Temp\Tiny11BootProbe.log"
for /L %%N in (1,1,40) do (
 for %%D in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
  if exist %%D:\TINY11_PROBE.TXT (
   echo TINY11_INSTALLED_READY>%%D:\TINY11_INSTALLED_READY.TXT
   ver >>%%D:\TINY11_INSTALLED_READY.TXT
   echo COMPUTER=%COMPUTERNAME%>>%%D:\TINY11_INSTALLED_READY.TXT
   powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%SystemRoot%\Setup\Scripts\Tiny11SerialProof.ps1" -Event SecondBoot >nul 2>&1
   dir %%D:\ >nul 2>&1
   rem Only automated second-boot probes request clean guest shutdown.
   rem Normal interactive Windows boots NEVER create this sentinel.
   if exist %%D:\TINY11_AUTOTEST_SHUTDOWN.TXT (
    findstr /x /c:"AUTOTEST_SHUTDOWN_V1" %%D:\TINY11_AUTOTEST_SHUTDOWN.TXT >nul 2>&1
    if not errorlevel 1 shutdown.exe /s /f /t 5 /c "Tiny11 automated second boot verified"
   )
   exit /b 0
  )
 )
 ping -n 6 127.0.0.1 >nul
)
echo SHARED_FAT_DISK_NOT_FOUND>>"%SystemRoot%\Temp\Tiny11BootProbe.log"
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $scriptDir 'Tiny11BootProbe.cmd') -Encoding ASCII
  @'
@echo off
setlocal EnableExtensions
if not exist "%SystemRoot%\Temp" mkdir "%SystemRoot%\Temp"
echo PREPARING_WINDOWS_READY_DISK>"%SystemRoot%\Temp\Tiny11FirstReady.log"
schtasks.exe /Create /F /SC ONSTART /RU SYSTEM /TN "Tiny11-Ready-Probe" /TR "C:\Windows\Setup\Scripts\Tiny11BootProbe.cmd" >>"%SystemRoot%\Temp\Tiny11FirstReady.log" 2>&1
if errorlevel 1 exit /b 1
powercfg.exe /h off >>"%SystemRoot%\Temp\Tiny11FirstReady.log" 2>&1
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%SystemRoot%\Setup\Scripts\Tiny11SerialProof.ps1" -Event FirstReady >>"%SystemRoot%\Temp\Tiny11FirstReady.log" 2>&1
for %%D in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
 if exist %%D:\TINY11_PROBE.TXT (
   echo TINY11_FIRST_LOGON_READY>%%D:\TINY11_FIRST_LOGON_READY.TXT
   ver >>%%D:\TINY11_FIRST_LOGON_READY.TXT
   echo USER=%USERNAME%>>%%D:\TINY11_FIRST_LOGON_READY.TXT
   echo INSTALLED_AND_OOBE_FINISHED>>%%D:\TINY11_FIRST_LOGON_READY.TXT
   dir %%D:\ >nul 2>&1
 )
)
del /f /q "%SystemRoot%\Panther\Unattend\Unattend.xml" >nul 2>&1
del /f /q "%SystemRoot%\Panther\Unattend.xml" >nul 2>&1
shutdown.exe /s /f /t 5 /c "Tiny11 prepared and ready for QCOW2 snapshot"
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $scriptDir 'Tiny11FirstReady.cmd') -Encoding ASCII
  Write-Host 'Prepared offline Tiny11 OOBE answer file, unique vmuser credential and first-boot shutdown automation'
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
    Write-Host 'Detaching Windows VHDX, flushing NTFS and VHDX journal'
    $detOut=(& diskpart.exe /s $detach | Out-String)
    $detOut | Write-Host
    $detOut | Set-Content (Join-Path $report 'diskpart-detach.txt')
    if($detOut -notmatch '(?i)successfully detached'){
      throw 'Native VHDX did not detach cleanly; stopping before qemu-img conversion'
    }
    $stillMounted=Get-DiskImage -ImagePath $vhdx -ErrorAction SilentlyContinue
    if($stillMounted -and $stillMounted.Attached){
      throw 'VHDX is still mounted after DiskPart detachment; refusing unsafe conversion'
    }
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
Write-Host "PASS: Tiny11 installed and compressed initial QCOW2. Ready state requires one successful guest first boot and clean shutdown."
