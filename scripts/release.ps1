# release.ps1 - Simple release script
# Usage: .\scripts\release.ps1 [major|minor|patch]

param(
    [Parameter(Position=0)]
    [ValidateSet('major', 'minor', 'patch')]
    [string]$BumpType = 'patch'
)

$ErrorActionPreference = 'Stop'

# Get latest tag
$latestTag = git describe --tags --abbrev=0 2>$null
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
    'major' { $major++; $minor = 0; $patch = 0 }
    'minor' { $minor++; $patch = 0 }
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

# Update CHANGELOG.md
$changelogPath = "CHANGELOG.md"
if (Test-Path $changelogPath) {
    $existing = Get-Content $changelogPath -Raw
    # Insert after header
    if ($existing -match '^(# Changelog\s*)') {
        $content = $Matches[1] + "`n" + $entry + "`n" + $existing.Substring($Matches[0].Length)
    } else {
        $content = "# Changelog`n`n" + $entry + "`n" + $existing
    }
} else {
    $content = "# Changelog`n`n" + $entry
}

Set-Content $changelogPath $content -NoNewline
Write-Host "`nUpdated CHANGELOG.md" -ForegroundColor Green

# Confirm
Write-Host "`nReady to create tag $newVersion" -ForegroundColor Yellow
$confirm = Read-Host "Continue? (y/n)"
if ($confirm -ne 'y') {
    Write-Host "Aborted. CHANGELOG.md was updated but no tag created."
    exit 0
}

# Commit changelog and create tag
git add CHANGELOG.md
git commit -m "chore: release $newVersion"
git tag $newVersion

Write-Host "`nCreated tag $newVersion" -ForegroundColor Green
Write-Host "Push with: git push && git push --tags" -ForegroundColor Cyan
