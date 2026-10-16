<#
.SYNOPSIS
    Prints a repo tree (like cmd's "tree /F") with an exclude list.

.DESCRIPTION
    Why not the built-in tree.exe: it cannot exclude directories, and its output
    turns into mojibake once redirected to a file (console code page). This one
    excludes by name, prints UTF-8 box-drawing characters, and can emit plain
    ASCII when the consumer is not UTF-8 aware.

    Keep this file ASCII-only, BOM-less and CRLF: Windows PowerShell 5.1 reads a
    BOM-less script as the ANSI code page, and a multi-byte comment character can
    then swallow the following newline (the failure looks like a random parse
    error on an unrelated line).

    scripts/tree.bat is the repo wrapper: fixed exclude list, output to
    scripts/info.txt, scan root pinned to the repository.

.PARAMETER Path
    Root to scan. Defaults to the current directory.

.PARAMETER Exclude
    Names (files or folders) to skip, matched exactly.

.PARAMETER Depth
    Maximum recursion depth; 0 means unlimited.

.PARAMETER ShowFiles
    Include files, not just directories (like "tree /F").

.PARAMETER Ascii
    Use +-- / \-- / | instead of box-drawing characters.

.EXAMPLE
    .\tree.ps1 -Exclude "node_modules",".git","build" -ShowFiles

.EXAMPLE
    .\tree.ps1 -Depth 2

.EXAMPLE
    .\tree.ps1 -ShowFiles | Out-File -Encoding utf8 info.txt
#>

[CmdletBinding()]
param(
    [string]$Path = ".",
    [string[]]$Exclude = @(),
    [int]$Depth = 0,
    [switch]$ShowFiles,
    [switch]$Ascii
)

# Force UTF-8 (no BOM) regardless of the console code page, otherwise redirected
# output comes back as "??????".
$utf8 = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'
$PSDefaultParameterValues['*:Encoding'] = 'utf8'

# Branch characters. Built from code points to keep this file ASCII-only.
if ($Ascii) {
    $tBranch = '+-- '
    $tLast   = '\-- '
    $tPipe   = '|   '
    $tSpace  = '    '
} else {
    $tBranch = [string][char]0x251C + [char]0x2500 + [char]0x2500 + ' '
    $tLast   = [string][char]0x2514 + [char]0x2500 + [char]0x2500 + ' '
    $tPipe   = [string][char]0x2502 + '   '
    $tSpace  = '    '
}

function Write-Tree {
    param(
        [string]$CurrentPath,
        [string]$Prefix = "",
        [int]$CurrentLevel = 0
    )

    try {
        $items = Get-ChildItem -LiteralPath $CurrentPath -Force -ErrorAction Stop
    } catch {
        Write-Warning "Cannot read: $CurrentPath"
        return
    }

    if ($Exclude.Count -gt 0) {
        $items = @($items | Where-Object { $_.Name -notin $Exclude })
    }

    # @() keeps these arrays even when empty; a bare $null would print branches
    # with no name attached.
    $folders = @($items | Where-Object { $_.PSIsContainer } | Sort-Object Name)
    $files   = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object Name)

    if ($ShowFiles) {
        $allItems = @($folders) + @($files)
    } else {
        $allItems = @($folders)
    }

    for ($i = 0; $i -lt $allItems.Count; $i++) {
        $item   = $allItems[$i]
        $isLast = ($i -eq $allItems.Count - 1)
        $connector = if ($isLast) { $tLast } else { $tBranch }

        Write-Output ([string]::Concat($Prefix, $connector, $item.Name))

        if ($item.PSIsContainer -and ($Depth -eq 0 -or $CurrentLevel -lt $Depth)) {
            $newPrefix = if ($isLast) { $Prefix + $tSpace } else { $Prefix + $tPipe }
            Write-Tree -CurrentPath $item.FullName -Prefix $newPrefix `
                       -CurrentLevel ($CurrentLevel + 1)
        }
    }
}

Write-Output ("Directory listing for: " + $Path)
Write-Output ("Excluded: " + ($Exclude -join ', '))
if ($Depth -gt 0) { Write-Output ("Max Depth: " + $Depth) }
Write-Output ""

$resolved = (Resolve-Path -LiteralPath $Path).ProviderPath
$rootName = Split-Path $resolved -Leaf
if ([string]::IsNullOrEmpty($rootName)) { $rootName = $resolved }

Write-Output $rootName
Write-Tree -CurrentPath $resolved -Prefix "" -CurrentLevel 0
