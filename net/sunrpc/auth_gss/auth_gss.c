/*
 * linux/net/sunrpc/auth_gss.c
 *
 * RPCSEC_GSS client authentication.
 * 
 *  Copyright (c) 2000 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Dug Song       <dugsong@monkey.org>
 *  Andy Adamson   <andros@umich.edu>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/sched.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/auth.h>
#include <linux/sunrpc/auth_gss.h>
#include <linux/sunrpc/svcauth_gss.h>
#include <linux/sunrpc/gss_err.h>
#include <linux/workqueue.h>
#include <linux/sunrpc/rpc_pipe_fs.h>
#include <linux/sunrpc/gss_api.h>
#include <asm/uaccess.h>

static struct rpc_authops authgss_ops;

static struct rpc_credops gss_credops;

#ifdef RPC_DEBUG
# define RPCDBG_FACILITY	RPCDBG_AUTH
#endif

#define NFS_NGROUPS	16

#define GSS_CRED_EXPIRE		(60 * HZ)	/* XXX: reasonable? */
#define GSS_CRED_SLACK		1024		/* XXX: unused */
/* length of a krb5 verifier (48), plus data added before arguments when
 * using integrity (two 4-byte integers): */
#define GSS_VERF_SLACK		56

/* XXX this define must match the gssd define
* as it is passed to gssd to signal the use of
* machine creds should be part of the shared rpc interface */

#define CA_RUN_AS_MACHINE  0x00000200 

/* dump the buffer in `emacs-hexl' style */
#define isprint(c)      ((c > 0x1f) && (c < 0x7f))

static DEFINE_RWLOCK(gss_ctx_lock);

struct gss_auth {
	struct rpc_auth rpc_auth;
	struct gss_api_mech *mech;
	enum rpc_gss_svc service;
	struct list_head upcalls;
	struct dentry *dentry;
	char path[48];
	spinlock_t lock;
};

static void gss_destroy_ctx(struct gss_cl_ctx *);
static struct rpc_pipe_ops gss_upcall_ops;

void
print_hexl(u32 *p, u_int length, u_int offset)
{
	u_int i, j, jm;
	u8 c, *cp;
	
	dprintk("RPC: print_hexl: length %d\n",length);
	dprintk("\n");
	cp = (u8 *) p;
	
	for (i = 0; i < length; i += 0x10) {
		dprintk("  %04x: ", (u_int)(i + offset));
		jm = length - i;
		jm = jm > 16 ? 16 : jm;
		
		for (j = 0; j < jm; j++) {
			if ((j % 2) == 1)
				dprintk("%02x ", (u_int)cp[i+j]);
			else
				dprintk("%02x", (u_int)cp[i+j]);
		}
		for (; j < 16; j++) {
			if ((j % 2) == 1)
				dprintk("   ");
			else
				dprintk("  ");
		}
		dprintk(" ");
		
		for (j = 0; j < jm; j++) {
			c = cp[i+j];
			c = isprint(c) ? c : '.';
			dprintk("%c", c);
		}
		dprintk("\n");
	}
}

EXPORT_SYMBOL(print_hexl);

static inline struct gss_cl_ctx *
gss_get_ctx(struct gss_cl_ctx *ctx)
{
	atomic_inc(&ctx->count);
	return ctx;
}

static inline void
gss_put_ctx(struct gss_cl_ctx *ctx)
{
	if (atomic_dec_and_test(&ctx->count))
		gss_destroy_ctx(ctx);
}

static void
gss_cred_set_ctx(struct rpc_cred *cred, struct gss_cl_ctx *ctx)
{
	struct gss_cred *gss_cred = container_of(cred, struct gss_cred, gc_base);
	struct gss_cl_ctx *old;
	write_lock(&gss_ctx_lock);
	old = gss_cred->gc_ctx;
	gss_cred->gc_ctx = ctx;
	cred->cr_flags |= RPCAUTH_CRED_UPTODATE;
	write_unlock(&gss_ctx_lock);
	if (old)
		gss_put_ctx(old);
}

static int
gss_cred_is_uptodate_ctx(struct rpc_cred *cred)
{
	struct gss_cred *gss_cred = container_of(cred, struct gss_cred, gc_base);
	int res = 0;

	read_lock(&gss_ctx_lock);
	if ((cred->cr_flags & RPCAUTH_CRED_UPTODATE) && gss_cred->gc_ctx)
		res = 1;
	read_unlock(&gss_ctx_lock);
	return res;
}

