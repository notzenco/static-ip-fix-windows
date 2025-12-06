param(
    [Parameter(Position = 0)]
    [ValidateSet('patch', 'fix', 'feature', 'major', 'alpha', 'beta', 'rc')]
    [string]$BumpType = 'patch',

    [switch]$y,
    [switch]$dry
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# 1. Get latest tag (vX.Y.Z or vX.Y.Z-<pre>.<n>)
# ---------------------------------------------------------------------------

$latestTag = git tag -l "v*" |
Sort-Object {
    # Strip leading 'v' and any '-pre.*' suffix for numeric sorting
    [version](($_ -replace '^v', '') -replace '-.+$', '')
} -Descending |
Select-Object -First 1

if (-not $latestTag) {
    $latestTag = 'v0.0.0'
    Write-Host "No existing tags found, starting from v0.0.0"
}

# ---------------------------------------------------------------------------
# 2. Parse version + optional prerelease
# ---------------------------------------------------------------------------

if ($latestTag -match '^v(\d+)\.(\d+)\.(\d+)(?:-([a-z]+)\.(\d+))?$') {
    $major = [int]$Matches[1]
    $minor = [int]$Matches[2]
    $patch = [int]$Matches[3]
    $preTag = $Matches[4]
    $preNum = [int]($Matches[5] | ForEach-Object { if ($_) { $_ } else { 0 } })
}
else {
    Write-Error "Could not parse version from tag: $latestTag"
    exit 1
}

# ---------------------------------------------------------------------------
# 3. Collect commits since last tag
# ---------------------------------------------------------------------------

if ($latestTag -eq 'v0.0.0') {
    $commits = git log --pretty=format:"%s|%h" --reverse
}
else {
    $commits = git log "$latestTag..HEAD" --pretty=format:"%s|%h" --reverse
}

if (-not $commits) {
    Write-Host "No commits since $latestTag" -ForegroundColor Yellow
    exit 0
}

# ---------------------------------------------------------------------------
# 4. Classify commits, enforce prefixes, detect breaking changes
# ---------------------------------------------------------------------------

$features = @()
$fixes = @()
$other = @()
$invalid = @()
$breaking = $false

foreach ($line in $commits) {
    if (-not $line) { continue }

    $parts = $line -split '\|'
    $msg = $parts[0]
    $hash = $parts[1]

    # Optional: skip merge commits from enforcement
    if ($msg -like 'Merge*') {
        continue
    }

    # Breaking-change detection
    if ($msg -match 'BREAKING CHANGE') { $breaking = $true }
    if ($msg -match '^feat!') { $breaking = $true }
    if ($msg -match '!:') { $breaking = $true }

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
        $invalid += "$msg ($hash)"
    }
}

if ($invalid.Count -gt 0) {
    Write-Host "`nThe following commits do not use a valid conventional prefix:" -ForegroundColor Red
    foreach ($c in $invalid) {
        Write-Host "  - $c" -ForegroundColor Red
    }

    Write-Error @"
Aborting release.

All commit messages since $latestTag must start with one of:
  feat:, fix:, docs:, test:, chore:, ci:, refactor:, perf:, security:, build:, deps:

Reword those commits (git rebase -i / amend) or exclude them from this release.
"@
    exit 1
}

# ---------------------------------------------------------------------------
# 5. Apply bump logic (major/minor/patch + alpha/beta/rc)
# ---------------------------------------------------------------------------

switch ($BumpType) {
    'major' {
        $major++; $minor = 0; $patch = 0
        $preTag = $null; $preNum = 0
    }

    'feature' {
        $minor++; $patch = 0
        $preTag = $null; $preNum = 0
    }

    'fix' {
        $patch++
        $preTag = $null; $preNum = 0
    }

    'patch' {
        $patch++
        $preTag = $null; $preNum = 0
    }

    'alpha' {
        if ($preTag -eq 'alpha') {
            $preNum++
        }
        else {
            $preTag = 'alpha'
            $preNum = 1
        }
    }

    'beta' {
        if ($preTag -eq 'beta') {
            $preNum++
        }
        else {
            $preTag = 'beta'
            $preNum = 1
        }
    }

    'rc' {
        if ($preTag -eq 'rc') {
            $preNum++
        }
        else {
            $preTag = 'rc'
            $preNum = 1
        }
    }
}

# Breaking override for non-pre-release bumps
if ($breaking -and $BumpType -notin @('alpha', 'beta', 'rc')) {
    Write-Host "`nBREAKING CHANGE detected -> forcing major bump" -ForegroundColor Yellow
    $major++; $minor = 0; $patch = 0
    $preTag = $null; $preNum = 0
}

# Build version string
if ($preTag) {
    $newVersion = "v$major.$minor.$patch-$preTag.$preNum"
}
else {
    $newVersion = "v$major.$minor.$patch"
}

Write-Host "`nNew version: $newVersion (from $latestTag)" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# 6. Build changelog entry
# ---------------------------------------------------------------------------

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

Write-Host "`nChangelog entry:`n" -ForegroundColor Green
Write-Host $entry

# ---------------------------------------------------------------------------
# 7. Dry-run support
# ---------------------------------------------------------------------------

if ($dry) {
    Write-Host "`nDry run: no files written, no commits, no tags." -ForegroundColor Yellow
    exit 0
}

# ---------------------------------------------------------------------------
# 8. Confirm before making changes
# ---------------------------------------------------------------------------

if (-not $y) {
    Write-Host "`nReady to release $newVersion" -ForegroundColor Yellow
    $confirm = Read-Host "Continue? (y/n)"
    if ($confirm -ne 'y') {
        Write-Host "Aborted. No changes made."
        exit 0
    }
}

# ---------------------------------------------------------------------------
# 9. Update CHANGELOG.md
# ---------------------------------------------------------------------------

$changelogPath = "CHANGELOG.md"
if (Test-Path $changelogPath) {
    $existing = Get-Content $changelogPath -Raw
    $content = "# Changelog`n`n" + $entry + "`n" + ($existing -replace '^# Changelog\s*', '')
}
else {
    $content = "# Changelog`n`n" + $entry
}

Set-Content $changelogPath $content -NoNewline
Write-Host "`nUpdated CHANGELOG.md" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 10. Commit, tag, push
# ---------------------------------------------------------------------------

git add CHANGELOG.md
git commit -m "chore: release $newVersion"
git tag $newVersion

Write-Host "Created tag $newVersion" -ForegroundColor Green

Write-Host "Pushing to origin..." -ForegroundColor Cyan
git push
git push --tags

Write-Host "`nReleased $newVersion" -ForegroundColor Green
