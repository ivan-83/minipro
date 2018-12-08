/*-
 * Copyright (c) 2018 Rozhuk Ivan <rozhuk.im@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 */


#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <stdlib.h> /* malloc, exit */
#include <unistd.h> /* close, write, sysconf */
#include <fcntl.h> // open
#include <string.h> /* bcopy, bzero, memcpy, memmove, memset, strerror... */
#include <stdio.h> /* snprintf, fprintf */
#include <errno.h>

#include "utils/macro.h"
#include "utils/mem_utils.h"
#include "utils/sys.h"
#include "database.h"
#include "config.h"


#define LOG_ERR(error, descr)						\
	    if (0 != error)						\
		fprintf(stderr, "%s , line: %i, error: %i - %s - %s\n",	\
		    __FUNCTION__, __LINE__, (error), strerror((error)), (descr))
#define LOG_ERR_FMT(error, fmt, args...)				\
	    if (0 != error)						\
		fprintf(stderr, "%s , line: %i, error: %i - %s - " fmt "\n", \
		    __FUNCTION__, __LINE__, (error), strerror((error)), ##args)
#define LOG_INFO_FMT(fmt, args...)					\
	    fprintf(stdout, fmt"\n", ##args)


static const uint8_t mp_chip_raw_marker[] = {
	0x00, 0x00, 0x00
};


#define MC_CHIP_NAME_SIZE	40

typedef struct mp_chip_raw_s {
	char		name[MC_CHIP_NAME_SIZE];
	uint32_t	variant;
	uint32_t	code_memory_size;
	uint32_t	data_memory_size;
	uint32_t	data_memory2_size;
	uint16_t	write_unlock;
	uint16_t	read_block_size;
	uint16_t	write_block_size;
	uint16_t	opts1;
	uint32_t	opts2;
	uint32_t	opts3;
	uint64_t	chip_id;
	uint32_t	chip_id_size;
	uint32_t	unknown1;		/* ??? */
	uint32_t	package_details;
	uint32_t	opts4;
	uint32_t	protocol_id;
	uint32_t	unknown2;		/* ??? */
	uint32_t	unknown3;		/* ??? */
} __attribute__((__packed__)) mp_chip_raw_t, *mp_chip_raw_p;


static inline uint32_t
U8TO32n_BIG(const uint8_t *p, size_t size) {
	size_t i;
	uint32_t ret = 0;

	if (4 < size) {
		size = 4;
	}
	for (i = 0; i < size; i ++) {
		ret = ((ret << 8) | p[i]);
	}

	return (ret);
}

static void
mp_entry_name_normalize(char *name) {
	register size_t i, name_size;

	if (NULL == name)
		return;

	name_size = strnlen(name, MC_CHIP_NAME_SIZE);
	if (0 == name_size)
		return;
	/* Remove multiple spaces. */
	for (i = 0; i < (name_size - 1); i ++) {
		if (' ' == name[i] && ' ' == name[(i + 1)]) {
			memmove(&name[i], &name[(i + 1)], (name_size - i));
			name_size --;
			i --;
		}
	}
	/* Remove spaces from end. */
	while (0 != name_size && ' ' == name[(name_size - 1)]) {
		name[(name_size - 1)] = 0;
		name_size --;
	}
	name_size --;
}


static void
mp_entry_print(mp_chip_raw_p mp_chip) {
	const char *fuses;
	uint32_t chip_id;

	if (NULL == mp_chip)
		return;

	mp_entry_name_normalize(mp_chip->name);
	chip_id = U8TO32n_BIG((uint8_t*)&mp_chip->chip_id, mp_chip->chip_id_size);
	switch (mp_chip->unknown3) {
	case 2:
		fuses = "avr_fuses";
		break;
	default:
		fuses = "NULL";
	}

	printf(
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
	    "opts4=0x%02x\n"
	    "package_details=0x%02x\n"
	    "write_unlock=0x%02x\n"
	    "fuses=%s\n"
	    "unknown1=0x%02x\n"
	    "unknown2=0x%02x\n"
	    "unknown3=0x%02x\n"
	    "\n",
	    mp_chip->name,
	    mp_chip->protocol_id,
	    mp_chip->variant,
	    mp_chip->read_block_size,
	    mp_chip->write_block_size,
	    mp_chip->code_memory_size,
	    mp_chip->data_memory_size,
	    mp_chip->data_memory2_size,
	    chip_id,
	    mp_chip->chip_id_size,
	    mp_chip->opts1,
	    mp_chip->opts2,
	    mp_chip->opts3,
	    mp_chip->opts4,
	    mp_chip->package_details,
	    mp_chip->write_unlock,
	    fuses,
	    mp_chip->unknown1,
	    mp_chip->unknown2,
	    mp_chip->unknown3);
}

static int
mp_entry_is_valid(mp_chip_raw_p mp_chip) {
	register size_t i, name_size, mem_size;
	const char valid_chars[] = " !\"#$%&'()*+,-./0123456789:;<=>?@"
				   "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
				   "abcdefghijklmnopqrstuvwxyz{|}~";
	if (NULL == mp_chip)
		return (0);

	/* Validate name. */
	name_size = strnlen(mp_chip->name, MC_CHIP_NAME_SIZE);
	if (0 == name_size)
		return (0); /* Name too short. */
	if ((MC_CHIP_NAME_SIZE - 2) <= name_size)
		return (0); /* Name too long. */
	for (i = 0; i < name_size; i ++) {
		if (NULL == mem_chr(valid_chars, sizeof(valid_chars),
		    (uint8_t)mp_chip->name[i]))
			return (0); /* Non valid char. */
	}
	for (i = name_size; i < MC_CHIP_NAME_SIZE; i ++) {
		if (0 != mp_chip->name[i])
			return (0); /* Non zero after string end. */
	}

	/* Validate mem size. */
	mem_size = (mp_chip->code_memory_size +
	    mp_chip->data_memory_size + mp_chip->data_memory2_size);
	if (0 == mem_size)
		return (0); /* Zero mem size. */

	/* Validate chip_id_size */
	if (sizeof(uint32_t) < mp_chip->chip_id_size)
		return (0); /* chip_id_size too long. */
	/* Validate chip_id */
	//if (0 == mp_chip->chip_id_size && 0 != mp_chip->chip_id)
	//	return (0); /* chip_id probably bad. */

	/* Validate package_details */
	if (0 == mp_chip->package_details)
		return (0); /* package_details probably bad. */
	if (0 != (CHIP_PKG_D_ADAPTER_MASK & mp_chip->package_details) &&
	    0 != (CHIP_PKG_D_DIP_MASK & mp_chip->package_details))
		return (0); /* package_details probably bad. */

	/* Looks OK. */
	return (1);
}

static int
mp_entry_dump(const uint8_t *buf, size_t buf_size) {
	const uint8_t *buf_end = (buf + buf_size);
	uint8_t *cur_pos;
	mp_chip_raw_t mp_chip;
	size_t count = 0;

	if (NULL == buf || 0 == buf_size)
		return (EINVAL);

	cur_pos = (uint8_t*)buf;
	for (;;) {
		cur_pos = mem_find_ptr(cur_pos, buf, buf_size,
		    mp_chip_raw_marker, sizeof(mp_chip_raw_marker));
		if (NULL == cur_pos)
			break;
		/* Probably there is one entry before. */
		cur_pos += (sizeof(mp_chip_raw_marker) - sizeof(mp_chip_raw_t));
		memcpy(&mp_chip, cur_pos, sizeof(mp_chip_raw_t));
		if (mp_entry_is_valid(&mp_chip)) {
			mp_entry_print(&mp_chip);
			count ++;
		}
		cur_pos += sizeof(mp_chip_raw_t);
		for (;(cur_pos + sizeof(mp_chip_raw_t)) < buf_end;) {
			memcpy(&mp_chip, cur_pos, sizeof(mp_chip_raw_t));
			if (0 == mp_entry_is_valid(&mp_chip))
				break;
			mp_entry_print(&mp_chip);
			count ++;
			cur_pos += sizeof(mp_chip_raw_t);
		}
	}

	return (0);
}

int
main(int argc, char *argv[]) {
	int error = 0;
	int fd = -1;
	uint8_t *buf = NULL;
	struct stat sb;

	bzero(&sb, sizeof(struct stat));

	/* Args check. */
	if (argc != 2) {
		error = EINVAL;
		LOG_ERR(error, "Only one file name required. (InfoIC.dll)");
		goto err_out;
	}

	/* Open file with data. */
	fd = open(argv[1], O_RDONLY);
	if (-1 == fd) {
		error = errno;
		LOG_ERR_FMT(error, "open(%s)", argv[1]);
		goto err_out;
	}
	/* Get file size. */
	if (0 != fstat(fd, &sb)) {
		error = errno;
		LOG_ERR_FMT(error, "fstat(%s)", argv[1]);
		goto err_out;
	}
	/* Map file data. */
	buf = mmap(NULL, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == buf) {
		error = errno;
		LOG_ERR(error, "mmap()");
		goto err_out;
	}

	error = mp_entry_dump(buf, (size_t)sb.st_size);

err_out:
	munmap((void*)buf, (size_t)sb.st_size);
	close(fd);

	return (error);
}
