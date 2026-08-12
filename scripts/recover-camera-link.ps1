param(
    [switch]$ProbeAravis,
    [switch]$ForceKnownIp
)

$ErrorActionPreference = 'Stop'

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an elevated PowerShell window.'
}

$cameraIp = '169.254.4.4'
$cameraMac = '00-11-1C-F6-47-16'
$root = Split-Path -Parent $PSScriptRoot
$arvTool = Join-Path $root 'third_party\device_support\Aravis\runtime\bin\arv-tool-0.8.exe'
$diagDir = Join-Path $root 'diagnostics'
New-Item -ItemType Directory -Path $diagDir -Force | Out-Null

$cameraAddress = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object IPAddress -eq '169.254.53.32' |
    Select-Object -First 1
$networkAdapter = if ($cameraAddress) {
    Get-NetAdapter -InterfaceIndex $cameraAddress.InterfaceIndex -ErrorAction SilentlyContinue
}
$devices = if ($networkAdapter -and $networkAdapter.PnPDeviceID) {
    @(Get-PnpDevice -InstanceId $networkAdapter.PnPDeviceID -PresentOnly -ErrorAction SilentlyContinue)
} else {
    @(Get-PnpDevice -Class Net -PresentOnly | Where-Object {
        $_.FriendlyName -match 'ASIX' -or $_.InstanceId -match 'VID_0B95'
    })
}
if ($devices.Count -eq 0) {
    throw 'No camera network adapter was found. Assign 169.254.53.32/16 to the camera adapter and run again.'
}
if ($devices.Count -gt 1) {
    Write-Host 'Multiple possible camera adapters found:' -ForegroundColor Yellow
    $devices | Select-Object Status, FriendlyName, InstanceId | Format-Table -AutoSize
    throw 'Disconnect unused ASIX adapters and run again.'
}

$device = $devices[0]
Write-Host ("Resetting: {0}" -f $device.FriendlyName) -ForegroundColor Cyan
if ($networkAdapter) {
    Write-Host ("Interface: {0}; link speed: {1}" -f $networkAdapter.Name, $networkAdapter.LinkSpeed) -ForegroundColor Cyan

    # The AX88772D driver can emit invalid checksums for the short UDP packets
    # used by GVCP. Keep checksum calculation in software on this adapter.
    foreach ($keyword in @('*UDPChecksumOffloadIPv4', '*IPChecksumOffloadIPv4')) {
        $property = Get-NetAdapterAdvancedProperty -Name $networkAdapter.Name `
            -RegistryKeyword $keyword -ErrorAction SilentlyContinue
        if ($property -and [int]$property.RegistryValue[0] -ne 0) {
            Set-NetAdapterAdvancedProperty -Name $networkAdapter.Name `
                -RegistryKeyword $keyword -RegistryValue 0 -NoRestart -ErrorAction Stop
            Write-Host ("Disabled {0} on the camera adapter." -f $keyword) -ForegroundColor Green
        }
    }
}
if ($device.FriendlyName -match 'Fast Ethernet' -or
    ($networkAdapter -and $networkAdapter.LinkSpeed -match '^100 Mbps')) {
    Write-Host 'WARNING: this adapter is AX88772D Fast Ethernet (100 Mbps), not a Gigabit adapter.' -ForegroundColor Yellow
    Write-Host 'A GigE Vision camera normally requires a USB 3.0 Gigabit adapter or a built-in Gigabit NIC.' -ForegroundColor Yellow
}
Disable-PnpDevice -InstanceId $device.InstanceId -Confirm:$false
try {
    Start-Sleep -Seconds 2
} finally {
    Enable-PnpDevice -InstanceId $device.InstanceId -Confirm:$false
}

# USB Ethernet adapters can report an address before their PHY has finished
# negotiating. Wait for a real link before optional address recovery.
$adapterReady = $false
for ($attempt = 1; $attempt -le 30 -and -not $adapterReady; ++$attempt) {
    $currentAdapter = if ($networkAdapter) {
        Get-NetAdapter -InterfaceIndex $networkAdapter.ifIndex -ErrorAction SilentlyContinue
    } else {
        Get-NetAdapter -ErrorAction SilentlyContinue |
            Where-Object InterfaceDescription -eq $device.FriendlyName |
            Select-Object -First 1
    }
    if ($currentAdapter -and $currentAdapter.Status -eq 'Up' -and $currentAdapter.LinkSpeed -notmatch '^0') {
        $adapterReady = $true
        $networkAdapter = $currentAdapter
        Write-Host ("Camera adapter link is up: {0}" -f $currentAdapter.LinkSpeed) -ForegroundColor Green
    } else {
        Start-Sleep -Seconds 1
    }
}
if (-not $adapterReady) {
    Write-Host 'Camera adapter did not reach an active link within 30 seconds.' -ForegroundColor Red
    exit 1
}
Start-Sleep -Seconds 3

