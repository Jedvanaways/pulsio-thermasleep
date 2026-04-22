<#
.SYNOPSIS
  Detect whether a plugged-in Logitech USB receiver is nRF24-based (flashable
  for Mousejack-style RF sniffing) or Bolt/BLE-based (not useful for this).

.DESCRIPTION
  Part of the Pulsio ThermaSleep to Home Assistant project.
  Reads the USB device list via Get-PnpDevice, matches any Logitech (VID 046D)
  devices against known product IDs, and prints a clear verdict.

  Repo: https://github.com/Jedvanaways/pulsio-thermasleep

.NOTES
  Quickest way to run on Windows (PowerShell, no admin rights needed):
    irm https://raw.githubusercontent.com/Jedvanaways/pulsio-thermasleep/main/tools/check-logitech-receiver.ps1 | iex

  If execution policy blocks it:
    powershell -ExecutionPolicy Bypass -Command "irm https://raw.githubusercontent.com/Jedvanaways/pulsio-thermasleep/main/tools/check-logitech-receiver.ps1 | iex"
#>

$ErrorActionPreference = 'Continue'

# PIDs that use the nRF24LU1+ chip - flashable with Bastille nrf-research-firmware
# (subject to firmware version - patched firmware blocks the USB flash exploit)
$Flashable = @{
    'C52B' = 'Unifying Receiver (standard, MX Master 1/2S/3 non-S)'
    'C52E' = 'Nano Receiver (non-Unifying, same chip family)'
    'C52F' = 'Wireless Mouse Nano Receiver (M185/M525)'
    'C532' = 'Unifying Receiver (newer variant)'
    'C534' = 'Unifying Receiver (Logi C-U0008)'
    'C539' = 'Lightspeed G Receiver (gaming, older)'
    'C53A' = 'Lightspeed G Receiver (newer)'
    'C53F' = 'Lightspeed Receiver (G Pro variant)'
    'C513' = 'Mini Receiver (older, 2010s)'
    'C517' = 'Wireless Receiver (older generic)'
    'C521' = 'Nano Receiver'
    'C52A' = 'Wireless Combo Receiver'
}

# Bolt receivers use a BLE chip - NOT useful for NRF24 sniffing
$BoltPids = @{
    'C548' = 'Bolt Receiver (BLE, MX Master 3S and later)'
    'C547' = 'Bolt Receiver variant'
    'C54D' = 'Bolt Receiver variant'
}

Write-Host ''
Write-Host '=========================================================='
Write-Host ' Logitech receiver detector - Pulsio ThermaSleep project'
Write-Host '=========================================================='
Write-Host ''

try {
    $devices = Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
        $_.InstanceId -match 'USB\\VID_046D'
    }
} catch {
    Write-Host 'ERROR: Get-PnpDevice failed.' -ForegroundColor Red
    Write-Host $_.Exception.Message
    Write-Host ''
    Write-Host 'Fallback - try this and look at the PID hex in device paths:'
    Write-Host '  Get-WmiObject Win32_PnPEntity | Where-Object { $_.DeviceID -match "VID_046D" } | Select-Object Name, DeviceID'
    exit 1
}

if (-not $devices) {
    Write-Host 'No Logitech (VID 046D) USB devices found.' -ForegroundColor Red
    Write-Host '  - Plug the dongle in, try a different USB port, and re-run.'
    Write-Host '  - If you are on a remote desktop, USB passthrough may need enabling.'
    exit 1
}

Write-Host ("Found {0} Logitech USB device(s):" -f @($devices).Count)
Write-Host ''

$hasFlashable = $false
$hasBolt      = $false
$hasUnknown   = $false
$seenPids     = @{}

foreach ($d in $devices) {
    if ($d.InstanceId -match 'PID_([0-9A-Fa-f]{4})') {
        $productId = $Matches[1].ToUpper()
        # Skip duplicates (composite HID devices register multiple interfaces)
        if ($seenPids.ContainsKey($productId)) { continue }
        $seenPids[$productId] = $true

        Write-Host ('  * ' + $d.FriendlyName)
        Write-Host ('    VID: 046D   PID: ' + $productId)

        if ($Flashable.ContainsKey($productId)) {
            Write-Host ('    >> FLASHABLE CANDIDATE - ' + $Flashable[$productId]) -ForegroundColor Green
            Write-Host '       nRF24LU1+ chip. Can be reflashed with Bastille firmware'
            Write-Host '       IF current Logitech firmware is unpatched.'
            $hasFlashable = $true
        }
        elseif ($BoltPids.ContainsKey($productId)) {
            Write-Host ('    >> NOT USEFUL - ' + $BoltPids[$productId]) -ForegroundColor Yellow
            Write-Host '       BLE chip, not nRF24. Cannot sniff NRF24 protocols.'
            $hasBolt = $true
        }
        else {
            Write-Host '    >> UNKNOWN Logitech device - may or may not be flashable.' -ForegroundColor Cyan
            Write-Host ('       Search online: "Logitech USB PID ' + $productId + ' chip"')
            $hasUnknown = $true
        }
        Write-Host ''
    }
}

Write-Host '=========================================================='
Write-Host ' Verdict'
Write-Host '=========================================================='

if ($hasFlashable) {
    Write-Host 'GOOD: at least one flashable Logitech receiver is present.' -ForegroundColor Green
    Write-Host ''
    Write-Host 'Next steps:'
    Write-Host '  1. Firmware version matters. Patched firmware (RQR012.09 or later)'
    Write-Host '     blocks the Bastille USB flash path.'
    Write-Host '  2. Open "Logitech Unifying Software" or "Logi Options" to see the'
    Write-Host '     receivers firmware version.'
    Write-Host '  3. Flashing WILL stop it working as a mouse receiver until restored.'
    Write-Host '     Bastilles repo includes logitech-usb-restore for recovery.'
    Write-Host ''
    Write-Host '  CAUTION: this looks like a work machine. Check IT policy before'
    Write-Host '  modifying company hardware - even temporary firmware changes'
    Write-Host '  may be a policy violation.'
    exit 0
}
elseif ($hasBolt -and -not $hasUnknown) {
    Write-Host 'NO: only Bolt receiver(s) detected.' -ForegroundColor Red
    Write-Host 'Bolt is BLE, not nRF24. Cannot be used for this project.'
    Write-Host ''
    Write-Host 'Fallback: order a Crazyradio PA (~GBP 30, bitcraze.io).'
    exit 2
}
else {
    Write-Host 'INDETERMINATE: only unrecognised Logitech PIDs seen.' -ForegroundColor Yellow
    Write-Host 'The PID is not in the known-flashable or known-Bolt lists.'
    Write-Host 'It might still be usable - search the PID online to confirm the chip.'
    exit 3
}
