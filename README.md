# Web Arch Linux Installer
`wali` is a web browser based installer for Arch Linux:

- In the live environment, `wali` runs a web server
- Install Arch from a web browser


# Install

## Target Machine
1. Boot into the latest Arch ISO and configure the internet connection as usual
2. `wali` doesn't create partitions yet, so do so as normal with `fdisk`, `cfdisk`, etc
3. Download and extract `wali`     
    - `curl -sfL https://raw.githubusercontent.com/ccooper1982/wali/main/scripts/install.sh | sh`
    - Install location is `/usr/local/bin/wali`
4. Use `ip addr` to find the appropriate IP address for the web server 
5. Start: `cd /usr/local/bin/wali && ./start.sh <ip_address> [port]`
    - Default port is `8080`

## Other Machine
1. In a browser, visit the URL (note it is `http`)
    - i.e. `http://192.168.1.2:8080/`

# Arch Install Process
Go through the menu options, configuring as required. Most
sections are self explanatory.

- For a bootable system, all stages up to and including "Boot Loader" must succeed
- If a subsequent stage fails the system can _probably_ still boot (reported as Partial Success).
   
 
## Partitions
- `boot` and `root` are required, and must be separate partitions
- `boot` is always a `vfat` filesystem
- `root` can only be `ext4` (`btrfs` coming soon)
- `home` can be mounted to:
    - `root` (default)
    - New partition: wipes the partition and create a new filesystem
    - Existing partition: mount only

> [!IMPORTANT]
> The boot partition is mounted at `/efi`.

## User
- A root password is required
- A user account with password is required


# Details
- The web server is integrated into [`webtoolkit`](https://github.com/emweb/wt) (`Wt`), a C++ web framework (the web equivalent of Qt)
- The install script increases the copy-on-write (`cow`) space from 250M to 600M
- This allows space for `Wt` and it's dependencies


# Build
- TODO
