# Hackintosh — ROG STRIX B560-F GAMING WIFI + i5-10400 + RX 6800XT

macOS **Ventura (13)** + **Windows** 雙系統的 OpenCore EFI。
開機進 OpenCore 圖形選單(OpenCanopy)自行選擇作業系統,兩顆系統各自獨立 NVMe。

> ⚠️ 這是**我自己這台機器**調出來的 EFI，**不是萬用包**。你的硬體(尤其 USB 對應、SMBIOS 序號)一定不同，**請勿整包照抄**。可以參考設定邏輯，但要自己重做 USB Map、產生自己的序號。

---

## 🖥 硬體規格

| 元件 | 型號 |
|---|---|
| 主機板 | ASUS ROG STRIX **B560-F GAMING WIFI** (LGA1200) |
| CPU | Intel Core **i5-10400** (Comet Lake, 6C/12T) |
| 顯示卡 | AMD Radeon **RX 6800XT** (RDNA2 / Navi 21) |
| 記憶體 | 16GB DDR4-2400 |
| macOS 碟 | WD Blue SN570 500GB NVMe |
| Windows 碟 | ADATA SX8200PNP 1TB NVMe + HGST 1TB 2.5" SATA(資料) |
| 有線網路 | Intel **I225-V** 2.5GbE |
| 無線網路 | **Broadcom BCM94360 系列**(PCIe 轉接卡；主機板內建 Intel AX 不支援) |
| 音效 | Realtek (layout-id 1) |

---

## ✅ 功能狀態

| 功能 | 狀態 | 備註 |
|---|---|---|
| 雙系統圖形選單 (OpenCanopy) | ✅ | 滑鼠/鍵盤皆可選 |
| 硬碟開機(免 USB / 免 F8) | ✅ | BIOS 開機順序設 WD Blue 第一 |
| 顯卡加速 (RX 6800XT) | ✅ | WhateverGreen + `agdpmod=pikera` |
| WiFi | ✅ | Broadcom 原生 |
| 藍牙 | ✅ | Broadcom 原生驅動；⚠️ **必須停用 BlueToolFixup**（見踩雷筆記） |
| 音效 (HDMI) | ✅ | |
| 有線網路 (I225-V) | ✅ | AppleIGC |
| 時間(雙系統不跑掉) | ✅ | 見下方踩雷筆記 |
| AirDrop / 接續互通 | ✅ | 需登入 iCloud |
| 睡眠 | ⚠️ | 睡得著也喚得醒，但無法維持深層睡眠（見踩雷筆記）|
| 關機(不跳 BIOS 安全模式) | ✅ | RTCMemoryFixup |

---

## ⚙️ 軟體 / 設定

- **Bootloader**: OpenCore **1.0.7**
- **SMBIOS**: `iMac20,1`
- **boot-args**: `keepsyms=1 agdpmod=pikera rtcfx_exclude=0E-FF brcmfx-country=US`
- **`Misc > Security > AllowSetDefault`**: `true` — 雙系統必開，OpenCore 選單按 **Ctrl+Enter** 把游標所在項目設為預設
- **`Misc > Boot > Timeout`**: `5` — 開機選單倒數 5 秒後自動進入預設項目；期間按任意鍵即停止倒數，可手動選擇

### Kexts

| Kext | 用途 |
|---|---|
| Lilu | 核心 patch 引擎 |
| VirtualSMC (+ SMCProcessor, SMCSuperIO) | SMC 模擬 / 感測器 |
| WhateverGreen | 顯卡 |
| AppleALC | 音效 |
| USBToolBox + UTBMap | USB 埠位對應(自製) |
| RTCMemoryFixup | 修關機跳 F1 + 時間 |
| AirportBrcmFixup (+ AirPortBrcmNIC_Injector) | WiFi |
| ~~BlueToolFixup~~ | ⚠️ **已停用** — 正牌 Broadcom 卡裝了它反而會壞（見踩雷筆記） |
| ~~BrcmFirmwareData + BrcmPatchRAM3~~ | ⚠️ **已停用** — 對本卡從不載入（PID `0a5c:21ff` 不在 `BrcmPatchRAM3` 支援清單），留著只是白佔核心記憶體 |
| NVMeFix | NVMe 電源管理 |
| RestrictEvents | 修正 CPU 名稱顯示 |
| FeatureUnlock | 解鎖 Sidecar / 隔空 / 接續互通 |
| AppleIGC | Intel I225-V 有線網卡 |

### ACPI (SSDT)

