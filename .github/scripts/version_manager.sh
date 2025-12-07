#!/bin/bash
set -euo pipefail

# version-manager.sh - Automated versioning and changelog management
# Uses conventional commits to determine version bumps and manage changelog

CHANGELOG_FILE="CHANGELOG.md"
DEFAULT_VERSION="1.0.0"

# -----------------------------------------------------------------------------
# Helper Functions
# -----------------------------------------------------------------------------

log_info() { echo -e "\033[0;36m[INFO]\033[0m $1"; }
log_success() { echo -e "\033[0;32m[SUCCESS]\033[0m $1"; }
log_warn() { echo -e "\033[0;33m[WARN]\033[0m $1"; }
log_error() { echo -e "\033[0;31m[ERROR]\033[0m $1"; }

get_latest_tag() {
    git tag -l "v*" --sort=-v:refname | head -n1 || echo ""
}

parse_version() {
    local tag="$1"
    tag="${tag#v}"  # Remove 'v' prefix
    tag="${tag%%-*}" # Remove prerelease suffix
    echo "$tag"
}

# -----------------------------------------------------------------------------
# Analyze commit message for version bump type
# Returns: major, minor, patch, or none
# -----------------------------------------------------------------------------
analyze_commit() {
    local msg="$1"

    # Check for breaking changes (major bump)
    if [[ "$msg" =~ ^[a-z]+\!: ]] || [[ "$msg" =~ BREAKING[[:space:]]CHANGE ]]; then
        echo "major"
        return
    fi

    # Check for features (minor bump)
    if [[ "$msg" =~ ^feat(\(.+\))?: ]]; then
        echo "minor"
        return
    fi

    # Check for fixes (patch bump)
    if [[ "$msg" =~ ^fix(\(.+\))?: ]]; then
        echo "patch"
        return
    fi

    # Other conventional commits (no version bump, but add to changelog)
    if [[ "$msg" =~ ^(docs|test|chore|ci|refactor|perf|security|build|deps|style)(\(.+\))?: ]]; then
        echo "none"
        return
    fi

    # Non-conventional commit
    echo "skip"
}

# -----------------------------------------------------------------------------
# Calculate new version based on bump type
# -----------------------------------------------------------------------------
calculate_new_version() {
    local current="$1"
    local bump_type="$2"

    if [[ -z "$current" ]]; then
        current="$DEFAULT_VERSION"
    fi

    local version="${current#v}"
    version="${version%%-*}"  # Remove prerelease suffix

    IFS='.' read -r major minor patch <<< "$version"
    major=${major:-0}
    minor=${minor:-0}
    patch=${patch:-0}

    case "$bump_type" in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
    esac

    echo "v${major}.${minor}.${patch}"
}

# -----------------------------------------------------------------------------
# Classify a single commit and return formatted entry
# -----------------------------------------------------------------------------
classify_commit() {
    local msg="$1"
    local hash="$2"
    local category=""
    local description=""

    if [[ "$msg" =~ ^feat(\(.+\))?!?:[[:space:]]*(.+) ]]; then
        category="Features"
        description="${BASH_REMATCH[2]}"
    elif [[ "$msg" =~ ^fix(\(.+\))?!?:[[:space:]]*(.+) ]]; then
        category="Bug Fixes"
        description="${BASH_REMATCH[2]}"
    elif [[ "$msg" =~ ^docs(\(.+\))?:[[:space:]]*(.+) ]]; then
        category="Documentation"
        description="${BASH_REMATCH[2]}"
    elif [[ "$msg" =~ ^perf(\(.+\))?:[[:space:]]*(.+) ]]; then
        category="Performance"
        description="${BASH_REMATCH[2]}"
    elif [[ "$msg" =~ ^security(\(.+\))?:[[:space:]]*(.+) ]]; then
        category="Security"
        description="${BASH_REMATCH[2]}"
    elif [[ "$msg" =~ ^(chore|ci|refactor|test|build|deps|style)(\(.+\))?:[[:space:]]*(.+) ]]; then
        category="Other Changes"
        description="${BASH_REMATCH[3]}"
    else
        return 1  # Not a conventional commit
    fi

    echo "${category}|${description}|${hash}"
}

# -----------------------------------------------------------------------------
# Initialize changelog if it doesn't exist
# -----------------------------------------------------------------------------
init_changelog() {
    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        cat > "$CHANGELOG_FILE" << 'EOF'
# Changelog

## [Upcoming]

All notable changes to this project will be documented in this file.
EOF
        log_info "Created new $CHANGELOG_FILE"
    fi
}

