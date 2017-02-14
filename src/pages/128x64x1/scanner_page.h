#ifndef _SCANNER_PAGE_H_
#define _SCANNER_PAGE_H_

// from CYRF datasheet
#define MIN_RADIOCHANNEL     0
#define MAX_RADIOCHANNEL     98
#define MAX_CC2500_CHAN 205

struct scanner_page {
    u8 channelnoise[128]; // на весь экран
    u8 channel;
    u8 time_to_scan;
    u8 enable;
    u8 scan_mode;
    u8 receiver;
    u8 attenuator;
    u8 cal_CC2500[MAX_CC2500_CHAN];
    u8 offset;  // offset from scale start
    u8 scanState;
    u8 use_cc2500;
    u8 min;     // minimal channel
    u8 max;     // maximal channel
    u8 pos;     // position of mark on LCD
    s32 stick;  // stick position
    u32 last_call;
};
#endif
