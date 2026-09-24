/*
 * SSDT-DISABLE-XDCI
 *
 * 停用 _SB.PC00.XDCI —— Intel PCH USB Device Controller / OTG (PCI 00:14.1)。桌機無用途，其 _PRW 回傳 GPRW(0x6D, 0x04) 會造成睡眠後假喚醒
 *
 * ⚠️ 以 _OSI("Darwin") 包住，**只在 macOS 生效**。
 *    本機 Windows 也是透過 OpenCore 選單開機，而 OpenCore 的 ACPI 修改
 *    會套用到之後載入的任何作業系統。若不加此判斷，Windows 下該裝置
 *    也會一併消失。非 Darwin 時回傳 0x0F（存在、啟用、正常、解碼資源）。
 *
 * 作法：原始 DSDT 中該裝置層沒有 _STA，因此直接新增不會撞名，
 *       也不需要任何 ACPI rename patch。注意路徑是 PC00 不是 PCI0。
 */
DefinitionBlock ("", "SSDT", 2, "ACDT", "DXDCI", 0x00000000)
{
    External (_SB_.PC00.XDCI, DeviceObj)

    Scope (_SB.PC00.XDCI)
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
