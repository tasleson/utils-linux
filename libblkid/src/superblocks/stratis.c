/*
 * Copyright (C) 2018 Tony Asleson <tasleson@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/*
 * Specification for on disk format
 * https://stratis-storage.github.io/StratisSoftwareDesign.pdf
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "superblocks.h"
#include "crc32c.h"

struct stratis_super_block {
	uint32_t crc32;
	uint8_t magic[16];
	uint64_t sectors;
	uint8_t reserved[4];
	uint8_t pool_uuid[32];
	uint8_t dev_uuid[32];
	uint64_t mda_size;
	uint64_t reserved_size;
	uint64_t flags;
	uint64_t initialization_time;
} __attribute__ ((__packed__));

#define BS 512
#define FIRST_COPY_OFFSET BS
#define SECOND_COPY_OFFSET (BS * 9)
#define SB_AREA_SIZE (BS * 16)

const char STRATIS_MAGIC[] = "!Stra0tis\x86\xff\x02^\x41rh";
#define MAGIC_LEN (sizeof(STRATIS_MAGIC) - 1)

#define MAGIC_OFFSET (offsetof(struct stratis_super_block, magic))
#define MAGIC_OFFSET_COPY_1 (FIRST_COPY_OFFSET + MAGIC_OFFSET)
#define MAGIC_OFFSET_COPY_2 (SECOND_COPY_OFFSET + MAGIC_OFFSET)

static int stratis_valid_sb(blkid_probe pr, uint8_t *p)
{
	struct stratis_super_block *stratis = (struct stratis_super_block *)p;
	uint32_t crc = 0;

	// generate CRC from byte position 4 for length 508 == 512 byte sector
	crc = crc32c(~0L, p + sizeof(stratis->crc32), BS - sizeof(stratis->crc32));
	crc ^= ~0L;

	return blkid_probe_verify_csum(pr, crc, le32_to_cpu(stratis->crc32));
}

static int probe_stratis(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct stratis_super_block *stratis = NULL;
	uint8_t *buf = blkid_probe_get_buffer(pr, 0, SB_AREA_SIZE);

	if (!buf) {
		return errno ? -errno : 1;
	}

	if (stratis_valid_sb(pr, buf + FIRST_COPY_OFFSET)) {
		stratis = (struct stratis_super_block*)(buf + FIRST_COPY_OFFSET);
	} else {
		if (!stratis_valid_sb(pr, buf + SECOND_COPY_OFFSET)) {
			return 1;
		}
		stratis = (struct stratis_super_block*)(buf + SECOND_COPY_OFFSET);
	}

	blkid_probe_strncpy_uuid(pr, stratis->dev_uuid, sizeof(stratis->dev_uuid));
	blkid_probe_set_value(pr, "POOL_UUID", (unsigned char*)stratis->pool_uuid,
				sizeof(stratis->pool_uuid));

	blkid_probe_sprintf_value(pr, "SECTORS", "%" PRIu64, stratis->sectors);
	blkid_probe_sprintf_value(pr, "INITIALIZATION_TIME", "%" PRIu64,
					stratis->initialization_time);

	return 0;
}

const struct blkid_idinfo stratis_idinfo =
{
	.name		= "stratis",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_stratis,
	.minsz		= SB_AREA_SIZE,
	.magics		=
	{
		{ .magic = STRATIS_MAGIC, .len = MAGIC_LEN, .sboff = MAGIC_OFFSET_COPY_1},
		{ .magic = STRATIS_MAGIC, .len = MAGIC_LEN, .sboff = MAGIC_OFFSET_COPY_2},
		{ NULL }
	}
};
