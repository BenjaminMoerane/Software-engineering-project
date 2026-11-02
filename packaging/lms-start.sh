#!/bin/sh
# Starts the LMS from the data folder so the database and submission_files/ always land in the same place.
# Usage: lms-start.sh [data-folder]   (default: /srv/lms)
# F14: resolve the program's folder to an absolute path BEFORE changing directory, so the launcher also works when
# it is started by a relative path (e.g. bin/lms-start.sh).
APP_DIR="$(cd "$(dirname "$0")" && pwd)" || exit 1
DATA_DIR="${1:-/srv/lms}"
mkdir -p "$DATA_DIR" || exit 1
cd "$DATA_DIR" || exit 1
exec "$APP_DIR/lms_app" "$(pwd)/school_lms.sqlite3"
