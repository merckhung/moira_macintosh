# testdata

The ROM and disk images used by the emulator are checked into this directory.

| File | Description |
|------|-------------|
| `MacSE.ROM` | 256 KB Macintosh SE ROM (`RomFileName` in `src/config.h`) |
| `OS_608_boot.dsk` | 1.44 MB bootable System 6.0.8 floppy image |
| `hd20.img` | Raw hard-disk image mounted as an extra drive |

Run with, e.g.:

```sh
./bazel-bin/src/moira_macintosh testdata/OS_608_boot.dsk testdata/hd20.img
```

Note: `MacSE.ROM` and the disk images contain Apple copyrighted material,
included here for convenience.
