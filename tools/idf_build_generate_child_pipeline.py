#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Script to dynamically generate GitLab CI child pipeline based on the number of apps found.
"""

import argparse
import subprocess
import sys

from jinja2 import Template

# TODO check why precommit passes even when vars don't have types defined

# Jinja2 template for child pipeline
CHILD_PIPELINE_TEMPLATE = """
workflow:
  name: "Build IDF Child Pipeline for {{ idf_name }}"
  rules:
    - when: "always"

stages:
  - build_idf_apps

{%- if total_nodes == 0 %}
no_apps_found:
  stage: build_idf_apps
  script:
    - echo "No apps found to build for {{ idf_image }}"
{%- else %}
.default_build_settings:
  stage: build_idf_apps
  tags:
    - build
  timeout: 2h
  artifacts:
    paths:
      - build_artifacts/
    expire_in: 1 week
    when: always
  before_script:
    - pip install --upgrade idf-build-apps
  script:
    - python tools/overwrite_component_path.py . ${IDF_PATH}/examples/
    - source eth_ci_env.sh
    - cd ${IDF_PATH}
    - >
      idf-build-apps build
      --config-file ${CI_PROJECT_DIR}/.ci/esp_idf_build_apps.toml
      --modified-components ${IDF_COMPONENTS_MOD}
      --parallel-count ${CI_NODE_TOTAL}
      --parallel-index ${CI_NODE_INDEX}
  after_script:
    - |
      # Copy build artifacts back to project directory to be able to upload them as artifacts
      mkdir -p ${CI_PROJECT_DIR}/build_artifacts
      cd ${IDF_PATH}
      find . -type f \\( \\
        -path "*/build*/config/sdkconfig.json" \\
        -o -path "*/build*/project_description.json" \\
        -o -path "*/build*/build_log.txt" \\
        -o -name "build_summary_*.xml" \\
        -o -name "app_info_*.txt" \\
      \\) | while read file; do
        target_dir="${CI_PROJECT_DIR}/build_artifacts/$(dirname ${file})"
        mkdir -p "${target_dir}"
        cp "${file}" "${target_dir}/"
      done

build_idf_apps_{{ idf_name }}:
  extends: .default_build_settings
  image: {{ idf_image }}
  allow_failure: true
  parallel:
    matrix:
      - CI_NODE_INDEX:
{%- for node in node_indices %}
        - {{ node }}
{%- endfor %}
  variables:
    CI_NODE_TOTAL: {{ total_nodes }}
{%- endif %}
"""


def run_idf_build_apps_find(config_file: str, modified_components: str, log_file: str) -> int:
    """Run idf-build-apps find and capture output."""
    cmd = ['idf-build-apps', 'find', '--config-file', config_file, '--modified-components', modified_components, '-o', log_file]

    print(f'Running: {" ".join(cmd)}')
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)  # noqa: S603 known false positive (https://github.com/astral-sh/ruff/issues/4045)

    return result.returncode


def get_app_count(log_file: str) -> int:
    """Get the number of apps from idf-build-apps output."""
    try:
        with open(log_file) as f:
            # Optimized way to count lines without loading entire file into memory
            return sum(1 for _ in f)
    except FileNotFoundError:
        # If the file is not found, it means something went wrong or no apps were found if the file wasn't created.
        # Assuming idf-build-apps creates the file even if empty if successful.
        print(f'Warning: Log file {log_file} not found', file=sys.stderr)
        return 0


def calculate_parallel_nodes(total_apps: int, max_apps_per_node: int) -> int:
    """Calculate the number of parallel nodes needed."""
    if total_apps == 0:
        return 0
    return (total_apps + max_apps_per_node - 1) // max_apps_per_node


def generate_child_pipeline(idf_image: str, total_nodes: int, output_file: str) -> None:
    """Generate the child pipeline YAML configuration for a specific IDF image."""

    # Create the node indices list
    node_indices = list(range(1, total_nodes + 1)) if total_nodes > 0 else []

    # Get the IDF version/tag from the image name (last part after :)
    try:
        idf_name = idf_image.rsplit(':', 1)[1]
    except IndexError:
        idf_name = 'unknown'

    # Render template
    template = Template(CHILD_PIPELINE_TEMPLATE)
    rendered = template.render(idf_image=idf_image, idf_name=idf_name, total_nodes=total_nodes, node_indices=node_indices)

    # Write to file
    with open(output_file, 'w') as f:
        f.write(rendered)

    if total_nodes == 0:
        print(f'No apps found for "{idf_image}", generating empty pipeline')
    else:
        print(f'Generated child pipeline for "{idf_image}" with {total_nodes} nodes')
        print(f'Total jobs: {total_nodes}')
    print(f'Child pipeline written to: {output_file}')


def main() -> int:
    parser = argparse.ArgumentParser(description='Generate GitLab CI child pipeline for building IDF apps')
    parser.add_argument('--config-file', required=True, help='Path to idf-build-apps config file')
    parser.add_argument('--modified-components', required=True, help='Modified components to build')
    parser.add_argument('--idf-image', required=True, help='IDF Docker image to build for')
    parser.add_argument('--max-apps-per-node', type=int, default=25, help='Maximum number of apps to build per node (default: 25)')
    parser.add_argument('--output', default='child_pipeline.yml', help='Output file for child pipeline (default: child_pipeline.yml)')

    args = parser.parse_args()

    # Generate log file name based on image
    idf_image = args.idf_image
    # Create a safe filename from the image name (e.g., espressif/idf:latest -> espressif-idf-latest)
    safe_image_name = idf_image.replace('/', '-').replace(':', '-')
    log_file = f'find_log_{safe_image_name}.txt'

    print(f'IDF Image: "{idf_image}"')
    print(f'Log file: {log_file}')

    # Run idf-build-apps find
    returncode = run_idf_build_apps_find(args.config_file, args.modified_components, log_file)

    if returncode != 0:
        print(f'Error: idf-build-apps find failed with return code {returncode}', file=sys.stderr)
        sys.exit(1)

    # Parse app count
    total_apps = get_app_count(log_file)
    # No check for None as it now returns int

    print(f'Total apps found for "{idf_image}": {total_apps}')

    # Calculate parallel nodes
    total_nodes = calculate_parallel_nodes(total_apps, args.max_apps_per_node)
    print(f'Parallel nodes needed: {total_nodes} (max {args.max_apps_per_node} apps per node)')

    # Generate child pipeline
    generate_child_pipeline(idf_image, total_nodes, args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())
