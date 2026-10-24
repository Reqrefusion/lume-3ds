param([string]$OriginalExe='', [string]$OutputDirectory='', [string]$Package='')
$ErrorActionPreference='Stop'
$stage=$null
try {
 if(!$OriginalExe){
  Add-Type -AssemblyName System.Windows.Forms
  $dialog=New-Object System.Windows.Forms.OpenFileDialog
  $dialog.Title='Select your original Lume.exe'
  $dialog.Filter='Lume.exe|*.exe|All files|*.*'
  if($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK){throw 'File selection cancelled.'}
  $OriginalExe=$dialog.FileName
 }
 if(!$Package){$Package=Join-Path $PSScriptRoot 'Lume3DS.l3dp'}
 if(!$OutputDirectory){$OutputDirectory=Join-Path $PSScriptRoot 'sdmc'}
 $OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
 if(Test-Path -LiteralPath $OutputDirectory){throw 'Output folder already exists. Select a new folder; existing files are never overwritten.'}
 $parent=[IO.Path]::GetDirectoryName($OutputDirectory)
 New-Item -ItemType Directory -Force -Path $parent | Out-Null
 $stage=Join-Path $parent ('.lume-install-'+[Guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $stage | Out-Null
 Add-Type -Path (Join-Path $PSScriptRoot 'package_core.cs')
 $payload=Join-Path $stage 'payload.zip'
 Write-Host 'Verifying the ready game package. No 3D generation is required.'
 [LumePackage]::Unseal([IO.Path]::GetFullPath($OriginalExe),[IO.Path]::GetFullPath($Package),$payload)
 Add-Type -AssemblyName System.IO.Compression
 $stream=[IO.File]::OpenRead($payload)
 $archive=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Read)
 try {
  $total=0L
  foreach($entry in $archive.Entries){
   $name=$entry.FullName
   if(!$name -or $name.Contains('\') -or $name.Contains(':') -or $name.StartsWith('/') -or ($name.Split('/') -contains '..')){throw 'Invalid package path.'}
   if($name -ne 'lume.3dsx' -and !$name.StartsWith('developer/')){throw 'Unexpected package entry.'}
   $total+=$entry.Length
   if($total -gt 1073741824L){throw 'Package exceeds the extraction size limit.'}
   $relative=if($name -eq 'lume.3dsx'){'3ds/lume/lume.3dsx'}else{$name}
   $destination=[IO.Path]::GetFullPath((Join-Path $stage $relative))
   if(!$destination.StartsWith($stage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Entry escapes the output directory.'}
   New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($destination)) | Out-Null
   $inputFile=$entry.Open();$outputFile=New-Object IO.FileStream($destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
   try {$inputFile.CopyTo($outputFile)} finally {$inputFile.Dispose();$outputFile.Dispose()}
  }
 } finally {$archive.Dispose();$stream.Dispose()}
 if(!(Test-Path -LiteralPath (Join-Path $stage '3ds/lume/lume.3dsx'))){throw 'Package does not contain the game.'}
 Remove-Item -LiteralPath $payload
 [IO.Directory]::Move($stage,$OutputDirectory);$stage=$null
 Write-Host ('READY: '+(Join-Path $OutputDirectory '3ds/lume/lume.3dsx'))
 Write-Host 'Copy the generated 3ds folder to the SD card root and start Lume from Homebrew Launcher.'
 exit 0
} catch {
 Write-Host ('ERROR: '+$_.Exception.Message)
 exit 1
} finally {
 if($stage -and (Test-Path -LiteralPath $stage)){
  $resolved=[IO.Path]::GetFullPath($stage)
  if([IO.Path]::GetDirectoryName($resolved) -eq $parent -and [IO.Path]::GetFileName($resolved).StartsWith('.lume-install-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
 }
}