static const void *
simple_get_bytes(const void *p, const void *end, void *res, size_t len)
{
	const void *q = (const void *)((const char *)p + len);
	if (unlikely(q > end || q < p))
		return ERR_PTR(-EFAULT);
	memcpy(res, p, len);
	return q;
}

static inline const void *
simple_get_netobj(const void *p, const void *end, struct xdr_netobj *dest)
{
	const void *q;
	unsigned int len;

	p = simple_get_bytes(p, end, &len, sizeof(len));
	if (IS_ERR(p))
		return p;
	q = (const void *)((const char *)p + len);
	if (unlikely(q > end || q < p))
		return ERR_PTR(-EFAULT);
	dest->data = kmalloc(len, GFP_KERNEL);
	if (unlikely(dest->data == NULL))
		return ERR_PTR(-ENOMEM);
	dest->len = len;
	memcpy(dest->data, p, len);
	return q;
}

static struct gss_cl_ctx *
gss_cred_get_ctx(struct rpc_cred *cred)
{
	struct gss_cred *gss_cred = container_of(cred, struct gss_cred, gc_base);
	struct gss_cl_ctx *ctx = NULL;

	read_lock(&gss_ctx_lock);
	if (gss_cred->gc_ctx)
		ctx = gss_get_ctx(gss_cred->gc_ctx);
	read_unlock(&gss_ctx_lock);
	return ctx;
}

static struct gss_cl_ctx *
gss_alloc_context(void)
{
	struct gss_cl_ctx *ctx;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx != NULL) {
		memset(ctx, 0, sizeof(*ctx));
		ctx->gc_proc = RPC_GSS_PROC_DATA;
		ctx->gc_seq = 1;	/* NetApp 6.4R1 doesn't accept seq. no. 0 */
		spin_lock_init(&ctx->gc_seq_lock);
		atomic_set(&ctx->count,1);
	}
	return ctx;
}

static const void *
gss_fill_context(const void *p, const void *end, struct gss_cl_ctx *ctx, struct gss_api_mech *gm)
{
	const void *q;
	unsigned int seclen;
	unsigned int timeout;
	u32 window_size;
	int ret;

	/* First unsigned int gives the lifetime (in seconds) of the cred */
	p = simple_get_bytes(p, end, &timeout, sizeof(timeout));
	if (IS_ERR(p))
		goto err;
	/* Sequence number window. Determines the maximum number of simultaneous requests */
	p = simple_get_bytes(p, end, &window_size, sizeof(window_size));
	if (IS_ERR(p))
		goto err;
	ctx->gc_win = window_size;
	/* gssd signals an error by passing ctx->gc_win = 0: */
	if (ctx->gc_win == 0) {
		/* in which case, p points to  an error code which we ignore */
		p = ERR_PTR(-EACCES);
		goto err;
	}
	/* copy the opaque wire context */
	p = simple_get_netobj(p, end, &ctx->gc_wire_ctx);
	if (IS_ERR(p))
		goto err;
	/* import the opaque security context */
	p  = simple_get_bytes(p, end, &seclen, sizeof(seclen));
	if (IS_ERR(p))
		goto err;
	q = (const void *)((const char *)p + seclen);
	if (unlikely(q > end || q < p)) {
		p = ERR_PTR(-EFAULT);
		goto err;
	}
	ret = gss_import_sec_context(p, seclen, gm, &ctx->gc_gss_ctx);
	if (ret < 0) {
		p = ERR_PTR(ret);
		goto err;
	}
	return q;
err:
	dprintk("RPC:      gss_fill_context returning %ld\n", -PTR_ERR(p));
	return p;
}


struct gss_upcall_msg {
	struct rpc_pipe_msg msg;
	struct list_head list;
	struct gss_auth *auth;
	struct rpc_wait_queue waitq;
	uid_t	uid;
	atomic_t count;
};

static void
gss_release_msg(struct gss_upcall_msg *gss_msg)
{
	if (!atomic_dec_and_test(&gss_msg->count))
		return;
	BUG_ON(!list_empty(&gss_msg->list));
	kfree(gss_msg);
}

static struct gss_upcall_msg *
__gss_find_upcall(struct gss_auth *gss_auth, uid_t uid)
{
	struct gss_upcall_msg *pos;
	list_for_each_entry(pos, &gss_auth->upcalls, list) {
		if (pos->uid != uid)
			continue;
		atomic_inc(&pos->count);
		dprintk("RPC:      gss_find_upcall found msg %p\n", pos);
		return pos;
	}
	dprintk("RPC:      gss_find_upcall found nothing\n");
	return NULL;
}

