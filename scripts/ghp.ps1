param(
    [Parameter(Position = 0)]
    [ValidateSet('patch', 'fix', 'feature', 'major', 'alpha', 'beta', 'rc', 'upcoming')]
    [string]$BumpType,   # no default anymore

    [switch]$y,
    [switch]$dry
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# 0. Default behavior: no bump type => just push, no release
# ---------------------------------------------------------------------------

if (-not $BumpType) {
    Write-Host "No bump type specified. Running 'git push' only (no release)." -ForegroundColor Yellow
    git push
    Write-Host "Done." -ForegroundColor Green
    exit 0
}

# ---------------------------------------------------------------------------
# 1. Get latest tag
# ---------------------------------------------------------------------------

$latestTag = git tag -l "v*" |
Sort-Object {
    [version](($_ -replace '^v', '') -replace '-.+$', '')
} -Descending |
Select-Object -First 1

if (-not $latestTag) {
    $latestTag = 'v0.0.0'
    Write-Host "No existing tags found, starting from v0.0.0"
}

# ---------------------------------------------------------------------------
# 2. Get commits since last tag
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
# 3. Classify commits, detect invalid ones, detect breaking changes
# ---------------------------------------------------------------------------

$features = @()
$fixes    = @()
$other    = @()
$invalid  = @()
$breaking = $false

foreach ($line in $commits) {
    if (-not $line) { continue }
    $parts = $line -split '\|'
    $msg   = $parts[0]
    $hash  = $parts[1]

    # Ignore merge commits
    if ($msg -like "Merge*") { continue }

    # BREAKING?
    if ($msg -match 'BREAKING CHANGE') { $breaking = $true }
    if ($msg -match '^feat!')          { $breaking = $true }
    if ($msg -match '!:')              { $breaking = $true }

    # Valid classifications
    if ($msg -match '^feat(\(.+\))?!?:\s*(.+)') {
        $features += [pscustomobject]@{ Text = $Matches[2]; Hash = $hash }
        continue
    }

    if ($msg -match '^fix(\(.+\))?!?:\s*(.+)') {
        $fixes += [pscustomobject]@{ Text = $Matches[2]; Hash = $hash }
        continue
    }

    if ($msg -match '^(docs|test|chore|ci|refactor|perf|security|build|deps)(\(.+\))?:\s*(.+)') {
        $other += [pscustomobject]@{ Text = $Matches[3]; Hash = $hash }
        continue
    }

    # Non-conforming commit → invalid
    $invalid += "$msg ($hash)"
}

# Fail release/log if any invalid commit exists
if ($invalid.Count -gt 0) {
    Write-Host "`nThe following commits do NOT use a valid conventional prefix:" -ForegroundColor Red
    $invalid | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }

    Write-Error @"
Aborting.

Every commit since $latestTag must start with one of:

  feat:, fix:, docs:, test:, chore:, ci:, refactor:, perf:, security:, build:, deps:

Fix these with:
  git rebase -i $latestTag
"@
    exit 1
}

# ---------------------------------------------------------------------------
# 4. SPECIAL MODE: BumpType = 'upcoming' → maintain '## [Upcoming]' section
# ---------------------------------------------------------------------------

