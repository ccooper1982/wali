# Web Arch Linux Installer
`wali` is a web browser based installer for Arch Linux:

- In the live environment, `wali` runs a web server
- Install Arch from another machine with a web UI
- 4MB download, 10MB extracted, with no dependencies
- A [video](https://www.youtube.com/watch?v=tHF04ex9AMg) of v1.2
  - Note: the mouse pointer doesn't show in the video

<!--[![wali install preview](https://github.com/ccooper1982/wali/blob/8fc128cd3bfd70783d3bb024c7e23c7d2d42d660/wali.png?raw=true)](https://www.youtube.com/watch?v=tHF04ex9AMg)-->


# Features
- Create partitions and `ext4` or `btrfs` filesystems
- Profiles: `niri`, `xfce`, `Hyprland` and `Plasma`
- Video: Guidance on appropriate video driver
- Packages: install additional packages
- Install: output displayed in browser

# Install

## Target Machine
1. Boot into the latest Arch ISO and configure the internet connection as usual
2. Download and extract `wali`     
    - `curl -sfL https://raw.githubusercontent.com/ccooper1982/wali/main/scripts/install.sh | sh`
    - Install location is `/root/wali`
3. Use `ip addr` to find the appropriate IP address for the web server 
4. Start:
    - `cd /root/wali`
    - `./start.sh <ip_address> [port]`
    - Default port is `8080`

## Other Machine
1. In a browser, visit the URL (**note** it is `http`)
    - i.e. `http://192.168.1.2:8080/`

# Arch Install Process
Go through the menu options, configuring as required. 

- For a bootable system, all stages up to and including "Boot Loader" must succeed
- If a subsequent stage fails, the system can _probably_ still boot (reported as Partial Success).
   
## Partitions
- In the "Filesystems" page, you can click "Manage Partitions" to create partitions
  - There is no UI progress indicator yet, except the buttons are disabled
- All partitions on the selected device are deleted and new partitions are created
- A new partition for home can be created (if not then, you will mount `/home` to the root partition)
- The home partition cannot be sized yet, it's either "Do nothing" or "Use remaining space"

## Mounts
- `boot` and `root` are required, and must be separate partitions
- `home` can be mounted to:
    - Root partition (default)
    - New partition: wipes the filesystem then creates a new filesystem
    - Existing partition: mount only

## User
- A root password is required
- A user account with password is required

# Details
- The web server is integrated into [`webtoolkit`](https://github.com/emweb/wt), a C++ web framework (the web equivalent of Qt)
- The install script syncs the time then downloads and extracts `wali`
- `wali` is linked statically to `wt`, as is `wt` to `boost`, so no additional dependencies are required


# Build
- C++23 compatible compiler, tested on GCC 15.x and clang 21.1.x

## Prepare
Assuming using `clang`:
- `sudo pacman -Sy clang pkg-config boost cmake meson linux-headers`

## Configure
Download `wt` and `plog` then configure:
- `meson setup build`

## Compile
Build `wt` then `wali`:
- `meson compile -C build`

The `wali` executable is `build/wali`.

