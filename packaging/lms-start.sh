#!/bin/sh
# Starts the LMS from the data folder so the database and submission_files/ always land in the same place.
# Usage: lms-start.sh [data-folder]   (default: /srv/lms)
DATA_DIR="${1:-/srv/lms}"
APP="$(dirname "$0")/lms_app"
mkdir -p "$DATA_DIR" || exit 1
cd "$DATA_DIR" || exit 1
exec "$APP" "$DATA_DIR/school_lms.sqlite3"
