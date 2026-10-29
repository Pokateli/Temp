# Run bootable Windows PE with QEMU software TCG on an ephemeral Windows Actions runner.
# Test ends when actual guest writes a READY.TXT marker to a virtual FAT disk.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$iso = Join-Path $env:RUNNER_TEMP 'winpe-qemu-test.iso'
$logs = Join-Path $env:GITHUB_WORKSPACE 'windows-vm-lab-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
if (-not (Test-Path $iso)) { throw "Missing WinPE ISO $iso" }

Write-Host 'Install QEMU Windows x64'
& choco install qemu --version=2026.8.11 --yes --no-progress --limit-output
if ($LASTEXITCODE -ne 0) { throw "Chocolatey QEMU install failed ($LASTEXITCODE)" }
$qemu = 'C:\Program Files\qemu\qemu-system-x86_64.exe'
if (-not (Test-Path $qemu)) {
    $qemu = Get-ChildItem -Path 'C:\Program Files','C:\Program Files (x86)' -Filter qemu-system-x86_64.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not (Test-Path $qemu)) { throw "QEMU binary not found" }
& $qemu --version
if ($LASTEXITCODE -ne 0) { throw "QEMU --version failed" }

$share = Join-Path $env:RUNNER_TEMP 'winpe-exchange'
New-Item -ItemType Directory -Force -Path $share | Out-Null
Set-Content -Path (Join-Path $share 'WINPE_PROBE.TXT') -Value 'HELLO_FROM_HOST' -Encoding ASCII
$marker = Join-Path $share 'WINPE_READY.TXT'
$serialLog = Join-Path $logs 'qemu-serial.txt'
$stdout = Join-Path $logs 'qemu-stdout.txt'
$stderr = Join-Path $logs 'qemu-stderr.txt'
$argumentList = @(
  '-machine','pc,accel=tcg,thread=multi',
  '-cpu','max',
  '-smp','2',
  '-m','3072',
  '-no-reboot',
  '-display','none',
  '-vga','std',
  '-monitor','tcp:127.0.0.1:4444,server=on,wait=off',
  '-serial',"file:$serialLog",
  '-nic','none',
  '-boot','order=d',
  '-drive',"file=$iso,media=cdrom,if=ide",
  '-drive',"file=fat:rw:$share,format=raw,if=ide"
)
Write-Host "Starting QEMU with TCG: $qemu"
Write-Host ("args: " + ($argumentList -join ' '))
$p = Start-Process -FilePath $qemu -ArgumentList $argumentList -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$ready = $false
try {
  $deadline = (Get-Date).AddMinutes(8)
  $start = Get-Date
  $captured = @{}
  $keysSent = @{}
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 5
    $elapsed = ((Get-Date) - $start).TotalSeconds
    # Windows PE El Torito boot sector waits at "Press any key to boot from CD/DVD".
    # QEMU is headless, so inject a key via QEMU human monitor instead of waiting forever.
    foreach ($seconds in @(5,10,15,20,25,30,35)) {
      if ($elapsed -ge $seconds -and -not $keysSent.ContainsKey($seconds)) {
        $keysSent[$seconds] = $true
        try {
          $keySocket = [System.Net.Sockets.TcpClient]::new('127.0.0.1',4444)
          $keyStream = $keySocket.GetStream()
          $keyWriter = [System.IO.StreamWriter]::new($keyStream,[System.Text.Encoding]::ASCII)
          $keyWriter.AutoFlush = $true
          $keyWriter.WriteLine('sendkey ret')
          Start-Sleep -Milliseconds 200
          $keyWriter.Dispose()
          $keySocket.Dispose()
          Write-Host "Sent QEMU keyboard Enter at elapsed $seconds seconds"
        } catch { Write-Warning "QEMU sendkey attempt failed: $_" }
      }
    }
    foreach ($seconds in @(45,120,240,360)) {
      if ($elapsed -ge $seconds -and -not $captured.ContainsKey($seconds)) {
        $captured[$seconds] = $true
        $shot = (Join-Path $logs ("screen-" + $seconds + ".ppm")).Replace('\','/')
        try {
          $socket = [System.Net.Sockets.TcpClient]::new('127.0.0.1',4444)
          $stream = $socket.GetStream()
          $writer = [System.IO.StreamWriter]::new($stream,[System.Text.Encoding]::ASCII)
          $writer.AutoFlush = $true
          $writer.WriteLine("screendump $shot")
          Start-Sleep -Milliseconds 500
          $writer.Dispose()
          $socket.Dispose()
          Write-Host "Requested QEMU screenshot: $shot"
        } catch { Write-Warning "Screenshot failed: $_" }
      }
    }
    if (Test-Path $marker) {
      $contents = Get-Content -Raw $marker
      if ($contents.Contains('WINPE_READY')) {
        Write-Host "Windows PE guest confirmed: $contents"
        $contents | Set-Content -Path (Join-Path $logs 'guest-ready.txt')
        $ready = $true
        break
      }
    }
    $p.Refresh()
    if ($p.HasExited) {
      Write-Host "QEMU terminated with exit code $($p.ExitCode)"
      break
    }
  }
} finally {
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Start-Sleep -Seconds 3
}
if (Test-Path $stderr) { Get-Content $stderr -Tail 100 | Out-Host }
if (-not $ready) { throw 'Guest has NOT proved WinPE boot through FAT ready marker; see qemu-stderr.txt' }
Write-Host "PASS: real Windows PE guest booted in QEMU TCG (no KVM/WHPX)"
