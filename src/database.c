#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "utils/macro.h"
#include "utils/strh2num.h"
#include "utils/mem_utils.h"
#include "utils/sys.h"
#include "utils/ini.h"

#include "minipro.h"
#include "database.h"

#define DB_CHIPS_PREALLOC	512


static fuse_decl_t atmel_lock[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 2, .offset = 1 /* Write only. */ }, /* Items count, include this. */
	{ .name = "lock_byte",	.cmd = MP_CMD_READ_LOCK,.size = 1, .offset = 0 },
};

static fuse_decl_t avr_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 3, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "lock_byte",	.cmd = MP_CMD_READ_LOCK,.size = 1, .offset = 0 },
};

static fuse_decl_t avr2_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 4, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses_lo",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "fuses_hi",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 1 },
	{ .name = "lock_byte",	.cmd = MP_CMD_READ_LOCK,.size = 1, .offset = 0 },
};

static fuse_decl_t avr3_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 5, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses_lo",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "fuses_hi",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 1 },
	{ .name = "fuses_ext",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 2 },
	{ .name = "lock_byte",	.cmd = MP_CMD_READ_LOCK,.size = 1, .offset = 0 },
};

static fuse_decl_t pic_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 6, .offset = 0 }, /* Items count, include this. */
	{ .name = "user_id0",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 0 },
	{ .name = "user_id1",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 2 },
	{ .name = "user_id2",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 4 },
	{ .name = "user_id3",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 6 },
	{ .name = "conf_word",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 0 },
};

static fuse_decl_t pic2_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 7, .offset = 0 }, /* Items count, include this. */
	{ .name = "user_id0",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 0 },
	{ .name = "user_id1",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 2 },
	{ .name = "user_id2",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 4 },
	{ .name = "user_id3",	.cmd = MP_CMD_READ_USER,.size = 2, .offset = 6 },
	{ .name = "conf_word",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 0 },
	{ .name = "conf_word1",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 2 },
};


/* Device ID table for the Microchip PIC controllers.
 * Extracted by Radioman from the last 6.82 minipro software.
 */
