/*
 * Scatterlist Cryptographic API.
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2002 David S. Miller (davem@redhat.com)
 *
 * Portions derived from Cryptoapi, by Alexander Kjeldaas <astor@fast.no>
 * and Nettle, by Niels M�ller.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/crypto.h>
#include "internal.h"

static LIST_HEAD(crypto_alg_list);
static struct rw_semaphore crypto_alg_sem;

static inline int crypto_alg_get(struct crypto_alg *alg)
{
	return try_inc_mod_count(alg->cra_module);
}

static inline void crypto_alg_put(struct crypto_alg *alg)
{
	if (alg->cra_module)
		__MOD_DEC_USE_COUNT(alg->cra_module);
}

struct crypto_alg *crypto_alg_lookup(char *name)
{
	struct list_head *p;
	struct crypto_alg *alg = NULL;
	
	down_read(&crypto_alg_sem);
	
	list_for_each(p, &crypto_alg_list) {
		struct crypto_alg *q =
			list_entry(p, struct crypto_alg, cra_list);

		if (!(strcmp(q->cra_name, name))) {

			if (crypto_alg_get(q))
				alg = q;
			break;
		}
	}

	up_read(&crypto_alg_sem);
	return alg;
}

static int crypto_init_flags(struct crypto_tfm *tfm, u32 flags)
{
	tfm->crt_flags = 0;
	
	if (flags & CRYPTO_TFM_REQ_ATOMIC)
		tfm->crt_flags |= CRYPTO_TFM_REQ_ATOMIC;
	
	switch (crypto_tfm_alg_type(tfm)) {
	case CRYPTO_ALG_TYPE_CIPHER:
		return crypto_init_cipher_flags(tfm, flags);
		
	case CRYPTO_ALG_TYPE_DIGEST:
		return crypto_init_digest_flags(tfm, flags);
		
	case CRYPTO_ALG_TYPE_COMP:
		return crypto_init_compress_flags(tfm, flags);
	
	default:
		BUG();
		
	}
	
	return -EINVAL;
}

static void crypto_init_ops(struct crypto_tfm *tfm)
{
	switch (crypto_tfm_alg_type(tfm)) {
	case CRYPTO_ALG_TYPE_CIPHER:
		crypto_init_cipher_ops(tfm);
		break;
		
	case CRYPTO_ALG_TYPE_DIGEST:
		crypto_init_digest_ops(tfm);
		break;
		
	case CRYPTO_ALG_TYPE_COMP:
		crypto_init_compress_ops(tfm);
		break;
	
	default:
		BUG();
		
	}
}

struct crypto_tfm *crypto_alloc_tfm(char *name, u32 flags)
{
	struct crypto_tfm *tfm = NULL;
	struct crypto_alg *alg;

	alg = crypto_alg_lookup(name);
#ifdef CONFIG_KMOD
	if (alg == NULL) {
		crypto_alg_autoload(name);
		alg = crypto_alg_lookup(name);
	}
#endif
	if (alg == NULL)
		goto out;
	
	tfm = kmalloc(sizeof(*tfm), GFP_KERNEL);
	if (tfm == NULL)
		goto out_put;

	if (alg->cra_ctxsize) {
		tfm->crt_ctx = kmalloc(alg->cra_ctxsize, GFP_KERNEL);
		if (tfm->crt_ctx == NULL)
			goto out_free_tfm;
	}

	tfm->__crt_alg = alg;
	
	if (crypto_init_flags(tfm, flags))
		goto out_free_ctx;
		
	crypto_init_ops(tfm);
	
	goto out;

out_free_ctx:
	if (tfm->__crt_alg->cra_ctxsize)
		kfree(tfm->crt_ctx);
out_free_tfm:
	kfree(tfm);
	tfm = NULL;
out_put:
	crypto_alg_put(alg);
out:
	return tfm;
}

void crypto_free_tfm(struct crypto_tfm *tfm)
{
	if (tfm->__crt_alg->cra_ctxsize)
		kfree(tfm->crt_ctx);
		
	if (crypto_tfm_alg_type(tfm) == CRYPTO_ALG_TYPE_CIPHER)
		if (tfm->crt_cipher.cit_iv)
			kfree(tfm->crt_cipher.cit_iv);
	
	crypto_alg_put(tfm->__crt_alg);
	kfree(tfm);
}

int crypto_register_alg(struct crypto_alg *alg)
{
	int ret = 0;
	struct list_head *p;
	
	down_write(&crypto_alg_sem);
	
	list_for_each(p, &crypto_alg_list) {
		struct crypto_alg *q =
			list_entry(p, struct crypto_alg, cra_list);
		
		if (!(strcmp(q->cra_name, alg->cra_name))) {
			ret = -EEXIST;
			goto out;
		}
	}
	list_add_tail(&alg->cra_list, &crypto_alg_list);
out:	
	up_write(&crypto_alg_sem);
	return ret;
}

int crypto_unregister_alg(struct crypto_alg *alg)
{
	int ret = -ENOENT;
	struct list_head *p;
	
	BUG_ON(!alg->cra_module);
	
	down_write(&crypto_alg_sem);
	list_for_each(p, &crypto_alg_list) {
		if (alg == (void *)p) {
			list_del(p);
			ret = 0;
			goto out;
		}
	}
out:	
	up_write(&crypto_alg_sem);
	return ret;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	struct list_head *v;
	loff_t n = *pos;

	down_read(&crypto_alg_sem);
	list_for_each(v, &crypto_alg_list)
		if (!n--)
			return list_entry(v, struct crypto_alg, cra_list);
	return NULL;
}

static void *c_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct list_head *v = p;
	
	(*pos)++;
	v = v->next;
	return (v == &crypto_alg_list) ?
		NULL : list_entry(v, struct crypto_alg, cra_list);
}

static void c_stop(struct seq_file *m, void *p)
{
	up_read(&crypto_alg_sem);
}

static int c_show(struct seq_file *m, void *p)
{
	struct crypto_alg *alg = (struct crypto_alg *)p;
	
	seq_printf(m, "name       : %s\n", alg->cra_name);
	seq_printf(m, "module     : %s\n", alg->cra_module ?
					alg->cra_module->name : "[static]");
	seq_printf(m, "blocksize  : %Zd\n", alg->cra_blocksize);
	
	switch (alg->cra_flags & CRYPTO_ALG_TYPE_MASK) {
	case CRYPTO_ALG_TYPE_CIPHER:
		seq_printf(m, "keysize    : %Zd\n", alg->cra_cipher.cia_keysize);
		seq_printf(m, "ivsize     : %Zd\n", alg->cra_cipher.cia_ivsize);
		break;
		
	case CRYPTO_ALG_TYPE_DIGEST:
		seq_printf(m, "digestsize : %Zd\n",
		           alg->cra_digest.dia_digestsize);
		break;
	}

	seq_putc(m, '\n');
	return 0;
}

static struct seq_operations crypto_seq_ops = {
	.start		= c_start,
	.next		= c_next,
	.stop		= c_stop,
	.show		= c_show
};

static int crypto_info_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &crypto_seq_ops);
}
        
struct file_operations proc_crypto_ops = {
	.open		= crypto_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release
};

static int __init init_crypto(void)
{
	struct proc_dir_entry *proc;
	
	printk(KERN_INFO "Initializing Cryptographic API\n");
	init_rwsem(&crypto_alg_sem);
	proc = create_proc_entry("crypto", 0, NULL);
	if (proc)
		proc->proc_fops = &proc_crypto_ops;
		
	return 0;
}

__initcall(init_crypto);

EXPORT_SYMBOL_GPL(crypto_register_alg);
EXPORT_SYMBOL_GPL(crypto_unregister_alg);
EXPORT_SYMBOL_GPL(crypto_alloc_tfm);
EXPORT_SYMBOL_GPL(crypto_free_tfm);
