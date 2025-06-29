#!/bin/bash

# This file generates documentation for LHF and other tools that are present in
# this project.

set -o errexit
set -o nounset

DOXYGEN_COMMAND=doxygen
JSON_SCHEMA_DOC_COMMAND=generate-schema-doc

DOXYGEN_CONFIG_PATH="./doxygen.cfg"
DOC_GEN_FOLDER="./doc_generated"
JSON_SCHEMA_PATH="./src/builder_tool/schema.json"
JSON_SCHEMA_DOC_OUT_PATH="$DOC_GEN_FOLDER/schema"

if ! command -v "$DOXYGEN_COMMAND"; then
	echo "Error: doxygen not found. Please install doxygen on your system."
	exit 1
fi

if ! command -v "$JSON_SCHEMA_DOC_COMMAND"; then
	echo "Error: '$JSON_SCHEMA_DOC_COMMAND' not found. Please install 'json-schema-for-humans' on your system."
	exit 1
fi

if [ -d "$DOC_GEN_FOLDER" ]; then
	echo "'$DOC_GEN_FOLDER' already exists. Deleting."
	rm -rf "$DOC_GEN_FOLDER"
fi

mkdir -p "$DOC_GEN_FOLDER"
mkdir -p "$JSON_SCHEMA_DOC_OUT_PATH"

$DOXYGEN_COMMAND $DOXYGEN_CONFIG_PATH

$JSON_SCHEMA_DOC_COMMAND "$JSON_SCHEMA_PATH" "$JSON_SCHEMA_DOC_OUT_PATH/index.html" \
	--config template_name=js \
	--config footer_show_time=false \
	--config expand_buttons=true

echo "Done."