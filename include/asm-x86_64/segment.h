#ifndef _ASM_SEGMENT_H
#define _ASM_SEGMENT_H

#define __KERNEL_CS	0x10
#define __KERNEL_DS	0x18

#define __KERNEL32_CS   0x38

#define __USER_LONGBASE	((GDT_ENTRY_LONGBASE * 8)  | 3)

/* 
 * we cannot use the same code segment descriptor for user and kernel
 * even not in the long flat model, because of different DPL /kkeil 
 * The segment offset needs to contain a RPL. Grr. -AK
 * GDT layout to get 64bit syscall right (sysret hardcodes gdt offsets) 
 */

#define __USER32_CS   0x23   /* 4*8+3 */ 
#define __USER_DS     0x2b   /* 5*8+3 */ 
#define __USER_CS     0x33   /* 6*8+3 */ 
#define __USER32_DS	__USER_DS 

#define GDT_ENTRY_TLS 1
#define GDT_ENTRY_TSS 8	/* needs two entries */
#define GDT_ENTRY_LDT 10
#define GDT_ENTRY_TLS_MIN 11
#define GDT_ENTRY_TLS_MAX 13
#define GDT_ENTRY_LONGBASE 14

#define GDT_ENTRY_TLS_ENTRIES 3

#define IDT_ENTRIES 256
#define GDT_ENTRIES 16
#define GDT_SIZE (GDT_ENTRIES * 8)
#define TLS_SIZE (GDT_ENTRY_TLS_ENTRIES * 8) 

#endif