static chip_id_map_t chip_id_map_tbl[] = {
	{ .shift = 0x00, .chip_id = 0x00000000 }, /* none. */
	{ .shift = 0x04, .chip_id = 0x000000e4 },
	{ .shift = 0x04, .chip_id = 0x000000e6 },
	{ .shift = 0x04, .chip_id = 0x000000e0 },
	{ .shift = 0x04, .chip_id = 0x000000e2 },
	{ .shift = 0x04, .chip_id = 0x0000007d },
	{ .shift = 0x05, .chip_id = 0x00000023 },
	{ .shift = 0x05, .chip_id = 0x00000091 },
	{ .shift = 0x05, .chip_id = 0x00000085 },
	{ .shift = 0x05, .chip_id = 0x00000085 },
	{ .shift = 0x05, .chip_id = 0x000000a2 },
	{ .shift = 0x05, .chip_id = 0x00000084 },
	{ .shift = 0x05, .chip_id = 0x00000025 },
	{ .shift = 0x05, .chip_id = 0x00000099 },
	{ .shift = 0x05, .chip_id = 0x0000008c },
	{ .shift = 0x05, .chip_id = 0x0000009a },
	{ .shift = 0x05, .chip_id = 0x000000a0 },
	{ .shift = 0x05, .chip_id = 0x00000005 },
	{ .shift = 0x05, .chip_id = 0x00000030 },
	{ .shift = 0x05, .chip_id = 0x00000031 },
	{ .shift = 0x05, .chip_id = 0x00000032 },
	{ .shift = 0x05, .chip_id = 0x00000033 },
	{ .shift = 0x05, .chip_id = 0x00000082 },
	{ .shift = 0x05, .chip_id = 0x00000083 },
	{ .shift = 0x05, .chip_id = 0x00000088 },
	{ .shift = 0x05, .chip_id = 0x00000082 },
	{ .shift = 0x05, .chip_id = 0x00000083 },
	{ .shift = 0x05, .chip_id = 0x00000088 },
	{ .shift = 0x05, .chip_id = 0x00000068 },
	{ .shift = 0x05, .chip_id = 0x00000069 },
	{ .shift = 0x05, .chip_id = 0x00000047 },
	{ .shift = 0x05, .chip_id = 0x0000004b },
	{ .shift = 0x05, .chip_id = 0x00000049 },
	{ .shift = 0x05, .chip_id = 0x0000004f },
	{ .shift = 0x05, .chip_id = 0x0000004d },
	{ .shift = 0x05, .chip_id = 0x0000004c },
	{ .shift = 0x04, .chip_id = 0x0000004e },
	{ .shift = 0x04, .chip_id = 0x00000100 },
	{ .shift = 0x05, .chip_id = 0x00000101 },
	{ .shift = 0x05, .chip_id = 0x00000102 },
	{ .shift = 0x05, .chip_id = 0x00000103 },
	{ .shift = 0x05, .chip_id = 0x00000104 },
	{ .shift = 0x05, .chip_id = 0x00000072 },
	{ .shift = 0x04, .chip_id = 0x00000076 },
	{ .shift = 0x04, .chip_id = 0x0000013e },
	{ .shift = 0x04, .chip_id = 0x0000013c },
	{ .shift = 0x04, .chip_id = 0x0000013a },
	{ .shift = 0x04, .chip_id = 0x00000138 },
	{ .shift = 0x04, .chip_id = 0x00000146 },
	{ .shift = 0x04, .chip_id = 0x0000005d },
	{ .shift = 0x05, .chip_id = 0x0000005f },
	{ .shift = 0x05, .chip_id = 0x0000007d },
	{ .shift = 0x05, .chip_id = 0x0000006f },
	{ .shift = 0x05, .chip_id = 0x00000090 },
	{ .shift = 0x05, .chip_id = 0x00000091 },
	{ .shift = 0x05, .chip_id = 0x00000112 },
	{ .shift = 0x05, .chip_id = 0x00000113 },
	{ .shift = 0x05, .chip_id = 0x0000010c },
	{ .shift = 0x05, .chip_id = 0x00000092 },
	{ .shift = 0x05, .chip_id = 0x00000114 },
	{ .shift = 0x05, .chip_id = 0x00000115 },
	{ .shift = 0x05, .chip_id = 0x0000010d },
	{ .shift = 0x05, .chip_id = 0x00000093 },
	{ .shift = 0x05, .chip_id = 0x0000007c },
	{ .shift = 0x05, .chip_id = 0x0000007e },
	{ .shift = 0x05, .chip_id = 0x00000086 },
	{ .shift = 0x05, .chip_id = 0x00000087 },
	{ .shift = 0x05, .chip_id = 0x000000c4 },
	{ .shift = 0x05, .chip_id = 0x000000c3 },
	{ .shift = 0x05, .chip_id = 0x000000c2 },
	{ .shift = 0x05, .chip_id = 0x000000c1 },
	{ .shift = 0x05, .chip_id = 0x000000c0 },
	{ .shift = 0x05, .chip_id = 0x000000cc },
	{ .shift = 0x05, .chip_id = 0x000000cb },
	{ .shift = 0x05, .chip_id = 0x000000ca },
	{ .shift = 0x05, .chip_id = 0x000000c9 },
	{ .shift = 0x05, .chip_id = 0x000000c8 },
	{ .shift = 0x05, .chip_id = 0x000000d9 },
	{ .shift = 0x05, .chip_id = 0x000000d8 },
	{ .shift = 0x05, .chip_id = 0x000000db },
	{ .shift = 0x05, .chip_id = 0x000000da },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x0000002b },
	{ .shift = 0x05, .chip_id = 0x0000002b },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x00000000 },
	{ .shift = 0x05, .chip_id = 0x0000008a },
	{ .shift = 0x05, .chip_id = 0x0000008a }
};


chip_id_map_p
chip_id_map(uint32_t index) {

	if (0 == index || index >= SIZEOF(chip_id_map_tbl))
		return (NULL);	
	return (&chip_id_map_tbl[index]);
}

