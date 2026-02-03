# Profiles

- The None profile is hardcoded
- Otherwise, there is a `json` config file per profile

## Config
A profile has general form:

```
{
  "name": "<name>",
  "info": "<brief description>",
  "iwd": <install iwd, networkd and resolved>,
  "packages_required": [<packages always installed>],
  "packages_optional": [<pacakges user can select (not implemented yet)>],
  "services_enable": [<services to enable>]
}
```
---

### iwd

If `true`:
- `iwd` is installed
- `iwd`, `networkd` and `resolved` services are enabled

Some desktops handle network (i.e. Plasma) handle.

### packages_optional

**Not implemtented.**

Allow user to select which to install.
