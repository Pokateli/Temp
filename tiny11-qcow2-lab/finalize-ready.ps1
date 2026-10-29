# First boot the installed Windows, complete OOBE, clean shutdown, then flatten to standalone ZSTD QCOW2.
$ErrorActionPreference='Stop'
$root=Join-Path $env:RUNNER_TEMP 'tiny11-lab'
$logs=Join-Path $env:GITHUB_WORKSPACE 'tiny11-qcow2-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$img='C:\Program Files\qemu\qemu-img.exe'
$qemu='C:\Program Files\qemu\qemu-system-x86_64.exe'
$base=Join-Path $root 'tiny11-zstd.qcow2'
$overlay=Join-Path $root 'tiny11-firstboot.qcow2'
$ready=Join-Path $root 'tiny11-ready-zstd.qcow2'
foreach($f in @($img,$qemu,$base,(Join-Path $root 'ready-credentials.txt'))){
 if(-not(Test-Path $f)){throw "Missing prerequisite $f"}
}
$fw=Join-Path $root 'firmware'
New-Item -ItemType Directory -Force -Path $fw | Out-Null
if(-not(Test-Path (Join-Path $fw 'OVMF_CODE.fd'))){
 & gh run download 40862608752 --repo Pokateli/Temp --name tiny11-ovmf-uefi-firmware --dir $fw
 if($LASTEXITCODE -ne 0){throw 'Cannot download verified OVMF'}
}
$code=Join-Path $fw 'OVMF_CODE.fd'
$vars=Join-Path $root 'OVMF_VARS-firstboot.fd'
Copy-Item (Join-Path $fw 'OVMF_VARS.fd') $vars -Force
& $img create -f qcow2 -F qcow2 -b $base $overlay
if($LASTEXITCODE -ne 0){throw 'QEMU overlay creation failed'}
$share=Join-Path $root 'sharefiles'
New-Item -ItemType Directory -Force -Path $share | Out-Null
Set-Content -LiteralPath (Join-Path $share 'TINY11_PROBE.TXT') -Value READY_FIRST_BOOT -Encoding ASCII
$marker=Join-Path $share 'TINY11_FIRST_LOGON_READY.TXT'
Remove-Item $marker -Force -ErrorAction SilentlyContinue
$stdout=Join-Path $logs 'ready-firstboot-stdout.log'
$stderr=Join-Path $logs 'ready-firstboot-stderr.log'
$serial=Join-Path $logs 'ready-firstboot-serial.log'
$args=@(
 '-machine','pc,accel=tcg','-cpu','max','-smp','2','-m','2560',
 '-display','none','-vga','std','-monitor','tcp:127.0.0.1:4444,server=on,wait=off',
 '-serial',"file:$serial",'-nic','none',
 '-drive',"if=pflash,format=raw,readonly=on,file=$code",
 '-drive',"if=pflash,format=raw,file=$vars",
 '-drive',"if=ide,format=qcow2,file=$overlay",
 '-drive',"file=fat:rw:$share,format=raw,if=ide"
)
Write-Host 'Starting one-time Windows 11 OOBE inside QEMU TCG (no KVM, no network)'
$p=Start-Process -FilePath $qemu -ArgumentList $args -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
function Screenshot([int]$elapsed){
 $dest=(Join-Path $logs "firstboot-$elapsed.ppm").Replace('\','/')
 try{
  $tcp=[Net.Sockets.TcpClient]::new('127.0.0.1',4444)
  $w=[IO.StreamWriter]::new($tcp.GetStream(),[Text.Encoding]::ASCII)
  $w.AutoFlush=$true
  $w.WriteLine("screendump $dest")
  Start-Sleep -Milliseconds 350
  $w.Dispose();$tcp.Dispose()
  Write-Host "Captured $elapsed seconds"
 }catch{Write-Warning "Screenshot unavailable: $_"}
}
$started=Get-Date
$seenReady=$false
$shots=@{}
$codeExit=$null
try {
 while(((Get-Date)-$started).TotalSeconds -lt 5100){
  Start-Sleep -Seconds 10
  $p.Refresh()
  $sec=[int]((Get-Date)-$started).TotalSeconds
  foreach($at in @(90,240,540,900,1500,2100,3000,3900,4800)){
   if($sec -ge $at -and -not $shots.ContainsKey($at)){
    $shots[$at]=$true;Screenshot $at
   }
  }
  if((-not $seenReady) -and (Test-Path $marker)){
   $data=Get-Content $marker -Raw
   if($data.Contains('TINY11_FIRST_LOGON_READY') -and $data.Contains('INSTALLED_AND_OOBE_FINISHED')){
    $seenReady=$true
    $data|Set-Content (Join-Path $logs 'windows-first-logon-proof.txt')
    Write-Host "First Windows desktop login confirmed at $($sec)s; waiting for guest shutdown"
   }
  }
  if($p.HasExited){$codeExit=$p.ExitCode;break}
  if($sec % 120 -lt 10){Write-Host "OOBE runtime $($sec)s, ready=$seenReady"}
 }
} finally {
 $p.Refresh()
 if(-not $p.HasExited){
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  throw 'Windows failed to power off cleanly. Disk capture aborted.'
 }
}
# QEMU's writable virtual FAT backend may publish the guest file on final
# filesystem refresh or only after process exit. Check again after clean exit.
if((-not $seenReady) -and (Test-Path $marker)){
 $data=Get-Content -LiteralPath $marker -Raw -ErrorAction SilentlyContinue
 if($data -and $data.Contains('TINY11_FIRST_LOGON_READY') -and $data.Contains('INSTALLED_AND_OOBE_FINISHED')){
  $seenReady=$true
  $data|Set-Content (Join-Path $logs 'windows-first-logon-proof.txt')
  Write-Host 'Windows first login marker detected after clean QEMU shutdown'
 }
}
if(-not $seenReady){throw "Windows first login marker missing; QEMU exit=$codeExit"}
if($codeExit -ne 0){throw "QEMU exit not clean: $codeExit"}
Write-Host 'PASS Windows OOBE finished, local vmuser logged in, Windows powered off cleanly'
& $img convert -p -c -f qcow2 -O qcow2 -o 'compat=1.1,compression_type=zstd' $overlay $ready
if($LASTEXITCODE -ne 0){throw 'Ready disk compression failed'}
& $img compare -f qcow2 -F qcow2 $overlay $ready
if($LASTEXITCODE -ne 0){throw 'Ready disk differs from clean-shutdown overlay'}
$info=(& $img info --output=json $ready|Out-String)|ConvertFrom-Json
if($info.'backing-filename'){throw 'Ready disk unexpectedly depends on backing QCOW2'}
if(($info|ConvertTo-Json -Depth 9) -notmatch 'zstd'){throw 'Ready disk is not ZSTD compressed'}
Copy-Item $code (Join-Path $root 'OVMF_CODE.fd') -Force
Copy-Item $vars (Join-Path $root 'OVMF_VARS.fd') -Force
$hash=(Get-FileHash $ready -Algorithm SHA256).Hash.ToLowerInvariant()
$chash=(Get-FileHash (Join-Path $root 'OVMF_CODE.fd') -Algorithm SHA256).Hash.ToLowerInvariant()
$vhash=(Get-FileHash (Join-Path $root 'OVMF_VARS.fd') -Algorithm SHA256).Hash.ToLowerInvariant()
$meta=[ordered]@{
 schema=1
 name='Tiny11 Windows 11 Enterprise evaluation, installed and OOBE-complete'
 disk='tiny11-ready-zstd.qcow2'
 virtual_size_bytes=[int64]$info.'virtual-size'
 disk_size_bytes=[int64](Get-Item $ready).Length
 compression='zstd'
 disk_sha256=$hash
 ovmf_code_sha256=$chash
 ovmf_vars_sha256=$vhash
 prepared_first_boot=$true
 install_required_on_next_start=$false
 default_user='vmuser'
 credential_filename='ready-credentials.txt'
 network='off'
 vm_ram_mib=2560
 vm_cpus=2
}
$meta|ConvertTo-Json -Depth 6|Set-Content (Join-Path $root 'tiny11-ready-manifest.json') -Encoding UTF8
@("$hash *tiny11-ready-zstd.qcow2","$chash *OVMF_CODE.fd","$vhash *OVMF_VARS.fd")|
 Set-Content (Join-Path $root 'SHA256SUMS') -Encoding ASCII
$meta|ConvertTo-Json -Depth 6|Set-Content (Join-Path $logs 'ready-manifest-public.json')
Write-Host "PASS READY QCOW2: bytes $((Get-Item $ready).Length), SHA256 $hash"
Write-Host 'Windows image is not uploaded to public GitHub without encryption'
