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

#ifndef OVERRIDE_PLACEMENT
#include "common.h"
#include "pages.h"
#include "gui/gui.h"
#include "../../protocol/interface.h"
#include "../../protocol/iface_cyrf6936.h"

enum {
    ROW_1_X = 0,
    ROW_2_X = 0,
    ROW_3_X = 0,
    ROW_4_X = 0,
    ROW_5_X = 0,
};

#define ROW_1_Y (LINE_HEIGHT + 2)
#define ROW_2_Y (LINE_HEIGHT + ROW_1_Y)
#define ROW_3_Y (LINE_HEIGHT + ROW_2_Y +6)
#define ROW_4_Y (LINE_HEIGHT + ROW_3_Y -1)

#endif //OVERRIDE_PLACEMENT



static struct usb_page  * const up = &pagemem.u.usb_page;
static struct about_obj * const gui = &gui_objs.u.about;

static const char *lbl2_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;

    tempstring_cpy((const char *) _tr("Deviation FW version:"));
    return tempstring;
}

static const char *lbl4_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;

    CYRF_Reset();
    u8 Power = CYRF_MaxPower();
    sprintf(tempstring, "Power  : %s\n",Power == CYRF_PWR_100MW ? "100mW" : "10mW" );
    return tempstring;
}
static const char *lbl5_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;

    u8 mfgdata[6];

    CYRF_GetMfgData(mfgdata);
    sprintf(tempstring ,"CYRF Mfg: %02X %02X %02X %02X %02X %02X\n",
            mfgdata[0],
            mfgdata[1],
            mfgdata[2],
            mfgdata[3],
            mfgdata[4],
            mfgdata[5]);
    

    return tempstring;
}

void PAGE_AboutInit(int page)
{
    (void)page;
    PAGE_RemoveAllObjects();
    PAGE_ShowHeader(PAGE_GetName(PAGEID_ABOUT));

    
    GUI_CreateLabelBox(&gui->label[1], ROW_1_X, ROW_1_Y, LCD_WIDTH, LINE_HEIGHT, &DEFAULT_FONT, lbl2_cb, NULL, NULL);
    GUI_CreateLabelBox(&gui->label[2], ROW_2_X, ROW_2_Y, LCD_WIDTH, LINE_HEIGHT, &TINY_FONT, NULL, NULL, _tr_noop(DeviationVersion));

    GUI_CreateLabelBox(&gui->label[3], ROW_3_X, ROW_3_Y, LCD_WIDTH, LINE_HEIGHT, &DEFAULT_FONT, lbl4_cb, NULL, NULL);
    GUI_CreateLabelBox(&gui->label[4], ROW_4_X, ROW_4_Y, LCD_WIDTH, LINE_HEIGHT, &TINY_FONT, lbl5_cb,NULL, NULL);
    
/*
    u8 tmp[12];

    printf("BootLoader    : '%s'\n",tmp);
    printf("SPI Flash     : '%X'\n",(unsigned int)SPIFlash_ReadID());


*/
}
