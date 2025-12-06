# Changelog

## [Upcoming]

### Bug Fixes
- move confirmation before changelog update, add -y flag (0e34a03)

### Other Changes
- rename to push.ps1, only run CI on tags and PRs (66bfb6e)
- cleanup (68a1fa4)
- ghp.ps1 (6ece37a)

All notable changes to this project will be documented in this file.

## [v1.0.0] - 2025-12-07

### Added
- Windows command-line tool for static IP configuration
- DNS-over-HTTPS (DoH) support with Cloudflare and Google
- DNS-only mode for quick DoH setup
- Configuration file support (INI format)
- Interface auto-detection
- Automatic rollback on failure
- Unit tests for utility functions
- Release script for automated versioning
- GitHub Actions CI/CD workflow
