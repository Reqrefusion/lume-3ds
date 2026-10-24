param([Parameter(Mandatory=$true)][string]$OriginalExe,[Parameter(Mandatory=$true)][string]$Payload,[Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'package_core.cs')
[LumePackage]::Seal([IO.Path]::GetFullPath($OriginalExe),[IO.Path]::GetFullPath($Payload),[IO.Path]::GetFullPath($Package))
Write-Host 'Prepared package created.'
