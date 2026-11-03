#include "board.h"
//#include "bsp/board_api.h"

#include "tusb.h"
#include "tusb_config.h"

#include "bootuf2.h"
extern void msc_bootuf2_init(void);

//进入boot后，会等待一段时间来接收标记，如果收到标记则进入升级；如果这段时间没收到标记，则跳到APP
//这个时间，是等待时间（以ms为单位）
#define UPLOAD_TIMEOUT_MS     2000//2000  //wait time for upload sign (in ms)

//如果升级时，使用led指示，则在这里定义led
//注意，这里的定义，必须与VE中一致。
/* UPLOAD_LED Pin */
#define UPLOAD_LED_GPIO_MASK  APB_MASK_GPIO4
#define UPLOAD_LED_GPIO       GPIO4
#define UPLOAD_LED_BIT        GPIO_BIT3
/* UF2_BOOT Pin */
#define UF2_BOOT_GPIO_MASK    APB_MASK_GPIO4
#define UF2_BOOT_GPIO         GPIO4
#define UF2_BOOT_BIT          GPIO_BIT4

static uint32_t apb_clken;
static uint32_t misc_ctrl;

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 500,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
void led_blinking_task(void);

void Jump_To_Application(void)
{
  //Reset all state
  INT_DisableIntGlobal();
  INT_DisableIntExternal();

  SYS->APB_CLKENABLE = apb_clken;
  SYS->MISC_CNTL = misc_ctrl;
  SYS_SwitchHSIClock(); //Switch to HSI Osc

  //SYS->APB_CLKENABLE = APB_MASK_FCB0; 
  SYS_EnableAPBClock(APB_MASK_FCB0);
  //Reload APP's logic.bin（form flash load）
  //if APP's LOGIC is compressed，call FCB_AutoDecompres, else call FCB_AutoConfig
  //int ret = FCB_AutoConfig(DFU_APP_LOGIC_ADDRESS); //restore APP logic
  FCB_AutoDecompress(UF2_APP_LOGIC_ADDRESS);
  //Load APP logic.bin complete，Siwcth off logic load module's clock
  SYS_DisableAPBClock(APB_MASK_FCB0);

  //以上全部做完，PC指针跳转到APP的位置，开始执行APP的代码
  UTIL_JumpToAddress(UF2_APP_CODE_ADDRESS); //jump to app
}

uint8_t Boot_Btn_Is_Press(void)
{
  static uint8_t boot_btn_is_initialized = 0;

  if( boot_btn_is_initialized == 0 )
  {
    /* UF2_BOOT GPIO */
    SYS_EnableAPBClock(UF2_BOOT_GPIO_MASK);
    GPIO_SetInput(UF2_BOOT_GPIO, UF2_BOOT_BIT);
    boot_btn_is_initialized = 1;
  } 

  if( GPIO_GetValue(UF2_BOOT_GPIO, UF2_BOOT_BIT) == 0 ) //UF2_BOOT Press Down
  {
    tusb_time_delay_ms_api(10);
    if( GPIO_GetValue(UF2_BOOT_GPIO, UF2_BOOT_BIT) == 0 )
    {
      return 1;
    }
  }

  return 0;
}

int main()
{
  /* Save Initilal State */
  apb_clken = SYS->APB_CLKENABLE;
  misc_ctrl = SYS->MISC_CNTL;

  /* Board Initial */
  board_init();

  if( !Boot_Btn_Is_Press() \
      && *((uint32_t *)UF2_APP_CODE_ADDRESS) != 0xFFFFFFFF \
      && *((uint32_t *)UF2_APP_LOGIC_ADDRESS) != 0xFFFFFFFF )
  {
    //Jump To APP
    Jump_To_Application();
  }

  //Switch HSE Crystal
  SYS_SwitchPLLClock(SYS_HSE_BYPASS_OFF); //convert HSE
  INT_Init(); // This will disabe all interrupts. INT_EnableIRQ should be called after this to enable any external interrupt.
  INT_EnableIntGlobal();
  INT_EnableIntExternal();
  
  //Excute upgradge program
  // init device stack on configured roothub port
  msc_bootuf2_init();

  while(1)
  {
    tud_task(); // tinyusb device task

    if ( bootuf2_is_write_done() )
    {
      if( *((uint32_t *)UF2_APP_CODE_ADDRESS) != 0xFFFFFFFF \
          && *((uint32_t *)UF2_APP_LOGIC_ADDRESS) != 0xFFFFFFFF )
      {
        Jump_To_Application();
        while(1) ;        
      }
      bootuf2_clear_state();
    }

    led_blinking_task();
  }
}


//--------------------------------------------------------------------+
// Board INIT
//--------------------------------------------------------------------+
__attribute__((weak)) uint32_t FCB_GetPLLFreq(uint32_t clkin_freq);
void board_init(void)
{
  PERIPHERAL_ENABLE(DMAC, 0);
  DMAC_Init();

  //BootLoader doesn't recommend using uncompressed logic; it takes up too much space.
  // embedded logic.bin release to cpld's runing area
  // Must be release the logic.bin before the following pins can work normally.
  // Provide a minimal working FPGA configuration to avoid problems if the default configuration is corrupted.
#ifdef AGM_LOGIC_EMBED_INC
  static __attribute__((section(".text.logic"))) __attribute__((aligned(4))) const uint8_t logic_config[] = {
#include AGM_LOGIC_EMBED_INC
  };
  PERIPHERAL_ENABLE(FCB, 0);
  SYS_SwitchHSIClock();

  //Before the boot logic.bin uses compression, it calls FCB_AutoDecompress here
  if (sizeof(logic_config) < FCB_AUTO_WORDS * 4) {
    FCB_AutoDecompress((uint32_t)logic_config);
  } else {
    FCB_AutoConfigDma((uint32_t)logic_config, DMAC_CHANNEL0);
  }
  PERIPHERAL_DISABLE(FCB, 0);
#endif
  //Use indpendent logic.bin，at 0x80020000 of Flash Area
  //FCB_AutoDecompress(0x80020000);

  /* LED GPIO */
  SYS_EnableAPBClock(UPLOAD_LED_GPIO_MASK);
  GPIO_SetOutput(UPLOAD_LED_GPIO, UPLOAD_LED_BIT);
  GPIO_SetHigh(UPLOAD_LED_GPIO, UPLOAD_LED_BIT);
}
//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;

  // Blink every interval ms
  if ( UTIL_GetTick() - start_ms < blink_interval_ms) return; // not enough time
    start_ms += blink_interval_ms;

  GPIO_Toggle(UPLOAD_LED_GPIO, UPLOAD_LED_BIT);
}