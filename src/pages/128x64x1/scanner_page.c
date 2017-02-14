/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Deviation is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Deviation.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "protocol/interface.h"
#include "pages.h"
#include "config/model.h"

#if HAS_SCANNER

#define ENABLE_SCAN_CC2500 (defined(PROTO_HAS_CC2500) /* && 0*/ )

#define MAX_SAMPLING 20
#define RSSI_OFFSET   95   // offset for displayed data

#define MIN(x,y) ((x)<(y)?(x):(y))


static struct scanner_page * const sp = &pagemem.u.scanner_page;
static struct scanner_obj * const gui = &gui_objs.u.scanner;


enum {
        SCANBARWIDTH   = (LCD_WIDTH / (MAX_RADIOCHANNEL - MIN_RADIOCHANNEL)),
        SCANBARXOFFSET = ((LCD_WIDTH - SCANBARWIDTH * (MAX_RADIOCHANNEL - MIN_RADIOCHANNEL))/2),
        SCANBARHEIGHT  = (32), // 32
        SCANBAR_TOP    = LCD_HEIGHT - SCANBARHEIGHT,
};

static s16 get_stick(u8 init){
    s32 ele = CHAN_ReadInput(MIXER_MapChannel(INP_ELEVATOR));
    if(init)
        sp->stick=ele;
    
    return (sp->stick-ele) * 100 / CHAN_MAX_VALUE; // in %
}

static void redraw_labels(){
    GUI_Redraw((guiObject_t *)&gui->freq_l);
    GUI_Redraw((guiObject_t *)&gui->freq_m);
    GUI_Redraw((guiObject_t *)&gui->freq_r);
}

static u16 scan_cb()// timer procedure
{
    int delay=20;
    u8 ch = sp->channel+sp->offset;
    
    if(sp->scanState == 0) {
        if(sp->time_to_scan == 0) {
            if(sp->receiver) {
                CC2500_WriteReg(CC2500_0A_CHANNR, ch);            // set channel
                CC2500_WriteReg(CC2500_25_FSCAL1, sp->cal_CC2500[ch]);       // restore calibration value for this channel
                delay = 20; 
            } else {
                u8 shf = (LCD_WIDTH - sp->max)/2;
                if(ch>shf && ch < (sp->max + shf))
                    CYRF_ConfigRFChannel(ch - shf);
                if(sp->attenuator) {
                    CYRF_WriteRegister(CYRF_06_RX_CFG, 0x0A);
                } else {
                    CYRF_WriteRegister(CYRF_06_RX_CFG, 0x4A);
                }
                delay = 350; //slow channel require 270usec for synthesizer to settle 
            }
            sp->time_to_scan = 1;
        }
        sp->scanState = 1;
    } else {
        int rssi=0;
        
        if(sp->receiver) {
            u16 RSSI_max = 1;
            for(u16 j=0; j<MAX_SAMPLING; j++){ 
                u8 RSSI_data = CC2500_ReadReg(CC2500_34_RSSI);

                // convert RSSI data from 2's complement to signed decimal
                if (RSSI_data >= 128) {
                    rssi = (RSSI_data - 256) / 2 - 70;
                } else {
                    rssi = RSSI_data / 2 - 70;
                }
                if (rssi > RSSI_max) RSSI_max = rssi; // keep maximum
            }
            rssi = (RSSI_max+RSSI_OFFSET) / 4; // 
        } else {
            if ( !(CYRF_ReadRegister(CYRF_05_RX_CTRL) & 0x80)) {
                CYRF_WriteRegister(CYRF_05_RX_CTRL, 0x80); //Prepare to receive
                Delay(10);
                CYRF_ReadRegister(CYRF_13_RSSI); //dummy read
                Delay(15);
            }

#if defined(EMULATOR) 
            rssi = sp->channel>32?64-sp->channel:sp->channel;
            if(rssi <0) rssi=16;
        
            rssi &= 0x1F;
#else
            u8 shf = (LCD_WIDTH - sp->max)/2;
            if(ch>shf && ch < (sp->max + shf))
                rssi = CYRF_ReadRegister(CYRF_13_RSSI) & 0x1F;
#endif
        }
        
        ch = sp->channel;
        if(sp->scan_mode) {
            sp->channelnoise[ch] = (sp->channelnoise[ch] + rssi) / 2;
        } else {
            if(rssi > sp->channelnoise[ch])
                sp->channelnoise[ch] = rssi;
        }

        sp->scanState++;
        delay = 300;
        if(sp->scanState == 5) {
            sp->scanState = 0;
            delay = 50;
        }        
        
        
        s8 mov = get_stick(0); 

        s16 pos=sp->pos;

        if(mov) {
            u8 change=0;

            LCD_DrawLine( pos, SCANBARHEIGHT+1, pos, SCANBARHEIGHT-3, 0);
            pos += mov;
            if(pos<0) { // move scale left
                if(sp->offset)    sp->offset -= -pos;
                pos=0;
                change=1;
            }
            
            if(pos>=LCD_WIDTH){ // move scale right
                if(sp->offset < (sp->max - LCD_WIDTH) )    sp->offset+= (pos-LCD_WIDTH+1);
                pos=LCD_WIDTH-1;
                change=1;
            }

            if(change)  redraw_labels();
            else        GUI_Redraw((guiObject_t *)&gui->freq_m);
        }
        LCD_DrawLine( pos, SCANBARHEIGHT+1, pos, SCANBARHEIGHT-3, 1);

    }
    return delay;
}

