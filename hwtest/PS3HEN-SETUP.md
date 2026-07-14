# Console prep — running the hwtest on a real PS3

Target unit for the #11912 run: **CECH-4021B** (Super Slim, CECH-40xx),
shipped on OFW **4.75**. Super Slim units **cannot run CFW** — PS3HEN
(Homebrew ENabler) over HFW (Hybrid Firmware) is the correct and only path,
and it's all we need to install and run the hwtest PKG.

Versions below were current as of 2026-07; **verify against the live sources
before flashing** — HFW/HEN track together and roll forward
(HFW 4.93.1 pairs with PS3HEN 3.5.0 at time of writing).

## What HEN is (and isn't)

- HEN is **not permanent**. HFW stays flashed, but HEN must be **re-enabled
  every cold boot** (launch the "Enable HEN" icon). This is expected, not a
  fault. Fine for our purpose — enable it, install/run, photograph.
- No downgrade is involved. From 4.75 you update **upward** to the latest HFW
  (a higher version number), which the console accepts as a normal update.
  Super Slim can't downgrade anyway, so there's nothing risky here.

## Prep checklist

1. **USB stick, FAT32.** Any small drive, formatted FAT32 (exFAT/NTFS won't be
   read by the PS3 updater).
2. **Download the latest HFW `PS3UPDAT.PUP`** from an authoritative source
   (PSX-Place HFW release thread / ps3xploit.me / ConsoleMods firmwares page).
   Confirm the version and, if a hash is published, verify it.
3. **Create `PS3/UPDATE/` on the USB** and put the PUP at
   `PS3/UPDATE/PS3UPDAT.PUP` (exact path/case).
4. **Charge/UPS the unit** — do not lose power mid-flash.
5. Have the hwtest **`rsx-zfunc-hwtest.pkg`** on the same (or another) FAT32
   USB, anywhere HEN's package manager can see it (root or a folder).

## Install sequence

1. **Update to HFW:** PS3 → Settings → System Update → Update via Storage
   Media → accept the HFW PUP. It reboots; version still reads ~4.9x.
2. **Enable HEN:** open the PS3 web browser → `https://ps3xploit.me` → use the
   **HEN Auto Installer**. On first install it writes the persistent bits and
   reboots; afterward an **"Enable HEN"** icon lives in the XMB **Game**
   column.
   - Network/USB HEN installers also exist if the browser route is flaky.
3. **Confirm HEN is active** (installer reports success / the enable icon
   returns you cleanly to XMB).

## Install & run the hwtest

**Two requirements the XMB installer is silent about — get both right or the
package simply won't appear in the list (no error, just absent):**

- **FAT32 USB, package at the ROOT.** The stock "Install Package Files" tool
  reads **FAT32 only** — it cannot see an **NTFS or exFAT** drive at all. (The
  NTFS drive from the disc-dump guide is for the 20 GB ISOs, which ManaGunZ
  reads; it is the *wrong* drive for the pkg. Use a small separate FAT32 stick,
  or the internal HDD, for the package.) Put `rsx-zfunc-hwtest.pkg` in the
  drive root, not a subfolder.
- **The pkg must be FINALIZED (retail-flagged).** The XMB tool only *lists*
  finalized packages (header byte `@0x04 = 0x80`); a non-finalized *debug* pkg
  (`0x00`) is silently filtered out. The `rsx-zfunc-hwtest.pkg` shipped in this
  repo is already finalized — verify with `xxd -l8 rsx-zfunc-hwtest.pkg` (5th
  byte must be `80`). *(An earlier build here shipped the non-finalized debug
  pkg by mistake — that's what "package not recognized" was.)*

Then:

1. With HEN enabled: XMB → **Game → Install Package Files** (a.k.a. Package
   Manager) → `rsx-zfunc-hwtest.pkg` → install.
2. Launch **"RSX zfunc-remap hwtest"** from the Game column.
3. It draws a static grid (see the main [README](README.md) for the layout).
   The four **yellow-framed** cells are the decisive GT5P parked state.
4. **Photograph the TV** with the left grid (Matrix A + B) legible. That photo
   is the deliverable.

### If the package still isn't listed

- Re-check the drive is **FAT32** (not NTFS/exFAT) and the pkg is in the
  **root**.
- Confirm the pkg is **finalized** (`@0x04 == 0x80`, above).
- Try HEN's package manager via **multiMAN / IRISMAN** instead of the stock XMB
  tool — those install debug *and* finalized pkgs and are less picky about
  location.
- Last resort: FTP the pkg to the internal HDD (`/dev_hdd0/packages/`) and
  install from there.

## What to look for (the verdict)

- **Yellow-framed cells non-black (expect white)** → RSX applies the zfunc
  compare to color-format fetches → hypothesis confirmed, explains the console
  output → we submit the finding upstream.
- **Yellow-framed cells black** → hypothesis is dead → we retract in-thread.
  (Either way the fork keeps its empirical fix — it matches console output
  regardless of the mechanism.)
- **Sanity gate first:** Matrix B rows must read black / magenta / white
  (top→bottom) and the reference bars black / gray / white. If they don't, the
  photo/display is off (or the harness) — don't interpret the yellow cells
  until the anchors look right. Compare directly against
  [rpcs3-reference-0.0.41-macos.png](rpcs3-reference-0.0.41-macos.png).

## Display note

The binary requests **720p** first (falls back to 480/576). Use an HDMI TV
that shows 720p cleanly, or a capture card. A phone photo of the TV is
sufficient — this test is deliberately readable without instrumentation.

## Don't

- Don't attempt CFW (Rebug/Ferrox/etc.) on this Super Slim — not supported,
  and chasing it invites a brick. HEN is the whole plan.
- Don't grab HFW/HEN from random mirrors — use PSX-Place / ps3xploit.me /
  ConsoleMods.