- `SSDT-EC-USBX-DESKTOP` — 假 EC 裝置 + USB 供電
- `SSDT-PLUG-DRTNIA` — CPU 原生電源管理
- `SSDT-AWAC` — 關 AWAC、改用 legacy RTC
- `SSDT-DISABLE-XDCI` — 停用 XDCI，消除假喚醒（見踩雷筆記）
- `SSDT-DISABLE-CNVW` — 停用主機板內建 Intel CNVi Wi-Fi（macOS 不支援，留著也只是多一個喚醒源）

### UEFI Drivers

`OpenRuntime` · `OpenCanopy`(圖形選單) · `OpenHfsPlus` · `OpenUsbKbDxe`

---

## 🔧 BIOS 設定

| 項目 | 值 |
|---|---|
| CSM | Disabled |
| Secure Boot → OS Type | Other OS |
| VT-d | Disabled |
| SATA Mode | AHCI |
| Above 4G Decoding | **Enabled**(6800XT 必要) |
| Re-Size BAR Support | Disabled |
| Fast Boot | Disabled |

---

## 🕳 踩雷筆記(給後人參考)

- **WiFi 卡別買錯**：主機板內建 Intel AX (CNVi) macOS 完全不支援。要買 **Broadcom BCM94360 系列**（原生）。
- **🔥 雙系統下寫停用裝置的 SSDT，一定要包 `_OSI("Darwin")`**：Windows 若也是透過 OpenCore 選單開機，**OpenCore 的 ACPI 修改會套用到之後載入的任何作業系統** —— 沒加判斷的話，你在 macOS 停掉的裝置在 Windows 也會消失。正確寫法：
  ```asl
  Method (_STA, 0, NotSerialized)
  {
      If (_OSI ("Darwin")) { Return (Zero) }   // macOS：隱藏
      Else                 { Return (0x0F) }   // 其他 OS：正常存在
  }
  ```
- **主機板內建的 Intel AX (CNVi) 建議直接用 SSDT 停掉**：macOS 沒有任何驅動會用它，但它的 ACPI 裝置 `_SB.PC00.CNVW` 仍掛在 GPE `0x6D` 上、`_PRW` 回傳 `GPRW(0x6D, 0x04)`，是假喚醒的候選來源。作法同 XDCI：裝置層沒有 `_STA`（出現的那個屬於巢狀 `PowerResource (WRST)`），直接用 `SSDT-DISABLE-CNVW.aml` 新增回傳 0 的 `_STA` 即可。
  ⚠️ 別搞混：PCIe 插槽上的 Broadcom 卡走 `RP##` → `PXSX`，與 `CNVW`（`00:14.3`）無關，停用 CNVW 不影響 Wi-Fi/藍牙。
- **⚠️ 不要用 `BCM_4350C2` 判斷是不是假卡（這條我踩過，浪費一整晚）**：`system_profiler SPBluetoothDataType` 顯示 `BCM_4350C2`（USB 名稱 `BCM2045A0`）**只代表韌體還沒上傳**，不代表買到假貨。正牌卡韌體上傳成功後會變 `BCM_20703A1`，且藍牙位址 = **Wi-Fi MAC +1**（本機 Wi-Fi `ac:bc:32:87:1f:03` → 藍牙 `AC:BC:32:87:1F:04`），這才是可靠的驗證方式。
- **🔥 藍牙身分錯亂 / AirDrop 不通 → 停用 `BlueToolFixup`**：`BlueToolFixup` 是給**非 Apple** 藍牙用的相容層。**正牌 Broadcom 卡裝了它，原生驅動路徑會被攔截**，macOS 改走第三方通用流程 → `Chipset: THIRD_PARTY_DONGLE`、每次開機**亂數產生**藍牙位址、假韌體版本 `v8453 c4096`。一般配對還能用，但 **Continuity 會驗證藍牙與 Wi-Fi 位址的配對關係，位址是亂數就直接拒絕 → AirDrop 掛掉**。
  修法：`config.plist` 的 `Kernel > Add` 把 `BlueToolFixup.kext` 設 `Enabled=false`，重開機即恢復。
  走過的死路（別再走一次）：NVRAM `bluetoothInternalControllerInfo` 快取、`pkill bluetoothd` / `ControllerPowerState`、板載 Genesys USB hub 枚舉延遲 —— 全都不是原因。
