#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "utils/strh2num.h"
#include "utils/mem_utils.h"
#include "utils/sys.h"
#include "utils/ini.h"

#include "minipro.h"
#include "database.h"

#define DB_CHIPS_PREALLOC	512


static fuse_decl_t avr_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 3, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "lock_byte",	.cmd = 0x41,		.size = 1, .offset = 0 },
};

static fuse_decl_t avr2_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 4, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses_lo",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "fuses_hi",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 1 },
	{ .name = "lock_byte",	.cmd = 0x41,		.size = 1, .offset = 0 },
};

static fuse_decl_t avr3_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 5, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses_lo",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "fuses_hi",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 1 },
	{ .name = "fuses_ext",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 2 },
	{ .name = "lock_byte",	.cmd = 0x41,		.size = 1, .offset = 0 },
};

static fuse_decl_t pic_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 6, .offset = 0 }, /* Items count, include this. */
	{ .name = "user_id0",	.cmd = 0x10,		.size = 2, .offset = 0 },
	{ .name = "user_id1",	.cmd = 0x10,		.size = 2, .offset = 2 },
	{ .name = "user_id2",	.cmd = 0x10,		.size = 2, .offset = 4 },
	{ .name = "user_id3",	.cmd = 0x10,		.size = 2, .offset = 6 },
	{ .name = "conf_word",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 0 },
};

static fuse_decl_t pic2_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 7, .offset = 0 }, /* Items count, include this. */
	{ .name = "user_id0",	.cmd = 0x10,		.size = 2, .offset = 0 },
	{ .name = "user_id1",	.cmd = 0x10,		.size = 2, .offset = 2 },
	{ .name = "user_id2",	.cmd = 0x10,		.size = 2, .offset = 4 },
	{ .name = "user_id3",	.cmd = 0x10,		.size = 2, .offset = 6 },
	{ .name = "conf_word",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 0 },
	{ .name = "conf_word1",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 2 },
};

static chip_p chips_db = NULL;


static int
chip_db_parse(ini_p ini, size_t sect_off, const uint8_t *sect_name,
    size_t sect_name_size, chip_p chip) {
	const uint8_t *val_name, *val;
	size_t val_off, val_name_size, val_size;
	uint32_t smask;

	if (NULL == ini || NULL == sect_name || NULL == chip)
		return (EINVAL);

	smask = 0;
	val_off = 0;
	while (0 == ini_sect_val_enum(ini, sect_off, &val_off,
	    &val_name, &val_name_size, &val, &val_size)) {
		if (0 == mem_cmpn_cstr("protocol_id", val_name, val_name_size)) {
			chip->protocol_id = ustrh2u8(val, val_size);
			smask |= (((uint32_t)1) << 0);
		} else if (0 == mem_cmpn_cstr("variant", val_name, val_name_size)) {
			chip->variant = ustrh2u8(val, val_size);
			smask |= (((uint32_t)1) << 1);
		} else if (0 == mem_cmpn_cstr("read_block_size", val_name, val_name_size)) {
			chip->read_block_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 2);
		} else if (0 == mem_cmpn_cstr("write_block_size", val_name, val_name_size)) {
			chip->write_block_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 3);
		} else if (0 == mem_cmpn_cstr("code_memory_size", val_name, val_name_size)) {
			chip->code_memory_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 4);
		} else if (0 == mem_cmpn_cstr("data_memory_size", val_name, val_name_size)) {
			chip->data_memory_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 5);
		} else if (0 == mem_cmpn_cstr("data_memory2_size", val_name, val_name_size)) {
			chip->data_memory2_size = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 6);
		} else if (0 == mem_cmpn_cstr("chip_id", val_name, val_name_size)) {
			chip->chip_id = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 7);
		} else if (0 == mem_cmpn_cstr("chip_id_size", val_name, val_name_size)) {
			chip->chip_id_size = ustrh2u8(val, val_size);
			smask |= (((uint32_t)1) << 8);
		} else if (0 == mem_cmpn_cstr("opts1", val_name, val_name_size)) {
			chip->opts1 = ustrh2u16(val, val_size);
			smask |= (((uint32_t)1) << 9);
		} else if (0 == mem_cmpn_cstr("opts2", val_name, val_name_size)) {
			chip->opts2 = ustrh2u16(val, val_size);
			smask |= (((uint32_t)1) << 10);
		} else if (0 == mem_cmpn_cstr("opts3", val_name, val_name_size)) {
			chip->opts3 = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 11);
		} else if (0 == mem_cmpn_cstr("opts4", val_name, val_name_size)) {
			chip->opts4 = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 12);
		} else if (0 == mem_cmpn_cstr("package_details", val_name, val_name_size)) {
			chip->package_details = ustrh2u32(val, val_size);
			smask |= (((uint32_t)1) << 13);
		} else if (0 == mem_cmpn_cstr("write_unlock", val_name, val_name_size)) {
			chip->write_unlock = ustrh2u16(val, val_size);
			smask |= (((uint32_t)1) << 14);
		} else if (0 == mem_cmpn_cstr("fuses", val_name, val_name_size)) {
			if (0 == mem_cmpn_cstr("NULL", val, val_size)) {
				chip->fuses = NULL;
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
				fprintf(stderr, "Unknown fuse value: \"%.*s\", line %zu.\n",
				    (int)val_size, val, val_off);
			}
			smask |= (((uint32_t)1) << 15);
		} else {
			fprintf(stderr, "Unknown field: \"%.*s\", line %zu.\n",
			    (int)val_name_size, val_name, val_off);
		}
		val_off ++;
	}

	if (((((uint32_t)1) << 16) - 1) != smask) {
		fprintf(stderr, "Section: \"%.*s\", at line %zu does not contain all required fields.\n",
		    (int)sect_name_size, sect_name, sect_off);
		return (EINVAL);
	}
	
	chip->name = zalloc((sect_name_size + sizeof(void*)));
	if (NULL == chip->name)
		return (ENOMEM);
	memcpy((void*)chip->name, sect_name, sect_name_size);

	return (0);
}