if ($networkAdapter) {
    $interfaceIndex = $networkAdapter.ifIndex
    Set-NetIPInterface -InterfaceIndex $interfaceIndex -AddressFamily IPv4 -Dhcp Disabled -ErrorAction SilentlyContinue
    $targetAddress = Get-NetIPAddress -IPAddress '169.254.53.32' -ErrorAction SilentlyContinue |
        Where-Object InterfaceIndex -eq $interfaceIndex |
        Select-Object -First 1
    if (-not $targetAddress) {
        $activeAddresses = @(Get-NetIPAddress -InterfaceIndex $interfaceIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue)
        $activeAddresses |
            Where-Object { $_.IPAddress -ne '127.0.0.1' } |
            Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
        $targetAddress = Get-NetIPAddress -IPAddress '169.254.53.32' -ErrorAction SilentlyContinue |
            Where-Object InterfaceIndex -eq $interfaceIndex |
            Select-Object -First 1
        if (-not $targetAddress) {
            try {
                New-NetIPAddress -InterfaceIndex $interfaceIndex -IPAddress '169.254.53.32' -PrefixLength 16 -ErrorAction Stop | Out-Null
            } catch {
                $targetAddress = Get-NetIPAddress -IPAddress '169.254.53.32' -ErrorAction SilentlyContinue |
                    Where-Object InterfaceIndex -eq $interfaceIndex |
                    Select-Object -First 1
                if (-not $targetAddress) {
                    throw
                }
            }
        }
    }
    Start-Sleep -Seconds 2
}

if (-not $ForceKnownIp) {
    Write-Host 'Camera adapter is ready. The application will discover serial 56S014 and use its current IP address.' -ForegroundColor Green
    Write-Host 'ForceIP was not used; 169.254.4.4 is an emergency diagnostic fallback only.' -ForegroundColor DarkGray
    exit 0
}
function Send-GvcpForceIp {
    param(
        [string]$SourceIp,
        [int]$InterfaceIndex = 0
    )

    $client = $null
    try {
        $sourceAddress = [Net.IPAddress]::Parse([string]$SourceIp)
        $client = [Net.Sockets.UdpClient]::new()
        $client.Client.SetSocketOption(
            [Net.Sockets.SocketOptionLevel]::Socket,
            [Net.Sockets.SocketOptionName]::ReuseAddress,
            $true)
        $boundToCameraInterface = $false
        for ($attempt = 1; $attempt -le 10 -and -not $boundToCameraInterface; ++$attempt) {
            try {
                # Use a source port chosen by Windows so another GVCP tool cannot
                # force the probe onto a different interface.
                $client.Client.Bind([Net.IPEndPoint]::new($sourceAddress, 0))
                $boundToCameraInterface = $true
            } catch [System.Net.Sockets.SocketException] {
                if ($attempt -lt 10) {
                    Start-Sleep -Milliseconds 500
                }
            }
        }
        if (-not $boundToCameraInterface) {
            $client.Close()
            $client = [Net.Sockets.UdpClient]::new()
            $client.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Any, 0))
        }
        $client.EnableBroadcast = $true
        $client.Client.ReceiveTimeout = 1200
        Write-Host ("ForceIP source route: {0}; socket: {1}" -f $sourceAddress, $client.Client.LocalEndPoint)

        # GVCP FORCE_IP_CMD for MAC 00:11:1C:F6:47:16,
        # temporary IP 169.254.4.4, mask 255.255.0.0, gateway 0.0.0.0.
        $packet = New-Object byte[] 64
        $packet[0] = 0x42; $packet[1] = 0x01
        $packet[2] = 0x00; $packet[3] = 0x04
        $packet[4] = 0x00; $packet[5] = 0x38
        $packet[6] = 0x12; $packet[7] = 0x35
        $packet[10] = 0x00; $packet[11] = 0x11; $packet[12] = 0x1c
        $packet[13] = 0xf6; $packet[14] = 0x47; $packet[15] = 0x16
        $packet[28] = 0xa9; $packet[29] = 0xfe; $packet[30] = 0x04; $packet[31] = 0x04
        $packet[44] = 0xff; $packet[45] = 0xff; $packet[46] = 0x00; $packet[47] = 0x00

        foreach ($target in @('169.254.255.255', '255.255.255.255')) {
            try {
                $endpoint = [Net.IPEndPoint]::new([Net.IPAddress]::Parse($target), 3956)
                [void]$client.Send($packet, $packet.Length, $endpoint)
                Write-Host ("ForceIP sent to {0}:3956" -f $target)
            } catch {
                Write-Host ("ForceIP send to {0} skipped: {1}" -f $target, $_.Exception.Message) -ForegroundColor Yellow
            }
        }

        $responses = 0
        $deadline = (Get-Date).AddSeconds(3)
        while ((Get-Date) -lt $deadline) {
            try {
                $remote = [Net.IPEndPoint]::new([Net.IPAddress]::Any, 0)
                $data = $client.Receive([ref]$remote)
                $command = if ($data.Length -ge 4) {
                    ($data[2] -shl 8) -bor $data[3]
                } else { -1 }
                if ($command -eq 0x0005 -and $remote.Address.ToString() -ne '198.18.0.1') {
                    $responses++
                    Write-Host ("ForceIP ACK from {0}: {1}" -f $remote, [BitConverter]::ToString($data))
                } else {
                    Write-Host ("Ignored local/non-ACK packet from {0}" -f $remote) -ForegroundColor DarkGray
                }
            } catch [Net.Sockets.SocketException] {
            }
        }
        Write-Host ("ForceIP responses: {0}" -f $responses)
    } catch {
        Write-Host ("ForceIP probe skipped: {0}" -f $_.Exception.Message) -ForegroundColor Yellow
    } finally {
        if ($client) {
            $client.Close()
        }
    }
}

