# Profiles

- The None profile is hardcoded
- Otherwise, there is a `json` config file per profile
- The minimal packages are installed, but always include internet connectivity and a terminal

## Config
A profile has general form:

```
{
  "name": "<name>",
  "info": "<brief description>",
  "iwd": <install iwd, networkd>,
  "netmanager":<install NetworkManager>
  "packages_required": [<packages always installed>],
  "packages_optional": [<pacakges user can select (not implemented yet)>],
  "services_enable": [<services to enable>]
}
```
---

> [!WARNING]
> `iwd` and `netmanager` are mutally exclusive, they should not both `true`.

### iwd

If `true`:
- `iwd` is installed
- `iwd` and `networkd` are installed

### netmanager

If `true`:
- `NetworkManager` is installed

### packages_optional

**Not implemtented.**

Allow user to select which to install.
