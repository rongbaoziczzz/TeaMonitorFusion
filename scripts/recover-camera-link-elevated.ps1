$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
$log = Join-Path $root 'diagnostics\camera-recovery-admin.log'
$marker = Join-Path $root 'diagnostics\camera-recovery-admin.exit'
New-Item -ItemType Directory -Path (Split-Path -Parent $log) -Force | Out-Null
Set-Content -LiteralPath $log -Value ("Started {0}" -f (Get-Date -Format o))
Set-Content -LiteralPath $marker -Value 'running'

try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File (Join-Path $PSScriptRoot 'recover-camera-link.ps1') *>&1 |
        Tee-Object -FilePath $log -Append
    $code = if ($null -ne $LASTEXITCODE) { [int]$LASTEXITCODE } else { 0 }
} catch {
    $_ | Out-String | Tee-Object -FilePath $log -Append
    $code = 99
}

Add-Content -LiteralPath $log -Value ("Finished {0}; exit code {1}" -f (Get-Date -Format o), $code)
Set-Content -LiteralPath $marker -Value $code
exit $code
