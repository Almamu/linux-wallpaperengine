#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "Please provide the current build's executable path. You might want to run this script off the same folder."
fi
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# ensure the output and images directory exists
[ -d "output" ] || mkdir output
[ -d "images" ] || mkdir images
python ${SCRIPT_DIR}/scripts/run.py $1 ~/.steam/steam/steamapps/workshop/content/431960/
python ${SCRIPT_DIR}/scripts/process.py