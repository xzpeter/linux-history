/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2002 Silicon Graphics, Inc. All rights reserved.
 */


#ifndef _ASM_IA64_SN_SGI_H
#define _ASM_IA64_SN_SGI_H

#include <linux/config.h>

#include <asm/sn/types.h>
#include <asm/uaccess.h>		/* for copy_??_user */
#include <linux/mm.h>
#include <linux/devfs_fs_kernel.h>

typedef int64_t  __psint_t;	/* needed by klgraph.c */

typedef enum { B_FALSE, B_TRUE } boolean_t;

#define ctob(x)			((uint64_t)(x)*NBPC)
#define btoc(x)			(((uint64_t)(x)+(NBPC-1))/NBPC)


/*
** Possible return values from graph routines.
*/
typedef enum graph_error_e {
	GRAPH_SUCCESS,		/* 0 */
	GRAPH_DUP,		/* 1 */
	GRAPH_NOT_FOUND,	/* 2 */
	GRAPH_BAD_PARAM,	/* 3 */
	GRAPH_HIT_LIMIT,	/* 4 */
	GRAPH_CANNOT_ALLOC,	/* 5 */
	GRAPH_ILLEGAL_REQUEST,	/* 6 */
	GRAPH_IN_USE		/* 7 */
} graph_error_t;

#define KM_SLEEP   0x0000
#define KM_NOSLEEP 0x0001		/* needed by kmem_alloc_node(), kmem_zalloc()
					 * calls */
#define VM_NOSLEEP 0x0001		/* needed kmem_alloc_node(), kmem_zalloc_node
					 * calls */
#define XG_WIDGET_PART_NUM      0xC102          /* KONA/xt_regs.h     XG_XT_PART_NUM_VALUE */

typedef uint64_t vhandl_t;


#ifndef NBPP
#define NBPP 4096
#endif

#ifndef D_MP
#define D_MP 1
#endif

#ifndef MAXDEVNAME
#define MAXDEVNAME 256
#endif

#ifndef NBPC
#define NBPC 0
#endif

#ifndef _PAGESZ
#define _PAGESZ 4096
#endif

typedef uint64_t mrlock_t;	/* needed by devsupport.c */

#define HUB_PIO_CONVEYOR 0x1
#define CNODEID_NONE ((cnodeid_t)-1)
#define XTALK_PCI_PART_NUM "030-1275-"
#define kdebug 0


#define COPYIN(a, b, c)		copy_from_user(b,a,c)
#define COPYOUT(a, b, c)	copy_to_user(b,a,c)

#define kvtophys(x)		(alenaddr_t) (x)
#define POFFMASK		(NBPP - 1)
#define poff(X)			((__psunsigned_t)(X) & POFFMASK)

#define BZERO(a,b)		memset(a, 0, b)

#define kern_malloc(x)		kmalloc(x, GFP_KERNEL)
#define kern_free(x)		kfree(x)

typedef cpuid_t cpu_cookie_t;
#define CPU_NONE		(-1)

/*
 * mutext support mapping
 */

#define mutex_spinlock_init(s)	spin_lock_init(s)
inline static unsigned long
mutex_spinlock(spinlock_t *sem) {
	unsigned long flags = 0;
//	spin_lock_irqsave(sem, flags);
	spin_lock(sem);
	return(flags);
}
// #define mutex_spinunlock(s,t)	spin_unlock_irqrestore(s,t)
#define mutex_spinunlock(s,t)	spin_unlock(s)


#define mutex_t			struct semaphore
#define mutex_init(s)		init_MUTEX(s)
#define mutex_init_locked(s)	init_MUTEX_LOCKED(s)
#define mutex_lock(s)		down(s)
#define mutex_unlock(s)		up(s)

#define io_splock(s)		mutex_spinlock(s)
#define io_spunlock(s,t)	spin_unlock(s)

#define spin_lock_destroy(s)

#if defined(DISABLE_ASSERT)
#define ASSERT(expr)
#define ASSERT_ALWAYS(expr)
#else
#define ASSERT(expr)  do {	\
        if(!(expr)) { \
		printk( "Assertion [%s] failed! %s:%s(line=%d)\n",\
			#expr,__FILE__,__FUNCTION__,__LINE__); \
		panic("Assertion panic\n"); 	\
        } } while(0)

#define ASSERT_ALWAYS(expr)	do {\
        if(!(expr)) { \
		printk( "Assertion [%s] failed! %s:%s(line=%d)\n",\
			#expr,__FILE__,__FUNCTION__,__LINE__); \
		panic("Assertion always panic\n"); 	\
        } } while(0)
#endif	/* DISABLE_ASSERT */

#define PRINT_PANIC		panic

#ifdef CONFIG_SMP
#define cpu_enabled(cpu)        (test_bit(cpu, &cpu_online_map))
#else
#define cpu_enabled(cpu)	(1)
#endif

/* print_register() defs */

/*
 * register values
 * map between numeric values and symbolic values
 */
struct reg_values {
	unsigned long long rv_value;
	char *rv_name;
};

/*
 * register descriptors are used for formatted prints of register values
 * rd_mask and rd_shift must be defined, other entries may be null
 */
struct reg_desc {
	unsigned long long rd_mask;	/* mask to extract field */
	int rd_shift;		/* shift for extracted value, - >>, + << */
	char *rd_name;		/* field name */
	char *rd_format;	/* format to print field */
	struct reg_values *rd_values;	/* symbolic names of values */
};

extern void print_register(unsigned long long, struct reg_desc *);

#include <asm/sn/hack.h>	/* for now */

#endif /* _ASM_IA64_SN_SGI_H */