if ($BumpType -eq 'upcoming') {
    Write-Host "`nUpdating CHANGELOG.md [Upcoming] section..." -ForegroundColor Cyan

    $changelogPath = "CHANGELOG.md"
    $existing = ""
    if (Test-Path $changelogPath) {
        $existing = Get-Content $changelogPath -Raw
    }

    if (-not $existing) {
        $existing = "# Changelog`n`n"
    }
    elseif ($existing -notmatch '^# Changelog') {
        $existing = "# Changelog`n`n" + $existing
    }

    # Build bullet lines for this run
    $featureLines = $features | ForEach-Object { "- $($_.Text) ($($_.Hash))" }
    $fixLines     = $fixes    | ForEach-Object { "- $($_.Text) ($($_.Hash))" }
    $otherLines   = $other    | ForEach-Object { "- $($_.Text) ($($_.Hash))" }

    # Find existing [Upcoming] block (if any)
    $upcomingHeader = "## [Upcoming]"
    $upcomingIndex  = $existing.IndexOf($upcomingHeader)

    if ($upcomingIndex -lt 0) {
        # No Upcoming section yet → create fresh one at top
        $upcomingBlock = "## [Upcoming]`n"

        if ($featureLines.Count -gt 0) {
            $upcomingBlock += "`n### Features`n" + ($featureLines -join "`n") + "`n"
        }
        if ($fixLines.Count -gt 0) {
            $upcomingBlock += "`n### Bug Fixes`n" + ($fixLines -join "`n") + "`n"
        }
        if ($otherLines.Count -gt 0) {
            $upcomingBlock += "`n### Other Changes`n" + ($otherLines -join "`n") + "`n"
        }

        $newContent = ($existing -replace '^# Changelog\s*', '# Changelog')  # normalize header
        # Inject Upcoming right after header
        $newContent = "# Changelog`n`n" + $upcomingBlock + "`n" + ($newContent -replace '^# Changelog\s*', '').TrimStart()
    }
    else {
        # Upcoming section exists → append only new commits (no duplicates)
        # Extract upcoming block
        $rest         = $existing.Substring($upcomingIndex)
        $nextHeaderIx = $rest.IndexOf("## [", 1)  # look for next section header after Upcoming
        if ($nextHeaderIx -gt 0) {
            $upcomingBlock = $rest.Substring(0, $nextHeaderIx)
            $afterUpcoming = $rest.Substring($nextHeaderIx)
        }
        else {
            $upcomingBlock = $rest
            $afterUpcoming = ""
        }

        # For deduplication, check by hash
        $updatedBlock = $upcomingBlock

        if ($featureLines.Count -gt 0) {
            if ($updatedBlock -notmatch "### Features") {
                $updatedBlock += "`n### Features`n"
            }
            foreach ($line in $featureLines) {
                $hash = ($line -split '\(')[-1].TrimEnd(')')
                if ($updatedBlock -notmatch [regex]::Escape("($hash)")) {
                    $updatedBlock += $line + "`n"
                }
            }
        }

        if ($fixLines.Count -gt 0) {
            if ($updatedBlock -notmatch "### Bug Fixes") {
                $updatedBlock += "`n### Bug Fixes`n"
            }
            foreach ($line in $fixLines) {
                $hash = ($line -split '\(')[-1].TrimEnd(')')
                if ($updatedBlock -notmatch [regex]::Escape("($hash)")) {
                    $updatedBlock += $line + "`n"
                }
            }
        }

        if ($otherLines.Count -gt 0) {
            if ($updatedBlock -notmatch "### Other Changes") {
                $updatedBlock += "`n### Other Changes`n"
            }
            foreach ($line in $otherLines) {
                $hash = ($line -split '\(')[-1].TrimEnd(')')
                if ($updatedBlock -notmatch [regex]::Escape("($hash)")) {
                    $updatedBlock += $line + "`n"
                }
            }
        }

        # Rebuild full changelog
        $beforeUpcoming = $existing.Substring(0, $upcomingIndex)
        $newContent = $beforeUpcoming + $updatedBlock.TrimEnd() + "`n`n" + $afterUpcoming.TrimStart()
    }

    Write-Host "`nPlanned [Upcoming] content:" -ForegroundColor Green
    Write-Host $newContent

    if ($dry) {
        Write-Host "`nDry run enabled — not writing CHANGELOG.md, not committing, not pushing." -ForegroundColor Yellow
        exit 0
    }

    if (-not $y) {
        $confirm = Read-Host "`nProceed with updating [Upcoming] in CHANGELOG.md and pushing? (y/n)"
        if ($confirm -ne 'y') {
            Write-Host "Aborted. No changes made."
            exit 0
        }
    }

    Set-Content $changelogPath $newContent -NoNewline
    Write-Host "`nUpdated CHANGELOG.md [Upcoming]." -ForegroundColor Green

    git add CHANGELOG.md
    git commit -m "chore: update upcoming changelog"
    git push

    Write-Host "`nUpcoming changelog update complete." -ForegroundColor Green
    exit 0
}

