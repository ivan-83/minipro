#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h> /* malloc, exit */
#include <stdio.h> /* snprintf, fprintf */
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <libusb.h>

#include "utils/macro.h"
#include "utils/mem_utils.h"
#include "utils/strh2num.h"
#include "minipro.h"


typedef struct minipro_handle_s {
	libusb_device_handle *usb_handle;
	libusb_context	*ctx;
	chip_p		chip;
	uint8_t		icsp;
	uint8_t		msg_hdr[16]; /* Message constan header with chip settings. */
	uint8_t		msg[4096];
	uint8_t		*read_block_buf;
	uint8_t		*write_block_buf;
	int		verboce;
	minipro_ver_t	ver;
} minipro_t;

static const uint8_t mp_chip_page_read_cmd[] = {
	MP_CMD_READ_CODE,
	MP_CMD_READ_DATA,
	0,
	0
};

static const uint8_t mp_chip_page_write_cmd[] = {
	MP_CMD_WRITE_CODE,
	MP_CMD_WRITE_DATA,
	0,
	0
};


typedef struct minipro_zif_pins_s {
	uint8_t pin;
	uint8_t latch;
	uint8_t oe;
	uint8_t mask;
} mp_zif_pins_t, *mp_zif_pins_p;

enum VPP_PINS {
	VPP1,	VPP2,	VPP3,	VPP4,	VPP9,	VPP10,	VPP30,	VPP31,
	VPP32,	VPP33,	VPP34,	VPP36,	VPP37,	VPP38,	VPP39,	VPP40
};

enum VCC_PINS {
	VCC1,	VCC2,	VCC3,	VCC4,	VCC5,	VCC6,	VCC7,	VCC8,
	VCC9,	VCC10,	VCC11,	VCC12,	VCC13,	VCC21,	VCC30,	VCC32,
	VCC33,	VCC34,	VCC35,	VCC36,	VCC37,	VCC38,	VCC39,	VCC40
};

enum GND_PINS {
	GND1,	GND2,	GND3,	GND4,	GND5,	GND6,	GND7,	GND8,
	GND9,	GND10,	GND11,	GND12,	GND14,	GND16,	GND20,	GND30,
	GND31,	GND32,	GND34,	GND35,	GND36,	GND37,	GND38,	GND39,
	GND40
};

/* 16 VPP pins. NPN trans. mask */
static mp_zif_pins_t vpp_pins[] = {
	{ .pin =  1, .latch = 1, .oe = 1, .mask = 0x04 },
	{ .pin =  2, .latch = 1, .oe = 1, .mask = 0x08 },
	{ .pin =  3, .latch = 0, .oe = 1, .mask = 0x04 },
	{ .pin =  4, .latch = 0, .oe = 1, .mask = 0x08 },
	{ .pin =  9, .latch = 0, .oe = 1, .mask = 0x20 },
	{ .pin = 10, .latch = 0, .oe = 1, .mask = 0x10 },
	{ .pin = 30, .latch = 1, .oe = 1, .mask = 0x01 },
	{ .pin = 31, .latch = 0, .oe = 1, .mask = 0x01 },
	{ .pin = 32, .latch = 1, .oe = 1, .mask = 0x80 },
	{ .pin = 33, .latch = 0, .oe = 1, .mask = 0x40 },
	{ .pin = 34, .latch = 0, .oe = 1, .mask = 0x02 },
	{ .pin = 36, .latch = 1, .oe = 1, .mask = 0x02 },
	{ .pin = 37, .latch = 0, .oe = 1, .mask = 0x80 },
	{ .pin = 38, .latch = 1, .oe = 1, .mask = 0x40 },
	{ .pin = 39, .latch = 1, .oe = 1, .mask = 0x20 },
	{ .pin = 40, .latch = 1, .oe = 1, .mask = 0x10 }
};

/* 24 VCC Pins. PNP trans. mask */
static mp_zif_pins_t vcc_pins[] = {
	{ .pin =  1, .latch = 2, .oe = 2, .mask = 0x7f },
	{ .pin =  2, .latch = 2, .oe = 2, .mask = 0xef },
	{ .pin =  3, .latch = 2, .oe = 2, .mask = 0xdf },
	{ .pin =  4, .latch = 3, .oe = 2, .mask = 0xfe },
	{ .pin =  5, .latch = 2, .oe = 2, .mask = 0xfb },
	{ .pin =  6, .latch = 3, .oe = 2, .mask = 0xfb },
	{ .pin =  7, .latch = 4, .oe = 2, .mask = 0xbf },
	{ .pin =  8, .latch = 4, .oe = 2, .mask = 0xfd },
	{ .pin =  9, .latch = 4, .oe = 2, .mask = 0xfb },
	{ .pin = 10, .latch = 4, .oe = 2, .mask = 0xf7 },
	{ .pin = 11, .latch = 4, .oe = 2, .mask = 0xfe },
	{ .pin = 12, .latch = 4, .oe = 2, .mask = 0x7f },
	{ .pin = 13, .latch = 4, .oe = 2, .mask = 0xef },
	{ .pin = 21, .latch = 4, .oe = 2, .mask = 0xdf },
	{ .pin = 30, .latch = 3, .oe = 2, .mask = 0xbf },
	{ .pin = 32, .latch = 3, .oe = 2, .mask = 0xfd },
	{ .pin = 33, .latch = 3, .oe = 2, .mask = 0xdf },
	{ .pin = 34, .latch = 3, .oe = 2, .mask = 0xf7 },
	{ .pin = 35, .latch = 3, .oe = 2, .mask = 0xef },
	{ .pin = 36, .latch = 3, .oe = 2, .mask = 0x7f },
	{ .pin = 37, .latch = 2, .oe = 2, .mask = 0xf7 },
	{ .pin = 38, .latch = 2, .oe = 2, .mask = 0xbf },
	{ .pin = 39, .latch = 2, .oe = 2, .mask = 0xfe },
	{ .pin = 40, .latch = 2, .oe = 2, .mask = 0xfd }
};

/* 25 GND Pins. NPN trans. mask */
static mp_zif_pins_t gnd_pins[] = {
	{ .pin =  1, .latch = 6, .oe = 2, .mask = 0x04 },
	{ .pin =  2, .latch = 6, .oe = 2, .mask = 0x08 },
	{ .pin =  3, .latch = 6, .oe = 2, .mask = 0x40 },
	{ .pin =  4, .latch = 6, .oe = 2, .mask = 0x02 },
	{ .pin =  5, .latch = 5, .oe = 2, .mask = 0x04 },
	{ .pin =  6, .latch = 5, .oe = 2, .mask = 0x08 },
	{ .pin =  7, .latch = 5, .oe = 2, .mask = 0x40 },
	{ .pin =  8, .latch = 5, .oe = 2, .mask = 0x02 },
	{ .pin =  9, .latch = 5, .oe = 2, .mask = 0x01 },
	{ .pin = 10, .latch = 5, .oe = 2, .mask = 0x80 },
	{ .pin = 11, .latch = 5, .oe = 2, .mask = 0x10 },
	{ .pin = 12, .latch = 5, .oe = 2, .mask = 0x20 },
	{ .pin = 14, .latch = 7, .oe = 2, .mask = 0x08 },
	{ .pin = 16, .latch = 7, .oe = 2, .mask = 0x40 },
	{ .pin = 20, .latch = 9, .oe = 2, .mask = 0x01 },
	{ .pin = 30, .latch = 7, .oe = 2, .mask = 0x04 },
	{ .pin = 31, .latch = 6, .oe = 2, .mask = 0x01 },
	{ .pin = 32, .latch = 6, .oe = 2, .mask = 0x80 },
	{ .pin = 34, .latch = 6, .oe = 2, .mask = 0x10 },
	{ .pin = 35, .latch = 6, .oe = 2, .mask = 0x20 },
	{ .pin = 36, .latch = 7, .oe = 2, .mask = 0x20 },
	{ .pin = 37, .latch = 7, .oe = 2, .mask = 0x10 },
	{ .pin = 38, .latch = 7, .oe = 2, .mask = 0x02 },
	{ .pin = 39, .latch = 7, .oe = 2, .mask = 0x80 },
	{ .pin = 40, .latch = 7, .oe = 2, .mask = 0x01 }
};


