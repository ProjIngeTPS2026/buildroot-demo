# Apping Buildroot external

This external tree is intended for Buildroot 2024.02.1.

Use it from a Buildroot checkout:

```sh
make BR2_EXTERNAL=/path/to/apping/buildroot-external menuconfig
```

Enable:

- `Target packages -> Apping -> apping-roc-toolkit-opus`
- `Target packages -> Apping -> apping-fixed-base`
- `Target packages -> Apping -> apping-portable-ui`

For incremental Apping package rebuilds, avoid a full `make clean` unless the
whole root filesystem really needs to be regenerated from scratch:

```sh
make apping-fixed-base-rebuild
make apping-portable-ui-rebuild
```

If CMake options or the Buildroot package build layout changed, re-run the
package configuration step instead:

```sh
make apping-fixed-base-reconfigure
make apping-portable-ui-reconfigure
```

The two application packages are separate. Roc is packaged as a shared
dependency because live audio uses `roc-send` and `roc-recv` with Opus multicast.

The fixed-base target configuration is installed as `/etc/apping/base.json`.
Set both `multicast_interface` and `audio_output_uri` there; the fixed-base
package does not use a separate `/etc/default/apping-fixed-base` file.