static void
__gss_unhash_msg(struct gss_upcall_msg *gss_msg)
{
	if (list_empty(&gss_msg->list))
		return;
	list_del_init(&gss_msg->list);
	if (gss_msg->msg.errno < 0)
		rpc_wake_up_status(&gss_msg->waitq, gss_msg->msg.errno);
	else
		rpc_wake_up(&gss_msg->waitq);
	atomic_dec(&gss_msg->count);
}

static void
gss_unhash_msg(struct gss_upcall_msg *gss_msg)
{
	struct gss_auth *gss_auth = gss_msg->auth;

	spin_lock(&gss_auth->lock);
	__gss_unhash_msg(gss_msg);
	spin_unlock(&gss_auth->lock);
}

static int
gss_upcall(struct rpc_clnt *clnt, struct rpc_task *task, struct rpc_cred *cred)
{
	struct gss_auth *gss_auth = container_of(clnt->cl_auth,
			struct gss_auth, rpc_auth);
	struct gss_upcall_msg *gss_msg, *gss_new = NULL;
	struct rpc_pipe_msg *msg;
	struct dentry *dentry = gss_auth->dentry;
	uid_t uid = cred->cr_uid;
	int res = 0;

	dprintk("RPC: %4u gss_upcall for uid %u\n", task->tk_pid, uid);

retry:
	spin_lock(&gss_auth->lock);
	gss_msg = __gss_find_upcall(gss_auth, uid);
	if (gss_msg)
		goto out_sleep;
	if (gss_new == NULL) {
		spin_unlock(&gss_auth->lock);
		gss_new = kmalloc(sizeof(*gss_new), GFP_KERNEL);
		if (!gss_new) {
			dprintk("RPC: %4u gss_upcall -ENOMEM\n", task->tk_pid);
			return -ENOMEM;
		}
		goto retry;
	}
	gss_msg = gss_new;
	memset(gss_new, 0, sizeof(*gss_new));
	INIT_LIST_HEAD(&gss_new->list);
	rpc_init_wait_queue(&gss_new->waitq, "RPCSEC_GSS upcall waitq");
	atomic_set(&gss_new->count, 2);
	msg = &gss_new->msg;
	msg->data = &gss_new->uid;
	msg->len = sizeof(gss_new->uid);
	gss_new->uid = uid;
	gss_new->auth = gss_auth;
	list_add(&gss_new->list, &gss_auth->upcalls);
	gss_new = NULL;
	/* Has someone updated the credential behind our back? */
	if (!gss_cred_is_uptodate_ctx(cred)) {
		/* No, so do upcall and sleep */
		task->tk_timeout = 0;
		rpc_sleep_on(&gss_msg->waitq, task, NULL, NULL);
		spin_unlock(&gss_auth->lock);
		res = rpc_queue_upcall(dentry->d_inode, msg);
		if (res)
			gss_unhash_msg(gss_msg);
	} else {
		/* Yes, so cancel upcall */
		__gss_unhash_msg(gss_msg);
		spin_unlock(&gss_auth->lock);
	}
	gss_release_msg(gss_msg);
	dprintk("RPC: %4u gss_upcall for uid %u result %d\n", task->tk_pid,
			uid, res);
	return res;
out_sleep:
	task->tk_timeout = 0;
	rpc_sleep_on(&gss_msg->waitq, task, NULL, NULL);
	spin_unlock(&gss_auth->lock);
	dprintk("RPC: %4u gss_upcall  sleeping\n", task->tk_pid);
	if (gss_new)
		kfree(gss_new);
	/* Note: we drop the reference here: we are automatically removed
	 * from the queue when we're woken up, and we should in any case
	 * have no further responsabilities w.r.t. the upcall.
	 */
	gss_release_msg(gss_msg);
	return 0;
}

static ssize_t
gss_pipe_upcall(struct file *filp, struct rpc_pipe_msg *msg,
		char __user *dst, size_t buflen)
{
	char *data = (char *)msg->data + msg->copied;
	ssize_t mlen = msg->len;
	ssize_t left;

	if (mlen > buflen)
		mlen = buflen;
	left = copy_to_user(dst, data, mlen);
	if (left < 0) {
		msg->errno = left;
		return left;
	}
	mlen -= left;
	msg->copied += mlen;
	msg->errno = 0;
	return mlen;
}

#define MSG_BUF_MAXSIZE 1024

