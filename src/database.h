#ifndef __DATABASE_H
#define __DATABASE_H

#include <sys/types.h>
#include <inttypes.h>


typedef struct fuse_decl_s {
	const char	*name;
	uint8_t		cmd;
	uint8_t		size;	/* Data size. */
	uint8_t		offset;	/* Data offset. */
} __attribute__((__packed__)) fuse_decl_t, *fuse_decl_p;


typedef struct chip_s {
	char		*name;
	uint8_t		protocol_id;
	//uint8_t		type;	/* Not used. */
	uint8_t		variant;
	uint32_t	code_memory_size; /* Presenting for every device. */
	uint32_t	data_memory_size;
	uint32_t	data_memory2_size;
	uint32_t	read_block_size;
	uint32_t	write_block_size;
	uint32_t	chip_id;	/* A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48). */
	uint8_t		chip_id_size;	/* chip_id_bytes_count */
	uint8_t		chip_id_shift;	/* PIC controllers device ID have a variable bitfield Revision number. */
	uint16_t	opts1;
	uint16_t	opts2;
	uint32_t	opts3;		// XXX: uint16_t
	uint32_t	opts4;
	uint32_t	package_details; /* Pins count or image ID for some devices. */
	uint16_t	write_unlock;	// XXX: uint8_t
	fuse_decl_p	fuses;		/* Configuration bytes that's presenting in some architectures. */
} __attribute__((__packed__)) chip_t, *chip_p;

#define CHIP_NAME_MAX			64	/* Max chip name len. */

/* type */
#define CHIP_TYPE_EEPROM		0x01 /* ROM/FLASH/NVRAM */
#define CHIP_TYPE_MCU_MPU		0x02 /* MCU/MPU */
#define CHIP_TYPE_PLD_GAL_CPLD		0x03 /* PLD/GAL/CPLD */
#define CHIP_TYPE_SRAM_DRAM		0x04 /* SRAM/DRAM */
#define CHIP_TYPE_LOGIC_IC		0x05 /* Logic IC */

/* opts4 */
#define CHIP_OPT4_SIZE_UNITS_MASK	0xff000000
#define CHIP_OPT4_SIZE_UNITS(__val)	((__val) >> 24)
#define CHIP_OPT4_SIZE_BYTES			0x00
#define CHIP_OPT4_SIZE_WORDS			0x01
#define CHIP_OPT4_SIZE_BITS			0x02
#define CHIP_OPT4_ERASE			0x00000010
#define CHIP_OPT4_CHIP_ID		0x00000020
#define CHIP_OPT4_ADDR_SCALE		0x00002000
#define CHIP_OPT4_PROTECTION		0x0000c000

/* package_details */
#define CHIP_PKG_D_ADAPTER_MASK		0x000000ff
#define CHIP_PKG_D_ADAPTER(__val)	((__val) & CHIP_PKG_D_ADAPTER_MASK)
#define CHIP_PKG_D_ADAPTER_TSOP48		0x01
#define CHIP_PKG_D_ADAPTER_SOP44		0x02
#define CHIP_PKG_D_ADAPTER_TSOP40A		0x03
#define CHIP_PKG_D_ADAPTER_TSOP40B		0x04
#define CHIP_PKG_D_ADAPTER_TSOP32		0x05
#define CHIP_PKG_D_ADAPTER_SOP56		0x06
#define CHIP_PKG_D_ADAPTER_TQFP64		0x07
#define CHIP_PKG_D_ADAPTER_25_TO_8_18V		0x08
#define CHIP_PKG_D_ADAPTER_25_TO_16_18V		0x09
#define CHIP_PKG_D_ISP_MASK		0x0000ff00
#define CHIP_PKG_D_ISP(__val)		(((__val) & CHIP_PKG_D_ISP_MASK) >> 8)
#define CHIP_PKG_D_DIP_MASK		0xff000000 /* 7f? */
#define CHIP_PKG_D_DIP(__val)		(((__val) & CHIP_PKG_D_DIP_MASK) >> 24)
#define CHIP_PKG_D_DIP_DIP8			0x08
#define CHIP_PKG_D_DIP_DIP14			0x0e
#define CHIP_PKG_D_DIP_DIP16			0x10
#define CHIP_PKG_D_DIP_DIP20			0x14
#define CHIP_PKG_D_DIP_DIP24			0x18
#define CHIP_PKG_D_DIP_DIP28			0x1c
#define CHIP_PKG_D_DIP_DIP32			0x20
#define CHIP_PKG_D_DIP_DIP40			0x28
#define CHIP_PKG_D_DIP_SOP8			0x88
#define CHIP_PKG_D_DIP_SOP14			0x8e
#define CHIP_PKG_D_DIP_SOP16			0x90
#define CHIP_PKG_D_DIP_SOP20			0x94
#define CHIP_PKG_D_DIP_SOP24			0x98
#define CHIP_PKG_D_DIP_SOP28			0x9c
#define CHIP_PKG_D_DIP_SOP32			0xa0
#define CHIP_PKG_D_DIP_SOP40			0xa8
#define CHIP_PKG_D_DIP_PLCC44			0xfd
#define CHIP_PKG_D_DIP_PLCC32			0xff



int	is_chip_id_prob_eq(const chip_p chip, const uint32_t id,
	    const uint8_t id_size);
int	is_chip_id_eq(const chip_p chip, const uint32_t id,
	    const uint8_t id_size);

void	chip_db_print_info(const chip_p chip);

void	chip_db_free(chip_p chips_db);
int	chip_db_load(const char *file_name, size_t file_name_size,
	    chip_p *chips_db, size_t *chips_db_count);

chip_p	chip_db_get_by_idx(const size_t index);
chip_p	chip_db_get_by_id(chip_p chips_db, const uint32_t chip_id,
	    const uint8_t chip_id_size);
chip_p	chip_db_get_by_name(chip_p chips_db, const char *name);

void	chip_db_dump_flt(chip_p chips_db, const char *name);
void	chip_db_dump(chip_p chips_db);

#endif
