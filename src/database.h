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
	const char	*name;
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

void	chip_db_free(void);
int	chip_db_load(const char *file_name, size_t file_name_size);

int	is_chip_id_prob_eq(const chip_p chip, const uint32_t id, const uint8_t id_size);
int	is_chip_id_eq(const chip_p chip, const uint32_t id, const uint8_t id_size);

chip_p	chip_db_get_by_id(const uint32_t chip_id, const uint8_t chip_id_size);
chip_p	chip_db_get_by_name(const char *name);

void	chip_db_dump_flt(const char *name);

void	chip_db_print_info(const chip_p chip);


#endif