static ssize_t
gss_pipe_downcall(struct file *filp, const char __user *src, size_t mlen)
{
	const void *p, *end;
	void *buf;
	struct rpc_clnt *clnt;
	struct gss_auth *gss_auth;
	struct auth_cred acred = { 0 };
	struct rpc_cred *cred;
	struct gss_upcall_msg *gss_msg;
	struct gss_cl_ctx *ctx;
	uid_t uid;
	int err = -EFBIG;

	if (mlen > MSG_BUF_MAXSIZE)
		goto out;
	err = -ENOMEM;
	buf = kmalloc(mlen, GFP_KERNEL);
	if (!buf)
		goto out;

	clnt = RPC_I(filp->f_dentry->d_inode)->private;
	err = -EFAULT;
	if (copy_from_user(buf, src, mlen))
		goto err;

	end = (const void *)((char *)buf + mlen);
	p = simple_get_bytes(buf, end, &uid, sizeof(uid));
	if (IS_ERR(p)) {
		err = PTR_ERR(p);
		goto err;
	}
	acred.uid = uid;
	err = -ENOENT;
	cred = rpcauth_lookup_credcache(clnt->cl_auth, &acred, 0);
	if (!cred)
		goto err;

	err = -ENOMEM;
	ctx = gss_alloc_context();
	if (ctx == NULL)
		goto err;
	err = 0;
	gss_auth = container_of(clnt->cl_auth, struct gss_auth, rpc_auth);
	p = gss_fill_context(p, end, ctx, gss_auth->mech);
	if (IS_ERR(p)) {
		err = PTR_ERR(p);
		if (err != -EACCES)
			goto err_put_ctx;
	} else
		gss_cred_set_ctx(cred, gss_get_ctx(ctx));
	spin_lock(&gss_auth->lock);
	gss_msg = __gss_find_upcall(gss_auth, acred.uid);
	if (gss_msg) {
		gss_msg->msg.errno = err;
		__gss_unhash_msg(gss_msg);
		spin_unlock(&gss_auth->lock);
		gss_release_msg(gss_msg);
	} else
		spin_unlock(&gss_auth->lock);
	gss_put_ctx(ctx);
	kfree(buf);
	dprintk("RPC:      gss_pipe_downcall returning length %Zu\n", mlen);
	return mlen;
err_put_ctx:
	gss_put_ctx(ctx);
err:
	kfree(buf);
out:
	dprintk("RPC:      gss_pipe_downcall returning %d\n", err);
	return err;
}

static void
gss_pipe_release(struct inode *inode)
{
	struct rpc_inode *rpci = RPC_I(inode);
	struct rpc_clnt *clnt;
	struct rpc_auth *auth;
	struct gss_auth *gss_auth;

	clnt = rpci->private;
	auth = clnt->cl_auth;
	gss_auth = container_of(auth, struct gss_auth, rpc_auth);
	spin_lock(&gss_auth->lock);
	while (!list_empty(&gss_auth->upcalls)) {
		struct gss_upcall_msg *gss_msg;

		gss_msg = list_entry(gss_auth->upcalls.next,
				struct gss_upcall_msg, list);
		gss_msg->msg.errno = -EPIPE;
		atomic_inc(&gss_msg->count);
		__gss_unhash_msg(gss_msg);
		spin_unlock(&gss_auth->lock);
		gss_release_msg(gss_msg);
		spin_lock(&gss_auth->lock);
	}
	spin_unlock(&gss_auth->lock);
}

static void
gss_pipe_destroy_msg(struct rpc_pipe_msg *msg)
{
	struct gss_upcall_msg *gss_msg = container_of(msg, struct gss_upcall_msg, msg);
	static unsigned long ratelimit;

	if (msg->errno < 0) {
		dprintk("RPC:      gss_pipe_destroy_msg releasing msg %p\n",
				gss_msg);
		atomic_inc(&gss_msg->count);
		gss_unhash_msg(gss_msg);
		if (msg->errno == -ETIMEDOUT || msg->errno == -EPIPE) {
			unsigned long now = jiffies;
			if (time_after(now, ratelimit)) {
				printk(KERN_WARNING "RPC: AUTH_GSS upcall timed out.\n"
						    "Please check user daemon is running!\n");
				ratelimit = now + 15*HZ;
			}
		}
		gss_release_msg(gss_msg);
	}
}

/* 
 * NOTE: we have the opportunity to use different 
 * parameters based on the input flavor (which must be a pseudoflavor)
 */
