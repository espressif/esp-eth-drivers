#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Utility to overwrite managed component override paths in IDF manifests."""

import argparse
import json
import os
import re
import subprocess
import sys

from pathlib import Path

import yaml

from packaging import specifiers
from packaging import version

RELEASE_COMPONENTS_MANIFEST = 'release-please-config.json'
ETH_CI_ENV_VARS_FILE = 'eth_ci_env.sh'


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            f'Search for idf_component.yml manifests and update override_path for '
            f'managed components defined in {RELEASE_COMPONENTS_MANIFEST}.'
        )
    )
    parser.add_argument(
        'components_root',
        type=Path,
        help='Directory where component directories are located.',
    )
    parser.add_argument(
        'search_root',
        type=Path,
        help='Directory where manifests should be searched recursively.',
    )
    return parser.parse_args()


def validate_inputs(components_root: Path, search_root: Path) -> None:
    if not components_root.exists():
        print(f'error: components root "{components_root}" does not exist', file=sys.stderr)
        raise SystemExit(1)

    if not search_root.exists():
        print(f'error: search root "{search_root}" does not exist', file=sys.stderr)
        raise SystemExit(1)


def find_workspace_root(components_root: Path) -> Path:
    """Find the workspace root by looking for release-please-config.json."""
    current = components_root.resolve()
    while current != current.parent:
        config_file = current / RELEASE_COMPONENTS_MANIFEST
        if config_file.exists():
            return current
        current = current.parent

    # If not found in parents, check if it's in components_root itself
    config_file = components_root / RELEASE_COMPONENTS_MANIFEST
    if config_file.exists():
        return components_root

    print(f'error: could not find {RELEASE_COMPONENTS_MANIFEST}', file=sys.stderr)
    raise SystemExit(1)


def read_release_components_config(workspace_root: Path, components_root: Path) -> dict[str, str]:
    """Read release-please-config.json and extract component names and paths."""
    config_file = workspace_root / RELEASE_COMPONENTS_MANIFEST

    try:
        with open(config_file) as f:
            config = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f'error: failed to read {config_file}: {e}', file=sys.stderr)
        raise SystemExit(1) from e

    packages = config.get('packages', {})
    if not packages:
        print(f'error: no packages found in {RELEASE_COMPONENTS_MANIFEST}', file=sys.stderr)
        raise SystemExit(1)

    # Map component name to local path
    components = {}
    for package_config in packages.values():
        if not isinstance(package_config, dict):
            continue
        component_name = package_config.get('component')
        if component_name:
            # Local path is relative to components root
            local_path = components_root / component_name
            components[component_name] = str(local_path)

    return components


def candidate_component_keys(component: str) -> list[str]:
    if '/' in component:
        return [component]
    return [component, f'espressif/{component}']


def find_manifests(root: Path) -> list[Path]:
    return [p for p in root.rglob('idf_component.yml') if p.is_file()]


def extract_app_name(manifest_path: Path) -> str:
    """Extract the app name from a manifest path.

    If the manifest is in a 'main' directory, return the parent directory name.
    Otherwise, return the direct parent directory name.

    Example: /path/to/static_ip/main/idf_component.yml -> 'static_ip'
    """
    parent = manifest_path.parent
    if parent.name == 'main':
        return parent.parent.name
    return parent.name


def create_dependency_entry(entry: dict | str | None, override_path: str) -> dict:
    """Create a dependency entry with the override_path and version set to '*'."""
    if isinstance(entry, dict):
        new_entry = dict(entry)
    else:
        new_entry = {}

    new_entry['override_path'] = override_path
    new_entry['version'] = '*'
    return new_entry


def update_manifest(
    manifest_path: Path, component_keys: list[str], override_path: str
) -> tuple[bool, bool]:
    data = yaml.safe_load(manifest_path.read_text())

    if not isinstance(data, dict):
        return False, False

    dependencies = data.get('dependencies')
    if not isinstance(dependencies, dict):
        return False, False

    found = False
    updated = False

    for key in component_keys:
        if key not in dependencies:
            continue

        found = True
        entry = dependencies[key]

        # Check if there is `path` entry, if so, we cannot overwrite the path since it's local component
        if isinstance(entry, dict) and entry.get('path') is not None:
            break

        # Check if already has the correct override_path
        if isinstance(entry, dict) and entry.get('override_path') == override_path:
            break

        new_entry = create_dependency_entry(entry, override_path)

        if new_entry != entry:
            dependencies[key] = new_entry
            updated = True
        # only update the first applicable key
        break

    if not found:
        return False, False

    if not updated:
        return True, False

    manifest_path.write_text(
        yaml.safe_dump(data, sort_keys=False, default_flow_style=False)
    )
    return True, True


def write_modified_components_env(changed_manifests: list[Path]) -> None:
    """Write environment variable file with modified components list.

    Args:
        changed_manifests: List of manifest paths that were modified.
    """
    if changed_manifests:
        app_names = []
        for manifest in changed_manifests:
            app_name = extract_app_name(manifest)
            if app_name not in app_names:
                app_names.append(app_name)

        # `esp_eth` is always a dependency of the build to ensure all Ethernet related apps are built.
        env_value = 'esp_eth;' + ';'.join(app_names)
    else:
        env_value = ';'

    with open(ETH_CI_ENV_VARS_FILE, 'a') as f:
        f.write(f'export IDF_COMPONENTS_MOD=\'{env_value}\'\n')

    print(f'Build modified components: "{env_value}"')
    print(f'Environment file written to: {ETH_CI_ENV_VARS_FILE}')