int
is_chip_id_prob_eq(const chip_p chip, const uint32_t id,
    const uint8_t id_size) {

	if (0 == chip->chip_id_size || 0 == id_size)
		return ((chip->chip_id_size == id_size));
	if (chip->chip_id_size == id_size)
		return ((chip->chip_id == id));
	if (chip->chip_id_size > id_size)
		return (0);
	/* chip->chip_id_size < id_size */
	return ((chip->chip_id ==
	    (id >> (8 * (id_size - chip->chip_id_size)))));
}

int
is_chip_id_eq(const chip_p chip, const uint32_t id,
    const uint8_t id_size) {

	if (chip->chip_id_size != id_size ||
	    0 == chip->chip_id_size)
		return (0);
	return ((chip->chip_id == id));
}


void
chip_db_print_info(const chip_p chip) {

	if (NULL == chip)
		return;

	printf("Name: %s\n", chip->name);

	/* Memory shape */
	printf("Memory: ");
	switch (CHIP_OPT4_SIZE_UNITS(chip->opts4)) {
	case CHIP_OPT4_SIZE_BYTES:
		printf("%d Bytes", chip->code_memory_size);
		break;
	case CHIP_OPT4_SIZE_WORDS:
		printf("%d Words", (chip->code_memory_size / 2));
		break;
	case CHIP_OPT4_SIZE_BITS:
		printf("%d Bits", chip->code_memory_size);
		break;
	default:
		printf(" unknown memory shape: 0x%x\n",
		    CHIP_OPT4_SIZE_UNITS(chip->opts4));
	}
	if (chip->data_memory_size) {
		printf(" + %d Bytes", chip->data_memory_size);
	}
	if (chip->data_memory2_size) {
		printf(" + %d Bytes", chip->data_memory2_size);
	}
	printf("\n");

	/* Package info */
	printf("Package: ");
	if (0 != (CHIP_PKG_D_ADAPTER_MASK & chip->package_details)) {
		printf("Adapter%03d.JPG\n",
		    CHIP_PKG_D_ADAPTER(chip->package_details));
	} else if (0 != (CHIP_PKG_D_DIP_MASK & chip->package_details)) {
		printf("DIP%d\n",
		    CHIP_PKG_D_DIP(chip->package_details));
	} else {
		printf("ISP only\n");
	}

	/* ISP connection info */
	printf("ISP: ");
	if (0 != (CHIP_PKG_D_ISP_MASK & chip->package_details)) {
		printf("ICP%03d.JPG\n",
		    CHIP_PKG_D_ISP(chip->package_details));
	} else {
		printf("-\n");
	}

	printf("Protocol: 0x%02x\n", chip->protocol_id);
	printf("Read buffer size: %d Bytes\n", chip->read_block_size);
	printf("Write buffer size: %d Bytes\n", chip->write_block_size);
}