static s32 show_bar_cb(void *data)
{
    long ch = (long)data;
    return sp->channelnoise[ch];
}

static const char *enablestr_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    return sp->enable ? _tr("Stop") : _tr("Start");
}

static const char *modestr_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    return sp->scan_mode ? _tr("Average") : _tr("Peak");
}

static const char *attstr_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    return sp->attenuator ? _tr("A -20dB") : _tr("A 0dB");
}

static void press_enable_cb(guiObject_t *obj, const void *data)
{
    (void)data;
#ifndef MODULAR
    sp->enable ^= 1;
    if (sp->enable) {
    
        get_stick(1); // remember stick pos on start
        
        if(sp->receiver) {
#if ENABLE_SCAN_CC2500
// CC2500 code inspired by http://forum.rcdesign.ru/f8/thread397991.html


            PROTOCOL_DeInit();
            FRSKYX_Cmds(0);
            PROTOCOL_SetBindState(0); //Disable binding message
            CLOCK_StopTimer();

            CC2500_WriteReg(CC2500_SRES, CC2500_SNOP);      // software reset for CC2500
            CC2500_WriteReg(CC2500_0B_FSCTRL1, 0x0F);   // Frequency Synthesizer Control (0x0F)
            CC2500_WriteReg(CC2500_08_PKTCTRL0, 0x12);  // Packet Automation Control (0x12)
            CC2500_WriteReg(CC2500_0D_FREQ2, 0x5C);     // Frequency control word, high byte
            CC2500_WriteReg(CC2500_0E_FREQ1, 0x4E);     // Frequency control word, middle byte
            CC2500_WriteReg(CC2500_0F_FREQ0, 0xDE);     // Frequency control word, low byte
            CC2500_WriteReg(CC2500_10_MDMCFG4, 0x0D);   // Modem Configuration
            CC2500_WriteReg(CC2500_11_MDMCFG3, 0x3B);   // Modem Configuration (0x3B)
            CC2500_WriteReg(CC2500_12_MDMCFG2, 0x00);   // Modem Configuration 0x30 - OOK modulation, 0x00 - FSK modulation (better sensitivity)
            CC2500_WriteReg(CC2500_13_MDMCFG1, 0x23);   // Modem Configuration
            CC2500_WriteReg(CC2500_14_MDMCFG0, 0xFF);   // Modem Configuration (0xFF)
            CC2500_WriteReg(CC2500_17_MCSM1, 0x0F);     // Always stay in RX mode
            CC2500_WriteReg(CC2500_18_MCSM0, 0x04);     // Main Radio Control State Machine Configuration (0x04)
            CC2500_WriteReg(CC2500_19_FOCCFG, 0x15);    // Frequency Offset Compensation configuration
            CC2500_WriteReg(CC2500_1B_AGCCTRL2, 0x83);  // AGC Control (0x83)
            CC2500_WriteReg(CC2500_1C_AGCCTRL1, 0x00);  // AGC Control
            CC2500_WriteReg(CC2500_1D_AGCCTRL0, 0x91);  // AGC Control
            CC2500_WriteReg(CC2500_23_FSCAL3, 0xEA);    // Frequency Synthesizer Calibration
            CC2500_WriteReg(CC2500_24_FSCAL2, 0x0A);    // Frequency Synthesizer Calibration
            CC2500_WriteReg(CC2500_25_FSCAL1, 0x00);    // Frequency Synthesizer Calibration
            CC2500_WriteReg(CC2500_26_FSCAL0, 0x11);    // Frequency Synthesizer Calibration


            // calibration procedure
            // collect and save calibration data for each displayed channel-
            for (int i = 0; i < MAX_CC2500_CHAN; i++)  {
                CC2500_WriteReg(CC2500_0A_CHANNR, i);            // set channel
                CC2500_WriteReg(CC2500_SIDLE, CC2500_SNOP);          // idle mode
                CC2500_WriteReg(CC2500_SCAL, CC2500_SNOP);           // start manual calibration
                Delay(2);                            // wait for calibration
                sp->cal_CC2500[i] = CC2500_ReadReg(CC2500_25_FSCAL1);         // read calibration value
            }

            CC2500_WriteReg(CC2500_0A_CHANNR, 0x00);           // set channel
            CC2500_WriteReg(CC2500_SFSTXON, CC2500_SNOP);          // calibrate and wait
            Delay(2);               // settling time, refer to datasheet
            CC2500_WriteReg(CC2500_SRX, CC2500_SNOP);              // enable rx

            CC2500_SetTxRxMode(RX_EN); //Receive mode
  
            CLOCK_StartTimer(1250, scan_cb);
            
#endif
        }else {

            PROTOCOL_DeInit();
            DEVO_Cmds(0);  //Switch to DEVO configuration
            PROTOCOL_SetBindState(0); //Disable binding message
            CLOCK_StopTimer();
            CYRF_SetTxRxMode(RX_EN); //Receive mode
            CLOCK_StartTimer(1250, scan_cb);
        }
    } else {
        PROTOCOL_Init(0);
    }
#endif




#if ENABLE_SCAN_CC2500
    if(sp->use_cc2500)
        GUI_ButtonEnable((guiObject_t *)&gui->receiver, sp->enable);
#endif
    GUI_Redraw(obj);
}

