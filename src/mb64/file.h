#pragma once

#include "libcart/include/cart.h"
#include "libcart/ff/ff.h"

#include "structs.h"

#define MB64_FORMAT_U8 u8
#define MB64_FORMAT_S8 s8
#define MB64_FORMAT_U16 u16
#define MB64_FORMAT_U64 u64

#define MB64_VERSION 1
#define MAX_FILE_NAME_SIZE 41
#define MAX_FILE_NAME_INPUT (MAX_FILE_NAME_SIZE - 6)
#define MAX_USERNAME_SIZE 31
#define MAX_USERNAME_INPUT (MAX_USERNAME_SIZE - 1)

#define MB64_MAX_TRAJECTORIES 20
#define MB64_TRAJECTORY_LENGTH 50

#include "../../libmb64/mb64_save_format.h"

#define MAX_FILES 251
extern u8 mb64_level_entry_version[MAX_FILES];
extern FRESULT gMountSuccess;
extern FRESULT global_code;
extern u8 mb64_level_entry_count;
extern TCHAR *mb64_level_dir_name;
extern TCHAR *mb64_hack_dir_name;
extern struct mb64_sram_config mb64_sram_configuration;
extern struct mb64_level_save_header mb64_save;

extern char mb64_file_name[MAX_FILE_NAME_SIZE];
extern FILINFO mb64_file_info;

#define gSDCard (gMountSuccess == FR_OK)

void mb64_file_init(void);
void save_level(void);
void load_level(void);