void
chip_db_free(void) {
	chip_p chip;

	if (NULL == chips_db)
		return;

	for (chip = chips_db; NULL != chip->name; chip ++) {
		free((void*)chip->name);
	}
	free(chips_db);
	chips_db = NULL;
}

int
chip_db_load(const char *file_name, size_t file_name_size) {
	int error;
	uint8_t *buf = NULL;
	const uint8_t *sect_name;
	size_t buf_size, sect_off, sect_name_size;
	size_t cdb_allocated = 0, cdb_count = 0;
	ini_p ini = NULL;

	error = read_file(file_name, file_name_size, 0, 0, (1024 * 1024 * 1024) /* 1Gb */,
	    &buf, &buf_size);
	if (0 != error)
		goto err_out;
	error = ini_create(&ini);
	if (0 != error)
		goto err_out;
	error = ini_buf_parse(ini, buf, buf_size);
	if (0 != error)
		goto err_out;
	/* Load chips. */
	sect_off = 0;
	while (0 == ini_sect_enum(ini, &sect_off, &sect_name, &sect_name_size)) {
		error = realloc_items((void**)&chips_db,
		    sizeof(chip_t), &cdb_allocated,
		    DB_CHIPS_PREALLOC, cdb_count);
		if (0 != error) {
			chip_db_free();
			goto err_out;
		}
		if (0 == chip_db_parse(ini, sect_off, sect_name, sect_name_size,
		    &chips_db[cdb_count])) {
			cdb_count ++;
		}
		sect_off ++;
	}

err_out:
	free(buf);
	ini_destroy(ini);

	return (error);
}


int
is_chip_id_prob_eq(const chip_p chip, const uint32_t id, const uint8_t id_size) {

	if (0 == chip->chip_id_size || 0 == id_size)
		return ((chip->chip_id_size == id_size));
	if (chip->chip_id_size == id_size)
		return ((chip->chip_id == id));
	if (chip->chip_id_size > id_size)
		return (0);
	/* chip->chip_id_size < id_size */
	return ((chip->chip_id == (id >> (8 * (id_size - chip->chip_id_size)))));
}

int
is_chip_id_eq(const chip_p chip, const uint32_t id, const uint8_t id_size) {

	if (chip->chip_id_size != id_size ||
	    0 == chip->chip_id_size)
		return (0);
	return ((chip->chip_id == id));
}

chip_p
chip_db_get_by_id(const uint32_t chip_id, const uint8_t chip_id_size) {
	chip_p chip;

	if (NULL == chips_db || 0 == chip_id_size)
		return (NULL);
	for (chip = chips_db; NULL != chip->name; chip ++) {
		if (is_chip_id_eq(chip, chip_id, chip_id_size))
			return (chip);
	}

	return (NULL);
}

