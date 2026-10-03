/*
 * hubport — 從 hub 層對藍牙卡所在的 port 動手，試著迫使 macOS 重新枚舉裝置。
 *
 * 對單一裝置做 ResetDevice / Suspend 時，macOS 拿快取的 descriptor 重新 attach；
 * 這裡改成直接對 Genesys hub 發 USB 2.0 hub class request（USB 2.0 spec 11.24），
 * 繞過 macOS 對該裝置的抽象層。
 *
 *   status              讀 hub descriptor 與每個 port 的狀態（唯讀）
 *   desc <locationID>   向裝置要 device descriptor（唯讀；macOS 會用快取回答，不代表卡的現況）
 *   ping <locationID>   GET_STATUS(device)，會真的發到線上，用來判斷裝置是否還在回應
 *   reenum <locationID> USBDeviceReEnumerate —— 由 macOS 自己 terminate 並重新枚舉裝置
 *   reset <port>        SetPortFeature(PORT_RESET)
 *   disable <port>      ClearPortFeature(PORT_ENABLE)
 *   suspend <port> [ms]    SetPortFeature(PORT_SUSPEND) → 等待（預設 700ms）→ ClearPortFeature(PORT_SUSPEND)
 *   suspreset <port> [ms]  SetPortFeature(PORT_SUSPEND) → 等待 → SetPortFeature(PORT_RESET)（在 suspend 中直接 reset）
 *   powercycle <port>   ClearPortFeature(PORT_POWER) → 1 秒 → SetPortFeature(PORT_POWER)
 *
 * 編譯：clang -O2 -Wall -o hubport hubport.c -framework IOKit -framework CoreFoundation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

#define HUB_LOCATION 0x14a00000u

/* USB 2.0 spec Table 11-17 */
#define PORT_ENABLE  1
#define PORT_SUSPEND 2
#define PORT_RESET   4
#define PORT_POWER   8

static IOUSBDeviceInterface187 **open_by_location(UInt32 loc)
{
    CFMutableDictionaryRef match = IOServiceMatching(kIOUSBDeviceClassName);
    io_iterator_t iter = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter) != KERN_SUCCESS)
        return NULL;

    io_service_t dev, found = IO_OBJECT_NULL;
    while ((dev = IOIteratorNext(iter))) {
        CFTypeRef ref = IORegistryEntryCreateCFProperty(dev, CFSTR(kUSBDevicePropertyLocationID), NULL, 0);
        UInt32 v = 0;
        if (ref) {
            CFNumberGetValue(ref, kCFNumberSInt32Type, &v);
            CFRelease(ref);
        }
        if (v == loc && !found) found = dev;
        else IOObjectRelease(dev);
    }
    IOObjectRelease(iter);
    if (!found) { fprintf(stderr, "hubport: no device at 0x%08x\n", loc); return NULL; }

    IOCFPlugInInterface **plugin = NULL;
    SInt32 score = 0;
    kern_return_t kr = IOCreatePlugInInterfaceForService(
        found, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin, &score);
    IOObjectRelease(found);
    if (kr != KERN_SUCCESS || !plugin) {
        fprintf(stderr, "hubport: IOCreatePlugInInterfaceForService failed (0x%08x)\n", kr);
        return NULL;
    }
    IOUSBDeviceInterface187 **usb = NULL;
    (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID187), (LPVOID *)&usb);
    IODestroyPlugInInterface(plugin);
    return usb;
}

static IOReturn ctrl(IOUSBDeviceInterface187 **usb, UInt8 type, UInt8 req,
                     UInt16 value, UInt16 index, void *buf, UInt16 len)
{
    IOUSBDevRequest r = {
        .bmRequestType = type, .bRequest = req, .wValue = value,
        .wIndex = index, .wLength = len, .pData = buf,
    };
    IOReturn kr = (*usb)->DeviceRequest(usb, &r);
    /* 有些 control request 要求先開啟裝置；hub 被 AppleUSBHub 持有，所以只在需要時才 Seize */
    if (kr == kIOReturnNotOpen || kr == kIOReturnExclusiveAccess) {
        if ((*usb)->USBDeviceOpen(usb) != KERN_SUCCESS)
            (*usb)->USBDeviceOpenSeize(usb);
        kr = (*usb)->DeviceRequest(usb, &r);
    }
    return kr;
}

static int port_status(IOUSBDeviceInterface187 **hub, int port)
{
    UInt16 st[2] = {0};
    IOReturn kr = ctrl(hub, 0xA3, kUSBRqGetStatus, 0, port, st, sizeof st);
    if (kr) { printf("  port %d: GET_STATUS failed 0x%08x\n", port, kr); return -1; }
    UInt16 s = USBToHostWord(st[0]), c = USBToHostWord(st[1]);
    printf("  port %d: status 0x%04x change 0x%04x [%s%s%s%s%s%s]\n", port, s, c,
           s & 0x0001 ? "connect " : "", s & 0x0002 ? "enable " : "",
           s & 0x0004 ? "suspend " : "", s & 0x0010 ? "reset " : "",
           s & 0x0100 ? "power " : "", s & 0x0400 ? "high-speed" : "");
    return 0;
}