static struct rpc_auth *
gss_create(struct rpc_clnt *clnt, rpc_authflavor_t flavor)
{
	struct gss_auth *gss_auth;
	struct rpc_auth * auth;

	dprintk("RPC:      creating GSS authenticator for client %p\n",clnt);

	if (!try_module_get(THIS_MODULE))
		return NULL;
	if (!(gss_auth = kmalloc(sizeof(*gss_auth), GFP_KERNEL)))
		goto out_dec;
	gss_auth->mech = gss_mech_get_by_pseudoflavor(flavor);
	if (!gss_auth->mech) {
		printk(KERN_WARNING "%s: Pseudoflavor %d not found!",
				__FUNCTION__, flavor);
		goto err_free;
	}
	gss_auth->service = gss_pseudoflavor_to_service(gss_auth->mech, flavor);
	/* FIXME: Will go away once privacy support is merged in */
	if (gss_auth->service == RPC_GSS_SVC_PRIVACY)
		gss_auth->service = RPC_GSS_SVC_INTEGRITY;
	INIT_LIST_HEAD(&gss_auth->upcalls);
	spin_lock_init(&gss_auth->lock);
	auth = &gss_auth->rpc_auth;
	auth->au_cslack = GSS_CRED_SLACK >> 2;
	auth->au_rslack = GSS_VERF_SLACK >> 2;
	auth->au_ops = &authgss_ops;
	auth->au_flavor = flavor;
	atomic_set(&auth->au_count, 1);

	if (rpcauth_init_credcache(auth, GSS_CRED_EXPIRE) < 0)
		goto err_put_mech;

	snprintf(gss_auth->path, sizeof(gss_auth->path), "%s/%s",
			clnt->cl_pathname,
			gss_auth->mech->gm_name);
	gss_auth->dentry = rpc_mkpipe(gss_auth->path, clnt, &gss_upcall_ops, RPC_PIPE_WAIT_FOR_OPEN);
	if (IS_ERR(gss_auth->dentry))
		goto err_put_mech;

	return auth;
err_put_mech:
	gss_mech_put(gss_auth->mech);
err_free:
	kfree(gss_auth);
out_dec:
	module_put(THIS_MODULE);
	return NULL;
}

static void
gss_destroy(struct rpc_auth *auth)
{
	struct gss_auth *gss_auth;

	dprintk("RPC:      destroying GSS authenticator %p flavor %d\n",
		auth, auth->au_flavor);

	gss_auth = container_of(auth, struct gss_auth, rpc_auth);
	rpc_unlink(gss_auth->path);
	gss_mech_put(gss_auth->mech);

	rpcauth_free_credcache(auth);
	kfree(gss_auth);
	module_put(THIS_MODULE);
}

/* gss_destroy_cred (and gss_destroy_ctx) are used to clean up after failure
 * to create a new cred or context, so they check that things have been
 * allocated before freeing them. */
static void
gss_destroy_ctx(struct gss_cl_ctx *ctx)
{
	dprintk("RPC:      gss_destroy_ctx\n");

	if (ctx->gc_gss_ctx)
		gss_delete_sec_context(&ctx->gc_gss_ctx);

	kfree(ctx->gc_wire_ctx.data);
	kfree(ctx);
}

static void
gss_destroy_cred(struct rpc_cred *rc)
{
	struct gss_cred *cred = container_of(rc, struct gss_cred, gc_base);

	dprintk("RPC:      gss_destroy_cred \n");

	if (cred->gc_ctx)
		gss_put_ctx(cred->gc_ctx);
	kfree(cred);
}

/*
 * Lookup RPCSEC_GSS cred for the current process
 */
static struct rpc_cred *
gss_lookup_cred(struct rpc_auth *auth, struct auth_cred *acred, int taskflags)
{
	return rpcauth_lookup_credcache(auth, acred, taskflags);
}

static struct rpc_cred *
gss_create_cred(struct rpc_auth *auth, struct auth_cred *acred, int taskflags)
{
	struct gss_auth *gss_auth = container_of(auth, struct gss_auth, rpc_auth);
	struct gss_cred	*cred = NULL;

	dprintk("RPC:      gss_create_cred for uid %d, flavor %d\n",
		acred->uid, auth->au_flavor);

	if (!(cred = kmalloc(sizeof(*cred), GFP_KERNEL)))
		goto out_err;

	memset(cred, 0, sizeof(*cred));
	atomic_set(&cred->gc_count, 1);
	cred->gc_uid = acred->uid;
	/*
	 * Note: in order to force a call to call_refresh(), we deliberately
	 * fail to flag the credential as RPCAUTH_CRED_UPTODATE.
	 */
	cred->gc_flags = 0;
	cred->gc_base.cr_ops = &gss_credops;
	cred->gc_service = gss_auth->service;

	return &cred->gc_base;

out_err:
	dprintk("RPC:      gss_create_cred failed\n");
	if (cred) gss_destroy_cred(&cred->gc_base);
	return NULL;
}