# -----------------------------------------------------------------------------
# Ensure Upcoming section exists at top
# -----------------------------------------------------------------------------
ensure_upcoming_section() {
    if ! grep -q "## \[Upcoming\]" "$CHANGELOG_FILE" 2>/dev/null; then
        local content
        content=$(cat "$CHANGELOG_FILE")
        cat > "$CHANGELOG_FILE" << EOF
# Changelog

## [Upcoming]

${content#"# Changelog"}
EOF
        log_info "Added [Upcoming] section"
    fi
}

# -----------------------------------------------------------------------------
# Add entries to Upcoming section (deduplicated by hash)
# -----------------------------------------------------------------------------
add_to_upcoming() {
    local entries_file="$1"

    if [[ ! -s "$entries_file" ]]; then
        log_info "No new entries to add to Upcoming"
        return 0
    fi

    init_changelog
    ensure_upcoming_section

    local temp_file
    temp_file=$(mktemp)

    # Read current changelog
    local in_upcoming=false
    local upcoming_content=""
    local rest_content=""
    local found_upcoming=false

    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" == "## [Upcoming]"* ]]; then
            in_upcoming=true
            found_upcoming=true
            upcoming_content="## [Upcoming]"$'\n'
            continue
        fi

        if $in_upcoming && [[ "$line" =~ ^##[[:space:]]\[v?[0-9] ]]; then
            in_upcoming=false
            rest_content="$line"$'\n'
            continue
        fi

        if $in_upcoming; then
            upcoming_content+="$line"$'\n'
        elif $found_upcoming; then
            rest_content+="$line"$'\n'
        else
            rest_content+="$line"$'\n'
        fi
    done < "$CHANGELOG_FILE"

    # Group entries by category
    declare -A categories
    while IFS='|' read -r category description hash; do
        [[ -z "$category" ]] && continue

        # Check if this hash already exists in upcoming
        if ! grep -q "($hash)" <<< "$upcoming_content" 2>/dev/null; then
            categories["$category"]+="- ${description} (${hash})"$'\n'
        fi
    done < "$entries_file"

    # Build new upcoming section
    local new_upcoming="## [Upcoming]"$'\n'

    # Preserve existing content structure, add new entries
    for cat in "Features" "Bug Fixes" "Security" "Performance" "Documentation" "Other Changes"; do
        local existing_cat=""
        local new_cat="${categories[$cat]:-}"

        # Extract existing entries for this category from upcoming_content
        if [[ "$upcoming_content" =~ "### $cat" ]]; then
            existing_cat=$(echo "$upcoming_content" | sed -n "/### $cat/,/^###/p" | grep "^- " || true)
        fi

        if [[ -n "$existing_cat" ]] || [[ -n "$new_cat" ]]; then
            new_upcoming+=$'\n'"### $cat"$'\n'
            [[ -n "$existing_cat" ]] && new_upcoming+="$existing_cat"$'\n'
            [[ -n "$new_cat" ]] && new_upcoming+="$new_cat"
        fi
    done

    # Write new changelog
    echo "# Changelog" > "$temp_file"
    echo "" >> "$temp_file"
    echo "$new_upcoming" >> "$temp_file"
    echo "" >> "$temp_file"

    # Add rest of changelog (skip header if present)
    echo "$rest_content" | sed '/^# Changelog/d' >> "$temp_file"

    mv "$temp_file" "$CHANGELOG_FILE"
    log_success "Updated Upcoming section"
}

# -----------------------------------------------------------------------------
# Finalize Upcoming section into a versioned release
# -----------------------------------------------------------------------------
finalize_release() {
    local new_version="$1"
    local date
    date=$(date +%Y-%m-%d)

    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        log_error "No changelog file found"
        return 1
    fi

    local temp_file
    temp_file=$(mktemp)

    # Read and transform changelog
    local in_upcoming=false
    local upcoming_content=""
    local header_written=false

    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" == "## [Upcoming]"* ]]; then
            in_upcoming=true
            # Write header and new version section
            if ! $header_written; then
                echo "# Changelog" >> "$temp_file"
                echo "" >> "$temp_file"
                echo "## [Upcoming]" >> "$temp_file"
                echo "" >> "$temp_file"
                echo "## [${new_version}] - ${date}" >> "$temp_file"
                header_written=true
            fi
            continue
        fi

        if $in_upcoming && [[ "$line" =~ ^##[[:space:]]\[v?[0-9] ]]; then
            in_upcoming=false
            echo "" >> "$temp_file"
            echo "$line" >> "$temp_file"
            continue
        fi

        if $in_upcoming; then
            # Write upcoming content under new version
            [[ -n "$line" ]] && echo "$line" >> "$temp_file"
        elif $header_written; then
            echo "$line" >> "$temp_file"
        fi
    done < "$CHANGELOG_FILE"

    mv "$temp_file" "$CHANGELOG_FILE"
    log_success "Finalized release ${new_version}"
}

# -----------------------------------------------------------------------------
# Extract release notes for a specific version
# -----------------------------------------------------------------------------
extract_release_notes() {
    local version="$1"
    local in_version=false
    local notes=""

    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ ^##[[:space:]]\[${version}\] ]]; then
            in_version=true
            continue
        fi

        if $in_version && [[ "$line" =~ ^##[[:space:]]\[ ]]; then
            break
        fi

        if $in_version; then
            notes+="$line"$'\n'
        fi
    done < "$CHANGELOG_FILE"

    echo "$notes"
}

# -----------------------------------------------------------------------------
# Main workflow: Process push event
# -----------------------------------------------------------------------------
process_push() {
    local commit_msg="$1"
    local commit_hash="$2"

    log_info "Processing commit: $commit_hash"
    log_info "Message: $commit_msg"

    # Analyze commit for version bump
    local bump_type
    bump_type=$(analyze_commit "$commit_msg")

    log_info "Bump type: $bump_type"

    # Get current version
    local current_tag
    current_tag=$(get_latest_tag)
    log_info "Current tag: ${current_tag:-none}"

    # Create temp file for entries
    local entries_file
    entries_file=$(mktemp)

    # Classify the commit
    if classified=$(classify_commit "$commit_msg" "$commit_hash"); then
        echo "$classified" > "$entries_file"
    fi

    # Add to upcoming section
    add_to_upcoming "$entries_file"
    rm -f "$entries_file"

    # Determine if we should create a release
    if [[ "$bump_type" == "major" ]] || [[ "$bump_type" == "minor" ]] || [[ "$bump_type" == "patch" ]]; then
        local new_version
        new_version=$(calculate_new_version "$current_tag" "$bump_type")

        log_info "Creating release: $new_version"

        # Finalize the release
        finalize_release "$new_version"

        # Output for GitHub Actions
        echo "release=true" >> "${GITHUB_OUTPUT:-/dev/stdout}"
        echo "version=$new_version" >> "${GITHUB_OUTPUT:-/dev/stdout}"

        # Extract release notes
        local notes
        notes=$(extract_release_notes "$new_version")

        # Write release notes to file for GitHub Actions
        echo "$notes" > release_notes.md

        log_success "Release $new_version prepared"
    else
        log_info "No release triggered (bump_type=$bump_type)"
        echo "release=false" >> "${GITHUB_OUTPUT:-/dev/stdout}"
    fi
}

# -----------------------------------------------------------------------------
# Main workflow: Process multiple commits (for rebases/merges)
# -----------------------------------------------------------------------------
process_commits_since_tag() {
    local latest_tag
    latest_tag=$(get_latest_tag)

    local range="${latest_tag:+${latest_tag}..HEAD}"
    range="${range:-HEAD}"

    log_info "Processing commits in range: $range"

    local max_bump="none"
    local entries_file
    entries_file=$(mktemp)

    # Process each commit
    while IFS='|' read -r msg hash; do
        [[ -z "$msg" ]] && continue
        [[ "$msg" == Merge* ]] && continue

        local bump
        bump=$(analyze_commit "$msg")

        # Track highest bump type
        case "$bump" in
            major) max_bump="major" ;;
            minor) [[ "$max_bump" != "major" ]] && max_bump="minor" ;;
            patch) [[ "$max_bump" == "none" ]] && max_bump="patch" ;;
        esac

        # Classify and collect
        if classified=$(classify_commit "$msg" "$hash"); then
            echo "$classified" >> "$entries_file"
        fi
    done < <(git log "$range" --pretty=format:"%s|%h" --reverse 2>/dev/null || true)

    # Add all entries to upcoming
    add_to_upcoming "$entries_file"
    rm -f "$entries_file"

    # Create release if needed
    if [[ "$max_bump" != "none" ]] && [[ "$max_bump" != "skip" ]]; then
        local new_version
        new_version=$(calculate_new_version "$latest_tag" "$max_bump")

        log_info "Creating release: $new_version (bump: $max_bump)"
        finalize_release "$new_version"

        echo "release=true" >> "${GITHUB_OUTPUT:-/dev/stdout}"
        echo "version=$new_version" >> "${GITHUB_OUTPUT:-/dev/stdout}"

        local notes
        notes=$(extract_release_notes "$new_version")
        echo "$notes" > release_notes.md
    else
        echo "release=false" >> "${GITHUB_OUTPUT:-/dev/stdout}"
    fi
}

# -----------------------------------------------------------------------------
# CLI Interface
# -----------------------------------------------------------------------------
case "${1:-}" in
    process-push)
        process_push "${2:-}" "${3:-}"
        ;;
    process-range)
        process_commits_since_tag
        ;;
    analyze)
        analyze_commit "${2:-}"
        ;;
    version)
        calculate_new_version "${2:-}" "${3:-}"
        ;;
    extract-notes)
        extract_release_notes "${2:-}"
        ;;
    init)
        init_changelog
        ensure_upcoming_section
        ;;
    *)
        echo "Usage: $0 {process-push|process-range|analyze|version|extract-notes|init}"
        echo ""
        echo "Commands:"
        echo "  process-push <msg> <hash>  Process a single commit"
        echo "  process-range              Process all commits since last tag"
        echo "  analyze <msg>              Analyze commit message for bump type"
        echo "  version <current> <bump>   Calculate new version"
        echo "  extract-notes <version>    Extract release notes for version"
        echo "  init                       Initialize changelog"
        exit 1
        ;;
esac
