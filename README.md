# Web Arch Linux Installer
`wali` is a web browser based installer for Arch Linux:

- In the live environment, `wali` runs a web server
- Install Arch from a web browser

# Install

## Target Machine
1. Boot into the latest Arch ISO and configure the internet connection as usual
2. Download and extract `wali` 
    - Assumed location is `/usr/local/bin/wali`
    - TODO
3. Install dependencies: `./usr/local/bin/wali/install.sh`
4. Use `ip addr` to find the appropriate the IP address for the web server 
5. Start: `./usr/local/bin/wali/start.sh <ip_address> [port]`
    - Default port is `8080`

## Other Machine
1. In a browser, visit the URL (note it is `http`)
    - i.e. `http://192.168.1.2:8080/`

# Usage
Go through the menu options, configuring as required. Most
sections are self explanatory, 

## Partitions
- `boot` and `root` are required, and must be separate partitions
- `boot` must be `vfat`
- `root` can only be `ext4` (`btrfs` coming soon)
- `home` can be mounted to:
    - `root` (default)
    - New partition: wipes the partition and create a new filesystem
    - Existing partition: mount only

> **Note**
The boot partition is mounted at `/efi`

## User
- A root password is required
- A user account with password is required

# Install Process
- For minimal success all stages up to and including "Boot Loader" must succeed
    - The Install Status will change to "Minimal"
- Subsequent stages:
    - Succeed: Install Status is "Complete"
    - Fail: Install Status is "Partial"

# Build
- TODO