void	minipro_chip_clean(minipro_p mp);

#define MP_LOG_TEXT(__text)						\
	if (0 != mp->verboce) {						\
		fprintf(stdout, "%s\n",	(__text));			\
	}
#define MP_LOG_TEXT_FMT(__fmt, args...)					\
	if (0 != mp->verboce) {						\
		fprintf(stdout, __fmt "\n", ##args);			\
	}

#define MP_LOG_ERR(__error, __descr)					\
	if (0 != (__error) && 0 != mp->verboce) {			\
		fprintf(stderr, "%s:%i %s: error: %i - %s: %s\n",	\
		    __FILE__, __LINE__, __FUNCTION__,			\
		    (__error), strerror((__error)), (__descr));		\
	}

#define MP_LOG_USB_ERR(__error, __descr)				\
	if (0 != (__error) && 0 != mp->verboce) {			\
		fprintf(stderr, "%s:%i %s: error: %i = %s - %s: %s\n",	\
		    __FILE__, __LINE__, __FUNCTION__, (__error),	\
		    libusb_error_name((__error)),			\
		    libusb_strerror((__error)), (__descr));		\
	}

#define MP_LOG_ERR_FMT(__error, __fmt, args...)				\
	if (0 != (__error) && 0 != mp->verboce) {			\
		fprintf(stderr, "%s:%i %s: error: %i - %s: " __fmt "\n", \
		    __FILE__, __LINE__, __FUNCTION__,			\
		    (__error), strerror((__error)), ##args);		\
	}

#define MP_RET_ON_ERR(__error) {					\
	int ret_error = (__error);					\
	if (0 != ret_error) {						\
		if (0 != mp->verboce)					\
			fprintf(stderr, "%s:%i %s: error = %i.\r\n",	\
			    __FILE__, __LINE__, __FUNCTION__, ret_error); \
		return (ret_error);					\
	}								\
}

#define MP_RET_ON_ERR_CLEANUP(__error) {				\
	error = (__error);						\
	if (0 != error) {						\
		if (0 != mp->verboce)					\
			fprintf(stderr, "%s:%i %s: error = %i.\r\n",	\
			    __FILE__, __LINE__, __FUNCTION__, error);	\
		goto err_out;						\
	}								\
}

#define MP_PROGRESS_UPDATE(__cb, __mp, __done, __total, __udata)	\
	if (NULL != (__cb)) {						\
		(__cb)((__mp), (__done), (__total), (__udata));		\
	}



static inline uint16_t
U8TO16_LITTLE(const uint8_t *p) {

	return (((uint16_t)p[0]) | ((uint16_t)(((uint16_t)p[1]) << 8)));
}

static inline uint32_t
U8TO32n_LITTLE(const uint8_t *p, size_t size) {
	uint32_t ret = 0;

	if (4 < size) {
		size = 4;
	}
	memcpy(&ret, p, size);

	return (ret);
}

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

static inline void
U32TO8_LITTLE(const uint32_t v, uint8_t *p) {
	p[0] = (uint8_t)(v      );
	p[1] = (uint8_t)(v >>  8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static inline void
U24TO8_LITTLE(const uint32_t v, uint8_t *p) {
	p[0] = (uint8_t)(v      );
	p[1] = (uint8_t)(v >>  8);
	p[2] = (uint8_t)(v >> 16);
}

static inline void
U16TO8_LITTLE(const uint16_t v, uint8_t *p) {
	p[0] = (uint8_t)(v      );
	p[1] = (uint8_t)(v >>  8);
}

static inline void
U32TO8n_LITTLE(const uint32_t v, size_t size, uint8_t *p) {

	if (4 < size) {
		size = 4;
	}
	memcpy(p, &v, size);
}


/* Internal staff. */
/* abc=zxy */
static int
buf_get_named_line_val32(const uint8_t *buf, size_t buf_size,
    const char *val_name, size_t val_name_size, uint32_t *value) {
	const uint8_t *ptr, *end;

	if (NULL == buf || 0 == buf_size ||
	    NULL == val_name || 0 == val_name_size || NULL == value)
		return (EINVAL);

	for (;;) {
		ptr = memmem(buf, buf_size, val_name, val_name_size);
		if (NULL == ptr)
			return (-1);
		/* Is start from new line? */
		if (buf < ptr && (
		    0x0a != ptr[-1] && /* LF */
		    0x0d != ptr[-1] && /* CR */
		    '\t' != ptr[-1] && /* TAB */
		    ' ' != ptr[-1]))
			continue; /* Not mutch, skip. */
		ptr += val_name_size;
		/* Is this name not part of another name? */
		if ('\t' == (*ptr) ||
		    ' ' == (*ptr) ||
		    '=' == (*ptr))
			break;
	}
	/* Found, extract value data. */
	ptr ++;
	/* Find end of line. */
	end = mem_chr_ptr(ptr, buf, buf_size, 0x0a/* LF */);
	if (NULL == end) {
		end = (buf + buf_size);
	} else if (ptr < end && 0x0d == end[-1]) { /* CR */
		end --;
	}
	/* Find value start. */
	for (; ptr < end && (' ' == (*ptr) || '=' == (*ptr) || '\t' == (*ptr)); ptr ++)
		;
	if (ptr == end)
		return (EINVAL);
	(*value) = ustrh2u32(ptr, (size_t)(end - ptr));

	return (0);
}

static size_t
memcmp_idx(const uint8_t *buf1, const uint8_t *buf2, const size_t size) {
	register size_t i;

	if (0 != memcmp(buf1, buf2, size)) {
		for (i = 0; size > i; i ++) {
			if (buf1[i] != buf2[i])
				return (i);
		}
		/* Never gets here. */
	}

	return (size);
}


static int
msg_transfer(minipro_p mp, uint8_t direction,
    uint8_t *buf, size_t buf_size, size_t *transferred) {
	int error, bytes_transferred = 0;

	error = libusb_claim_interface(mp->usb_handle, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_claim_interface().");
		goto err_out;
	}
	error = libusb_bulk_transfer(mp->usb_handle, (1 | direction),
	    buf, (int)buf_size, &bytes_transferred, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_bulk_transfer().");
		goto err_out;
	}
	error = libusb_release_interface(mp->usb_handle, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_release_interface().");
		goto err_out;
	}

err_out:
	if (NULL != transferred) {
		(*transferred) = (size_t)bytes_transferred;
	}

	return (error);
}

static int
msg_send(minipro_p mp, uint8_t *buf, size_t buf_size,
    size_t *transferred) {
	int error;
	size_t bytes_transferred;

	error = msg_transfer(mp, LIBUSB_ENDPOINT_OUT, buf, buf_size,
	    &bytes_transferred);
	if (NULL != transferred) {
		(*transferred) = bytes_transferred;
	}
	if (0 != error)
		return (error);
	/* Always check sended size! */
	if (bytes_transferred != buf_size) {
		error = EIO;
		MP_LOG_ERR_FMT(error,
		    "expected %zu bytes but %zu bytes transferred.",
		    buf_size, bytes_transferred);
	}
	return (error);
}

static int
msg_recv(minipro_p mp, uint8_t *buf, size_t buf_size,
    size_t *transferred) {

	return (msg_transfer(mp, LIBUSB_ENDPOINT_IN, buf, buf_size,
	    transferred));
}

static void
msg_chip_hdr_gen(chip_p chip, uint8_t icsp, uint8_t *buf,
    size_t buf_size) {

	memset(buf, 0x00, buf_size);
	//buf[0] = 0x00;
	buf[1] = chip->protocol_id;
	buf[2] = chip->variant;
	//buf[3] = 0x00;
	buf[4] = ((chip->data_memory_size >> 8) & 0xff);

	U16TO8_LITTLE(chip->opts1, &buf[5]);
	buf[8] = buf[6];
	U16TO8_LITTLE(chip->opts2, &buf[6]);
	U16TO8_LITTLE(chip->opts3, &buf[9]);

	buf[11] = icsp;
	U32TO8_LITTLE(chip->code_memory_size, &buf[12]);
}

static void
msg_chip_hdr_set(minipro_p mp, uint8_t cmd, size_t msg_size) {

	memcpy(mp->msg, mp->msg_hdr, msg_size);
	mp->msg[0] = cmd;
	if (sizeof(mp->msg_hdr) < msg_size) {
		memset((mp->msg + sizeof(mp->msg_hdr)), 0x00,
		    (msg_size - sizeof(mp->msg_hdr)));
	}
}

static int
msg_send_chip_hdr(minipro_p mp, uint8_t cmd, size_t msg_size,
    size_t *transferred) {

	if (NULL == mp ||
	    (NULL == mp->chip &&
	     NULL == memchr(mp_cmd_wo_chip, cmd, sizeof(mp_cmd_wo_chip))))
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, msg_size);
	return (msg_send(mp, mp->msg, msg_size, transferred));
}


/* API */

int
minipro_open(uint16_t vendor_id, uint16_t product_id,
    int verboce, minipro_p *handle_ret) {
	int error;
	minipro_p mp;

	if (NULL == handle_ret)
		return (EINVAL);
	mp = zalloc(sizeof(minipro_t));
	if (NULL == mp)
		return (ENOMEM);
	mp->verboce = verboce;

	error = libusb_init(&mp->ctx);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_init().");
		goto err_out;
	}

	mp->usb_handle = libusb_open_device_with_vid_pid(mp->ctx,
	    vendor_id, product_id);
	if (NULL == mp->usb_handle) {
		error = ENOENT;
		MP_LOG_ERR(error, "error opening device.");
		goto err_out;
	}

	error = minipro_get_version_info(mp, &mp->ver);
	if (0 != error) {
		MP_LOG_ERR(error, "minipro_get_version_info().");
		goto err_out;
	}

	(*handle_ret) = mp;

	return (0);

err_out:
	minipro_close(mp);
	return (error);
}