# ---------------------------------------------------------------------------
# 5. Release-mode version bump logic (patch/fix/feature/major/alpha/beta/rc)
# ---------------------------------------------------------------------------

# From here on, behavior is the same as before for real releases

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
        if ($preTag -eq 'alpha') { $preNum++ }
        else { $preTag = 'alpha'; $preNum = 1 }
    }

    'beta' {
        if ($preTag -eq 'beta') { $preNum++ }
        else { $preTag = 'beta'; $preNum = 1 }
    }

    'rc' {
        if ($preTag -eq 'rc') { $preNum++ }
        else { $preTag = 'rc'; $preNum = 1 }
    }
}

# BREAKING CHANGE always forces a major (unless prerelease)
if ($breaking -and $BumpType -notin @('alpha', 'beta', 'rc')) {
    Write-Host "`nBREAKING CHANGE detected → forcing major bump." -ForegroundColor Yellow
    $major++; $minor = 0; $patch = 0
    $preTag = $null; $preNum = 0
}

# Build final version string
if ($preTag) {
    $newVersion = "v$major.$minor.$patch-$preTag.$preNum"
}
else {
    $newVersion = "v$major.$minor.$patch"
}

Write-Host "`nNew Version → $newVersion (from $latestTag)" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# 6. Build CHANGELOG entry for release
# ---------------------------------------------------------------------------

$date = Get-Date -Format "yyyy-MM-dd"
$entry = "## [$newVersion] - $date`n"

if ($features.Count -gt 0) {
    $entry += "`n### Features`n"
    $entry += (($features | ForEach-Object { "- $($_.Text) ($($_.Hash))" }) -join "`n") + "`n"
}

if ($fixes.Count -gt 0) {
    $entry += "`n### Bug Fixes`n"
    $entry += (($fixes | ForEach-Object { "- $($_.Text) ($($_.Hash))" }) -join "`n") + "`n"
}

if ($other.Count -gt 0) {
    $entry += "`n### Other Changes`n"
    $entry += (($other | ForEach-Object { "- $($_.Text) ($($_.Hash))" }) -join "`n") + "`n"
}

Write-Host "`nChangelog entry:" -ForegroundColor Green
Write-Host $entry

# ---------------------------------------------------------------------------
# 7. Dry run mode (release)
# ---------------------------------------------------------------------------

if ($dry) {
    Write-Host "`nDry run enabled — no files, commits, or tags will be created." -ForegroundColor Yellow
    exit 0
}

# ---------------------------------------------------------------------------
# 8. Confirmation (release)
# ---------------------------------------------------------------------------

if (-not $y) {
    $confirm = Read-Host "`nProceed with release $newVersion? (y/n)"
    if ($confirm -ne 'y') {
        Write-Host "Aborted."
        exit 0
    }
}

# ---------------------------------------------------------------------------
# 9. Update CHANGELOG.md (release)
# ---------------------------------------------------------------------------

$changelog = "CHANGELOG.md"
if (Test-Path $changelog) {
    $existing = Get-Content $changelog -Raw
    $content  = "# Changelog`n`n" + $entry + "`n" + ($existing -replace '^# Changelog\s*', '')
} else {
    $content = "# Changelog`n`n" + $entry
}

Set-Content $changelog $content -NoNewline
Write-Host "`nUpdated CHANGELOG.md" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 10. Commit, create tag, push (release)
# ---------------------------------------------------------------------------

git add CHANGELOG.md
git commit -m "chore: release $newVersion"
git tag $newVersion

Write-Host "Tag created: $newVersion" -ForegroundColor Green

git push
git push --tags

Write-Host "`nRelease complete → $newVersion" -ForegroundColor Green
