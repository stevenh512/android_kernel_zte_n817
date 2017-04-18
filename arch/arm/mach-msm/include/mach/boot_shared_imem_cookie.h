#ifndef BOOT_SHARED_IMEM_COOKIE_H
#define BOOT_SHARED_IMEM_COOKIE_H

/*===========================================================================

                                Boot Shared Imem Cookie
                                Header File

GENERAL DESCRIPTION
  This header file contains declarations and definitions for Boot's shared 
  imem cookies. Those cookies are shared between boot and other subsystems 

Copyright  2013 by QUALCOMM Technologies, Incorporated.  All Rights Reserved.
============================================================================*/

/*===========================================================================

                           EDIT HISTORY FOR FILE

This section contains comments describing changes made to this file.
Notice that changes are listed in reverse chronological order.

  $Header: //source/qcom/qct/core/pkg/bootloaders/rel/2.1/boot_images/core/boot/secboot3/src/boot_shared_imem_cookie.h#2 $
  $DateTime: 2013/04/18 11:09:07 $ 
  $Author: coresvc $

when       who          what, where, why
--------   --------     ------------------------------------------------------
03/19/13   dh           Add UEFI_RAW_RAM_DUMP_MAGIC_NUM,
                        Allocate abnormal_reset_occurred cookie for UEFI 
02/28/13   kedara       Added feature flag FEATURE_TPM_HASH_POPULATE
02/21/13   dh           Move SHARED_IMEM_TPM_HASH_REGION_OFFSET to boot_shared_imem_cookie.h
02/15/13   dh           Add tz_dload_dump_shared_imem_cookie_type
11/27/12   dh           Add rpm_sync_cookie
07/24/12   dh           Add ddr_training_cookie
06/07/11   dh           Initial creation
============================================================================*/
/*===========================================================================
when         who        what, where, why                         comment tag
2010-12-15   ruijiagui  Add ZTE_FEATURE_SD_DUMP feature          ZTE_RJG_RIL_20121214
===========================================================================*/

/*=============================================================================

                            INCLUDE FILES FOR MODULE

=============================================================================*/
#include <linux/types.h>
#include <linux/compiler-gcc.h>

/*=============================================================================

            LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains local definitions for constants, macros, types,
variables and other items needed by this module.

=============================================================================*/

#ifndef SHARED_IMEM_BASE
#define SHARED_IMEM_BASE 0xFE805000
#endif

#ifndef SHARED_IMEM_BOOT_BASE
#define SHARED_IMEM_BOOT_BASE SHARED_IMEM_BASE
#endif


/* 
 * Following magic number indicates the boot shared imem region is initialized
 * and the content is valid
 */
#define BOOT_SHARED_IMEM_MAGIC_NUM        0xC1F8DB40

/* 
 * Magic number for UEFI crash dump
 */
#define UEFI_CRASH_DUMP_MAGIC_NUM         0x1

/* 
 * Magic number for raw ram dump
 */
#define BOOT_RAW_RAM_DUMP_MAGIC_NUM       0x2

/* Default value used to initialize shared imem region */
#define SHARED_IMEM_REGION_DEF_VAL  0xFFFFFFFF 

/* 
 * Magic number for RPM sync
 */
#define BOOT_RPM_SYNC_MAGIC_NUM  0x112C3B1C 

/*ZTE_BOOT_20120816 huang.yanjun for fuse ---->*/
/*
 * Type Definition
 */
typedef struct {
  uint32_t magic_1;
  uint32_t magic_2;
  uint32_t fusion_type;
  uint32_t fusion_status;
} ftm_fusion_info;
/*ZTE_BOOT_20120816 huang.yanjun for fuse <----*/


//ZTE_RJG_RIL_20130320 begin
typedef struct {
  uint32_t magic_num;
  uint32_t buffer_addr;
  uint32_t buffer_size;
  uint32_t memory_swap;
  uint32_t head_offset;
} err_f3_trace_info_type_s;
//ZTE_RJG_RIL_20130320 end


/* 
 * Following structure defines all the cookies that have been placed 
 * in boot's shared imem space.
 * The size of this struct must NOT exceed SHARED_IMEM_BOOT_SIZE
 */
struct boot_shared_imem_cookie_type
{
  /* First 8 bytes are two dload magic numbers */
  uint32_t dload_magic_1;
  uint32_t dload_magic_2;
  
  /* Magic number which indicates boot shared imem has been initialized
     and the content is valid.*/ 
  uint32_t shared_imem_magic;
  
  /* Magic number for UEFI ram dump, if this cookie is set along with dload magic numbers,
     we don't enter dload mode but continue to boot. This cookie should only be set by UEFI*/
  uint32_t uefi_ram_dump_magic;
  
  /* Pointer that points to etb ram dump buffer, should only be set by HLOS */
  uint32_t etb_buf_addr;
  
  /* Region where HLOS will write the l2 cache dump buffer start address */
  uint32_t *l2_cache_dump_buff_ptr;
  
  uint32_t ddr_training_cookie;
  
  /* Cookie that will be used to sync with RPM */
  uint32_t rpm_sync_cookie;
  /* Abnormal reset cookie used by UEFI */
  uint32_t abnormal_reset_occurred;

  /* Reset Status Register */
  uint32_t reset_status_register;
  /* Please add new cookie here, do NOT modify or rearrange the existing cookies*/
  /*ZTE_BOOT_20120816 huang.yanjun for fuse */
  ftm_fusion_info fusion_info;

  /* ZTE_BOOT_20120823_SLF Pointer that points to hw reset flag address */
  uint32_t hw_reset_addr;

  /*
   * Added for EFS sync during system suspend & restore
   * by ZTE_BOOT_JIA_20121008 jia.jia
   */
  uint32_t app_suspend_state;
  uint32_t modemsleeptime; //sleeptime, zenghuipeng 20120828 add

  /*err_fatal magic number which will be set in kernel*/
  //Add by ruijiagui,ZTE_RJG_RIL_20121214
  uint32_t err_fatal_magic;


  //ZTE_RJG_RIL_20130320 begin
  err_f3_trace_info_type_s f3_trace_info;
  //ZTE_RJG_RIL_20130320 end

  //used for sdlog flag pass by app
  uint32_t app_mem_reserved;
  
  uint32_t modemawaketime;//lianghouxing 20121119 add to store how long modem keep awake
  uint32_t modemsleep_or_awake;//lianghouxing 20121119 add to indicate modem is sleep or awake,1 sleep,0 means never enter sleep yet, 2 awake
  uint32_t physlinktime;/////liukejing 20120606 add to count data link time
  uint32_t modemawake_timeout_crash;////ZTE_PM_liukejing 20130621

  uint32_t efs_backup_state; //ZTE_BOOT_for golden backup and recovery
  uint32_t efs_recovery_flag;//ZTE_BOOT for golden backup and recovery
  uint32_t ddr_fix_flag;     //sync for P821A16,LCD blue screen
};


/*  Global pointer points to the boot shared imem cookie structure  */
extern struct boot_shared_imem_cookie_type *boot_shared_imem_cookie_ptr;


#endif /* BOOT_SHARED_IMEM_COOKIE_H */