void
minipro_close(minipro_p mp) {

	if (NULL == mp)
		return;

	minipro_chip_clean(mp);
	libusb_close(mp->usb_handle);
	free(mp);
}

int
minipro_get_version_info(minipro_p mp, minipro_ver_p ver) {
	size_t rcvd;

	if (NULL == mp || NULL == ver)
		return (EINVAL);
	memset(ver, 0x00, sizeof(minipro_ver_t));
	MP_RET_ON_ERR(msg_send_chip_hdr(mp, MP_CMD_GET_VERSION, 5, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd));
	if ((sizeof(minipro_ver_t) - 1) > rcvd) { /* In boot mode returned 39 bytes. */
		MP_LOG_ERR_FMT(EMSGSIZE,
		    "expected at least %zu bytes but %zu bytes received.",
		    (sizeof(minipro_ver_t) - 1), rcvd);
		return (EMSGSIZE);
	}
	memcpy(ver, mp->msg, MIN(rcvd, sizeof(minipro_ver_t)));

	return (0);
}

int
minipro_is_version_info_ok(minipro_p mp) {

	if (NULL == mp)
		return (EINVAL);

	/* Model/dev ver preprocess. */
	switch (mp->ver.device_version) {
	case MP_DEV_VER_TL866A:
	case MP_DEV_VER_TL866CS:
		break;
	default:
		return (EBADMSG);
	}

	/* Device status. */
	switch (mp->ver.device_status) {
	case MP_DEV_VER_STATUS_NORMAL:
		break;
	case MP_DEV_VER_STATUS_BOOTLOADER: /* Can't use the device if it's in boot mode! */
		MP_LOG_TEXT("Bootloader mode detected, cant work with "
		    "device.");
		return (EPROTONOSUPPORT);
	default:
		MP_LOG_TEXT_FMT("Unknown device status: %"PRIu8,
		    mp->ver.device_status);
		return (EPROTONOSUPPORT);
	}

	return (0);
}

void
minipro_print_info(minipro_p mp) {
	const char *dvstr, *sstr;
	char str_buf[64];

	/* Model/dev ver preprocess. */
	switch (mp->ver.device_version) {
	case MP_DEV_VER_TL866A:
	case MP_DEV_VER_TL866CS:
		dvstr = minipro_dev_ver_str[mp->ver.device_version];
		break;
	default:
		snprintf(str_buf, sizeof(str_buf),
		    "%s (ver = %"PRIu8")",
		    minipro_dev_ver_str[MP_DEV_VER_TL866_UNKNOWN],
		    mp->ver.device_version);
		dvstr = (const char*)str_buf;
	}

	/* Device status. */
	switch (mp->ver.device_status) {
	case MP_DEV_VER_STATUS_NORMAL:
	case MP_DEV_VER_STATUS_BOOTLOADER:
		sstr = minipro_dev_status_str[mp->ver.device_status];
		break;
	default:
		sstr = minipro_dev_status_str[MP_DEV_VER_STATUS_UNKNOWN];
	}

	printf("Minipro: %s, fw: v%02d.%"PRIu8".%"PRIu8"%s, code: "
	    "%.*s, serial: %.*s, status: %"PRIu8" - %s\n",
	    dvstr,
	    mp->ver.hardware_version, mp->ver.firmware_version_major,
	    mp->ver.firmware_version_minor,
	    ((MP_FW_VER_MIN > ((mp->ver.firmware_version_major << 8) |
	     mp->ver.firmware_version_minor)) ?
	      " (newer fw avaible)" : ""),
	    (int)sizeof(mp->ver.device_code), mp->ver.device_code,
	    (int)sizeof(mp->ver.serial_num), mp->ver.serial_num,
	    mp->ver.device_status, sstr);
}


