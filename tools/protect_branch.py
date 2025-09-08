#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys

import requests

# Configuration
GITLAB_URL = os.environ['GITLAB_URL']
GITLAB_TOKEN = os.environ['GITLAB_TOKEN']
DEFAULT_GITLAB_NAMESPACE = 'espressif'
GITLAB_NAMESPACE = os.environ.get('GITLAB_NAMESPACE', DEFAULT_GITLAB_NAMESPACE)
REPO = 'esp-eth-drivers'

# Access levels
NO_ACCESS = 0
DEVELOPER_ROLE = 30
MAINTAINER_ROLE = 40
ADMINISTRATOR = 60

def set_branch_protection(branch_name:str='master', push_access_level:int=NO_ACCESS, merge_access_level:int=DEVELOPER_ROLE) -> bool:
    if not GITLAB_URL or not GITLAB_TOKEN:
        print('Error: GITLAB_URL and GITLAB_TOKEN must be set')
        return False

    headers = {
        'PRIVATE-TOKEN': GITLAB_TOKEN,
        'Content-Type': 'application/json'
    }

    # First unprotect the branch
    print(f'Unprotecting {branch_name} branch...')
    delete_url = f'{GITLAB_URL}/api/v4/projects/{GITLAB_NAMESPACE}%2F{REPO}/protected_branches/{branch_name}'
    response = requests.delete(delete_url, headers=headers, timeout=10)

    if response.status_code == requests.codes.not_found:
        print('Branch was not protected (404 - not found)')
    elif response.status_code == requests.codes.no_content:
        print('Branch unprotected successfully')
    else:
        print(f'Warning: Unexpected response when unprotecting: {response.status_code}')

    # Then re-protect with desired settings
    print(f'Protecting {branch_name} branch...')
    protect_url = f'{GITLAB_URL}/api/v4/projects/{GITLAB_NAMESPACE}%2F{REPO}/protected_branches'
    data = {
        'name': branch_name,
        'push_access_level': push_access_level,
        'merge_access_level': merge_access_level
    }

    response = requests.post(protect_url, headers=headers, json=data, timeout=10)

    if response.status_code == requests.codes.created:
        print('Branch protected with new settings successfully')
    else:
        print(f'Error protecting branch: {response.status_code}')
        if response.text:
            print(f'Response: {response.text}')
        return False
    return True


def main():
    parser = argparse.ArgumentParser(description='Set branch protection levels for GitLab repository')
    parser.add_argument('--branch', default='master', help='Branch name to protect (default: master)')
    parser.add_argument('--push-level', type=int, required=True,
                       help='Push access level (0=NO_ACCESS, 30=DEVELOPER, 40=MAINTAINER, 60=ADMIN)')
    parser.add_argument('--merge-level', type=int, required=True,
                       help='Merge access level (0=NO_ACCESS, 30=DEVELOPER, 40=MAINTAINER, 60=ADMIN)')

    args = parser.parse_args()

    if set_branch_protection(branch_name=args.branch,
                            push_access_level=args.push_level,
                            merge_access_level=args.merge_level) is False:
        sys.exit(1)
    sys.exit(0)


if __name__ == '__main__':
    main()
