/*
 * btusbfix — 讓 BrcmPatchRAM 寫進卡 RAM 的韌體生效，不必讓整台機器睡眠。
 *
 * BrcmPatchRAM 結尾只發 HCI_RESET，本機這張卡不會因此自行重新枚舉，USB
 * descriptor 仍是 ROM 模式的 0a5c:21ff，macOS 看不到 HCI controller。
 * 這裡的假設是「讓 macOS 重讀 descriptor，但別讓卡斷電」—— 後來證實不成立：
 * descriptor 重讀了卡仍回報 21ff，只有系統睡眠能讓它切換（見 藍牙韌體排查.md 第二節）。
 *
 *   suspend  USBDeviceSuspend(TRUE/FALSE) —— 模擬睡眠喚醒對裝置做的事
 *   reset    ResetDevice() —— 發 USB reset 訊號但不 power cycle
 *
 * USBDeviceReEnumerate 一樣無效：韌體保得住（BrcmPatchRAM 回報 update not needed），
 * 但裝置重新枚舉後仍是 21ff。
 *
 * 編譯：clang -O2 -Wall -o btusbfix btusbfix.c -framework IOKit -framework CoreFoundation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

#define TARGET_VID 0x0a5c
#define TARGET_PID 0x21ff

static void usage(const char *p)
{
    fprintf(stderr, "usage: %s <suspend|reset> [delay_seconds]\n", p);
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(argv[0]); return 2; }
    int do_suspend = (strcmp(argv[1], "suspend") == 0);
    int do_reset   = (strcmp(argv[1], "reset")   == 0);
    if (!do_suspend && !do_reset) { usage(argv[0]); return 2; }

    /* 開機時 BrcmPatchRAM 實測約 4.4 秒寫完韌體；太早動手會讓韌體白寫 */
    unsigned int delay = (argc > 2) ? (unsigned int)strtoul(argv[2], NULL, 10) : 8;
    if (delay) sleep(delay);

    CFMutableDictionaryRef match = IOServiceMatching(kIOUSBDeviceClassName);
    if (!match) { fprintf(stderr, "btusbfix: IOServiceMatching failed\n"); return 1; }

    SInt32 vid = TARGET_VID, pid = TARGET_PID;
    CFNumberRef vidRef = CFNumberCreate(NULL, kCFNumberSInt32Type, &vid);
    CFNumberRef pidRef = CFNumberCreate(NULL, kCFNumberSInt32Type, &pid);
    CFDictionarySetValue(match, CFSTR(kUSBVendorID), vidRef);
    CFDictionarySetValue(match, CFSTR(kUSBProductID), pidRef);
    CFRelease(vidRef);
    CFRelease(pidRef);

    io_iterator_t iter = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter) != KERN_SUCCESS) {
        fprintf(stderr, "btusbfix: IOServiceGetMatchingServices failed\n");
        return 1;
    }
    io_service_t dev = IOIteratorNext(iter);
    IOObjectRelease(iter);

    /* 找不到就代表韌體已生效（裝置已是 05ac:8290）—— 不需要做事 */
    if (!dev) {
        printf("btusbfix: no %04x:%04x found, nothing to do\n", TARGET_VID, TARGET_PID);
        return 0;
    }

    IOCFPlugInInterface **plugin = NULL;
    SInt32 score = 0;
    kern_return_t kr = IOCreatePlugInInterfaceForService(
        dev, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin, &score);
    IOObjectRelease(dev);
    if (kr != KERN_SUCCESS || !plugin) {
        fprintf(stderr, "btusbfix: IOCreatePlugInInterfaceForService failed (0x%08x)\n", kr);
        return 1;
    }

    IOUSBDeviceInterface182 **usb = NULL;
    HRESULT hr = (*plugin)->QueryInterface(
        plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID182), (LPVOID *)&usb);
    IODestroyPlugInInterface(plugin);
    if (hr != 0 || !usb) {
        fprintf(stderr, "btusbfix: QueryInterface(182) failed (0x%lx)\n", (long)hr);
        return 1;
    }

    /* 這些操作要求裝置已開啟；BrcmPatchRAM3 可能還持有它，故 fallback 用 Seize 搶 */
    kr = (*usb)->USBDeviceOpen(usb);
    if (kr != KERN_SUCCESS) {
        kr = (*usb)->USBDeviceOpenSeize(usb);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "btusbfix: USBDeviceOpen/Seize failed (0x%08x)\n", kr);
            (*usb)->Release(usb);
            return 1;
        }
    }

    if (do_suspend) {
        kr = (*usb)->USBDeviceSuspend(usb, TRUE);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "btusbfix: suspend failed (0x%08x)\n", kr);
            (*usb)->USBDeviceClose(usb);
            (*usb)->Release(usb);
            return 1;
        }
        usleep(700000);   /* 給卡一點時間，比照睡眠喚醒的間隔 */
        kr = (*usb)->USBDeviceSuspend(usb, FALSE);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "btusbfix: resume failed (0x%08x)\n", kr);
            (*usb)->USBDeviceClose(usb);
            (*usb)->Release(usb);
            return 1;
        }
        (*usb)->USBDeviceClose(usb);
        (*usb)->Release(usb);
        printf("btusbfix: suspend/resume done on %04x:%04x\n", TARGET_VID, TARGET_PID);
        return 0;
    }

    kr = (*usb)->ResetDevice(usb);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "btusbfix: ResetDevice failed (0x%08x)\n", kr);
        (*usb)->USBDeviceClose(usb);
        (*usb)->Release(usb);
        return 1;
    }
    /* reset 後裝置可能已 detach，close 失敗無所謂 */
    (*usb)->USBDeviceClose(usb);
    (*usb)->Release(usb);
    printf("btusbfix: ResetDevice done on %04x:%04x\n", TARGET_VID, TARGET_PID);
    return 0;
}