$forceIpSource = '169.254.53.32'
$forceIpInterface = if ($networkAdapter) { [int]$networkAdapter.ifIndex } else { 0 }
for ($forceAttempt = 1; $forceAttempt -le 3; ++$forceAttempt) {
    Send-GvcpForceIp -SourceIp $forceIpSource -InterfaceIndex $forceIpInterface
    if ($forceAttempt -lt 3) {
        Start-Sleep -Seconds 2
    }
}

# Older GigE Vision cameras may reboot their network stack after ForceIP.
# Wait for both ICMP and the expected MAC instead of probing Aravis too early.
$ping = $false
$arp = $null
$arpOutput = @()
$linkDeadline = (Get-Date).AddSeconds(20)
Write-Host 'Waiting for the camera network stack...' -ForegroundColor Cyan
while ((Get-Date) -lt $linkDeadline -and (-not $ping -or -not $arp)) {
    $pingOutput = cmd.exe /d /c "ping -S $forceIpSource -n 1 -w 800 $cameraIp 2>&1"
    $ping = [bool]($pingOutput | Select-String -SimpleMatch 'TTL=')
    $arpOutput = cmd.exe /d /c "arp -a $cameraIp 2>&1"
    $arp = $arpOutput | Select-String -Pattern '00[-:]11[-:]1c[-:]f6[-:]47[-:]16'
    if (-not $ping -or -not $arp) {
        Start-Sleep -Seconds 1
    }
}
Write-Host ("Ping {0}: {1}" -f $cameraIp, $ping) -ForegroundColor ($(if ($ping) { 'Green' } else { 'Red' }))
if ($arp) {
    Write-Host ("ARP {0}: present ({1})" -f $cameraMac, $arp.ToString().Trim()) -ForegroundColor Green
} else {
    Write-Host ("ARP {0}: missing" -f $cameraMac) -ForegroundColor Red
}

$aravisReady = $true
if ($ProbeAravis -and (Test-Path -LiteralPath $arvTool)) {
    $log = Join-Path $diagDir 'arv-tool-recovery.txt'
    $stdout = Join-Path $diagDir 'arv-tool-recovery.stdout.txt'
    $stderr = Join-Path $diagDir 'arv-tool-recovery.stderr.txt'
    $env:Path = (Join-Path $root 'third_party\device_support\Aravis\runtime\bin') + ';' + $env:Path
    Remove-Item -LiteralPath $stdout,$stderr,$log -Force -ErrorAction SilentlyContinue
    $p = Start-Process -FilePath $arvTool `
        -ArgumentList '-a', $cameraIp, '-d', 'device:3,cp:3', 'features' `
        -WorkingDirectory (Split-Path -Parent $arvTool) `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    if (-not $p.WaitForExit(15000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        Add-Content -LiteralPath $stderr -Value 'Aravis probe timed out after 15 seconds.'
    }
    $probeOutput = @(Get-Content -LiteralPath $stdout,$stderr -ErrorAction SilentlyContinue)
    Set-Content -LiteralPath $log -Value $probeOutput
    Write-Host "Aravis probe log: $log"
    Get-Content -LiteralPath $log
    $aravisReady = -not [bool]($probeOutput | Select-String -Pattern "Can't connect|Ack reception timeout|No device found")
} elseif ($ProbeAravis) {
    Write-Host "Aravis tool not found: $arvTool" -ForegroundColor Yellow
    $aravisReady = $false
} else {
    Write-Host 'Camera network is ready. Aravis probe skipped so the application can take the first control connection.' -ForegroundColor Green
}

if (-not $ping) {
    Write-Host 'The camera is not reachable at the expected IP. Check camera power, cable, and its IP assignment.' -ForegroundColor Yellow
    exit 2
}
if (-not $arp) {
    exit 3
}
if ($ProbeAravis -and -not $aravisReady) {
    Write-Host 'The camera answers on the network, but its GigE Vision control channel is unavailable.' -ForegroundColor Yellow
    exit 4
}
exit 0