static int
minipro_hardware_check_pins(minipro_p mp, mp_zif_pins_p pins,
    size_t pins_count, int is_gnd, size_t *errors_count) {
	int error = 0, pin_ok;
	size_t i, rcvd;

	if (NULL == mp || NULL == pins || NULL == errors_count)
		return (EINVAL);

	/* Reset pin drivers state. */
	MP_RET_ON_ERR(msg_send_chip_hdr(mp,
	    MP_CMD_RST_PIN_DRIVERS, 10, NULL));

	/* Testing pins drivers. */
	for (i = 0; i < pins_count; i ++) {
		msg_chip_hdr_set(mp, MP_CMD_SET_LATCH, 32);
		if (0 == is_gnd) {
			mp->msg[7] = 1; /* This is number of latches we want to set (1-8). */
		} else {
			mp->msg[7] = ((pins[i].latch == 9) ?
			    2 : 1); /* Special handle for pin GND20. */
		}
		mp->msg[8] = pins[i].oe; /* This is output enable we want to select(/OE) (1=OE_VPP, 2=OE_VCC+GND, 3=BOTH). */
		mp->msg[9] = pins[i].latch; /* This is latch number we want to set (0-7; see the schematic diagram). */
		mp->msg[10] = pins[i].mask; /* This is latch value we want to write (see the schematic diagram). */
		MP_RET_ON_ERR_CLEANUP(msg_send(mp, mp->msg, 32, NULL));
		/* Wait. */
		usleep(MP_SET_LATCH_DELAY);
		/* Read pins status. */
		MP_RET_ON_ERR(msg_send_chip_hdr(mp,
		    MP_CMD_READ_ZIF_PINS, 18, NULL));
		MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg,
		    sizeof(mp->msg), &rcvd));
		if (2 > rcvd ||
		    0 != mp->msg[1]) {
			error = -1;
			MP_LOG_ERR_FMT(error,
			    "Overcurrent protection detected while "
			    "testing pin %02"PRIu8,
			    pins[i].pin);
			goto err_out;
		}
		if ((6 + pins[i].pin) >= rcvd) {
			error = EBADMSG;
			MP_LOG_ERR_FMT(error,
			    "Not enough data readed for pin %02"PRIu8" "
			    "readed: %zu, expected: %zu",
			    pins[i].pin,
			    rcvd, (size_t)(6 + pins[i].pin));
			goto err_out;
		}
		pin_ok = ((0 == is_gnd) ?
		    (0 == mp->msg[(6 + pins[i].pin)]) :
		    (0 != mp->msg[(6 + pins[i].pin)]));
		if (0 != pin_ok) {
			(*errors_count) ++;
		}
		MP_LOG_TEXT_FMT("	pin %02"PRIu8" is %s",
		    pins[i].pin,
		    ((0 != pin_ok) ? "Bad" : "OK"));
	}

err_out:
	msg_send_chip_hdr(mp, MP_CMD_RST_PIN_DRIVERS, 10, NULL);
	minipro_end_transaction(mp); /* Call after msg processed. */
	return (error);
}
int
minipro_hardware_check(minipro_p mp, size_t *errors_count) {
	int error;
	size_t rcvd;

	if (NULL == mp || NULL == errors_count)
		return (EINVAL);

	(*errors_count) = 0;

	/* Reset pin drivers state. */
	MP_RET_ON_ERR(msg_send_chip_hdr(mp,
	    MP_CMD_RST_PIN_DRIVERS, 10, NULL));

	MP_LOG_TEXT("Testing 16 VPP pin drivers...");
	MP_RET_ON_ERR(minipro_hardware_check_pins(mp,
	    vpp_pins, SIZEOF(vpp_pins), 0, errors_count));

	MP_LOG_TEXT("Testing 24 VCC pin drivers...");
	MP_RET_ON_ERR(minipro_hardware_check_pins(mp,
	    vcc_pins, SIZEOF(vcc_pins), 0, errors_count));

	MP_LOG_TEXT("Testing 25 GND pin drivers...");
	MP_RET_ON_ERR(minipro_hardware_check_pins(mp,
	    gnd_pins, SIZEOF(gnd_pins), 1, errors_count));

	/* Testing VPP overcurrent protection. */
	msg_chip_hdr_set(mp, MP_CMD_SET_LATCH, 32);
	mp->msg[7] = 2; /* We will set two latches. */
	mp->msg[8] = MP_OE_ALL; /* Both OE_VPP and OE_GND active. */
	mp->msg[9] = vpp_pins[VPP1].latch;
	mp->msg[10] = vpp_pins[VPP1].mask; /* Put the VPP voltage to the ZIF pin1. */
	mp->msg[11] = gnd_pins[GND1].latch;
	mp->msg[12] = gnd_pins[GND1].mask; /* Now put the same pin ZIF 1 to the GND. */
	MP_RET_ON_ERR_CLEANUP(msg_send(mp, mp->msg, 32, NULL));
	/* Wait. */
	usleep(MP_SET_LATCH_DELAY);
	/* Read OVC status. */
	MP_RET_ON_ERR(msg_send_chip_hdr(mp,
	    MP_CMD_READ_ZIF_PINS, 18, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg,
	    sizeof(mp->msg), &rcvd));
	if (2 > rcvd ||
	    0 == mp->msg[1]) {
		MP_LOG_TEXT("VPP overcurrent protection failed!");
		(*errors_count) ++;
	} else {
		MP_LOG_TEXT("VPP overcurrent protection is OK.");		
	}

	/* Testing VCC overcurrent protection. */
	msg_chip_hdr_set(mp, MP_CMD_SET_LATCH, 32);
	mp->msg[7] = 2; /* We will set two latches. */
	mp->msg[8] = MP_OE_VCC_GND; /* OE GND is active. */
	mp->msg[9] = vcc_pins[VCC40].latch;
	mp->msg[10] = vcc_pins[VCC40].mask; /* Put the VCC voltage to the ZIF pin 40. */
	mp->msg[11] = gnd_pins[GND40].latch;
	mp->msg[12] = gnd_pins[GND40].mask; /* Now put the same pin ZIF 40 to the GND. */
	MP_RET_ON_ERR_CLEANUP(msg_send(mp, mp->msg, 32, NULL));
	/* Wait. */
	usleep(MP_SET_LATCH_DELAY);
	/* Read OVC status. */
	MP_RET_ON_ERR(msg_send_chip_hdr(mp,
	    MP_CMD_READ_ZIF_PINS, 18, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg,
	    sizeof(mp->msg), &rcvd));
	if (2 > rcvd ||
	    0 == mp->msg[1]) {
		MP_LOG_TEXT("VCC overcurrent protection failed!");
		(*errors_count) ++;
	} else {
		MP_LOG_TEXT("VCC overcurrent protection is OK.");		
	}

err_out:
	msg_send_chip_hdr(mp, MP_CMD_RST_PIN_DRIVERS, 10, NULL);
	minipro_end_transaction(mp); /* Call after msg processed. */
	return (error);
}

