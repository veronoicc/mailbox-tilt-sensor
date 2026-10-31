#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Publish web/ directory to the gh-pages branch
# ==============================================================================

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

echo "Creating gh-pages deployment from web/..."

# Stage web files into a temporary directory
TEMP_DIR="$(mktemp -d)"
cp -r web/* "$TEMP_DIR/"

# Switch to or create orphan gh-pages branch
CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if git show-ref --quiet refs/heads/gh-pages; then
  git checkout gh-pages
  git rm -rf . > /dev/null 2>&1 || true
else
  git checkout --orphan gh-pages
  git rm -rf . > /dev/null 2>&1 || true
fi

# Copy web files to root of gh-pages
cp -r "$TEMP_DIR"/* .
rm -rf "$TEMP_DIR"

# Commit changes
git add .
git commit -m "Deploy WebBluetooth console to GitHub Pages"

echo ""
echo "gh-pages branch successfully generated!"
echo "To publish online:"
echo "  git push -u origin gh-pages --force"
echo ""
echo "Switching back to $CURRENT_BRANCH..."
git checkout "$CURRENT_BRANCH"
