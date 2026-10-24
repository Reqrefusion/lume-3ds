param([Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
$release=Split-Path -Parent $PSScriptRoot
Add-Type -Path (Join-Path $release 'package_core.cs')
New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
$base=[IO.Path]::GetFullPath($WorkDirectory)
$original=Join-Path $base 'original.bin';$wrong=Join-Path $base 'wrong.bin';$input=Join-Path $base 'payload.bin';$package=Join-Path $base 'fixture.l3dp';$output=Join-Path $base 'unpacked.bin'
[IO.File]::WriteAllBytes($original,[Text.Encoding]::UTF8.GetBytes('A real local original fixture, not its public checksum'))
[IO.File]::WriteAllBytes($wrong,[Text.Encoding]::UTF8.GetBytes('different original'))
[IO.File]::WriteAllBytes($input,[Text.Encoding]::UTF8.GetBytes(('payload verification '*1000)))
[LumePackage]::Seal($original,$input,$package);[LumePackage]::Unseal($original,$package,$output)
if([LumePackage]::Sha256($input) -ne [LumePackage]::Sha256($output)){throw 'Round trip failed'}
$checks=1
function MustFail([string]$source,[string]$capsule,[string]$dest){
 $failed=$false;try {[LumePackage]::Unseal($source,$capsule,$dest)} catch {$failed=$true}
 if(!$failed -or (Test-Path -LiteralPath $dest)){throw 'Invalid package produced output'}
}
MustFail $wrong $package (Join-Path $base 'wrong-output');$checks++
foreach($position in @(50,120)){
 $data=[IO.File]::ReadAllBytes($package);$data[$position]=$data[$position] -bxor 1
 $bad=Join-Path $base ('bad-'+$position+'.l3dp');[IO.File]::WriteAllBytes($bad,$data)
 MustFail $original $bad (Join-Path $base ('bad-output-'+$position));$checks++
}
$bytes=[IO.File]::ReadAllBytes($package);$short=New-Object byte[] ($bytes.Length-1);[Array]::Copy($bytes,$short,$short.Length)
$bad=Join-Path $base 'truncated.l3dp';[IO.File]::WriteAllBytes($bad,$short);MustFail $original $bad (Join-Path $base 'short-output');$checks++
$before=[LumePackage]::Sha256($output);$failed=$false
try {[LumePackage]::Unseal($original,$package,$output)} catch {$failed=$true}
if(!$failed -or [LumePackage]::Sha256($output) -ne $before){throw 'Existing output changed'};$checks++
if(@(Get-ChildItem -LiteralPath $base -Filter '*.part').Count){throw 'Partial plaintext left behind'}
Write-Host ('PASS package checks: '+$checks+' (roundtrip, wrong original, header/cipher changes, truncation, overwrite)')
