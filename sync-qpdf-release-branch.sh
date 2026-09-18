#!/usr/bin/env bash
# Usage: ./sync-qpdf-release-branch.sh 12.4   (fetches branch 12.4 from upstream qpdf and pushes it to the fork)
set -euo pipefail

BRANCH="${1:?Usage: $0 <release-branch, e.g. 12.4>}"
UPSTREAM_URL="https://github.com/qpdf/qpdf.git"

git fetch "$UPSTREAM_URL" "refs/heads/$BRANCH"
git push origin "FETCH_HEAD:refs/heads/$BRANCH"