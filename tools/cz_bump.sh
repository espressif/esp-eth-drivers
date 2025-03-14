#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Please specify version for cz bump, for example:"
    echo "    ./tools/cz_bump.sh --increment=PATCH"
    exit 0
fi

git branch -D bump/new_version || true
git checkout -b bump/new_version

# bump new version: https://commitizen-tools.github.io/commitizen/commands/bump/
cz bump --no-verify $@

git push -u -f \
  -o merge_request.create \
  origin bump/new_version
