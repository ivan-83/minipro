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
	uint8_t		variant;
	uint32_t	addressing_mode;
	uint32_t	read_block_size;
	uint32_t	write_block_size;

	uint32_t	code_memory_size; /* Presenting for every device. */
	uint32_t	data_memory_size;
	uint32_t	data_memory2_size;
	uint32_t	chip_id;	/* A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48). */
	uint8_t		chip_id_size;	/* chip_id_bytes_count */
	uint16_t	opts1;
	uint16_t	opts2;
	uint32_t	opts3;		// XXX: uint16_t
	uint32_t	opts4;
	uint32_t	package_details; /* Pins count or image ID for some devices. */
	uint16_t	write_unlock;	// XXX: uint8_t

	fuse_decl_p	fuses;		/* Configuration bytes that's presenting in some architectures. */
} __attribute__((__packed__)) chip_t, *chip_p;

#define CHIP_NAME_MAX		64	/* Max chip name len. */

#define CHIP_OPT4_TSOP48		0x01002078
#define CHIP_OPT4_SIZE_UNITS_MASK	0xff000000
#define CHIP_OPT4_SIZE_UNITS(__opts4)	((__opts4) >> 24)
#define CHIP_OPT4_SIZE_BYTES			0x00
#define CHIP_OPT4_SIZE_WORDS			0x01
#define CHIP_OPT4_SIZE_BITS			0x02
#define CHIP_OPT4_ERASE			0x00000010
#define CHIP_OPT4_CHIP_ID		0x00000020
#define CHIP_OPT4_ADDR_SCALE		0x00002000
#define CHIP_OPT4_PROTECTION		0x0000c000


typedef struct chip_id_map_s {
	uint8_t		shift;
	uint32_t	chip_id;
} chip_id_map_t, *chip_id_map_p;



chip_id_map_p chip_id_map(uint32_t index);

void	chip_db_free(void);
int	chip_db_load(const char *file_name, size_t file_name_size);
size_t	chip_db_get_count(void);

int	is_chip_id_prob_eq(const chip_p chip, const uint32_t id,
	    const uint8_t id_size);
int	is_chip_id_eq(const chip_p chip, const uint32_t id,
	    const uint8_t id_size);

chip_p	chip_db_get_by_id(const uint32_t chip_id,
	    const uint8_t chip_id_size);
chip_p	chip_db_get_by_name(const char *name);

void	chip_db_dump_flt(const char *name);

void	chip_db_print_info(const chip_p chip);

#endif