static void press_mode_cb(guiObject_t *obj, const void *data)
{
    (void)data;
#ifndef MODULAR
    sp->scan_mode ^= 1;
#endif
    GUI_Redraw(obj);
}


#if ENABLE_SCAN_CC2500

static const char *recvstr_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    return sp->receiver ? _tr("CC2500") : _tr("CYRF");
}


static void press_recv_cb(guiObject_t *obj, const void *data)
{
    (void)data;
#ifndef MODULAR
    sp->receiver ^= 1;
#endif
    if(sp->receiver){
        sp->min = 0;
        sp->max = MAX_CC2500_CHAN;
    } else {
        sp->min = MIN_RADIOCHANNEL;
        sp->max = MAX_RADIOCHANNEL;
    }
    
    redraw_labels();
    GUI_Redraw(obj);
}

#endif


static void press_attenuator_cb(guiObject_t *obj, const void *data)
{
    (void)data;
#ifndef MODULAR
    sp->attenuator ^= 1;
#endif
    GUI_Redraw(obj);
}

static u16 get_freq(u8 pos){
    u16 freq;

    if(sp->receiver) {
        freq= .405456 * pos + 2400.009949; // cc2500
    } else {
        u8 shf = (LCD_WIDTH - sp->max)/2;
        freq= 1 * (pos-shf) + 2400; // cyrf
    }
    return freq;
}


static const char *lfrq_cb(guiObject_t *obj, const void *data)
{
    (void)obj;    
    (void)data;
    
    
    sprintf(tempstring, "%d", get_freq(sp->offset));
    return tempstring;    
}

static const char *mfrq_cb(guiObject_t *obj, const void *data)
{
    (void)obj;    
    (void)data;
    
    sprintf(tempstring, "%d", get_freq(sp->offset+ sp->pos));
    return tempstring;    
}


static const char *rfrq_cb(guiObject_t *obj, const void *data)
{
    (void)obj;    
    (void)data;
    
    sprintf(tempstring, "%d", get_freq(sp->offset + MIN(sp->max, LCD_WIDTH) ));
    return tempstring;    
}



#define  BUTTON_Y  (LINE_HEIGHT + 1)
#define  LABEL_TOP  (BUTTON_Y + LINE_HEIGHT+1)
enum {
    BUTTON1_WIDTH  = 43,
    BUTTON2_WIDTH  = 35,
    BUTTON3_WIDTH  = 42,
    BUTTON4_WIDTH  = 48,
    BUTTON_GAP    = 1,
    BUTTON1_X     = LCD_WIDTH - (BUTTON1_WIDTH + BUTTON_GAP) - (BUTTON2_WIDTH),
    BUTTON2_X     = LCD_WIDTH - (BUTTON2_WIDTH),
    BUTTON3_X     = LCD_WIDTH - (BUTTON1_WIDTH + BUTTON_GAP) - (BUTTON2_WIDTH) - (BUTTON3_WIDTH + BUTTON_GAP) ,
    