static int cmd_status(void)
{
    IOUSBDeviceInterface187 **hub = open_by_location(HUB_LOCATION);
    if (!hub) return 1;
    UInt8 d[16] = {0};
    IOReturn kr = ctrl(hub, 0xA0, kUSBRqGetDescriptor, 0x29 << 8, 0, d, sizeof d);
    if (kr) { fprintf(stderr, "hubport: hub descriptor failed 0x%08x\n", kr); return 1; }
    UInt16 ch = d[3] | (d[4] << 8);
    printf("hub: %d ports, power switching = %s, PwrOn2PwrGood = %d ms\n", d[2],
           (ch & 3) == 0 ? "ganged" : (ch & 3) == 1 ? "per-port" : "none", d[5] * 2);
    for (int p = 1; p <= d[2]; p++) port_status(hub, p);
    (*hub)->Release(hub);
    return 0;
}

static int cmd_desc(UInt32 loc)
{
    IOUSBDeviceInterface187 **usb = open_by_location(loc);
    if (!usb) return 1;
    IOUSBDeviceDescriptor d = {0};
    IOReturn kr = ctrl(usb, 0x80, kUSBRqGetDescriptor, kUSBDeviceDesc << 8, 0, &d, sizeof d);
    if (kr) { fprintf(stderr, "hubport: GET_DESCRIPTOR failed 0x%08x\n", kr); return 1; }
    printf("device 0x%08x reports %04x:%04x bcdDevice %04x\n", loc,
           USBToHostWord(d.idVendor), USBToHostWord(d.idProduct), USBToHostWord(d.bcdDevice));
    (*usb)->Release(usb);
    return 0;
}

static int cmd_ping(UInt32 loc)
{
    IOUSBDeviceInterface187 **usb = open_by_location(loc);
    if (!usb) return 1;
    UInt16 st = 0;
    IOReturn kr = ctrl(usb, 0x80, kUSBRqGetStatus, 0, 0, &st, sizeof st);
    printf("device 0x%08x GET_STATUS: 0x%08x status 0x%04x\n", loc, kr, USBToHostWord(st));
    (*usb)->Release(usb);
    return kr ? 1 : 0;
}

static int cmd_reenum(UInt32 loc)
{
    IOUSBDeviceInterface187 **usb = open_by_location(loc);
    if (!usb) return 1;
    if ((*usb)->USBDeviceOpen(usb) != KERN_SUCCESS && (*usb)->USBDeviceOpenSeize(usb) != KERN_SUCCESS) {
        fprintf(stderr, "hubport: USBDeviceOpen/Seize failed\n");
        (*usb)->Release(usb);
        return 1;
    }
    IOReturn kr = (*usb)->USBDeviceReEnumerate(usb, 0);
    printf("device 0x%08x ReEnumerate: 0x%08x\n", loc, kr);
    /* 重新枚舉後原物件已 terminate，close 失敗無所謂 */
    (*usb)->USBDeviceClose(usb);
    (*usb)->Release(usb);
    return kr ? 1 : 0;
}

static int cmd_port(const char *op, int port, unsigned ms)
{
    IOUSBDeviceInterface187 **hub = open_by_location(HUB_LOCATION);
    if (!hub) return 1;
    printf("before:\n"); port_status(hub, port);

    IOReturn kr = 0;
    if (!strcmp(op, "reset")) {
        kr = ctrl(hub, 0x23, kUSBRqSetFeature, PORT_RESET, port, NULL, 0);
    } else if (!strcmp(op, "disable")) {
        kr = ctrl(hub, 0x23, kUSBRqClearFeature, PORT_ENABLE, port, NULL, 0);
    } else if (!strcmp(op, "suspend")) {
        kr = ctrl(hub, 0x23, kUSBRqSetFeature, PORT_SUSPEND, port, NULL, 0);
        usleep(ms * 1000);
        if (!kr) kr = ctrl(hub, 0x23, kUSBRqClearFeature, PORT_SUSPEND, port, NULL, 0);
    } else if (!strcmp(op, "suspreset")) {
        kr = ctrl(hub, 0x23, kUSBRqSetFeature, PORT_SUSPEND, port, NULL, 0);
        usleep(ms * 1000);
        if (!kr) kr = ctrl(hub, 0x23, kUSBRqSetFeature, PORT_RESET, port, NULL, 0);
    } else if (!strcmp(op, "powercycle")) {
        kr = ctrl(hub, 0x23, kUSBRqClearFeature, PORT_POWER, port, NULL, 0);
        sleep(1);
        if (!kr) kr = ctrl(hub, 0x23, kUSBRqSetFeature, PORT_POWER, port, NULL, 0);
    } else {
        fprintf(stderr, "hubport: unknown op %s\n", op);
        return 2;
    }
    printf("%s port %d: 0x%08x\n", op, port, kr);
    usleep(200000);
    printf("after:\n"); port_status(hub, port);
    (*hub)->USBDeviceClose(hub);
    (*hub)->Release(hub);
    return kr ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "status")) return cmd_status();
    if (argc >= 3 && !strcmp(argv[1], "desc")) return cmd_desc((UInt32)strtoul(argv[2], NULL, 0));
    if (argc >= 3 && !strcmp(argv[1], "reenum")) return cmd_reenum((UInt32)strtoul(argv[2], NULL, 0));
    if (argc >= 3 && !strcmp(argv[1], "ping")) return cmd_ping((UInt32)strtoul(argv[2], NULL, 0));
    if (argc >= 3) return cmd_port(argv[1], atoi(argv[2]), argc > 3 ? (unsigned)atoi(argv[3]) : 700);
    fprintf(stderr, "usage: %s status | desc|ping|reenum <locationID> | <reset|disable|suspend|suspreset|powercycle> <port> [ms]\n", argv[0]);
    return 2;
}