static int
gss_match(struct auth_cred *acred, struct rpc_cred *rc, int taskflags)
{
	return (rc->cr_uid == acred->uid);
}

/*
* Marshal credentials.
* Maybe we should keep a cached credential for performance reasons.
*/
static u32 *
gss_marshal(struct rpc_task *task, u32 *p, int ruid)
{
	struct rpc_cred *cred = task->tk_msg.rpc_cred;
	struct gss_cred	*gss_cred = container_of(cred, struct gss_cred,
						 gc_base);
	struct gss_cl_ctx	*ctx = gss_cred_get_ctx(cred);
	u32		*cred_len;
	struct rpc_rqst *req = task->tk_rqstp;
	u32             maj_stat = 0;
	struct xdr_netobj mic;
	struct kvec	iov;
	struct xdr_buf	verf_buf;

	dprintk("RPC: %4u gss_marshal\n", task->tk_pid);

	*p++ = htonl(RPC_AUTH_GSS);
	cred_len = p++;

	spin_lock(&ctx->gc_seq_lock);
	req->rq_seqno = ctx->gc_seq++;
	spin_unlock(&ctx->gc_seq_lock);

	*p++ = htonl((u32) RPC_GSS_VERSION);
	*p++ = htonl((u32) ctx->gc_proc);
	*p++ = htonl((u32) req->rq_seqno);
	*p++ = htonl((u32) gss_cred->gc_service);
	p = xdr_encode_netobj(p, &ctx->gc_wire_ctx);
	*cred_len = htonl((p - (cred_len + 1)) << 2);

	/* We compute the checksum for the verifier over the xdr-encoded bytes
	 * starting with the xid and ending at the end of the credential: */
	iov.iov_base = req->rq_snd_buf.head[0].iov_base;
	if (task->tk_client->cl_xprt->stream)
		/* See clnt.c:call_header() */
		iov.iov_base += 4;
	iov.iov_len = (u8 *)p - (u8 *)iov.iov_base;
	xdr_buf_from_iov(&iov, &verf_buf);

	/* set verifier flavor*/
	*p++ = htonl(RPC_AUTH_GSS);

	mic.data = (u8 *)(p + 1);
	maj_stat = gss_get_mic(ctx->gc_gss_ctx,
			       GSS_C_QOP_DEFAULT, 
			       &verf_buf, &mic);
	if (maj_stat == GSS_S_CONTEXT_EXPIRED) {
		cred->cr_flags &= ~RPCAUTH_CRED_UPTODATE;
	} else if (maj_stat != 0) {
		printk("gss_marshal: gss_get_mic FAILED (%d)\n", maj_stat);
		goto out_put_ctx;
	}
	p = xdr_encode_opaque(p, NULL, mic.len);
	gss_put_ctx(ctx);
	return p;
out_put_ctx:
	gss_put_ctx(ctx);
	return NULL;
}

/*
* Refresh credentials. XXX - finish
*/
static int
gss_refresh(struct rpc_task *task)
{
	struct rpc_clnt *clnt = task->tk_client;
	struct rpc_cred *cred = task->tk_msg.rpc_cred;

	if (!gss_cred_is_uptodate_ctx(cred))
		return gss_upcall(clnt, task, cred);
	return 0;
}

