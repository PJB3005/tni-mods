# PJB's TNI mods

Here's my mods for [Tower Networking Inc](https://store.steampowered.com/app/2939600/Tower_Networking_Inc/). See the [upstream repo](https://github.com/treefarmer741/Tower-Networking-Inc-modding-kit) for some details, though note that I use a custom build system.

## Bazel build

This repo uses [Bazel](https://bazel.build/) to build. Install it, and then building is as simple as:

```
bazel build //mods/...
```

Or to build a single mod (without packaging `.zip`):


```
bazel build //mods/mipmap
```

> [!WARNING]
> Windows users may need to run [these instructions](https://bazel.build/configure/windows) as well. I've already done that, so idk if the repo builds otherwise.

The produced mod directories (ready for copying or symlinking to the game folder) will be in `bazel-bin/mods/`.
