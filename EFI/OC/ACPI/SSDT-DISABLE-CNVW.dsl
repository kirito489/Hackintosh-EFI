/*
 * SSDT-DISABLE-CNVW
 *
 * 停用 _SB.PC00.CNVW —— 主機板內建 Intel CNVi Wi-Fi (PCI 00:14.3)。macOS 完全不支援，且同樣掛在 GPE 0x6D 上，是假喚醒的候選來源。PCIe 插槽上的 Broadcom 卡走 RP##/PXSX，與此無關
 *
 * ⚠️ 以 _OSI("Darwin") 包住，**只在 macOS 生效**。
 *    本機 Windows 也是透過 OpenCore 選單開機，而 OpenCore 的 ACPI 修改
 *    會套用到之後載入的任何作業系統。若不加此判斷，Windows 下該裝置
 *    也會一併消失。非 Darwin 時回傳 0x0F（存在、啟用、正常、解碼資源）。
 *
 * 作法：原始 DSDT 中該裝置層沒有 _STA，因此直接新增不會撞名，
 *       也不需要任何 ACPI rename patch。注意路徑是 PC00 不是 PCI0。
 */
DefinitionBlock ("", "SSDT", 2, "ACDT", "DCNVW", 0x00000000)
{
    External (_SB_.PC00.CNVW, DeviceObj)

    Scope (_SB.PC00.CNVW)
    {
        Method (_STA, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (Zero)
            }
            Else
            {
                Return (0x0F)
            }
        }
    }
}
