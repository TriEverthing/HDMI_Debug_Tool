/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef BOOTUF2_CONFIG_H
#define BOOTUF2_CONFIG_H

#define CONFIG_PRODUCT                      "AGM32"
#define CONFIG_BOARD                        "AGM32_BootLaoder"
#define CONFIG_BOOTUF2_INDEX_URL            "https://github.com/cherry-embedded"
#define CONFIG_BOOTUF2_JOIN_URL             "http://qm.qq.com/cgi-bin/qm/qr?_wv=1027&k=GyH2M5XfWTHQzmZis4ClpgvfdObPrvtk&authKey=LmcLhfno%2BiW51wmgVC%2F8WoYwUXqiclzWDHMU1Jy1d6S8cECJ4Q7bfJ%2FTe67RLakI&noverify=0&group_code=642693751"

#define CONFIG_BOOTUF2_CACHE_SIZE           4096
#define CONFIG_BOOTUF2_SECTOR_SIZE          512     //512B for Composite 
#define CONFIG_BOOTUF2_SECTOR_PER_CLUSTER   8       //AGM32 Erase Sector Minimum 4kB
#define CONFIG_BOOTUF2_SECTOR_RESERVED      1
#define CONFIG_BOOTUF2_NUM_OF_FAT           2
#define CONFIG_BOOTUF2_ROOT_ENTRIES         64

//设置要跳转到的APP的 code_addr 和 logic_addr。
//这里必须要根据真实的APP的信息来填
#define UF2_BOOT_FLASH_BASE                 0x80000000
#define UF2_BOOT_FLASH_SIZE                 (256*1024)
//APP code address , 28kB Flash for UF2_BootLoader, 4kB for Configparm
#define UF2_APP_CODE_ADDRESS                0x80008000    
#define UF2_APP_LOGIC_ADDRESS               ( UF2_BOOT_FLASH_SIZE + UF2_BOOT_FLASH_BASE - 48*1024  ) //APP logic address

#define CONFIG_BOOTUF2_FAMILYID             0x12345678

#define CONFIG_BOOTUF2_FLASHMAX             ( UF2_BOOT_FLASH_SIZE + UF2_BOOT_FLASH_BASE - UF2_APP_CODE_ADDRESS  )  //flash
#define CONFIG_BOOTUF2_PAGE_COUNTMAX        ( CONFIG_BOOTUF2_FLASHMAX / 256 )

#define CONFIG_BOOTUF2_FLASH_DMA            0

#endif