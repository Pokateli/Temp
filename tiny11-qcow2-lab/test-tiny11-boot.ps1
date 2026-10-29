# Real Tiny11 QEMU TCG boot test; only guest marker confirms successful OS boot.
$ErrorActionPreference='Stop'
$root=Join-Path $env:RUNNER_TEMP 'tiny11-lab'
$logs=Join-Path $env:GITHUB_WORKSPACE 'tiny11-qcow2-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$qcow=Join-Path $root 'tiny11-zstd.qcow2'
if(-not(Test-Path $qcow)){throw 'Tiny11 compressed QCOW2 not found'}
$qemu='C:\Program Files\qemu\qemu-system-x86_64.exe'
if(-not(Test-Path $qemu)){throw 'QEMU missing'}
$fw=Join-Path $root 'firmware'
New-Item -ItemType Directory -Force -Path $fw | Out-Null
if(-not(Test-Path (Join-Path $fw 'OVMF_CODE.fd'))){
    Write-Host 'Download known-good OVMF from prior Temp workflow'
    & gh run download 40862608752 --repo Pokateli/Temp --name tiny11-ovmf-uefi-firmware --dir $fw
    if($LASTEXITCODE -ne 0){throw 'Cannot download UEFI firmware'}
}
foreach($f in @('OVMF_CODE.fd','OVMF_VARS.fd','SHA256SUMS')){
    if(-not(Test-Path (Join-Path $fw $f))){throw "Missing firmware asset $f"}
}
$vars=Join-Path $root 'OVMF_VARS-test.fd'
Copy-Item (Join-Path $fw 'OVMF_VARS.fd') $vars -Force
$code=Join-Path $fw 'OVMF_CODE.fd'
$share=Join-Path $root 'sharefiles'
New-Item -ItemType Directory -Force -Path $share | Out-Null
Set-Content -Path (Join-Path $share 'TINY11_PROBE.TXT') -Value INITIAL_WINDOWS_BOOT_TEST -Encoding ASCII
$marker=Join-Path $share 'TINY11_READY.TXT'
Remove-Item $marker -Force -ErrorAction SilentlyContinue
$stdout=Join-Path $logs 'qemu-tiny11-stdout.log'
$stderr=Join-Path $logs 'qemu-tiny11-stderr.log'
$ser=Join-Path $logs 'qemu-tiny11-serial.log'
$args=@(
  '-machine','pc,accel=tcg','-cpu','max','-smp','2','-m','2560',
  '-display','none','-vga','std',
  '-monitor','tcp:127.0.0.1:4444,server=on,wait=off',
  '-serial',"file:$ser",'-nic','none',
  '-drive',"if=pflash,format=raw,readonly=on,file=$code",
  '-drive',"if=pflash,format=raw,file=$vars",
  '-drive',"if=ide,format=qcow2,file=$qcow",
  '-drive',"file=fat:rw:$share,format=raw,if=ide"
)
Write-Host 'Starting Tiny11 QEMU TCG guest, 2560 MB, no network'
$p=Start-Process $qemu -ArgumentList $args -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
function Save-Screen([int]$at){
    $dest=(Join-Path $logs "qemu-screen-$at.ppm").Replace('\','/')
    try {
        $tcp=[Net.Sockets.TcpClient]::new('127.0.0.1',4444)
        $stream=$tcp.GetStream()
        $writer=[IO.StreamWriter]::new($stream,[Text.Encoding]::ASCII)
        $writer.AutoFlush=$true
        $writer.WriteLine("screendump $dest")
        Start-Sleep -Milliseconds 400
        $writer.Dispose()
        $tcp.Dispose()
        Write-Host "Screenshot requested at $at seconds"
    }catch{Write-Warning "Screenshot error: $_"}
}
$confirmed=$false
$started=Get-Date
$shots=@{}
try {
  while(((Get-Date)-$started).TotalSeconds -lt 1050){
    Start-Sleep -Seconds 10
    $sec=[int]((Get-Date)-$started).TotalSeconds
    foreach($point in @(60,180,420,840)){
      if($sec -ge $point -and -not $shots.ContainsKey($point)){
        $shots[$point]=$true
        Save-Screen $point
      }
    }
    if(Test-Path $marker){
      $msg=Get-Content $marker -Raw
      if($msg.Contains('TINY11_READY')){
        $confirmed=$true
        Write-Host "TINY11_GUEST_READY: $msg"
        $msg | Set-Content -Path (Join-Path $logs 'tiny11-boot-proof.txt')
        break
      }
    }
    $p.Refresh()
    if($p.HasExited){
      Write-Host "QEMU ended early: exit code $($p.ExitCode)"
      break
    }
    if($sec % 60 -lt 10){Write-Host "Tiny11 TCG boot probe elapsed: $($sec)s"}
  }
} finally {
    $p.Refresh()
    if(-not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
    Start-Sleep -Seconds 3
}
if(Test-Path $stderr){Get-Content $stderr -Tail 50|Out-Host}
if(-not $confirmed){
  Write-Host 'No TINY11_READY marker: screenshot alone is NOT proof Windows booted.'
  throw 'Windows guest boot was not verified'
}
Write-Host 'PASS: Tiny11 ran SetupComplete inside QEMU TCG without KVM'
