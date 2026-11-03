/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "board.h"
#include "bsp/board_api.h"

#include "tusb.h"

#include "bootuf2.h"

#if CFG_TUD_MSC

// whether host does safe-eject
static bool ejected = false;

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
  (void) lun;
  *block_count = bootuf2_get_sector_count();
  *block_size = bootuf2_get_sector_size();
  //TU_LOG(3, "sector count:%d, sector size:%d.\r\n", *block_num, *block_size);
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) lun;
    (void) offset;
    boot2uf2_read_sector(lba, buffer, bufsize / bootuf2_get_sector_size());
    return (int32_t)bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void) lun;
    (void) offset;
    //TU_LOG(3, "usbd_msc_sector_write : sector = 0x%08X , length = %d.\r\n" , sector , length );
    bootuf2_write_sector(lba, buffer, bufsize / bootuf2_get_sector_size() );
    return (int32_t) bufsize;
}

// Invoked when received SCSI_CMD_INQUIRY, v2 with full inquiry response
// Some inquiry_resp's fields are already filled with default values, application can update them
// Return length of inquiry response, typically sizeof(scsi_inquiry_resp_t) (36 bytes), can be longer if included vendor data.
uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp, uint32_t bufsize) {
  (void) lun;
  (void) bufsize;
  const char vid[] = "AGMSemi";
  const char pid[] = "UF2_BootLoader";
  const char rev[] = "1.0";

  (void) strncpy((char*) inquiry_resp->vendor_id, vid, 8);
  (void) strncpy((char*) inquiry_resp->product_id, pid, 16);
  (void) strncpy((char*) inquiry_resp->product_rev, rev, 4);

  return sizeof(scsi_inquiry_resp_t); // 36 bytes
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void) lun;

  // RAM disk is ready until ejected
  if (ejected) {
    // Additional Sense 3A-00 is NOT_FOUND
    return tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
  }

  return true;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void) lun;
  (void) power_condition;

  if (load_eject) {
    if (start) {
      // load disk storage
    } else {
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  (void) lun;
  return true;
}


// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
  (void) lun;
  (void) scsi_cmd;
  (void) buffer;
  (void) bufsize;

  // currently no other commands is supported

  // Set Sense = Invalid Command Operation
  (void) tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

  return -1; // stall/failed command request;
}

#endif


void msc_bootuf2_init(void)
{
    boot2uf2_flash_init();
    //
    bootuf2_init();
    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);


}

void boot2uf2_flash_init(void)
{

}

int bootuf2_flash_write(uint32_t address , const uint8_t *data, size_t size)
{
    uint32_t page_count = 0 ;

    //address = address + FLASH_BASE ;
    if( address < UF2_APP_CODE_ADDRESS || address > ( UF2_BOOT_FLASH_SIZE + UF2_BOOT_FLASH_BASE )) 
        return -1;

    if (FLASH_IsReadProtected()) 
    {
        return -1;
    }

    /* Flash must be unlocked before any erase or program operation. */
    FLASH_Unlock();

    /* Erase Flash, Minimun FLASH_SECTOR_SIZE = 4KB */
    FLASH_Erase(address, size); //4KB

    /* Program Flash， must be after erase flash */
#if CONFIG_BOOTUF2_FLASH_DMA
    // Use DMA to program flash, and process user code while waiting.
    PERIPHERAL_ENABLE(DMAC, 0);
    DMAC_Init();
    FLASH_EnableFastProgram();
    for (uint32_t offset = 0; offset < size; offset += DMAC_MAX_TRANSFER_SIZE) {
        uint32_t xfer_size = size - offset;

        if (xfer_size > DMAC_MAX_TRANSFER_SIZE) {
            xfer_size = DMAC_MAX_TRANSFER_SIZE;
        }

        DMAC_Config(DMAC_CHANNEL0, (uint32_t)data + offset, address + offset,
                    DMAC_ADDR_INCR_ON, DMAC_ADDR_INCR_ON, DMAC_WIDTH_8_BIT, DMAC_WIDTH_8_BIT,
                    DMAC_BURST_256, DMAC_BURST_256, xfer_size, DMAC_MEM_TO_MEM_DMA_CTRL, 0, 0);

        while (DMAC_IsEnabledChannel(DMAC_CHANNEL0)) {
            // User code can be processed here
        }
    }
    FLASH_StopFastProgram();
#else
    FLASH_FastProgram(address, (uint32_t)data, size);
#endif

    /*for ( page_count = 0 ; page_count < size ; ) 
    {
        //Programage FLASH_PAGE_SIZE Byte
        FLASH_FastProgram(address + page_count, (uint32_t)(data + page_count), FLASH_PAGE_SIZE);
        page_count += FLASH_PAGE_SIZE ;
    }*/

    /* Flash must be locked after any erase or program operation. */
    FLASH_Lock();
    
    return 0;
}