int
minipro_chip_set(minipro_p mp, chip_p chip, uint8_t icsp) {
	int error;
	uint8_t tsop48;

	if (NULL == mp)
		return (EINVAL);
	/* Cleanup old. */
	minipro_chip_clean(mp);
	if (NULL == chip)
		return (0);

	if (0 == chip->read_block_size ||
	    0 == chip->write_block_size) {
		MP_LOG_ERR_FMT(EINVAL,
		    "Cant handle this chip: read_block_size = %i, "
		    "write_block_size = %i, zero size not allowed.",
		    chip->read_block_size,
		    chip->write_block_size);
		return (EINVAL);
	}
	if ((sizeof(mp->msg) - 7) < chip->read_block_size ||
	    (sizeof(mp->msg) - 7) < chip->write_block_size) {
		MP_LOG_ERR_FMT(EINVAL,
		    "Cant handle this chip: increase msg_hdr[%zu] buf "
		    "to %i and recompile.",
		    (size_t)sizeof(mp->msg),
		    (7 + MAX(chip->read_block_size, chip->write_block_size)));
		return (EINVAL);
	}

	/* Set new. */
	mp->read_block_buf = malloc((chip->read_block_size + 16));
	mp->write_block_buf = malloc((chip->write_block_size + 16));
	if (NULL == mp->read_block_buf ||
	    NULL == mp->write_block_buf) {
		minipro_chip_clean(mp);
		return (ENOMEM);
	}
	mp->chip = chip;
	mp->icsp = icsp;
	/* Generate msg header with chip constans. */
	msg_chip_hdr_gen(chip, icsp, mp->msg_hdr, sizeof(mp->msg_hdr));

	if (CHIP_OPT4_TSOP48 != chip->opts4)
		return (0);
	/* Unlocking the TSOP48 adapter (if applicable). */
	error = minipro_unlock_tsop48(mp, &tsop48);
	if (0 != error) {
		minipro_chip_clean(mp);
		MP_LOG_ERR(error, "Cant unlock TSOP48.");
		return (error);
	}
	switch (tsop48) {
	case MP_TSOP48_TYPE_V3:
		MP_LOG_TEXT("Found TSOP adapter V3.");
		break;
	case MP_TSOP48_TYPE_NONE:
		minipro_chip_clean(mp);
		MP_LOG_ERR(EINVAL, "TSOP adapter not found!");
		return (EINVAL);
	case MP_TSOP48_TYPE_V2:
		MP_LOG_TEXT("Found TSOP adapter V0.");
		break;
	case MP_TSOP48_TYPE_FAKE1:
	case MP_TSOP48_TYPE_FAKE2:
		MP_LOG_TEXT("Fake TSOP adapter found!");
		break;
	}

	/* Overcurrency status check. */
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	error = minipro_overcurrency_chk(mp);
	MP_RET_ON_ERR(minipro_end_transaction(mp));
	MP_RET_ON_ERR(error);

	return (0);
}
	
void
minipro_chip_clean(minipro_p mp) {

	if (NULL == mp)
		return;

	/* Turn off the power on zif socket. */
	minipro_end_transaction(mp);
	memset(mp->msg_hdr, 0x00, sizeof(mp->msg_hdr));
	memset(mp->msg, 0x00, sizeof(mp->msg));
	/* Free res. */
	free(mp->read_block_buf);
	free(mp->write_block_buf);
	mp->read_block_buf = NULL;
	mp->write_block_buf = NULL;
	mp->chip = NULL;
}

chip_p
minipro_chip_get(minipro_p mp) {

	if (NULL == mp)
		return (NULL);
	return (mp->chip);
}


int
minipro_begin_transaction(minipro_p mp) {

	return (msg_send_chip_hdr(mp, MP_CMD_WRITE_CONFIG, 48, NULL));
}

int
minipro_end_transaction(minipro_p mp) {

	return (msg_send_chip_hdr(mp, MP_CMD_END_TRANSACTION, 4, NULL));
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
int
minipro_get_chip_id(minipro_p mp, uint32_t *chip_id_type,
    uint32_t *chip_id, uint8_t *chip_id_size, uint32_t *chip_id_rev) {
	int error;
	size_t rcvd;
	uint32_t chip_id_tm;
	chip_id_map_p id_map;

	if (NULL == mp || NULL == mp->chip || NULL == chip_id_type ||
	    NULL == chip_id || NULL == chip_id_size)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_RET_ON_ERR_CLEANUP(msg_send_chip_hdr(mp, MP_CMD_GET_CHIP_ID,
	    8, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg, sizeof(mp->msg),
	    &rcvd));
	if (2 > rcvd) {
		error = EMSGSIZE;
		goto err_out;
	}
	
	chip_id_tm = U8TO32n_BIG(&mp->msg[2], (mp->msg[1] & 0x03));
	switch (mp->msg[0]) {
	case MP_CHIP_ID_TYPE1:
	case MP_CHIP_ID_TYPE2:
	case MP_CHIP_ID_TYPE5:
		(*chip_id) = chip_id_tm;
		(*chip_id_rev) = 0;
		break;
	case MP_CHIP_ID_TYPE3:
		(*chip_id) = (chip_id_tm >> 5);
		(*chip_id_rev) = (chip_id_tm & 0x0000001f);
		break;
	case MP_CHIP_ID_TYPE4:
		id_map = chip_id_map(mp->chip->opts3);
		if (NULL == id_map) {
			error = EINVAL;
			goto err_out;
		}
		(*chip_id) = (chip_id_tm >> id_map->shift);
		(*chip_id_rev) = (chip_id_tm &
		    ((0x00000001 << id_map->shift) - 1));
		break;
	default:
		error = EINVAL;
		goto err_out;
	}

	(*chip_id_type) = mp->msg[0];
	(*chip_id_size) = (mp->msg[1] & 0x03);

err_out:
	minipro_end_transaction(mp); /* Call after msg processed. */
	return (error);
}

int
minipro_erase(minipro_p mp) {
	int error;
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip)
		return (EINVAL);
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	msg_chip_hdr_set(mp, MP_CMD_ERASE, 15);
	mp->msg[2] = mp->chip->write_unlock;
	MP_RET_ON_ERR_CLEANUP(msg_send(mp, mp->msg, 15, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg, sizeof(mp->msg),
	    &rcvd)); /* rcvd == 10 */
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

err_out:
	minipro_end_transaction(mp); /* Let MP_CMD_ERASE to take an effect. */
	return (error);
}

int
minipro_protect_set(minipro_p mp, int val) {
	int error;

	if (NULL == mp || NULL == mp->chip)
		return (EINVAL);
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_RET_ON_ERR_CLEANUP(msg_send_chip_hdr(mp,
	    ((0 != val) ? MP_CMD_PROTECT_ON : MP_CMD_PROTECT_OFF), 10,
	    NULL));
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_unlock_tsop48(minipro_p mp, uint8_t *type) {
	uint16_t crc = 0;
	size_t i, rcvd;
	struct timespec st;

	if (NULL == mp || NULL == mp->chip || NULL == type)
		return (EINVAL);
	MP_RET_ON_ERR(clock_gettime(CLOCK_MONOTONIC, &st));
	msg_chip_hdr_set(mp, MP_CMD_UNLOCK_TSOP48, 17);
	/* Set "random" data. */
	U32TO8_LITTLE((uint32_t)st.tv_nsec, &mp->msg[7]);
	U32TO8_LITTLE((uint32_t)st.tv_sec, &mp->msg[11]);
	/* Calc CRC16. */
	for (i = 7; i < 15; i ++) {
		crc  = (crc >> 8) | (uint16_t)(crc << 8);
		crc ^= mp->msg[i];
		crc ^= (crc & 0xff) >> 4;
		crc ^= (crc << 12);
		crc ^= ((crc & 0xff) << 5);
	}
	/* More data pack. */
	mp->msg[15] = mp->msg[9];
	mp->msg[16] = mp->msg[11];
	mp->msg[9] = (uint8_t)crc;
	mp->msg[11] = (uint8_t)(crc >> 8);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 17, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd)); /* rcvd == 17 */
	if (2 > rcvd)
		return (EMSGSIZE);
	(*type) = mp->msg[1];

	return (0);
}

int
minipro_get_status(minipro_p mp, minipro_status_p status) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip || NULL == status)
		return (EINVAL);

	MP_RET_ON_ERR(msg_send_chip_hdr(mp, MP_CMD_REQ_STATUS, 5, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd)); /* rcvd == 32 */
	if (10 > rcvd)
		return (EMSGSIZE);
	status->error = U8TO16_LITTLE(&mp->msg[0]);
	status->c1 = U8TO16_LITTLE(&mp->msg[2]);
	status->c2 = U8TO16_LITTLE(&mp->msg[4]);
	status->address = U8TO32n_LITTLE(&mp->msg[6], 3);
	status->ovp = mp->msg[9]; /* Overcurrency protection. */

	return (0);
}

