[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0,
               ValueFromRemainingArguments = $true)]
    [ValidateNotNullOrEmpty()]
    [string[]]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$skillRoot = Split-Path -Parent $PSScriptRoot
$formatterPath = Join-Path $skillRoot 'clang-format.exe'
$stylePath = Join-Path $skillRoot '.clang-format'

if(-not (Test-Path -LiteralPath $formatterPath -PathType Leaf))
{
    throw "clang-format.exe was not found at: $formatterPath"
}
if(-not (Test-Path -LiteralPath $stylePath -PathType Leaf))
{
    throw "Format configuration was not found at: $stylePath"
}

# Validate every explicit input before changing any file. No directory scanning.
$resolvedFiles = foreach($item in $Path)
{
    $resolvedPath = (Resolve-Path -LiteralPath $item).Path
    $extension = [System.IO.Path]::GetExtension($resolvedPath).ToLowerInvariant()
    if(-not (Test-Path -LiteralPath $resolvedPath -PathType Leaf) -or
       ('.c' -ne $extension -and '.h' -ne $extension))
    {
        throw "Only explicit C/H files may be formatted: $item"
    }
    $resolvedPath
}

foreach($resolvedPath in $resolvedFiles)
{
    $originalText = [System.IO.File]::ReadAllText($resolvedPath)
    $header = [regex]::Match($originalText, '\A\s*/\*[\s\S]*?\*/')
    & $formatterPath -i "--style=file:$stylePath" --fallback-style=none $resolvedPath
    if(0 -ne $LASTEXITCODE)
    {
        throw "clang-format failed for: $resolvedPath"
    }
    # Preserve the project's approved file-header layout.
    if($header.Success)
    {
        $formattedText = [System.IO.File]::ReadAllText($resolvedPath)
        $formattedHeader = [regex]::Match($formattedText, '\A\s*/\*[\s\S]*?\*/')
        if($formattedHeader.Success -and $formattedHeader.Value -cne $header.Value)
        {
            $formattedText = $header.Value + $formattedText.Substring($formattedHeader.Length)
            [System.IO.File]::WriteAllText($resolvedPath, $formattedText,
                [System.Text.UTF8Encoding]::new($false))
        }
    }
    Write-Output "Formatted: $resolvedPath"
}
