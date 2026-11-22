void timer_init(void);       // 設第一次鬧鐘 + 打開 sie.STIE 和 sstatus.SIE
void timer_interrupt(void);  // 處理一次時鐘中斷
