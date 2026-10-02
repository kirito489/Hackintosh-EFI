#!/bin/sh
#
# 開機後補一次睡眠喚醒，讓 BrcmPatchRAM 寫進卡 RAM 的韌體生效。
#
# BrcmPatchRAM 結尾只發 HCI_RESET，本機這張卡不會因此自行重新枚舉，
# 於是 USB descriptor 仍是 ROM 模式的 0a5c:21ff，macOS 看不到 HCI controller。
# suspend/resume 會讓 macOS 重讀 descriptor 而不 reset 卡（韌體保得住），
# 裝置便以 05ac:8290 出現。USB reset 反而會清掉韌體，所以不能用 ReEnumerate。

# 等 BrcmPatchRAM 寫完韌體：實測三次都在開機後 4.4 秒完成（Processing time 1.79x 秒），
# 這裡留約 1.6 秒邊際。調更短會有在韌體寫完前就睡的風險，韌體會白寫。
sleep 6

# 韌體已生效就不必睡 —— 保持 idempotent，也避免沒必要地打斷使用
if ! /usr/sbin/ioreg -r -c IOUSBHostDevice -w0 | grep -q "0a5c:21ff"; then
    echo "$(date '+%F %T') 裝置已非 21ff，無需動作"
    exit 0
fi

# 排定喚醒再睡，不依賴主機板自己醒。
# 25 秒是實測安全值：hibernatemode 3（safe sleep）要把 RAM 寫成 hibernation image，
# 加上各 driver 的 SetState 延遲（AppleHDADriver 單獨就花 1 秒），整個睡眠流程需 13 秒以上。
# 試過 12 秒：喚醒排程在睡眠流程未完成時就觸發，系統只進到 darkwake，USB 不會重新枚舉。
WAKE=$(/bin/date -v+25S '+%m/%d/%y %H:%M:%S')
/usr/bin/pmset schedule wake "$WAKE"
echo "$(date '+%F %T') 排定 $WAKE 喚醒，現在進入睡眠"
/usr/bin/pmset sleepnow
