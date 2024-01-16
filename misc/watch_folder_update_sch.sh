#!/bin/bash

# Root working directories
ROOT_DIR="/opt/ovenmediaengine/media"
MANIFEST_ROOT="/opt/ovenmediaengine/media"

# Check for arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <watch_folder_path_relative_to_root_dir> <name_of_sch_file_to_update>"
    exit 1
fi

WATCH_FOLDER=$1 # Can just be "/" to scan the root directory
SCH_FILE=$2
FOLDER_PATH="$ROOT_DIR/$WATCH_FOLDER"
XML_FILE="$MANIFEST_ROOT/$SCH_FILE.sch" # Path to the XML manifest file

# Function to update the XML file
update_xml() {
    # Remove existing <Item> entries from <Program>
    xmlstarlet ed -L -d "//Program/Item" $XML_FILE

    # Add sorted <Item> entries to <Program>
    for full_path in $(find "$FOLDER_PATH" -maxdepth 1 -type f \( -name "*.mp4" -o -name "*.mov" \) -printf "%T+ %p\n" | sort | cut -d " " -f 2-); do
        RELATIVE_PATH="${full_path#$ROOT_DIR/}"
        xmlstarlet ed -L -s "//Program" -t elem -n Item -v "" \
            -i "//Item[not(@url)]" -t attr -n url -v "file://$RELATIVE_PATH" \
            -i "//Item[@url='file://$RELATIVE_PATH']" -t attr -n start -v "0" \
            -i "//Item[@url='file://$RELATIVE_PATH']" -t attr -n duration -v "-1" $XML_FILE
    done
}

# Initial update
update_xml

# Watch for changes in the folder
while inotifywait -e create -e delete -e modify -e move -r "$FOLDER_PATH"; do
    update_xml
done