static int
chip_db_ini_parse_item(ini_p ini, size_t soff, const uint8_t *sname,
    size_t sname_sz, chip_p chip) {
	const uint8_t *vn, *val;
	size_t voff, vn_sz, val_size;
	uint32_t smask;
	chip_id_map_p id_map;

	if (NULL == ini || NULL == sname || NULL == chip)
		return (EINVAL);

	/* Parse fields. */
	smask = 0;
	voff = 0;
	while (0 == ini_sect_val_enum(ini, soff, &voff,
	    &vn, &vn_sz, &val, &val_size)) {
		if (0 == mem_cmpn_cstr("protocol_id", vn, vn_sz)) {
			chip->protocol_id = ustrh2u8(val, val_size);
			smask |= (((uint32_t)1) << 0);
		} else if (0 == mem_cmpn_cstr("variant", vn, vn_sz)) {
			chip->variant = ustrh2u8(val, val_size);
			smask |= (((uint32_t)1) << 1);
		} else if (0 == mem_cmpn_cstr("read_block_size", vn, vn_sz)) {
			chip->read_block_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 2);
		} else if (0 == mem_cmpn_cstr("write_block_size", vn, vn_sz)) {
			chip->write_block_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 3);
		} else if (0 == mem_cmpn_cstr("code_memory_size", vn, vn_sz)) {
			chip->code_memory_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 4);
		} else if (0 == mem_cmpn_cstr("data_memory_size", vn, vn_sz)) {
			chip->data_memory_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 5);
		} else if (0 == mem_cmpn_cstr("data_memory2_size", vn, vn_sz)) {
			chip->data_memory2_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 6);
		} else if (0 == mem_cmpn_cstr("chip_id", vn, vn_sz)) {
			chip->chip_id = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 7);
		} else if (0 == mem_cmpn_cstr("chip_id_size", vn, vn_sz)) {
			chip->chip_id_size = ustrh2u8(val, val_size);
			smask |= (((uint32_t)1) << 8);
		} else if (0 == mem_cmpn_cstr("opts1", vn, vn_sz)) {
			chip->opts1 = ustrh2u16(val, val_size);
			smask |= (((uint32_t)1) << 9);
		} else if (0 == mem_cmpn_cstr("opts2", vn, vn_sz)) {
			chip->opts2 = ustrh2u16(val, val_size);
			smask |= (((uint32_t)1) << 10);
		} else if (0 == mem_cmpn_cstr("opts3", vn, vn_sz)) {
			chip->opts3 = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 11);
		} else if (0 == mem_cmpn_cstr("opts4", vn, vn_sz)) {
			chip->opts4 = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 12);
		} else if (0 == mem_cmpn_cstr("package_details", vn, vn_sz)) {
			chip->package_details = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 13);
		} else if (0 == mem_cmpn_cstr("write_unlock", vn, vn_sz)) {
			chip->write_unlock = ustrh2u16(val, val_size);
			smask |= (((uint32_t)1) << 14);
		} else if (0 == mem_cmpn_cstr("fuses", vn, vn_sz)) {
			if (0 == mem_cmpn_cstr("NULL", val, val_size)) {
				chip->fuses = NULL;
			} else if (0 == mem_cmpn_cstr("atmel_lock", val, val_size)) {
				chip->fuses = atmel_lock;
			} else if (0 == mem_cmpn_cstr("avr_fuses", val, val_size)) {
				chip->fuses = avr_fuses;
			} else if (0 == mem_cmpn_cstr("avr2_fuses", val, val_size)) {
				chip->fuses = avr2_fuses;
			} else if (0 == mem_cmpn_cstr("avr3_fuses", val, val_size)) {
				chip->fuses = avr3_fuses;
			} else if (0 == mem_cmpn_cstr("pic_fuses", val, val_size)) {
				chip->fuses = pic_fuses;
			} else if (0 == mem_cmpn_cstr("pic2_fuses", val, val_size)) {
				chip->fuses = pic2_fuses;
			} else {
				chip->fuses = NULL;
				fprintf(stderr,
				    "Unknown fuse value: \"%.*s\", "
				    "line %zu.\n",
				    (int)val_size, val, voff);
			}
			smask |= (((uint32_t)1) << 15);
		} else {
			fprintf(stderr,
			    "Unknown field: \"%.*s\", line %zu.\n",
			    (int)vn_sz, vn, voff);
		}
		voff ++;
	}

	/* Is all fields set? */
	if (((((uint32_t)1) << 16) - 1) != smask) {
		fprintf(stderr,
		    "Section: \"%.*s\", at line %zu does not contain "
		    "all required fields.\n",
		    (int)sname_sz, sname, soff);
		return (EINVAL);
	}

	/* This is a workaround for al Microchip controllers.
	 * These controllers have the Chip ID defined elsewhere, not in
	 * the infoic.dll but in a table located inside the Minipro.exe
	 * For these controlers (almost 600 devices) the opts3 is an
	 * index in this table.
	 */
	if ((0 == chip->chip_id && 0 != chip->chip_id_size) ||
	    0 != (CHIP_OPT4_CHIP_ID & chip->opts4)) {
		id_map = chip_id_map(chip->opts3);
		if (NULL != id_map) {
			chip->chip_id = id_map->chip_id;
		}
	}

	/* Store name. */
	chip->name = mem_dup2(sname, sname_sz, 1);
	if (NULL == chip->name)
		return (ENOMEM);

	return (0);
}
static int
chip_db_ini_load(uint8_t *buf, size_t buf_size, chip_p *chips_db,
    size_t *chips_db_count) {
	int error;
	const uint8_t *sname;
	size_t soff, sname_sz;
	size_t cdb_allocated = 0, cdb_count = 0;
	ini_p ini = NULL;
	chip_p cdb = NULL;

	if (NULL == buf || 0 == buf_size ||
	    NULL == chips_db || NULL == chips_db_count)
		return (EINVAL);

	error = ini_create(&ini);
	if (0 != error)
		goto err_out;
	error = ini_buf_parse(ini, buf, buf_size);
	if (0 != error)
		goto err_out;
	/* Load chips. */
	soff = 0;
	while (0 == ini_sect_enum(ini, &soff, &sname, &sname_sz)) {
		error = realloc_items((void**)&cdb,
		    sizeof(chip_t), &cdb_allocated,
		    DB_CHIPS_PREALLOC, cdb_count);
		if (0 != error)
			goto err_out;
		if (0 == chip_db_ini_parse_item(ini, soff, sname, sname_sz,
		    &cdb[cdb_count])) {
			cdb_count ++;
		}
		soff ++;
	}

	/* Make sure that last NULL element exist. */
	error = realloc_items((void**)&cdb,
	    sizeof(chip_t), &cdb_allocated,
	    4, (cdb_count + 1));
	if (0 == error) {
		(*chips_db) = cdb;
		(*chips_db_count) = cdb_count;
	}

err_out:
	if (0 != error) {
		chip_db_free(cdb);
	}
	ini_destroy(ini);

	return (error);
}