static u32 *
gss_validate(struct rpc_task *task, u32 *p)
{
	struct rpc_cred *cred = task->tk_msg.rpc_cred;
	struct gss_cred	*gss_cred = container_of(cred, struct gss_cred,
						gc_base);
	struct gss_cl_ctx *ctx = gss_cred_get_ctx(cred);
	u32		seq, qop_state;
	struct kvec	iov;
	struct xdr_buf	verf_buf;
	struct xdr_netobj mic;
	u32		flav,len;
	u32		maj_stat;

	dprintk("RPC: %4u gss_validate\n", task->tk_pid);

	flav = ntohl(*p++);
	if ((len = ntohl(*p++)) > RPC_MAX_AUTH_SIZE)
                goto out_bad;
	if (flav != RPC_AUTH_GSS)
		goto out_bad;
	seq = htonl(task->tk_rqstp->rq_seqno);
	iov.iov_base = &seq;
	iov.iov_len = sizeof(seq);
	xdr_buf_from_iov(&iov, &verf_buf);
	mic.data = (u8 *)p;
	mic.len = len;

	maj_stat = gss_verify_mic(ctx->gc_gss_ctx, &verf_buf, &mic, &qop_state);
	if (maj_stat == GSS_S_CONTEXT_EXPIRED)
		cred->cr_flags &= ~RPCAUTH_CRED_UPTODATE;
	if (maj_stat)
		goto out_bad;
       switch (gss_cred->gc_service) {
       case RPC_GSS_SVC_NONE:
	       /* verifier data, flavor, length: */
	       task->tk_auth->au_rslack = XDR_QUADLEN(len) + 2;
	       break;
       case RPC_GSS_SVC_INTEGRITY:
	       /* verifier data, flavor, length, length, sequence number: */
	       task->tk_auth->au_rslack = XDR_QUADLEN(len) + 4;
	       break;
       case RPC_GSS_SVC_PRIVACY:
	       goto out_bad;
       }
	gss_put_ctx(ctx);
	dprintk("RPC: %4u GSS gss_validate: gss_verify_mic succeeded.\n",
			task->tk_pid);
	return p + XDR_QUADLEN(len);
out_bad:
	gss_put_ctx(ctx);
	dprintk("RPC: %4u gss_validate failed.\n", task->tk_pid);
	return NULL;
}

static inline int
gss_wrap_req_integ(struct rpc_cred *cred, struct gss_cl_ctx *ctx,
		kxdrproc_t encode, struct rpc_rqst *rqstp, u32 *p, void *obj)
{
	struct xdr_buf	*snd_buf = &rqstp->rq_snd_buf;
	struct xdr_buf	integ_buf;
	u32             *integ_len = NULL;
	struct xdr_netobj mic;
	u32		offset, *q;
	struct kvec	*iov;
	u32             maj_stat = 0;
	int		status = -EIO;

	integ_len = p++;
	offset = (u8 *)p - (u8 *)snd_buf->head[0].iov_base;
	*p++ = htonl(rqstp->rq_seqno);

	status = encode(rqstp, p, obj);
	if (status)
		return status;

	if (xdr_buf_subsegment(snd_buf, &integ_buf,
				offset, snd_buf->len - offset))
		return status;
	*integ_len = htonl(integ_buf.len);

	/* guess whether we're in the head or the tail: */
	if (snd_buf->page_len || snd_buf->tail[0].iov_len) 
		iov = snd_buf->tail;
	else
		iov = snd_buf->head;
	p = iov->iov_base + iov->iov_len;
	mic.data = (u8 *)(p + 1);

	maj_stat = gss_get_mic(ctx->gc_gss_ctx,
			GSS_C_QOP_DEFAULT, &integ_buf, &mic);
	status = -EIO; /* XXX? */
	if (maj_stat == GSS_S_CONTEXT_EXPIRED)
		cred->cr_flags &= ~RPCAUTH_CRED_UPTODATE;
	else if (maj_stat)
		return status;
	q = xdr_encode_opaque(p, NULL, mic.len);

	offset = (u8 *)q - (u8 *)p;
	iov->iov_len += offset;
	snd_buf->len += offset;
	return 0;
}

static int
gss_wrap_req(struct rpc_task *task,
	     kxdrproc_t encode, void *rqstp, u32 *p, void *obj)
{
	struct rpc_cred *cred = task->tk_msg.rpc_cred;
	struct gss_cred	*gss_cred = container_of(cred, struct gss_cred,
			gc_base);
	struct gss_cl_ctx *ctx = gss_cred_get_ctx(cred);
	int             status = -EIO;

	dprintk("RPC: %4u gss_wrap_req\n", task->tk_pid);
	if (ctx->gc_proc != RPC_GSS_PROC_DATA) {
		/* The spec seems a little ambiguous here, but I think that not
		 * wrapping context destruction requests makes the most sense.
		 */
		status = encode(rqstp, p, obj);
		goto out;
	}
	switch (gss_cred->gc_service) {
		case RPC_GSS_SVC_NONE:
			status = encode(rqstp, p, obj);
			break;
		case RPC_GSS_SVC_INTEGRITY:
			status = gss_wrap_req_integ(cred, ctx, encode,
								rqstp, p, obj);
			break;
       		case RPC_GSS_SVC_PRIVACY:
			break;
	}
out:
	gss_put_ctx(ctx);
	dprintk("RPC: %4u gss_wrap_req returning %d\n", task->tk_pid, status);
	return status;
}