int
minipro_overcurrency_chk(minipro_p mp) {
	minipro_status_t status;

	if (NULL == mp || NULL == mp->chip)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_get_status(mp, &status));
	if (0 != status.ovp) {
		MP_LOG_ERR(-1, "Overcurrency protection.");
		return (-1);
	}

	return (0);
}

int
minipro_read_block(minipro_p mp, uint8_t cmd, uint32_t addr,
    uint8_t *buf, size_t buf_size) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip ||
	    (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, 18);
	U16TO8_LITTLE((uint16_t)buf_size, &mp->msg[2]);
	/* Translating address to protocol-specific. */
	if (0 != (CHIP_OPT4_ADDR_SCALE & mp->chip->opts4)) {
		addr = (addr >> 1);
	}
	U24TO8_LITTLE(addr, &mp->msg[4]);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(mp, buf, buf_size, &rcvd));
	if (rcvd != buf_size)
		return (EMSGSIZE);
	/* Overcurrency status check. */
	MP_RET_ON_ERR(minipro_overcurrency_chk(mp));

	return (0);
}

int
minipro_write_block(minipro_p mp, uint8_t cmd, uint32_t addr,
    const uint8_t *buf, size_t buf_size) {
	minipro_status_t status;

	if (NULL == mp || NULL == mp->chip ||
	    (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, 7);
	U16TO8_LITTLE((uint16_t)buf_size, &mp->msg[2]);
	if (0 != (CHIP_OPT4_ADDR_SCALE & mp->chip->opts4)) {
		addr = (addr >> 1);
	}
	U24TO8_LITTLE(addr, &mp->msg[4]);
	memcpy(&mp->msg[7], buf, buf_size);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, (7 + buf_size), NULL));

	/* Status check. */
	MP_RET_ON_ERR(minipro_get_status(mp, &status));
	if (0 != status.ovp) {
		MP_LOG_ERR(-1, "Overcurrency protection.");
		return (-1);
	}
	if (0 != status.error) {
		MP_LOG_ERR_FMT(-1,
		    "Verification failed at address: 0x%04x, "
		    "written = 0x%02x, readed = 0x%02x.",
		    status.address, status.c2, status.c1);
		return (-1);
	}

	return (0);
}

int
minipro_read_fuses(minipro_p mp, uint8_t cmd,
    uint8_t *buf, size_t buf_size) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip ||
	    (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, 18);
	/* Note that PICs with 1 config word will show buf_size == 2. (for pic2_fuses) */
	mp->msg[2] = ((MP_CMD_READ_CFG == cmd && 4 == buf_size) ?
	    0x02 : 0x01);
	mp->msg[5] = 0x10;
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd)); /* rcvd == (7 + buf_size) */
	if ((7 + buf_size) > rcvd)
		return (EMSGSIZE);
	memcpy(buf, &mp->msg[7], buf_size);
	/* Overcurrency status check. */
	MP_RET_ON_ERR(minipro_overcurrency_chk(mp));

	return (0);
}

int
minipro_write_fuses(minipro_p mp, uint8_t cmd,
    const uint8_t *buf, size_t buf_size) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip ||
	    (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	/* Perform actual writing. */
	switch ((0xf0 & cmd)) {
	case 0x10:
		msg_chip_hdr_set(mp, (cmd + 1), 64);
		mp->msg[2] = ((4 == buf_size) ? 0x02 : 0x01); /* 2 fuse PICs have len = 8 */
		mp->msg[4] = 0xc8;
		mp->msg[5] = 0x0f;
		mp->msg[6] = 0x00; /* Do not optimize this. */
		memcpy(&mp->msg[7], buf, buf_size);
		MP_RET_ON_ERR(msg_send(mp, mp->msg, 64, NULL));
		break;
	case 0x40:
		msg_chip_hdr_set(mp, (cmd - 1), 10);
		memcpy(&mp->msg[7], buf, buf_size);
		MP_RET_ON_ERR(msg_send(mp, mp->msg, 10, NULL));
		break;
	}

	/* The device waits us to get the status now. */
	msg_chip_hdr_set(mp, cmd, 18);
	/* Note that PICs with 1 config word will show buf_size == 2. (for pic2_fuses) */
	mp->msg[2] = ((MP_CMD_READ_CFG == cmd && 4 == buf_size) ?
	    0x02 : 0x01);
	memcpy(&mp->msg[7], buf, buf_size);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd));
	if ((7 + buf_size) > rcvd)
		return (EMSGSIZE);
	if (memcmp(buf, &mp->msg[7], buf_size))
		return (-1);
	/* Overcurrency status check. */
	MP_RET_ON_ERR(minipro_overcurrency_chk(mp));

	return (0);
}


int
minipro_read_buf(minipro_p mp, uint8_t cmd,
    uint32_t addr, uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	uint32_t blk_size, offset;
	size_t to_read = buf_size, tm;

	if (NULL == mp || NULL == mp->chip || NULL == buf ||
	    0 == buf_size)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

	blk_size = mp->chip->read_block_size;
	offset = (addr % blk_size); /* Offset from first block start. */
	/* Need to read part from first block / pre alligment. */
	if (0 != offset) {
		addr -= offset; /* Allign addr to block size. */
		MP_PROGRESS_UPDATE(cb, mp, 0, buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		tm = MIN((blk_size - offset), to_read); /* Data size to store in buf. */
		memcpy(buf, (mp->read_block_buf + offset), tm);
		addr += tm;
		buf += tm;
		to_read -= tm;
	}

	/* Read alligned blocks. */
	while (blk_size <= to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read),
		    buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    buf, blk_size));
		addr += blk_size;
		buf += blk_size;
		to_read -= blk_size;
	}

	/* Last block part / post alligment. */
	if (0 != to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read),
		    buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		memcpy(buf, mp->read_block_buf, to_read);
	}

	MP_PROGRESS_UPDATE(cb, mp, buf_size, buf_size, udata);

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_verify_buf(minipro_p mp, uint8_t cmd,
    uint32_t addr, const uint8_t *buf, size_t buf_size,
    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	uint32_t blk_size, offset;
	size_t to_read = buf_size, tm, diff_off;

	if (NULL == mp || NULL == mp->chip ||
	    NULL == buf || 0 == buf_size ||
	    NULL == err_offset || NULL == buf_val || NULL == chip_val)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

	blk_size = mp->chip->read_block_size;
	offset = (addr % blk_size); /* Offset from first block start. */
	/* Need to read part from first block / pre alligment. */
	if (0 != offset) {
		addr -= offset; /* Allign addr to block size. */
		MP_PROGRESS_UPDATE(cb, mp, 0, buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		tm = MIN((blk_size - offset), to_read); /* Data size to store in buf. */
		diff_off = memcmp_idx(buf,
		    (mp->read_block_buf + offset), tm);
		if (diff_off != tm) {
			/* movemem() for get (*chip_val) at diff pos offset. */
			memmove(mp->read_block_buf,
			    (mp->read_block_buf + offset), tm);
			goto diff_out;
		}
		addr += tm;
		buf += tm;
		to_read -= tm;
	}

	/* Read alligned blocks. */
	while (blk_size <= to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read),
		    buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		diff_off = memcmp_idx(buf, mp->read_block_buf, blk_size);
		if (diff_off != blk_size)
			goto diff_out;
		addr += blk_size;
		buf += blk_size;
		to_read -= blk_size;
	}

	/* Last block part / post alligment. */
	if (0 != to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read),
		    buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		diff_off = memcmp_idx(buf, mp->read_block_buf, to_read);
		if (diff_off != to_read)
			goto diff_out;
	}

	MP_PROGRESS_UPDATE(cb, mp, buf_size, buf_size, udata);
	MP_RET_ON_ERR(minipro_end_transaction(mp));
	
	(*err_offset) = buf_size;

	return (0);