- **AirDrop 找不到自己的 iPhone**：先確認 Mac 沒有連著**那支手機的個人熱點**。iPhone 開熱點時 Wi-Fi 進入 AP 模式，無法同時做 AWDL 點對點。用 `ipconfig getifaddr en1` 檢查，開頭是 `172.20.10.x` 就是連到熱點了。（手機和路由器同名時特別容易中招。）
- **🔥 睡下去十幾秒就自己醒（假喚醒）→ 停用 `XDCI`**：`pmset -g log` 若看到大量 `DarkWake ... due to XDCI/`，元兇是 `_SB.PC00.XDCI`（Intel PCH USB Device Controller / OTG，PCI `00:14.1`）。桌機不會把自己當 USB 周邊，這控制器毫無用途，但它的 `_PRW` 回傳 `GPRW(0x6D, 0x04)` 會在 S3 觸發喚醒。本機累計超過 200 次。
  修法：用 `SSDT-DISABLE-XDCI.aml` 幫它加一個回傳 0 的 `_STA`。原始 DSDT 中 XDCI **沒有** `_STA`（只有 `_ADR`/`_S0W`/`_PRW`/`_DSW`/`_DSM`），所以直接新增不會撞名，**不需要任何 ACPI rename patch**。注意路徑是 `PC00` 不是 `PCI0`。
  ⚠️ **不要改用 `SSDT-GPRW`**：GPE `0x6D` 由 `GLAN`/`XHCI`/`XDCI`/`HDAS`/`CNVW` 共用，動 `GPRW` 會連帶關掉 USB 與網路喚醒，鍵盤滑鼠將無法喚醒電腦。
  B560-F 的 BIOS **沒有** XDCI 的開關選項，只能走 SSDT。取得 DSDT 的方法：OpenCore RELEASE 版不支援 `SysReport`，改用 **MaciASL** → `File → New from ACPI → DSDT`，再用 ⌘A ⌘C 複製、`pbpaste` 存檔（直接存 `.aml` 會因重新編譯失敗）。
- **⚠️ 已知問題：仍無法維持深層睡眠**：修掉 XDCI 之後，仍有固定節奏的 `DarkWake ... due to XHCI HDAS CNVW/Network`（約每 27 秒一次，螢幕不亮），以及約 2 分鐘後一次歸因為 `HID Activity` 的 FullWake（螢幕會亮）。
  已排除：`XDCI`、Power Nap、`womp`(Wake on LAN)、`proximitywake`、實體鍵盤滑鼠（拔線仍會醒）、`AURA LED Controller`（無喚醒斷言）。
  未驗證的剩餘嫌疑：**藍牙遠端喚醒** —— 有 5 個配對裝置，`RemoteWakeEnabled` 預設為允許，未連線的配對裝置仍可主動喚醒 Mac。要測就跑
  `sudo defaults write /Library/Preferences/com.apple.Bluetooth RemoteWakeEnabled -int 0`（代價是藍牙鍵鼠無法喚醒電腦）。
  影響評估：機器**睡得著、喚得醒、資料安全**，只是待機耗電偏高。屬品質問題非故障。
- **關機後開機跳 BIOS 安全模式 (F1)**:macOS 的 AppleRTC 亂寫 CMOS,弄壞 ASUS BIOS 校驗值。用 **RTCMemoryFixup** + boot-arg `rtcfx_exclude`。
- **時間卡在 1999 / 每次開機跑掉**:`rtcfx_exclude=00-FF`(全鎖)會連時間都不讓寫 → 縮成 **`rtcfx_exclude=0E-FF`**(只鎖 BIOS 校驗區、放行時間區 0x00–0x0D)+ macOS 開自動對時。雙系統時間打架另需在 Windows 設 `RealTimeIsUniversal=1`。
- **OpenCore 選單鍵盤沒反應**：很可能是 **Timeout 太短**（選單自己倒數跑掉）。除錯期間先設 `Timeout=0` 讓它停著等你選；
  確認鍵盤可用後（需 `OpenUsbKbDxe.efi`），再改成 `Timeout=5` + `AllowSetDefault=true`，用 **Ctrl+Enter** 設定預設項目，之後開機倒數 5 秒自動進入，按任意鍵可中斷倒數。
- **裝好後拔 USB 卻進 Windows**:OpenCore 只在 USB 上時,要把 EFI 複製到 macOS 碟的 EFI 分割區,再把 BIOS 開機順序設成該碟。
- **OpenCanopy 圖形選單空白**:`Resources/` 資料夾要有素材(從 [OcBinaryData](https://github.com/acidanthera/OcBinaryData) 拿 Font / Image / Label)。

---

## 🙏 Credits

- [Acidanthera](https://github.com/acidanthera) — OpenCorePkg 及各 Kext
- [Dortania OpenCore Install Guide](https://dortania.github.io/OpenCore-Install-Guide/)

## ⚠️ Disclaimer

僅供學習與個人使用。Hackintosh 非 Apple 官方支援,自行承擔風險。**升級 macOS 大版本前務必先備份 EFI。**
