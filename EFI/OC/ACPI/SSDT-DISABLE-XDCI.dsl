/*
 * SSDT-DISABLE-XDCI
 *
 * 停用 _SB.PC00.XDCI（Intel PCH USB Device Controller / OTG，PCI 00:14.1）
 *
 * 原因：桌機不會把自己當成 USB 周邊使用，此控制器毫無用途，
 *       但它的 _PRW 回傳 GPRW(0x6D, 0x04)，會在睡眠後數十秒
 *       觸發 DarkWake，導致機器無法維持深層睡眠。
 *
 * 作法：原始 DSDT 中 XDCI 沒有 _STA 方法（僅 _ADR/_S0W/_PRW/_DSW/_DSM），
 *       因此直接新增一個回傳 0 的 _STA 即可隱藏該裝置，不會與既有名稱衝突，
 *       也不需要任何 ACPI rename patch。
 *
 * 為何不用 SSDT-GPRW：GPE 0x6D 由 GLAN / XHCI / XDCI / HDAS / CNVW 共用，
 *       改動 GPRW 會連帶停用 USB 與網路喚醒（鍵盤滑鼠將無法喚醒電腦）。
 */
DefinitionBlock ("", "SSDT", 2, "ACDT", "DXDCI", 0x00000000)
{
    External (_SB_.PC00.XDCI, DeviceObj)

    Scope (_SB.PC00.XDCI)
    {
        Method (_STA, 0, NotSerialized)
        {
            Return (Zero)
        }
    }
}