chip_p
chip_db_get_by_name(const char *name) {
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
chip_db_dump_flt(const char *name) {
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

void
chip_db_print_info(const chip_p chip) {

	if (NULL == chip)
		return;

	printf("Name: %s\n", chip->name);

	/* Memory shape */
	printf("Memory: ");
	switch ((0xff000000 & chip->opts4)) {
	case 0x00000000:
		printf("%d Bytes", chip->code_memory_size);
		break;
	case 0x01000000:
		printf("%d Words", (chip->code_memory_size / 2));
		break;
	case 0x02000000:
		printf("%d Bits", chip->code_memory_size);
		break;
	default:
		printf(" unknown memory shape: 0x%x\n", (0xff000000 & chip->opts4));
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
	if (0 != (0x000000ff & chip->package_details)) {
		printf("Adapter%03d.JPG\n",
		    (0x000000ff & chip->package_details));
	} else if (0 != (0xff000000 & chip->package_details)) {
		printf("DIP%d\n",
		    ((chip->package_details >> 24) & 0x7f));
	} else {
		printf("ISP only\n");
	}

	/* ISP connection info */
	printf("ISP: ");
	if (0 != (0x0000ff00 & chip->package_details)) {
		printf("ICP%03d.JPG\n",
		    ((chip->package_details >> 8) & 0x000000ff));
	} else {
		printf("-\n");
	}

	printf("Protocol: 0x%02x\n", chip->protocol_id);
	printf("Read buffer size: %d Bytes\n", chip->read_block_size);
	printf("Write buffer size: %d Bytes\n", chip->write_block_size);
}


#ifdef DEBUG
void
chip_db_dump_to_h(void) {
	chip_p chip;

	if (NULL == chips_db)
		return;
	for (chip = chips_db; chip->name; chip ++) {
		printf("{\n");
		printf("	.name = \"%s\",\n", chip->name);
		printf("	.protocol_id = 0x%02x,\n", (0xffff & chip->protocol_id));
		printf("	.variant = 0x%02x,\n", chip->variant);
		printf("	.read_block_size = 0x%02x,\n", chip->read_block_size);
		printf("	.write_block_size = 0x%02x,\n", chip->write_block_size);
		printf("	.code_memory_size = 0x%02x,\n", chip->code_memory_size);
		printf("	.data_memory_size = 0x%02x,\n", chip->data_memory_size);
		printf("	.data_memory2_size = 0x%02x,\n", chip->data_memory2_size);
		printf("	.chip_id = 0x%02x,\n", chip->chip_id);
		printf("	.chip_id_size = 0x%02x,\n", chip->chip_id_size);
		printf("	.opts1 = 0x%02x,\n", chip->opts1);
		printf("	.opts2 = 0x%02x,\n", chip->opts2);
		printf("	.opts3 = 0x%02x,\n", chip->opts3);
		printf("	.opts4 = 0x%02x,\n", chip->opts4);
		printf("	.package_details = 0x%02x,\n", chip->package_details);
		printf("	.write_unlock = 0x%02x,\n", chip->write_unlock);
		printf("	.fuses = ");
		if (NULL == chip->fuses) {
			printf("NULL,\n");
		} else if (avr_fuses == chip->fuses) {
			printf("avr_fuses,\n");
		} else if (avr2_fuses == chip->fuses) {
			printf("avr2_fuses,\n");
		} else if (avr3_fuses == chip->fuses) {
			printf("avr3_fuses,\n");
		} else if (pic_fuses == chip->fuses) {
			printf("pic_fuses,\n");
		} else if (pic2_fuses == chip->fuses) {
			printf("pic2_fuses,\n");
		} else {
			printf("???,\n"); /* Must not reach here. */
		}
		printf("},\n");
	}
}

void
chip_db_dump_to_ini(void) {
	chip_p chip;
	char buf[4096];
	size_t buf_used;

	if (NULL == chips_db)
		return;
	for (chip = chips_db; chip->name; chip ++) {
		buf_used = (size_t)snprintf(buf, sizeof(buf),
		    "\n"
		    "[%s]\n"
		    "protocol_id=0x%02x\n"
		    "variant=0x%02x\n"
		    "read_block_size=0x%02x\n"
		    "write_block_size=0x%02x\n"
		    "code_memory_size=0x%02x\n"
		    "data_memory_size=0x%02x\n"
		    "data_memory2_size=0x%02x\n"
		    "chip_id=0x%02x\n"
		    "chip_id_size=0x%02x\n"
		    "opts1=0x%02x\n"
		    "opts2=0x%02x\n"
		    "opts3=0x%02x\n"
		    "opts4=0x%02xl\n"
		    "package_details=0x%02x\n"
		    "write_unlock=0x%02x\n"
		    "fuses=",
		    chip->name,
		    (0xffff & chip->protocol_id),
		    chip->variant,
		    chip->read_block_size,
		    chip->write_block_size,
		    chip->code_memory_size,
		    chip->data_memory_size,
		    chip->data_memory2_size,
		    chip->chip_id,
		    chip->chip_id_size,
		    chip->opts1,
		    chip->opts2,
		    chip->opts3,
		    chip->opts4,
		    chip->package_details,
		    chip->write_unlock);
		if (NULL == chip->fuses) {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "NULL\n");
		} else if (avr_fuses == chip->fuses) {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "avr_fuses\n");
		} else if (avr2_fuses == chip->fuses) {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "avr2_fuses\n");
		} else if (avr3_fuses == chip->fuses) {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "avr3_fuses\n");
		} else if (pic_fuses == chip->fuses) {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "pic_fuses\n");
		} else if (pic2_fuses == chip->fuses) {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "pic2_fuses\n");
		} else {
			buf_used += (size_t)snprintf((buf + buf_used), (sizeof(buf) - buf_used),
			    "???\n"); /* Must not reach here. */
		}
		printf("%s", buf);
	}
}
#endif