static inline int
gss_unwrap_resp_integ(struct rpc_cred *cred, struct gss_cl_ctx *ctx,
		struct rpc_rqst *rqstp, u32 **p)
{
	struct xdr_buf	*rcv_buf = &rqstp->rq_rcv_buf;
	struct xdr_buf integ_buf;
	struct xdr_netobj mic;
	u32 data_offset, mic_offset;
	u32 integ_len;
	u32 maj_stat;
	int status = -EIO;

	integ_len = ntohl(*(*p)++);
	if (integ_len & 3)
		return status;
	data_offset = (u8 *)(*p) - (u8 *)rcv_buf->head[0].iov_base;
	mic_offset = integ_len + data_offset;
	if (mic_offset > rcv_buf->len)
		return status;
	if (ntohl(*(*p)++) != rqstp->rq_seqno)
		return status;

	if (xdr_buf_subsegment(rcv_buf, &integ_buf, data_offset,
				mic_offset - data_offset))
		return status;

	if (xdr_buf_read_netobj(rcv_buf, &mic, mic_offset))
		return status;

	maj_stat = gss_verify_mic(ctx->gc_gss_ctx, &integ_buf,
			&mic, NULL);
	if (maj_stat == GSS_S_CONTEXT_EXPIRED)
		cred->cr_flags &= ~RPCAUTH_CRED_UPTODATE;
	if (maj_stat != GSS_S_COMPLETE)
		return status;
	return 0;
}

static int
gss_unwrap_resp(struct rpc_task *task,
		kxdrproc_t decode, void *rqstp, u32 *p, void *obj)
{
	struct rpc_cred *cred = task->tk_msg.rpc_cred;
	struct gss_cred *gss_cred = container_of(cred, struct gss_cred,
			gc_base);
	struct gss_cl_ctx *ctx = gss_cred_get_ctx(cred);
	int             status = -EIO;

	if (ctx->gc_proc != RPC_GSS_PROC_DATA)
		goto out_decode;
	switch (gss_cred->gc_service) {
		case RPC_GSS_SVC_NONE:
			break;
		case RPC_GSS_SVC_INTEGRITY:
			status = gss_unwrap_resp_integ(cred, ctx, rqstp, &p);
			if (status)
				goto out;
			break;
       		case RPC_GSS_SVC_PRIVACY:
			break;
	}
out_decode:
	status = decode(rqstp, p, obj);
out:
	gss_put_ctx(ctx);
	dprintk("RPC: %4u gss_unwrap_resp returning %d\n", task->tk_pid,
			status);
	return status;
}
  
static struct rpc_authops authgss_ops = {
	.owner		= THIS_MODULE,
	.au_flavor	= RPC_AUTH_GSS,
#ifdef RPC_DEBUG
	.au_name	= "RPCSEC_GSS",
#endif
	.create		= gss_create,
	.destroy	= gss_destroy,
	.lookup_cred	= gss_lookup_cred,
	.crcreate	= gss_create_cred
};

static struct rpc_credops gss_credops = {
	.cr_name	= "AUTH_GSS",
	.crdestroy	= gss_destroy_cred,
	.crmatch	= gss_match,
	.crmarshal	= gss_marshal,
	.crrefresh	= gss_refresh,
	.crvalidate	= gss_validate,
	.crwrap_req	= gss_wrap_req,
	.crunwrap_resp	= gss_unwrap_resp,
};

static struct rpc_pipe_ops gss_upcall_ops = {
	.upcall		= gss_pipe_upcall,
	.downcall	= gss_pipe_downcall,
	.destroy_msg	= gss_pipe_destroy_msg,
	.release_pipe	= gss_pipe_release,
};

/*
 * Initialize RPCSEC_GSS module
 */
static int __init init_rpcsec_gss(void)
{
	int err = 0;

	err = rpcauth_register(&authgss_ops);
	if (err)
		goto out;
	err = gss_svc_init();
	if (err)
		goto out_unregister;
	return 0;
out_unregister:
	rpcauth_unregister(&authgss_ops);
out:
	return err;
}

static void __exit exit_rpcsec_gss(void)
{
	gss_svc_shutdown();
	rpcauth_unregister(&authgss_ops);
}

MODULE_LICENSE("GPL");
module_init(init_rpcsec_gss)
module_exit(exit_rpcsec_gss)
