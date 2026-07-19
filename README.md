# Mashed Fix (Alpha)
A widescreen fix for **Mashed** and **Mashed: Fully Loaded**. 

This release comes bundled with **DSOAL** for plug-and-play installation, as the game requires a DirectSound wrapper to launch on modern Windows.

Note that 2D UI and HUD element positioning is not yet fully implemented (which is why the mod is still in alpha). However, it is already much more pleasant to play than the native game with its stretched widescreen HUD.
---

### Screenshots
<details>
  <summary><b>Click to view screenshots</b></summary>
  <br>
  <img src="https://github.com/user-attachments/assets/9773eeaa-8d66-499f-a3ad-d94916558c7d" width="800" alt="Widescreen fix 1" />
  <img src="https://github.com/user-attachments/assets/369b6d15-b92c-4ec6-bbc3-525a7186e088" width="800" alt="Widescreen fix 2" />
  <img src="https://github.com/user-attachments/assets/25cf2b22-54cc-48fb-a9e4-b3067ae6528c" width="800" alt="Widescreen fix 3" />
  <img src="https://github.com/user-attachments/assets/b477e9fa-b99d-4039-8c39-ac8a4260564e" width="800" alt="Widescreen fix 4" />
  <img src="https://github.com/user-attachments/assets/c89c9728-e65d-4597-9d95-df4fe5851508" width="800" alt="Widescreen fix 5" />
</details>

---

### How to Install

1. Download the release `.zip` archive.
2. Extract all files into your main game directory (where the game `.exe` is located):
   * `dinput8.dll` (The widescreen fix)
   * `dsound.dll` (DSOAL)
   * `dsoal-aldrv.dll` (DSOAL dependency)
   * `alsoft.ini` (DSOAL configuration)
3. Run the game. 
   * **Note:** The configuration file `MashedFix.ini` will be created automatically in your game folder on the first run.

---

### Configuration Options (`MashedFix.ini`)

You can customize the mod by editing the automatically generated `MashedFix.ini` file with any text editor.

#### [General]
* `SingleCoreAffinity` (Default: `1`): Runs the game on 1 CPU core (prevents crashes).
* `CrashHandler` (Default: `1`): Generates a `.dmp` file on crash.
* `TrampolineFix` (Default: `1`): Fixes physics bugs on jumps and trampolines.

#### [Display]
* `BorderlessFullscreen` (Default: `1`): Enables borderless windowed mode.
* `LimitFPS` (Default: `1`): Enables FPS limiter.
* `TargetFPS` (Default: `60.0`): Target frame rate.

#### [Widescreen]
* `AspectFix` (Default: `1`): Fixes camera aspect ratio.
* `HudFix` (Default: `1`): Stops HUD elements from stretching.
* `FlagFix` (Default: `1`): Fixes stretched 3D flags.
* `ShowWinnerHUDFix` (Default: `1`): Fixes winner screen layout.

#### [AudioAndFocus]
* `MuteOnFocusLoss` (Default: `1`): Mutes game audio when alt-tabbed.
* `PauseOnFocusLoss` (Default: `1`): Pauses game when alt-tabbed.

#### [Debug]
* `AOBLog` (Default: `0`): Creates a debug log file `AOBLog.txt` for memory patches.

---

### Support the Project / Donations
To make this fix possible, I have to reverse-engineer the game, which takes a lot of time and effort. Donations directly help support ongoing development and get this project closer to a final release. 

If you find this useful, please consider supporting my work:

* **USDT (TRC-20):** `TBgghhAiMGUbh4Du5JVpzv8zFBv9npugdC`
* **BTC:** `bc1q5mvca58yaa2dc49yzhds3n7h28xzg0zjwxss03`
* **LTC:** `Lhdef5oPpxXFzH1vwxMFhaVZcyacyzJHgF`
* **ETH (ERC-20):** `0x58876e3cd3f3a4b3ab4763a68bf9955b6d127c9a`
* **TON(GRAM):** `UQCwabfDSQXKs0rtzSZXOxknia6RN4t0bOwV5O_P00AIh11h`

---

### Credits
* **DSOAL** by kcat and contributors (Licensed under LGPL).
