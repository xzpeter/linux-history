#ifndef _MULTIPATH_H
#define _MULTIPATH_H

#include <linux/raid/md.h>
#include <linux/bio.h>

struct multipath_info {
	mdk_rdev_t	*rdev;

	/*
	 * State bits:
	 */
	int		operational;

	int		used_slot;
};

struct multipath_private_data {
	mddev_t			*mddev;
	struct multipath_info	multipaths[MD_SB_DISKS];
	int			raid_disks;
	int			working_disks;
	mdk_thread_t		*thread;
	spinlock_t		device_lock;

	mempool_t		*pool;
};

typedef struct multipath_private_data multipath_conf_t;

/*
 * this is the only point in the RAID code where we violate
 * C type safety. mddev->private is an 'opaque' pointer.
 */
#define mddev_to_conf(mddev) ((multipath_conf_t *) mddev->private)

/*
 * this is our 'private' 'collective' MULTIPATH buffer head.
 * it contains information about what kind of IO operations were started
 * for this MULTIPATH operation, and about their status:
 */

struct multipath_bh {
	mddev_t			*mddev;
	struct bio		*master_bio;
	struct bio		bio;
	int			path;
	struct multipath_bh	*next_mp; /* next for retry */
};
#endif
