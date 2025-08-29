#!/bin/bash

REPO=kostaond/esp-eth-drivers

if [ -z "${GITHUB_TOKEN:-}" ]; then
  echo "Error: Please export your GitHub token to the GITHUB_TOKEN environment variable first." >&2
  exit 1
fi

CURRENT_DIR="$(pwd)"
ROOT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")/..")
REL_PATH=$(realpath --relative-to="$ROOT_DIR" "$CURRENT_DIR")

if [[ "$REL_PATH" == "." ]]; then
  echo "You are at the repository root directory!"
  echo " When run from the repository root, the script will bump the version of ALL components with pending changes."
  echo
  read -p "Are you sure you want to continue? (y/N): " confirm
  if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
    echo "Abort ..."
    exit 0
  fi
  RP_PATH=""
  echo "Version bump of all components."
else
  RP_PATH="--path=${REL_PATH}"
  echo "Version bump of \"${REL_PATH}\" component."
fi

release-please release-pr --repo-url=$REPO --token=$GITHUB_TOKEN $RP_PATH $@