diff_out:
	(*err_offset) = (diff_off + (buf_size - to_read));
	(*chip_val) = mp->read_block_buf[diff_off];
	(*buf_val) = buf[diff_off];
err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_write_buf(minipro_p mp, uint8_t cmd,
    uint32_t addr, const uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	uint8_t read_cmd;
	uint32_t blk_size, offset;
	size_t to_write = buf_size, tm;

	if (NULL == mp || NULL == mp->chip ||
	    NULL == buf || 0 == buf_size)
		return (EINVAL);

	/* Read block cmd. */
	switch (cmd) {
	case MP_CMD_WRITE_CODE:
		read_cmd = MP_CMD_READ_CODE;
		break;
	case MP_CMD_WRITE_DATA:
		read_cmd = MP_CMD_READ_DATA;
		break;
	default:
		return (EINVAL);
	}

	blk_size = mp->chip->write_block_size;
	offset = (addr % blk_size); /* Offset from first block start. */
	/* Need to write part from first block / pre alligment. */
	if (0 != offset) {
		addr -= offset; /* Allign addr to block size. */
		MP_PROGRESS_UPDATE(cb, mp, 0, buf_size, udata);
		/* Read head of first block. */
		/* read_block_size may not match write_block_size,
		 * use minipro_read_buf() to handle this case. */
		MP_RET_ON_ERR(minipro_read_buf(mp, read_cmd,
		    addr, mp->write_block_buf, offset, NULL, NULL));

		MP_RET_ON_ERR(minipro_begin_transaction(mp));

		/* Update block. */
		tm = MIN((blk_size - offset), to_write); /* Data size to store in buf. */
		memcpy((mp->write_block_buf + offset), buf, tm);
		/* Write updated block. */
		MP_RET_ON_ERR_CLEANUP(minipro_write_block(mp, cmd, addr,
		    mp->write_block_buf, blk_size));
		addr += tm;
		buf += tm;
		to_write -= tm;
	} else {
		MP_RET_ON_ERR(minipro_begin_transaction(mp));
		/* Overcurrency status check. */
		MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));
	}

	/* Write alligned blocks. */
	while (blk_size <= to_write) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_write),
		    buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_write_block(mp, cmd, addr,
		    buf, blk_size));
		addr += blk_size;
		buf += blk_size;
		to_write -= blk_size;
	}

	/* Last block part / post alligment. */
	if (0 != to_write) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_write),
		    buf_size, udata);
		/* Read tail of last block. */
		MP_RET_ON_ERR(minipro_end_transaction(mp));
		MP_RET_ON_ERR(minipro_read_buf(mp, read_cmd,
		    (addr + (uint32_t)to_write),
		    (mp->write_block_buf + to_write),
		    (blk_size - to_write), NULL, NULL));
		/* Set data and write. */
		memcpy(mp->write_block_buf, buf, to_write);
		MP_RET_ON_ERR(minipro_begin_transaction(mp));
		MP_RET_ON_ERR_CLEANUP(minipro_write_block(mp, cmd, addr,
		    mp->write_block_buf, blk_size));
	}

	MP_PROGRESS_UPDATE(cb, mp, buf_size, buf_size, udata);

err_out:
	minipro_end_transaction(mp);
	return (error);
}


