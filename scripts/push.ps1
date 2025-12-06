# push.ps1 - Release script
# Usage: .\scripts\push.ps1 [patch|fix|feature] [-y]

param(
    [Parameter(Position=0)]
    [ValidateSet('patch', 'fix', 'feature')]
    [string]$BumpType = 'patch',
    [switch]$y
)

$ErrorActionPreference = 'Stop'

# Get latest tag
$latestTag = git tag -l "v*" | Sort-Object { [version]($_ -replace '^v', '') } -Descending | Select-Object -First 1
if (-not $latestTag) {
    $latestTag = 'v0.0.0'
    Write-Host "No existing tags found, starting from v0.0.0"
}

# Parse version
if ($latestTag -match 'v?(\d+)\.(\d+)\.(\d+)') {
    $major = [int]$Matches[1]
    $minor = [int]$Matches[2]
    $patch = [int]$Matches[3]
} else {
    Write-Error "Could not parse version from tag: $latestTag"
    exit 1
}

# Bump version
switch ($BumpType) {
    'feature' { $minor++; $patch = 0 }
    'fix' { $patch++ }
    'patch' { $patch++ }
}

$newVersion = "v$major.$minor.$patch"
Write-Host "`nBumping: $latestTag -> $newVersion ($BumpType)`n" -ForegroundColor Cyan

# Get commits since last tag
if ($latestTag -eq 'v0.0.0') {
    $commits = git log --pretty=format:"%s|%h" --reverse
} else {
    $commits = git log "$latestTag..HEAD" --pretty=format:"%s|%h" --reverse
}

if (-not $commits) {
    Write-Host "No commits since $latestTag" -ForegroundColor Yellow
    exit 0
}

# Categorize commits
$features = @()
$fixes = @()
$other = @()

foreach ($line in $commits) {
    if (-not $line) { continue }
    $parts = $line -split '\|'
    $msg = $parts[0]
    $hash = $parts[1]

    if ($msg -match '^feat(\(.+\))?!?:\s*(.+)') {
        $features += "- $($Matches[2]) ($hash)"
    }
    elseif ($msg -match '^fix(\(.+\))?!?:\s*(.+)') {
        $fixes += "- $($Matches[2]) ($hash)"
    }
    elseif ($msg -match '^(docs|test|chore|ci|refactor|perf|security|build|deps)(\(.+\))?:\s*(.+)') {
        $other += "- $($Matches[3]) ($hash)"
    }
    else {
        $other += "- $msg ($hash)"
    }
}

# Build changelog entry
$date = Get-Date -Format "yyyy-MM-dd"
$entry = "## [$newVersion] - $date`n"

if ($features.Count -gt 0) {
    $entry += "`n### Features`n"
    $entry += ($features -join "`n") + "`n"
}

if ($fixes.Count -gt 0) {
    $entry += "`n### Bug Fixes`n"
    $entry += ($fixes -join "`n") + "`n"
}

if ($other.Count -gt 0) {
    $entry += "`n### Other Changes`n"
    $entry += ($other -join "`n") + "`n"
}

Write-Host "Changelog entry:" -ForegroundColor Green
Write-Host $entry

# Confirm before making any changes
if (-not $y) {
    Write-Host "`nReady to release $newVersion" -ForegroundColor Yellow
    $confirm = Read-Host "Continue? (y/n)"
    if ($confirm -ne 'y') {
        Write-Host "Aborted. No changes made."
        exit 0
    }
}

# Update CHANGELOG.md
$changelogPath = "CHANGELOG.md"
if (Test-Path $changelogPath) {
    $existing = Get-Content $changelogPath -Raw
    if ($existing -match '^(# Changelog\s*\r?\n)') {
        $content = $Matches[1] + "`n" + $entry + "`n" + $existing.Substring($Matches[0].Length)
    } else {
        $content = "# Changelog`n`n" + $entry + "`n" + $existing
    }
} else {
    $content = "# Changelog`n`n" + $entry
}

Set-Content $changelogPath $content -NoNewline
Write-Host "`nUpdated CHANGELOG.md" -ForegroundColor Green

# Commit changelog and create tag
git add CHANGELOG.md
git commit -m "chore: release $newVersion"
git tag $newVersion

Write-Host "`nCreated tag $newVersion" -ForegroundColor Green

# Push everything
Write-Host "Pushing to origin..." -ForegroundColor Cyan
git push
git push --tags

Write-Host "`nReleased $newVersion" -ForegroundColor Green
