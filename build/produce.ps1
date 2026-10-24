param([ValidateSet('base','video','audit','extract','audio','ui','pages','all')][string]$Stage='all',[string]$Exe='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
try {
 Push-Location $root
 try {
  $python=Join-Path $root 'tools/python/python.exe'
  if(!(Test-Path -LiteralPath $python)){
   $python=Join-Path $root 'tools/production-python/Scripts/python.exe'
   if(!(Test-Path -LiteralPath $python)){
    & py -3.12 -m venv (Join-Path $root 'tools/production-python')
    if($LASTEXITCODE -ne 0){throw 'Python 3.12 is required to create the production environment.'}
   }
  }
  & $python -B scripts/exe/dependencies.py
  if($LASTEXITCODE -ne 0){throw 'Dependency setup failed.'}
  $arguments=@('-B','scripts/exe/pipeline.py','--root',$root,'--stage',$Stage)
  if($Exe){$arguments+=@('--exe',$Exe)}
  & $python @arguments
  exit $LASTEXITCODE
 } finally {Pop-Location}
} catch {Write-Host ('ERROR: '+$_.Exception.Message);exit 1}
