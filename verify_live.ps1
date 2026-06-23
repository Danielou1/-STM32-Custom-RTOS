param (
    [string]$PortName = "COM16",
    [int]$DurationSeconds = 25
)

$ErrorActionPreference = "Stop"

# 1. Clean up old files
$rawFile = "dos_Tessla.txt"
$filteredFile = "dos_Tessla_filtered.txt"
if (Test-Path $rawFile) { Remove-Item $rawFile }
if (Test-Path $filteredFile) { Remove-Item $filteredFile }

# 2. Open Serial Port
Write-Host "Opening $PortName at 115200 baud..." -ForegroundColor Green
$port = New-Object System.IO.Ports.SerialPort $PortName, 115200, None, 8, one
$port.ReadTimeout = 1000
$port.Open()

# Optional: Try to reset STM32 target via DTR/RTS toggle
Write-Host "Resetting target MCU..." -ForegroundColor Cyan
$port.DtrEnable = $true
$port.RtsEnable = $true
Start-Sleep -Milliseconds 100
$port.DtrEnable = $false
$port.RtsEnable = $false

Write-Host "Recording serial output for $DurationSeconds seconds... (Press physical RST button on board if needed)" -ForegroundColor Yellow

$startTime = Get-Date
$endTime = $startTime.AddSeconds($DurationSeconds)

while ((Get-Date) -lt $endTime) {
    if ($port.BytesToRead -gt 0) {
        $data = $port.ReadExisting()
        [System.IO.File]::AppendAllText((Join-Path $PSScriptRoot $rawFile), $data)
    }
    Start-Sleep -Milliseconds 50
}

Write-Host "Closing $PortName." -ForegroundColor Green
$port.Close()

# 3. Clean the trace
Write-Host "Filtering trace log..." -ForegroundColor Cyan
python clean_trace.py

if (-not (Test-Path $filteredFile)) {
    Write-Error "Error: $filteredFile was not generated."
    exit 1
}

# 4. Run TeSSLa Verification
Write-Host "Running TeSSLa verification..." -ForegroundColor Green
java -jar "C:\Users\mouns\Master Studium\tessla-assembly-2.1.0.jar" interpreter tessla/verification.tessla dos_Tessla_filtered.txt