def get_idf_version() -> str:
    """Get ESP-IDF version from idf.py --version command.

    Returns:
        Version string (e.g., "5.0", "5.5.1") extracted from the full version output.
        Examples:
            "ESP-IDF v5.0-dirty" -> "5.0"
            "ESP-IDF v5.5.1-700-g4483e2628de" -> "5.5.1"

    Raises:
        SystemExit: If IDF_PATH is not set, idf.py is not found, or version cannot be determined.
    """
    # Use full path to execute an arbitrary executable by adjusting the PATH environment variable.
    idf_path_str = os.environ.get('IDF_PATH')
    if not idf_path_str:
        print('error: IDF_PATH environment variable is not set', file=sys.stderr)
        raise SystemExit(1)

    idf_path = Path(idf_path_str)
    if not idf_path.exists():
        print(f'error: IDF_PATH "{idf_path}" does not exist', file=sys.stderr)
        raise SystemExit(1)

    idf_py = idf_path / 'tools' / 'idf.py'
    if not idf_py.exists():
        print(f'error: idf.py not found at "{idf_py}"', file=sys.stderr)
        raise SystemExit(1)

    try:
        result = subprocess.run([str(idf_py), '--version'], capture_output=True, text=True, check=True)  # noqa: S603 known false positive (https://github.com/astral-sh/ruff/issues/4045)
        version_output = result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f'error: failed to run idf.py --version: {e}', file=sys.stderr)
        raise SystemExit(1) from e

    # Parse version from output like "ESP-IDF v5.5.1-700-g4483e2628de"
    # Extract version number after "v" and before any "-" or end of string
    match = re.search(r'v(\d+\.\d+(?:\.\d+)?)', version_output)
    if not match:
        print(f'error: could not parse version from: {version_output}', file=sys.stderr)
        raise SystemExit(1)

    return match.group(1)


def get_component_idf_requirement(component_path: Path) -> str | None:
    """Get IDF version requirement from component's idf_component.yml.

    Args:
        component_path: Path to the component directory.

    Returns:
        IDF version requirement string (e.g., ">=5.3") or None if not found.
    """
    manifest_path = component_path / 'idf_component.yml'
    if manifest_path.exists():
        try:
            data = yaml.safe_load(manifest_path.read_text())
        except (OSError, yaml.YAMLError) as e:
            print(f'Warning: failed to read {manifest_path}: {e}', file=sys.stderr)
            return None

        if not isinstance(data, dict):
            return None

        dependencies = data.get('dependencies')
        if not isinstance(dependencies, dict):
            return None

        idf_req = dependencies.get('idf')
        if isinstance(idf_req, str):
            return idf_req
        elif isinstance(idf_req, dict):
            version = idf_req.get('version')
            if isinstance(version, str):
                return version
    return None


def is_component_compatible(component_path: Path, idf_version: str) -> bool:
    """Check if component is compatible with the given IDF version.

    Args:
        component_path: Path to the component directory.
        idf_version: IDF version string (e.g., "5.3", "5.5.1").

    Returns:
        True if compatible or no requirement specified, False otherwise.
    """
    requirement = get_component_idf_requirement(component_path)

    # If no requirement specified, assume compatible
    if requirement is None:
        return True

    try:
        spec = specifiers.SpecifierSet(requirement)
        ver = version.Version(idf_version)
        return ver in spec
    except (specifiers.InvalidSpecifier, version.InvalidVersion) as e:
        print(
            f'Warning: invalid version specifier "{requirement}" or version "{idf_version}": {e}',
            file=sys.stderr,
        )
        # On parse error, assume compatible to avoid breaking builds
        return True


def main() -> int:
    args = parse_args()
    components_root = args.components_root.expanduser().resolve()
    search_root = args.search_root.expanduser().resolve()

    validate_inputs(components_root, search_root)

    # Find workspace root and read component configuration
    workspace_root = find_workspace_root(components_root)
    components = read_release_components_config(workspace_root, components_root)

    print(f'Found {len(components)} components in {RELEASE_COMPONENTS_MANIFEST}')
    print(f'Workspace root: {workspace_root}')
    print(f'Components root: {components_root}')
    print(f'Search root: {search_root}')

    # Get current IDF version
    idf_version = get_idf_version()
    print(f'Current IDF version: {idf_version}\n')

    # Find all manifests once
    manifests = find_manifests(search_root)
    print(f'Found {len(manifests)} manifest(s) to process\n')

    # Process each component
    total_changed = []
    skipped_incompatible = []
    for component_name, local_path in sorted(components.items()):
        # Validate that the component directory exists
        component_path = Path(local_path)
        if not component_path.exists():
            print(f'Warning: component path "{local_path}" does not exist, skipping {component_name}')
            continue

        # Check if component is compatible with current IDF version
        if not is_component_compatible(component_path, idf_version):
            requirement = get_component_idf_requirement(component_path)
            print(f'Skipping "{component_name}" (requires IDF {requirement}, current: {idf_version})')
            skipped_incompatible.append(component_name)
            continue

        component_keys = candidate_component_keys(component_name)
        changed_for_component = []

        for manifest in manifests:
            found, updated = update_manifest(manifest, component_keys, local_path)
            if found and updated:
                changed_for_component.append(manifest.resolve())

        if changed_for_component:
            print(f'Component "{component_name}":')
            for manifest in changed_for_component:
                print(f'  - {manifest}')
                total_changed.append(manifest)

    print(f'\nTotal manifests updated: {len(total_changed)}')
    if skipped_incompatible:
        print(f'Components skipped due to IDF version incompatibility: {len(skipped_incompatible)}')

    # Write environment variable file with modified components
    write_modified_components_env(total_changed)

    return 0


if __name__ == '__main__':
    sys.exit(main())
