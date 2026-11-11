# Getting a PSP build of DaedalusX64

This fork builds a PSP version automatically every time a commit is pushed.
You do not need to install anything on your computer to get it.

## 1. Download the build from GitHub

1. Open the fork on GitHub: `https://github.com/Larry70455/daedalus`
2. Click the **Actions** tab at the top of the page.
   - If you see a message saying workflows are disabled for this fork, click
     the green button to enable them. GitHub turns Actions off for new forks
     until the owner enables them once.
3. In the list on the left, click **CI**. Pick the newest run with a green
   check mark for the branch you want (for example `master` or a feature
   branch).
   - To start a build by hand, click **Run workflow** on the right, choose a
     branch, and click the green button.
4. Scroll to the bottom of the run page to the **Artifacts** section and click
   **DaedalusX64-PSP**. A zip file downloads. (You must be logged in to GitHub
   to download artifacts. They are kept for 90 days.)
5. Unzip it. Inside is a folder called `DaedalusX64` that contains
   `EBOOT.PBP`, `Plugins`, `Roms`, `SaveGames`, `SaveStates` and a few other
   files. `BUILD_INFO.txt` records which commit it was built from, which is
   useful when you report results.

## 2. Install on the PSP Go

1. Connect the PSP Go to your computer over USB (Settings > USB Connection).
2. Copy the whole `DaedalusX64` folder into `PSP/GAME/`, so you end up with
   `PSP/GAME/DaedalusX64/EBOOT.PBP`.
   - On a PSP Go this is usually the internal storage (shown to the PSP as
     `ef0:`). A Memory Stick Micro (`ms0:`) works too, as long as the same
     folder layout is used.
   - Keep the `Plugins` folder next to `EBOOT.PBP`. The emulator loads the
     Media Engine and other helper modules from it.
3. Put your ROM files (`.z64`, `.n64`, `.v64`) into
   `PSP/GAME/DaedalusX64/Roms/`.
   - Note: the current code only scans this `Roms` folder. A separate `/N64`
     folder is **not** scanned on master. If you want that, it can be added as
     a small later change.
4. Disconnect USB and launch DaedalusX64 from the Game menu. The emulator
   sets the CPU to 333 MHz itself when it starts (see
   `Source/SysPSP/main.cpp`), so a CPU speed chosen in the ARK menu is
   overridden.

When updating to a newer build, you can overwrite everything except
`SaveGames` and `SaveStates` if you want to keep your saves.

## Measuring performance

To record where the time goes in a game, use the `DaedalusX64-PSP-PerfLog`
artifact instead. See [PERF_LOG.md](PERF_LOG.md).

## 3. Building it yourself (optional)

You only need this if you want to build without GitHub. Linux or WSL with
Docker installed:

```sh
docker run --rm -v "$PWD":/src -w /src pspdev/pspdev:latest \
  sh -c "apk add build-base git bash cmake && ./build_daedalus.sh PSP"
```

The result is in the `DaedalusX64` folder in the repository root. Builds for
other platforms still use `./build_daedalus.sh` with no argument (Linux,
macOS), `CTR` (3DS) or `WIN` (Windows cross build).