int
minipro_fuses_read(minipro_p mp,
    uint8_t *buf, size_t buf_size, size_t *buf_used,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	size_t i, j, size, count, used = 0;
	fuse_decl_p fuses;
	uint8_t cmd, tmbuf[512];
	uint32_t val;

	if (NULL == mp || NULL == mp->chip || NULL == mp->chip->fuses ||
	    NULL == buf || 0 == buf_size || NULL == buf_used)
		return (EINVAL);

	fuses = mp->chip->fuses;
	count = fuses[0].size;
	cmd = fuses[1].cmd;
	size = fuses[1].size;

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

	MP_PROGRESS_UPDATE(cb, mp, 0, count, udata);

	for (i = 2; i < count; i ++) {
		size += fuses[i].size;
		MP_PROGRESS_UPDATE(cb, mp, i, count, udata);
		if (fuses[i].cmd < cmd) {
			error = EINVAL;
			MP_LOG_ERR_FMT(error,
			    "fuse_decls are not sorted: item %zu = "
			    "0x%02x is less then 0x%02x.",
			    i, fuses[i].cmd, cmd);
			goto err_out;
		}
		/* Skip already processed items. */
		if ((i + 1) < count &&
		    cmd >= fuses[(i + 1)].cmd)
			continue;
		MP_RET_ON_ERR_CLEANUP(minipro_read_fuses(mp, cmd, tmbuf,
		    size));
		/* Unpacking readed tmbuf to fuse_decls with same cmd. */
		for (j = 1; j < count; j ++) {
			if (cmd != fuses[j].cmd)
				continue;
			val = U8TO32n_LITTLE(&tmbuf[fuses[j].offset],
			    fuses[j].size);
			used += (size_t)snprintf((char*)(buf + used),
			    (buf_size - used),
			    "%s = 0x%04x\n",
			    fuses[j].name, val);
		}
		cmd = fuses[(i + 1)].cmd;
		size = 0;
	}

	MP_PROGRESS_UPDATE(cb, mp, count, count, udata);

	(*buf_used) = used;

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_fuses_verify(minipro_p mp, const uint8_t *buf, size_t buf_size,
    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	size_t i, j, size, count;
	fuse_decl_p fuses;
	uint8_t cmd, tmbuf[512];
	uint32_t val, valb;

	if (NULL == mp || NULL == mp->chip || NULL == mp->chip->fuses ||
	    NULL == buf || 0 == buf_size ||
	    NULL == err_offset || NULL == buf_val || NULL == chip_val)
		return (EINVAL);

	fuses = mp->chip->fuses;
	count = fuses[0].size;
	cmd = fuses[1].cmd;
	size = fuses[1].size;

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

	MP_PROGRESS_UPDATE(cb, mp, 0, count, udata);

	for (i = 2; i < count; i ++) {
		size += fuses[i].size;
		MP_PROGRESS_UPDATE(cb, mp, i, count, udata);
		if (fuses[i].cmd < cmd) {
			error = EINVAL;
			MP_LOG_ERR_FMT(error,
			    "fuse_decls are not sorted: item %zu = "
			    "0x%02x is less then 0x%02x.",
			    i, fuses[i].cmd, cmd);
			goto err_out;
		}
		/* Skip already processed items. */
		if ((i + 1) < count &&
		    cmd >= fuses[(i + 1)].cmd)
			continue;
		MP_RET_ON_ERR_CLEANUP(minipro_read_fuses(mp, cmd, tmbuf,
		    size));
		/* Unpacking readed tmbuf to fuse_decls with same cmd. */
		for (j = 1; j < count; j ++) {
			if (cmd != fuses[j].cmd)
				continue;
			val = U8TO32n_LITTLE(&tmbuf[fuses[j].offset],
			    fuses[j].size);
			error = buf_get_named_line_val32(buf, buf_size,
			    fuses[j].name, strlen(fuses[j].name), &valb);
			if (0 != error) {
				MP_LOG_ERR_FMT(error,
				    "value for item %zu - \"%s\" "
				    "not found.",
				    j, fuses[j].name);
				goto err_out;
			}
			if (valb != val) {
				(*err_offset) = j;
				(*chip_val) = val;
				(*buf_val) = valb;
				goto err_out;
			}
		}
		cmd = fuses[(i + 1)].cmd;
		size = 0;
	}

	MP_PROGRESS_UPDATE(cb, mp, count, count, udata);

	(*err_offset) = buf_size;

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_fuses_write(minipro_p mp, const uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	size_t i, j, size, count;
	fuse_decl_p fuses;
	uint8_t cmd, tmbuf[512];
	uint32_t val;

	if (NULL == mp || NULL == mp->chip || NULL == mp->chip->fuses ||
	    NULL == buf || 0 == buf_size)
		return (EINVAL);

	fuses = mp->chip->fuses;
	count = fuses[0].size;
	cmd = fuses[1].cmd;
	size = fuses[1].size;

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	/* Overcurrency status check. */
	MP_RET_ON_ERR_CLEANUP(minipro_overcurrency_chk(mp));

	MP_PROGRESS_UPDATE(cb, mp, 0, count, udata);

	for (i = 2; i < count; i ++) {
		size += fuses[i].size;
		MP_PROGRESS_UPDATE(cb, mp, i, count, udata);
		if (fuses[i].cmd < cmd) {
			error = EINVAL;
			MP_LOG_ERR_FMT(error,
			    "fuse_decls are not sorted: item %zu "
			    "(\"%s\") = 0x%02x is less then 0x%02x.",
			    i, fuses[i].name, fuses[i].cmd, cmd);
			goto err_out;
		}
		/* Skip already processed items. */
		if ((i + 1) < count &&
		    cmd >= fuses[(i + 1)].cmd)
			continue;
		/* Packing fuse_decls to tmbuf. */
		memset(tmbuf, 0x00, size);
		for (j = 1; i < count; j ++) {
			if (cmd != fuses[j].cmd)
				continue;
			error = buf_get_named_line_val32(buf, buf_size,
			    fuses[j].name, strlen(fuses[j].name), &val);
			if (0 != error) {
				MP_LOG_ERR_FMT(error,
				    "value for item %zu - \"%s\" "
				    "not found.",
				    j, fuses[j].name);
				goto err_out;
			}
			U32TO8n_LITTLE(val, fuses[j].size,
			    &tmbuf[fuses[j].offset]);
		}
		MP_RET_ON_ERR_CLEANUP(minipro_write_fuses(mp, cmd,
		    tmbuf, size));
		cmd = fuses[(i + 1)].cmd;
		size = 0;
	}

	MP_PROGRESS_UPDATE(cb, mp, count, count, udata);

err_out:
	minipro_end_transaction(mp);
	return (error);
}


int
minipro_page_read(minipro_p mp, int page, uint32_t address, size_t size,
    uint8_t **buf, size_t *buf_size,
    minipro_progress_cb cb, void *udata) {
	int error;
	uint8_t *buffer = NULL;
	size_t chip_size;

	if (NULL == mp || NULL == mp->chip ||
	    NULL == buf || NULL == buf_size)
		return (EINVAL);

	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == page) ?
		    mp->chip->code_memory_size :
		    mp->chip->data_memory_size);
		if (0 == chip_size ||
		    ((size_t)address + size) > chip_size)
			return (EINVAL); /* No page or out of range. */
		buffer = malloc(size + sizeof(void*));
		if (NULL == buffer)
			return (ENOMEM);
		error = minipro_read_buf(mp,
		    mp_chip_page_read_cmd[page],
		    address, buffer, size, cb, udata);
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (NULL == mp->chip->fuses)
			return (EINVAL); /* No page. */
		buffer = malloc(MP_FUSES_BUF_SIZE_MAX);
		if (NULL == buffer)
			return (ENOMEM);
		error = minipro_fuses_read(mp, buffer,
		    MP_FUSES_BUF_SIZE_MAX, &size, cb, udata);
		break;
	default:
		return (EINVAL);
	}

	if (0 != error) { /* Fail, cleanup. */
		free(buffer);
		return (error);
	}
	/* OK. */
	(*buf) = buffer;
	(*buf_size) = size;

	return (0);
}

int
minipro_page_verify(minipro_p mp, int page, uint32_t address,
    const uint8_t *buf, size_t buf_size,
    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
    minipro_progress_cb cb, void *udata) {
	int error;
	size_t chip_size;

	if (NULL == mp || NULL == mp->chip ||
	    NULL == buf || 0 == buf_size ||
	    NULL == err_offset || NULL == buf_val || NULL == chip_val)
		return (EINVAL);

	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == page) ?
		    mp->chip->code_memory_size :
		    mp->chip->data_memory_size);
		if (0 == chip_size ||
		    ((size_t)address + buf_size) > chip_size)
			return (EINVAL); /* No page or out of range. */
		error = minipro_verify_buf(mp,
		    mp_chip_page_read_cmd[page],
		    address, buf, buf_size,
		    err_offset, buf_val, chip_val, cb, udata);
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (NULL == mp->chip->fuses)
			return (EINVAL); /* No page. */
		error = minipro_fuses_verify(mp, buf, buf_size,
		    err_offset, buf_val, chip_val, cb, udata);
		break;
	default:
		return (EINVAL);
	}

	return (error);
}

int
minipro_page_write(minipro_p mp, uint32_t flags,
    int page, uint32_t address,
    const uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error;
	size_t chip_size;

	if (NULL == mp || NULL == mp->chip ||
	    NULL == buf || 0 == buf_size)
		return (EINVAL);

	/* Pre checks. */
	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == page) ?
		    mp->chip->code_memory_size :
		    mp->chip->data_memory_size);
		if (0 == chip_size ||
		    ((size_t)address + buf_size) > chip_size)
			return (EINVAL); /* No page or out of range. */
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (NULL == mp->chip->fuses)
			return (EINVAL); /* No page. */
		break;
	default:
		return (EINVAL);
	}

	/* Overcurrency status check. */
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	error = minipro_overcurrency_chk(mp);
	MP_RET_ON_ERR(minipro_end_transaction(mp));
	MP_RET_ON_ERR(error);

	/* Erase before writing. */
	if (0 == (MP_PAGE_WR_F_NO_ERASE & flags) &&
	    0 != (CHIP_OPT4_ERASE & mp->chip->opts4)) {
		MP_RET_ON_ERR(minipro_erase(mp));
	}

	/* Turn off protection before writing. */
	if (0 != (CHIP_OPT4_PROTECTION & mp->chip->opts4) &&
	    0 == (MP_PAGE_WR_F_PRE_NO_UNPROTECT & flags)) {
		minipro_protect_set(mp, 0);
	}

	/* Write. */
	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		error = minipro_write_buf(mp,
		    mp_chip_page_write_cmd[page],
		    address, buf, buf_size, cb, udata);
		break;
	case MP_CHIP_PAGE_CONFIG:
		error = minipro_fuses_write(mp, buf, buf_size,
		    cb, udata);
		break;
	}

	/* Turn on protection after writing. */
	if (0 != (CHIP_OPT4_PROTECTION & mp->chip->opts4) &&
	    0 == (MP_PAGE_WR_F_POST_NO_PROTECT & flags)) {
		minipro_protect_set(mp, 1);
	}

	return (error);
}
