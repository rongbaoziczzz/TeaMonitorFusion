$ErrorActionPreference = 'Stop'

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw '请右键使用“以管理员身份运行 PowerShell”执行此脚本。'
}

$serviceName = 'ebUniversalProForEthernet'
sc.exe config $serviceName start= demand | Out-Host
sc.exe stop $serviceName | Out-Host

$bindings = Get-NetAdapterBinding -ComponentID 'pt_ebuniversalproforethernet' -ErrorAction SilentlyContinue |
    Where-Object Enabled
foreach ($binding in $bindings) {
    Disable-NetAdapterBinding -Name $binding.Name -ComponentID $binding.ComponentID -Confirm:$false
}

Write-Host ''
Write-Host 'eBUS 已设置为手动启动，当前过滤绑定已禁用。' -ForegroundColor Green
Write-Host '验证结果：'
sc.exe qc $serviceName | Select-String 'START_TYPE'
sc.exe query $serviceName | Select-String 'STATE'
Get-NetAdapterBinding -ComponentID 'pt_ebuniversalproforethernet' -ErrorAction SilentlyContinue |
    Select-Object Name, Enabled |
    Format-Table -AutoSize
