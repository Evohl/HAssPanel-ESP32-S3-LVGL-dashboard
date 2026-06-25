/**
 * @file lv_conf.h
 * LVGL v8.3.x Konfiguration – Home Assistant Panel
 */

#if 1  /* Auf 0 setzen um Inhalt zu deaktivieren */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0
#define LV_COLOR_SCREEN_TRANSP    0
#define LV_COLOR_MIX_ROUND_OFS    0
#define LV_COLOR_CHROMA_KEY       lv_color_hex(0x00ff00)

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM      0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE    (64U * 1024U)
    #define LV_MEM_ADR     0
    #define LV_MEM_AUTO_DEFRAG  1
#else
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_REALLOC realloc
    #define LV_MEM_CUSTOM_FREE    free
#endif
#define LV_MEM_BUF_MAX_NUM 16

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD   20
#define LV_INDEV_DEF_READ_PERIOD  30

/*================
 * TICK INTERFACE
 ================*/
#define LV_TICK_CUSTOM     1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE  <Arduino.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/*================
 * LOGGING
 ================*/
#define LV_USE_LOG      0
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   0

/*=================
 * ASSERT
 *=================*/
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_ASSERT_HANDLER_INCLUDE   <stdint.h>
#define LV_ASSERT_HANDLER           while(1);

/*====================
   COMPILER SETTINGS
 *====================*/
#define LV_BIG_ENDIAN_SYSTEM    0
#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TASK_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE  4
#define LV_ATTRIBUTE_MEM_ALIGN       __attribute__((aligned(4)))
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning
#define LV_USE_LARGE_COORD  0

/*==================
 *   FONT USAGE
 *==================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  0
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0
#define LV_FONT_UNSCII_8   0
#define LV_FONT_UNSCII_16  0
#define LV_FONT_CUSTOM_DECLARE
#define LV_FONT_DEFAULT    &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE   0
#define LV_USE_FONT_SUBPX       0
#define LV_FONT_SUBPX_BGR       0
#define LV_USE_FONT_COMPRESSED  0

/*=================
 *  TEXT SETTINGS
 *=================*/
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN          0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN  3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI     0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 *  WIDGET USAGE
 *==================*/
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        0
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/*==================
 * EXTRA COMPONENTS
 *==================*/
#define LV_USE_WIDGETS    1
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*==================
 * THEMES
 *==================*/
#define LV_USE_THEME_DEFAULT  1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK              1
    #define LV_THEME_DEFAULT_GROW              0
    #define LV_THEME_DEFAULT_TRANSITION_TIME   80
#endif
#define LV_USE_THEME_SIMPLE  0
#define LV_USE_THEME_MONO    0

/*==================
 * LAYOUTS
 *==================*/
#define LV_USE_FLEX  1
#define LV_USE_GRID  1

/*==================
 * GPU
 *==================*/
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_SWM341_DMA   0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/*==================
 * OTHERS
 *==================*/
#define LV_USE_SNAPSHOT    0
#define LV_USE_MONKEY      0
#define LV_USE_GRIDNAV     0
#define LV_USE_FRAGMENT    0
#define LV_USE_IMGFONT     0
#define LV_USE_MSG         0
#define LV_USE_IME_PINYIN  0

/*================
 * EXAMPLES
 *================*/
#define LV_BUILD_EXAMPLES 0

#endif  /*LV_CONF_H*/
#endif  /*End of "Content enable"*/
