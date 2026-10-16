[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0,
               ValueFromRemainingArguments = $true)]
    [ValidateNotNullOrEmpty()]
    [string[]]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath '..\..'))
$formatterPath = Join-Path -Path $projectRoot -ChildPath 'clang-format.exe'
$allowedRoots = @(
    '00_Project_Config',
    '01_App',
    '02_Service',
    '03_Platform',
    '04_Impl',
    '06_Component'
)

if(-not (Test-Path -LiteralPath $formatterPath -PathType Leaf))
{
    throw "clang-format.exe was not found at: $formatterPath"
}

foreach($item in $Path)
{
    $resolvedPath = (Resolve-Path -LiteralPath $item).Path
    $extension = [System.IO.Path]::GetExtension($resolvedPath).ToLowerInvariant()
    $relativePath = [System.IO.Path]::GetRelativePath($projectRoot,
                                                       $resolvedPath)
    $topDirectory = $relativePath.Split(
        [System.IO.Path]::DirectorySeparatorChar)[0]

    if('.c' -ne $extension -and '.h' -ne $extension)
    {
        throw "Only C/H files may be formatted: $relativePath"
    }
    if($relativePath.StartsWith('..'))
    {
        throw "The file is outside the project: $resolvedPath"
    }
    if($allowedRoots -notcontains $topDirectory)
    {
        throw "The file is outside the self-developed code roots: $relativePath"
    }

    & $formatterPath -i --style=file --fallback-style=none $resolvedPath
    if(0 -ne $LASTEXITCODE)
    {
        throw "clang-format failed for: $relativePath"
    }
    Write-Output "Formatted: $relativePath"
}