    BUTTON4_X = LCD_WIDTH - (BUTTON4_WIDTH),
    BUTTON4_Y = 0,
    
    LABEL_W = 28,
    LABEL_CHAN_H = 6,
};

void scan_one(){
#ifndef MODULAR
    if(! sp->enable)
        return;
    GUI_Redraw(&gui->bar[sp->channel]);
    //printf("%02X : %d\n",sp->channel,sp->channelnoise[sp->channel]);
    sp->channel++;
    if(sp->channel == LCD_WIDTH)
        sp->channel = 0;
    sp->channelnoise[sp->channel] = 0;
    sp->time_to_scan = 0;
#endif
}

void idle_callback() {
    u32 ms = CLOCK_getms();
    
    if(ms - sp->last_call > 10) {
        scan_one();
        sp->last_call = ms;
    }

}


void PAGE_ScannerInit(int page)
{
    u8 i;
    (void)page;
    
    PAGE_SetModal(0);
    PAGE_ShowHeader(PAGE_GetName(PAGEID_SCANNER));

    sp->enable = 0;
    sp->scan_mode = 0;
    sp->attenuator = 0;
    sp->receiver=0;
    sp->channel = 0;
    sp->time_to_scan = 0;
    sp->scanState = 0;
    sp->pos = 0;
    sp->last_call=0;

    sp->min = MIN_RADIOCHANNEL;
    sp->max = MAX_RADIOCHANNEL;

    sp->use_cc2500=PROTOCOL_HasModule(PROTOCOL_FRSKYX);


/*guiButton_t *button, u16 x, u16 y, u16 width, u16 height, const struct LabelDesc *desc,
    const char *(*strCallback)(struct guiObject *, const void *),
    void (*CallBack)(struct guiObject *obj, const void *data), const void *cb_data)
*/    

    GUI_CreateButtonPlateText(&gui->enable, BUTTON2_X, BUTTON_Y, BUTTON2_WIDTH, LINE_HEIGHT, &DEFAULT_FONT,
                              enablestr_cb, press_enable_cb, NULL);

    GUI_CreateButtonPlateText(&gui->scan_mode, BUTTON1_X, BUTTON_Y, BUTTON1_WIDTH, LINE_HEIGHT, &DEFAULT_FONT,
                              modestr_cb, press_mode_cb, NULL);

    GUI_CreateButtonPlateText(&gui->attenuator, BUTTON3_X, BUTTON_Y, BUTTON3_WIDTH, LINE_HEIGHT, &DEFAULT_FONT,
                              attstr_cb, press_attenuator_cb, NULL);
                              
#if ENABLE_SCAN_CC2500
    if(sp->use_cc2500)
        GUI_CreateButtonPlateText(&gui->receiver, BUTTON4_X, BUTTON4_Y, BUTTON4_WIDTH, LINE_HEIGHT, &DEFAULT_FONT,
                              recvstr_cb, press_recv_cb, NULL);
#endif                  

    struct LabelDesc labelValue = MICRO_FONT;  // only digits, can use smaller font to show more channels

    u8 height = LABEL_CHAN_H;

    GUI_CreateLabelBox(&gui->freq_l, 0, LABEL_TOP,
        LABEL_W, height, &labelValue, lfrq_cb, NULL,  NULL);


    GUI_CreateLabelBox(&gui->freq_m, (LCD_WIDTH-LABEL_W)/2, LABEL_TOP,
        LABEL_W, height, &labelValue, mfrq_cb, NULL,  NULL);


    labelValue.style = LABEL_RIGHT;
    GUI_CreateLabelBox(&gui->freq_r, LCD_WIDTH-LABEL_W, LABEL_TOP,
        LABEL_W, height, &labelValue, rfrq_cb, NULL, NULL);

    for(i = 0; i < LCD_WIDTH; i++) {
        sp->channelnoise[i] = 0;
        GUI_CreateBarGraph(&gui->bar[i], i * SCANBARWIDTH, SCANBAR_TOP, SCANBARWIDTH, SCANBARHEIGHT, 0, 0x20, BAR_VERTICAL, show_bar_cb, (void *)((long)i));
        GUI_Redraw(&gui->bar[i]);
    }
    
    
    on_idle_loop=0; // побыстрее шевелиться будем
}

void PAGE_ScannerEvent() // on regular execution in EventLoop() but only 10 times per second
{
    scan_one();
}

void PAGE_ScannerExit()
{
    on_idle_loop=0;
#ifndef MODULAR
    if(sp->enable)
        PROTOCOL_Init(0);
#endif
}

#endif //HAS_SCANNER
