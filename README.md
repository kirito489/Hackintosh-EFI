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
| 關機(不跳 BIOS 安全模式) | ✅ | RTCMemoryFixup |

---

## ⚙️ 軟體 / 設定

- **Bootloader**: OpenCore **1.0.7**
- **SMBIOS**: `iMac20,1`
- **boot-args**: `keepsyms=1 agdpmod=pikera rtcfx_exclude=0E-FF brcmfx-country=US`

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
| BrcmFirmwareData + BrcmPatchRAM3 | 保留但**對本卡未作用**（PID `0a5c:21ff` 不在支援清單） |
| NVMeFix | NVMe 電源管理 |
| RestrictEvents | 修正 CPU 名稱顯示 |
| FeatureUnlock | 解鎖 Sidecar / 隔空 / 接續互通 |
| AppleIGC | Intel I225-V 有線網卡 |

### ACPI (SSDT)

- `SSDT-EC-USBX-DESKTOP` — 假 EC 裝置 + USB 供電
- `SSDT-PLUG-DRTNIA` — CPU 原生電源管理
- `SSDT-AWAC` — 關 AWAC、改用 legacy RTC

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
- **⚠️ 不要用 `BCM_4350C2` 判斷是不是假卡（這條我踩過，浪費一整晚）**：`system_profiler SPBluetoothDataType` 顯示 `BCM_4350C2`（USB 名稱 `BCM2045A0`）**只代表韌體還沒上傳**，不代表買到假貨。正牌卡韌體上傳成功後會變 `BCM_20703A1`，且藍牙位址 = **Wi-Fi MAC +1**（本機 Wi-Fi `ac:bc:32:87:1f:03` → 藍牙 `AC:BC:32:87:1F:04`），這才是可靠的驗證方式。
- **🔥 藍牙身分錯亂 / AirDrop 不通 → 停用 `BlueToolFixup`**：`BlueToolFixup` 是給**非 Apple** 藍牙用的相容層。**正牌 Broadcom 卡裝了它，原生驅動路徑會被攔截**，macOS 改走第三方通用流程 → `Chipset: THIRD_PARTY_DONGLE`、每次開機**亂數產生**藍牙位址、假韌體版本 `v8453 c4096`。一般配對還能用，但 **Continuity 會驗證藍牙與 Wi-Fi 位址的配對關係，位址是亂數就直接拒絕 → AirDrop 掛掉**。
  修法：`config.plist` 的 `Kernel > Add` 把 `BlueToolFixup.kext` 設 `Enabled=false`，重開機即恢復。
  走過的死路（別再走一次）：NVRAM `bluetoothInternalControllerInfo` 快取、`pkill bluetoothd` / `ControllerPowerState`、板載 Genesys USB hub 枚舉延遲 —— 全都不是原因。
- **AirDrop 找不到自己的 iPhone**：先確認 Mac 沒有連著**那支手機的個人熱點**。iPhone 開熱點時 Wi-Fi 進入 AP 模式，無法同時做 AWDL 點對點。用 `ipconfig getifaddr en1` 檢查，開頭是 `172.20.10.x` 就是連到熱點了。（手機和路由器同名時特別容易中招。）
- **關機後開機跳 BIOS 安全模式 (F1)**:macOS 的 AppleRTC 亂寫 CMOS,弄壞 ASUS BIOS 校驗值。用 **RTCMemoryFixup** + boot-arg `rtcfx_exclude`。
- **時間卡在 1999 / 每次開機跑掉**:`rtcfx_exclude=00-FF`(全鎖)會連時間都不讓寫 → 縮成 **`rtcfx_exclude=0E-FF`**(只鎖 BIOS 校驗區、放行時間區 0x00–0x0D)+ macOS 開自動對時。雙系統時間打架另需在 Windows 設 `RealTimeIsUniversal=1`。
- **OpenCore 選單鍵盤沒反應**:很可能是 **Timeout 太短**(選單自己倒數跑掉)。設 `Timeout=0` 讓它停著等你選。
- **裝好後拔 USB 卻進 Windows**:OpenCore 只在 USB 上時,要把 EFI 複製到 macOS 碟的 EFI 分割區,再把 BIOS 開機順序設成該碟。
- **OpenCanopy 圖形選單空白**:`Resources/` 資料夾要有素材(從 [OcBinaryData](https://github.com/acidanthera/OcBinaryData) 拿 Font / Image / Label)。

---

## 🙏 Credits

- [Acidanthera](https://github.com/acidanthera) — OpenCorePkg 及各 Kext
- [Dortania OpenCore Install Guide](https://dortania.github.io/OpenCore-Install-Guide/)

## ⚠️ Disclaimer

僅供學習與個人使用。Hackintosh 非 Apple 官方支援,自行承擔風險。**升級 macOS 大版本前務必先備份 EFI。**