void
chip_db_free(chip_p chips_db) {
	chip_p chip;

	if (NULL == chips_db)
		return;

	for (chip = chips_db; NULL != chip->name; chip ++) {
		free(chip->name);
	}
	free(chips_db);
}

int
chip_db_load(const char *file_name, size_t file_name_size,
    chip_p *chips_db, size_t *chips_db_count) {
	int error;
	uint8_t *buf = NULL;
	size_t buf_size;

	error = read_file(file_name, file_name_size, 0, 0,
	    (1024 * 1024 * 1024) /* 1Gb */,
	    &buf, &buf_size);
	if (0 != error)
		return (error);
	error = chip_db_ini_load(buf, buf_size, chips_db, chips_db_count);

	free(buf);

	return (error);
}


chip_p
chip_db_get_by_id(chip_p chips_db, const uint32_t chip_id,
    const uint8_t chip_id_size) {
	chip_p chip;

	if (NULL == chips_db || 0 == chip_id_size)
		return (NULL);
	for (chip = chips_db; NULL != chip->name; chip ++) {
		if (0 == (CHIP_OPT4_CHIP_ID & chip->opts4))
			continue;
		if (is_chip_id_eq(chip, chip_id, chip_id_size))
			return (chip);
	}

	return (NULL);
}

chip_p
chip_db_get_by_name(chip_p chips_db, const char *name) {
	chip_p chip;

	if (NULL == chips_db)
		return (NULL);
	for (chip = chips_db; NULL != chip->name; chip ++) {
		if (!strcasecmp(name, chip->name))
			return (chip);
	}

	return (NULL);
}

void
chip_db_dump_flt(chip_p chips_db, const char *name) {
	chip_p chip;
	size_t name_size = 0;

	if (NULL == chips_db)
		return;
	if (NULL != name) {
		name_size = strnlen(name, CHIP_NAME_MAX);
	}
	for (chip = chips_db; NULL != chip->name; chip ++) {
		if (0 != name_size &&
		    strncasecmp(chip->name, name, name_size))
			continue;
		printf("0x%04x:	%s\n", chip->chip_id, chip->name);
	}
}
