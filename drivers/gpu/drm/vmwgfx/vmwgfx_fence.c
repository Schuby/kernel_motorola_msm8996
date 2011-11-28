/**************************************************************************
 *
 * Copyright © 2011 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "drmP.h"
#include "vmwgfx_drv.h"

#define VMW_FENCE_WRAP (1 << 31)

struct vmw_fence_manager {
	int num_fence_objects;
	struct vmw_private *dev_priv;
	spinlock_t lock;
	struct list_head fence_list;
	struct work_struct work;
	u32 user_fence_size;
	u32 fence_size;
	u32 event_fence_action_size;
	bool fifo_down;
	struct list_head cleanup_list;
	uint32_t pending_actions[VMW_ACTION_MAX];
	struct mutex goal_irq_mutex;
	bool goal_irq_on; /* Protected by @goal_irq_mutex */
	bool seqno_valid; /* Protected by @lock, and may not be set to true
			     without the @goal_irq_mutex held. */
};

struct vmw_user_fence {
	struct ttm_base_object base;
	struct vmw_fence_obj fence;
};

/**
 * struct vmw_event_fence_action - fence action that delivers a drm event.
 *
 * @e: A struct drm_pending_event that controls the event delivery.
 * @action: A struct vmw_fence_action to hook up to a fence.
 * @fence: A referenced pointer to the fence to keep it alive while @action
 * hangs on it.
 * @dev: Pointer to a struct drm_device so we can access the event stuff.
 * @kref: Both @e and @action has destructors, so we need to refcount.
 * @size: Size accounted for this object.
 * @tv_sec: If non-null, the variable pointed to will be assigned
 * current time tv_sec val when the fence signals.
 * @tv_usec: Must be set if @tv_sec is set, and the variable pointed to will
 * be assigned the current time tv_usec val when the fence signals.
 */
struct vmw_event_fence_action {
	struct drm_pending_event e;
	struct vmw_fence_action action;
	struct vmw_fence_obj *fence;
	struct drm_device *dev;
	struct kref kref;
	uint32_t size;
	uint32_t *tv_sec;
	uint32_t *tv_usec;
};

/**
 * Note on fencing subsystem usage of irqs:
 * Typically the vmw_fences_update function is called
 *
 * a) When a new fence seqno has been submitted by the fifo code.
 * b) On-demand when we have waiters. Sleeping waiters will switch on the
 * ANY_FENCE irq and call vmw_fences_update function each time an ANY_FENCE
 * irq is received. When the last fence waiter is gone, that IRQ is masked
 * away.
 *
 * In situations where there are no waiters and we don't submit any new fences,
 * fence objects may not be signaled. This is perfectly OK, since there are
 * no consumers of the signaled data, but that is NOT ok when there are fence
 * actions attached to a fence. The fencing subsystem then makes use of the
 * FENCE_GOAL irq and sets the fence goal seqno to that of the next fence
 * which has an action attached, and each time vmw_fences_update is called,
 * the subsystem makes sure the fence goal seqno is updated.
 *
 * The fence goal seqno irq is on as long as there are unsignaled fence
 * objects with actions attached to them.
 */

static void vmw_fence_obj_destroy_locked(struct kref *kref)
{
	struct vmw_fence_obj *fence =
		container_of(kref, struct vmw_fence_obj, kref);

	struct vmw_fence_manager *fman = fence->fman;
	unsigned int num_fences;

	list_del_init(&fence->head);
	num_fences = --fman->num_fence_objects;
	spin_unlock_irq(&fman->lock);
	if (fence->destroy)
		fence->destroy(fence);
	else
		kfree(fence);

	spin_lock_irq(&fman->lock);
}


/**
 * Execute signal actions on fences recently signaled.
 * This is done from a workqueue so we don't have to execute
 * signal actions from atomic context.
 */

static void vmw_fence_work_func(struct work_struct *work)
{
	struct vmw_fence_manager *fman =
		container_of(work, struct vmw_fence_manager, work);
	struct list_head list;
	struct vmw_fence_action *action, *next_action;
	bool seqno_valid;

	do {
		INIT_LIST_HEAD(&list);
		mutex_lock(&fman->goal_irq_mutex);

		spin_lock_irq(&fman->lock);
		list_splice_init(&fman->cleanup_list, &list);
		seqno_valid = fman->seqno_valid;
		spin_unlock_irq(&fman->lock);

		if (!seqno_valid && fman->goal_irq_on) {
			fman->goal_irq_on = false;
			vmw_goal_waiter_remove(fman->dev_priv);
		}
		mutex_unlock(&fman->goal_irq_mutex);

		if (list_empty(&list))
			return;

		/*
		 * At this point, only we should be able to manipulate the
		 * list heads of the actions we have on the private list.
		 * hence fman::lock not held.
		 */

		list_for_each_entry_safe(action, next_action, &list, head) {
			list_del_init(&action->head);
			if (action->cleanup)
				action->cleanup(action);
		}
	} while (1);
}

struct vmw_fence_manager *vmw_fence_manager_init(struct vmw_private *dev_priv)
{
	struct vmw_fence_manager *fman = kzalloc(sizeof(*fman), GFP_KERNEL);

	if (unlikely(fman == NULL))
		return NULL;

	fman->dev_priv = dev_priv;
	spin_lock_init(&fman->lock);
	INIT_LIST_HEAD(&fman->fence_list);
	INIT_LIST_HEAD(&fman->cleanup_list);
	INIT_WORK(&fman->work, &vmw_fence_work_func);
	fman->fifo_down = true;
	fman->user_fence_size = ttm_round_pot(sizeof(struct vmw_user_fence));
	fman->fence_size = ttm_round_pot(sizeof(struct vmw_fence_obj));
	fman->event_fence_action_size =
		ttm_round_pot(sizeof(struct vmw_event_fence_action));
	mutex_init(&fman->goal_irq_mutex);

	return fman;
}

void vmw_fence_manager_takedown(struct vmw_fence_manager *fman)
{
	unsigned long irq_flags;
	bool lists_empty;

	(void) cancel_work_sync(&fman->work);

	spin_lock_irqsave(&fman->lock, irq_flags);
	lists_empty = list_empty(&fman->fence_list) &&
		list_empty(&fman->cleanup_list);
	spin_unlock_irqrestore(&fman->lock, irq_flags);

	BUG_ON(!lists_empty);
	kfree(fman);
}

static int vmw_fence_obj_init(struct vmw_fence_manager *fman,
			      struct vmw_fence_obj *fence,
			      u32 seqno,
			      uint32_t mask,
			      void (*destroy) (struct vmw_fence_obj *fence))
{
	unsigned long irq_flags;
	unsigned int num_fences;
	int ret = 0;

	fence->seqno = seqno;
	INIT_LIST_HEAD(&fence->seq_passed_actions);
	fence->fman = fman;
	fence->signaled = 0;
	fence->signal_mask = mask;
	kref_init(&fence->kref);
	fence->destroy = destroy;
	init_waitqueue_head(&fence->queue);

	spin_lock_irqsave(&fman->lock, irq_flags);
	if (unlikely(fman->fifo_down)) {
		ret = -EBUSY;
		goto out_unlock;
	}
	list_add_tail(&fence->head, &fman->fence_list);
	num_fences = ++fman->num_fence_objects;

out_unlock:
	spin_unlock_irqrestore(&fman->lock, irq_flags);
	return ret;

}

struct vmw_fence_obj *vmw_fence_obj_reference(struct vmw_fence_obj *fence)
{
	if (unlikely(fence == NULL))
		return NULL;

	kref_get(&fence->kref);
	return fence;
}

/**
 * vmw_fence_obj_unreference
 *
 * Note that this function may not be entered with disabled irqs since
 * it may re-enable them in the destroy function.
 *
 */
void vmw_fence_obj_unreference(struct vmw_fence_obj **fence_p)
{
	struct vmw_fence_obj *fence = *fence_p;
	struct vmw_fence_manager *fman;

	if (unlikely(fence == NULL))
		return;

	fman = fence->fman;
	*fence_p = NULL;
	spin_lock_irq(&fman->lock);
	BUG_ON(atomic_read(&fence->kref.refcount) == 0);
	kref_put(&fence->kref, vmw_fence_obj_destroy_locked);
	spin_unlock_irq(&fman->lock);
}

void vmw_fences_perform_actions(struct vmw_fence_manager *fman,
				struct list_head *list)
{
	struct vmw_fence_action *action, *next_action;

	list_for_each_entry_safe(action, next_action, list, head) {
		list_del_init(&action->head);
		fman->pending_actions[action->type]--;
		if (action->seq_passed != NULL)
			action->seq_passed(action);

		/*
		 * Add the cleanup action to the cleanup list so that
		 * it will be performed by a worker task.
		 */

		list_add_tail(&action->head, &fman->cleanup_list);
	}
}

/**
 * vmw_fence_goal_new_locked - Figure out a new device fence goal
 * seqno if needed.
 *
 * @fman: Pointer to a fence manager.
 * @passed_seqno: The seqno the device currently signals as passed.
 *
 * This function should be called with the fence manager lock held.
 * It is typically called when we have a new passed_seqno, and
 * we might need to update the fence goal. It checks to see whether
 * the current fence goal has already passed, and, in that case,
 * scans through all unsignaled fences to get the next fence object with an
 * action attached, and sets the seqno of that fence as a new fence goal.
 *
 * returns true if the device goal seqno was updated. False otherwise.
 */
static bool vmw_fence_goal_new_locked(struct vmw_fence_manager *fman,
				      u32 passed_seqno)
{
	u32 goal_seqno;
	__le32 __iomem *fifo_mem;
	struct vmw_fence_obj *fence;

	if (likely(!fman->seqno_valid))
		return false;

	fifo_mem = fman->dev_priv->mmio_virt;
	goal_seqno = ioread32(fifo_mem + SVGA_FIFO_FENCE_GOAL);
	if (likely(passed_seqno - goal_seqno >= VMW_FENCE_WRAP))
		return false;

	fman->seqno_valid = false;
	list_for_each_entry(fence, &fman->fence_list, head) {
		if (!list_empty(&fence->seq_passed_actions)) {
			fman->seqno_valid = true;
			iowrite32(fence->seqno,
				  fifo_mem + SVGA_FIFO_FENCE_GOAL);
			break;
		}
	}

	return true;
}


/**
 * vmw_fence_goal_check_locked - Replace the device fence goal seqno if
 * needed.
 *
 * @fence: Pointer to a struct vmw_fence_obj the seqno of which should be
 * considered as a device fence goal.
 *
 * This function should be called with the fence manager lock held.
 * It is typically called when an action has been attached to a fence to
 * check whether the seqno of that fence should be used for a fence
 * goal interrupt. This is typically needed if the current fence goal is
 * invalid, or has a higher seqno than that of the current fence object.
 *
 * returns true if the device goal seqno was updated. False otherwise.
 */
static bool vmw_fence_goal_check_locked(struct vmw_fence_obj *fence)
{
	u32 goal_seqno;
	__le32 __iomem *fifo_mem;

	if (fence->signaled & DRM_VMW_FENCE_FLAG_EXEC)
		return false;

	fifo_mem = fence->fman->dev_priv->mmio_virt;
	goal_seqno = ioread32(fifo_mem + SVGA_FIFO_FENCE_GOAL);
	if (likely(fence->fman->seqno_valid &&
		   goal_seqno - fence->seqno < VMW_FENCE_WRAP))
		return false;

	iowrite32(fence->seqno, fifo_mem + SVGA_FIFO_FENCE_GOAL);
	fence->fman->seqno_valid = true;

	return true;
}

void vmw_fences_update(struct vmw_fence_manager *fman)
{
	unsigned long flags;
	struct vmw_fence_obj *fence, *next_fence;
	struct list_head action_list;
	bool needs_rerun;
	uint32_t seqno, new_seqno;
	__le32 __iomem *fifo_mem = fman->dev_priv->mmio_virt;

	seqno = ioread32(fifo_mem + SVGA_FIFO_FENCE);
rerun:
	spin_lock_irqsave(&fman->lock, flags);
	list_for_each_entry_safe(fence, next_fence, &fman->fence_list, head) {
		if (seqno - fence->seqno < VMW_FENCE_WRAP) {
			list_del_init(&fence->head);
			fence->signaled |= DRM_VMW_FENCE_FLAG_EXEC;
			INIT_LIST_HEAD(&action_list);
			list_splice_init(&fence->seq_passed_actions,
					 &action_list);
			vmw_fences_perform_actions(fman, &action_list);
			wake_up_all(&fence->queue);
		} else
			break;
	}

	needs_rerun = vmw_fence_goal_new_locked(fman, seqno);

	if (!list_empty(&fman->cleanup_list))
		(void) schedule_work(&fman->work);
	spin_unlock_irqrestore(&fman->lock, flags);

	/*
	 * Rerun if the fence goal seqno was updated, and the
	 * hardware might have raced with that update, so that
	 * we missed a fence_goal irq.
	 */

	if (unlikely(needs_rerun)) {
		new_seqno = ioread32(fifo_mem + SVGA_FIFO_FENCE);
		if (new_seqno != seqno) {
			seqno = new_seqno;
			goto rerun;
		}
	}
}

bool vmw_fence_obj_signaled(struct vmw_fence_obj *fence,
			    uint32_t flags)
{
	struct vmw_fence_manager *fman = fence->fman;
	unsigned long irq_flags;
	uint32_t signaled;

	spin_lock_irqsave(&fman->lock, irq_flags);
	signaled = fence->signaled;
	spin_unlock_irqrestore(&fman->lock, irq_flags);

	flags &= fence->signal_mask;
	if ((signaled & flags) == flags)
		return 1;

	if ((signaled & DRM_VMW_FENCE_FLAG_EXEC) == 0)
		vmw_fences_update(fman);

	spin_lock_irqsave(&fman->lock, irq_flags);
	signaled = fence->signaled;
	spin_unlock_irqrestore(&fman->lock, irq_flags);

	return ((signaled & flags) == flags);
}

int vmw_fence_obj_wait(struct vmw_fence_obj *fence,
		       uint32_t flags, bool lazy,
		       bool interruptible, unsigned long timeout)
{
	struct vmw_private *dev_priv = fence->fman->dev_priv;
	long ret;

	if (likely(vmw_fence_obj_signaled(fence, flags)))
		return 0;

	vmw_fifo_ping_host(dev_priv, SVGA_SYNC_GENERIC);
	vmw_seqno_waiter_add(dev_priv);

	if (interruptible)
		ret = wait_event_interruptible_timeout
			(fence->queue,
			 vmw_fence_obj_signaled(fence, flags),
			 timeout);
	else
		ret = wait_event_timeout
			(fence->queue,
			 vmw_fence_obj_signaled(fence, flags),
			 timeout);

	vmw_seqno_waiter_remove(dev_priv);

	if (unlikely(ret == 0))
		ret = -EBUSY;
	else if (likely(ret > 0))
		ret = 0;

	return ret;
}

void vmw_fence_obj_flush(struct vmw_fence_obj *fence)
{
	struct vmw_private *dev_priv = fence->fman->dev_priv;

	vmw_fifo_ping_host(dev_priv, SVGA_SYNC_GENERIC);
}

static void vmw_fence_destroy(struct vmw_fence_obj *fence)
{
	struct vmw_fence_manager *fman = fence->fman;

	kfree(fence);
	/*
	 * Free kernel space accounting.
	 */
	ttm_mem_global_free(vmw_mem_glob(fman->dev_priv),
			    fman->fence_size);
}

int vmw_fence_create(struct vmw_fence_manager *fman,
		     uint32_t seqno,
		     uint32_t mask,
		     struct vmw_fence_obj **p_fence)
{
	struct ttm_mem_global *mem_glob = vmw_mem_glob(fman->dev_priv);
	struct vmw_fence_obj *fence;
	int ret;

	ret = ttm_mem_global_alloc(mem_glob, fman->fence_size,
				   false, false);
	if (unlikely(ret != 0))
		return ret;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (unlikely(fence == NULL)) {
		ret = -ENOMEM;
		goto out_no_object;
	}

	ret = vmw_fence_obj_init(fman, fence, seqno, mask,
				 vmw_fence_destroy);
	if (unlikely(ret != 0))
		goto out_err_init;

	*p_fence = fence;
	return 0;

out_err_init:
	kfree(fence);
out_no_object:
	ttm_mem_global_free(mem_glob, fman->fence_size);
	return ret;
}


static void vmw_user_fence_destroy(struct vmw_fence_obj *fence)
{
	struct vmw_user_fence *ufence =
		container_of(fence, struct vmw_user_fence, fence);
	struct vmw_fence_manager *fman = fence->fman;

	kfree(ufence);
	/*
	 * Free kernel space accounting.
	 */
	ttm_mem_global_free(vmw_mem_glob(fman->dev_priv),
			    fman->user_fence_size);
}

static void vmw_user_fence_base_release(struct ttm_base_object **p_base)
{
	struct ttm_base_object *base = *p_base;
	struct vmw_user_fence *ufence =
		container_of(base, struct vmw_user_fence, base);
	struct vmw_fence_obj *fence = &ufence->fence;

	*p_base = NULL;
	vmw_fence_obj_unreference(&fence);
}

int vmw_user_fence_create(struct drm_file *file_priv,
			  struct vmw_fence_manager *fman,
			  uint32_t seqno,
			  uint32_t mask,
			  struct vmw_fence_obj **p_fence,
			  uint32_t *p_handle)
{
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;
	struct vmw_user_fence *ufence;
	struct vmw_fence_obj *tmp;
	struct ttm_mem_global *mem_glob = vmw_mem_glob(fman->dev_priv);
	int ret;

	/*
	 * Kernel memory space accounting, since this object may
	 * be created by a user-space request.
	 */

	ret = ttm_mem_global_alloc(mem_glob, fman->user_fence_size,
				   false, false);
	if (unlikely(ret != 0))
		return ret;

	ufence = kzalloc(sizeof(*ufence), GFP_KERNEL);
	if (unlikely(ufence == NULL)) {
		ret = -ENOMEM;
		goto out_no_object;
	}

	ret = vmw_fence_obj_init(fman, &ufence->fence, seqno,
				 mask, vmw_user_fence_destroy);
	if (unlikely(ret != 0)) {
		kfree(ufence);
		goto out_no_object;
	}

	/*
	 * The base object holds a reference which is freed in
	 * vmw_user_fence_base_release.
	 */
	tmp = vmw_fence_obj_reference(&ufence->fence);
	ret = ttm_base_object_init(tfile, &ufence->base, false,
				   VMW_RES_FENCE,
				   &vmw_user_fence_base_release, NULL);


	if (unlikely(ret != 0)) {
		/*
		 * Free the base object's reference
		 */
		vmw_fence_obj_unreference(&tmp);
		goto out_err;
	}

	*p_fence = &ufence->fence;
	*p_handle = ufence->base.hash.key;

	return 0;
out_err:
	tmp = &ufence->fence;
	vmw_fence_obj_unreference(&tmp);
out_no_object:
	ttm_mem_global_free(mem_glob, fman->user_fence_size);
	return ret;
}


/**
 * vmw_fence_fifo_down - signal all unsignaled fence objects.
 */

void vmw_fence_fifo_down(struct vmw_fence_manager *fman)
{
	unsigned long irq_flags;
	struct list_head action_list;
	int ret;

	/*
	 * The list may be altered while we traverse it, so always
	 * restart when we've released the fman->lock.
	 */

	spin_lock_irqsave(&fman->lock, irq_flags);
	fman->fifo_down = true;
	while (!list_empty(&fman->fence_list)) {
		struct vmw_fence_obj *fence =
			list_entry(fman->fence_list.prev, struct vmw_fence_obj,
				   head);
		kref_get(&fence->kref);
		spin_unlock_irq(&fman->lock);

		ret = vmw_fence_obj_wait(fence, fence->signal_mask,
					 false, false,
					 VMW_FENCE_WAIT_TIMEOUT);

		if (unlikely(ret != 0)) {
			list_del_init(&fence->head);
			fence->signaled |= DRM_VMW_FENCE_FLAG_EXEC;
			INIT_LIST_HEAD(&action_list);
			list_splice_init(&fence->seq_passed_actions,
					 &action_list);
			vmw_fences_perform_actions(fman, &action_list);
			wake_up_all(&fence->queue);
		}

		spin_lock_irq(&fman->lock);

		BUG_ON(!list_empty(&fence->head));
		kref_put(&fence->kref, vmw_fence_obj_destroy_locked);
	}
	spin_unlock_irqrestore(&fman->lock, irq_flags);
}

void vmw_fence_fifo_up(struct vmw_fence_manager *fman)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&fman->lock, irq_flags);
	fman->fifo_down = false;
	spin_unlock_irqrestore(&fman->lock, irq_flags);
}


int vmw_fence_obj_wait_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	struct drm_vmw_fence_wait_arg *arg =
	    (struct drm_vmw_fence_wait_arg *)data;
	unsigned long timeout;
	struct ttm_base_object *base;
	struct vmw_fence_obj *fence;
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;
	int ret;
	uint64_t wait_timeout = ((uint64_t)arg->timeout_us * HZ);

	/*
	 * 64-bit division not present on 32-bit systems, so do an
	 * approximation. (Divide by 1000000).
	 */

	wait_timeout = (wait_timeout >> 20) + (wait_timeout >> 24) -
	  (wait_timeout >> 26);

	if (!arg->cookie_valid) {
		arg->cookie_valid = 1;
		arg->kernel_cookie = jiffies + wait_timeout;
	}

	base = ttm_base_object_lookup(tfile, arg->handle);
	if (unlikely(base == NULL)) {
		printk(KERN_ERR "Wait invalid fence object handle "
		       "0x%08lx.\n",
		       (unsigned long)arg->handle);
		return -EINVAL;
	}

	fence = &(container_of(base, struct vmw_user_fence, base)->fence);

	timeout = jiffies;
	if (time_after_eq(timeout, (unsigned long)arg->kernel_cookie)) {
		ret = ((vmw_fence_obj_signaled(fence, arg->flags)) ?
		       0 : -EBUSY);
		goto out;
	}

	timeout = (unsigned long)arg->kernel_cookie - timeout;

	ret = vmw_fence_obj_wait(fence, arg->flags, arg->lazy, true, timeout);

out:
	ttm_base_object_unref(&base);

	/*
	 * Optionally unref the fence object.
	 */

	if (ret == 0 && (arg->wait_options & DRM_VMW_WAIT_OPTION_UNREF))
		return ttm_ref_object_base_unref(tfile, arg->handle,
						 TTM_REF_USAGE);
	return ret;
}

int vmw_fence_obj_signaled_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_vmw_fence_signaled_arg *arg =
		(struct drm_vmw_fence_signaled_arg *) data;
	struct ttm_base_object *base;
	struct vmw_fence_obj *fence;
	struct vmw_fence_manager *fman;
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;
	struct vmw_private *dev_priv = vmw_priv(dev);

	base = ttm_base_object_lookup(tfile, arg->handle);
	if (unlikely(base == NULL)) {
		printk(KERN_ERR "Fence signaled invalid fence object handle "
		       "0x%08lx.\n",
		       (unsigned long)arg->handle);
		return -EINVAL;
	}

	fence = &(container_of(base, struct vmw_user_fence, base)->fence);
	fman = fence->fman;

	arg->signaled = vmw_fence_obj_signaled(fence, arg->flags);
	spin_lock_irq(&fman->lock);

	arg->signaled_flags = fence->signaled;
	arg->passed_seqno = dev_priv->last_read_seqno;
	spin_unlock_irq(&fman->lock);

	ttm_base_object_unref(&base);

	return 0;
}


int vmw_fence_obj_unref_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_vmw_fence_arg *arg =
		(struct drm_vmw_fence_arg *) data;

	return ttm_ref_object_base_unref(vmw_fpriv(file_priv)->tfile,
					 arg->handle,
					 TTM_REF_USAGE);
}

/**
 * vmw_event_fence_action_destroy
 *
 * @kref: The struct kref embedded in a struct vmw_event_fence_action.
 *
 * The vmw_event_fence_action destructor that may be called either after
 * the fence action cleanup, or when the event is delivered.
 * It frees both the vmw_event_fence_action struct and the actual
 * event structure copied to user-space.
 */
static void vmw_event_fence_action_destroy(struct kref *kref)
{
	struct vmw_event_fence_action *eaction =
		container_of(kref, struct vmw_event_fence_action, kref);
	struct ttm_mem_global *mem_glob =
		vmw_mem_glob(vmw_priv(eaction->dev));
	uint32_t size = eaction->size;

	kfree(eaction->e.event);
	kfree(eaction);
	ttm_mem_global_free(mem_glob, size);
}


/**
 * vmw_event_fence_action_delivered
 *
 * @e: The struct drm_pending_event embedded in a struct
 * vmw_event_fence_action.
 *
 * The struct drm_pending_event destructor that is called by drm
 * once the event is delivered. Since we don't know whether this function
 * will be called before or after the fence action destructor, we
 * free a refcount and destroy if it becomes zero.
 */
static void vmw_event_fence_action_delivered(struct drm_pending_event *e)
{
	struct vmw_event_fence_action *eaction =
		container_of(e, struct vmw_event_fence_action, e);

	kref_put(&eaction->kref, vmw_event_fence_action_destroy);
}


/**
 * vmw_event_fence_action_seq_passed
 *
 * @action: The struct vmw_fence_action embedded in a struct
 * vmw_event_fence_action.
 *
 * This function is called when the seqno of the fence where @action is
 * attached has passed. It queues the event on the submitter's event list.
 * This function is always called from atomic context, and may be called
 * from irq context. It ups a refcount reflecting that we now have two
 * destructors.
 */
static void vmw_event_fence_action_seq_passed(struct vmw_fence_action *action)
{
	struct vmw_event_fence_action *eaction =
		container_of(action, struct vmw_event_fence_action, action);
	struct drm_device *dev = eaction->dev;
	struct drm_file *file_priv = eaction->e.file_priv;
	unsigned long irq_flags;

	kref_get(&eaction->kref);
	spin_lock_irqsave(&dev->event_lock, irq_flags);

	if (likely(eaction->tv_sec != NULL)) {
		struct timeval tv;

		do_gettimeofday(&tv);
		*eaction->tv_sec = tv.tv_sec;
		*eaction->tv_usec = tv.tv_usec;
	}

	list_add_tail(&eaction->e.link, &file_priv->event_list);
	wake_up_all(&file_priv->event_wait);
	spin_unlock_irqrestore(&dev->event_lock, irq_flags);
}

/**
 * vmw_event_fence_action_cleanup
 *
 * @action: The struct vmw_fence_action embedded in a struct
 * vmw_event_fence_action.
 *
 * This function is the struct vmw_fence_action destructor. It's typically
 * called from a workqueue.
 */
static void vmw_event_fence_action_cleanup(struct vmw_fence_action *action)
{
	struct vmw_event_fence_action *eaction =
		container_of(action, struct vmw_event_fence_action, action);

	vmw_fence_obj_unreference(&eaction->fence);
	kref_put(&eaction->kref, vmw_event_fence_action_destroy);
}


/**
 * vmw_fence_obj_add_action - Add an action to a fence object.
 *
 * @fence - The fence object.
 * @action - The action to add.
 *
 * Note that the action callbacks may be executed before this function
 * returns.
 */
void vmw_fence_obj_add_action(struct vmw_fence_obj *fence,
			      struct vmw_fence_action *action)
{
	struct vmw_fence_manager *fman = fence->fman;
	unsigned long irq_flags;
	bool run_update = false;

	mutex_lock(&fman->goal_irq_mutex);
	spin_lock_irqsave(&fman->lock, irq_flags);

	fman->pending_actions[action->type]++;
	if (fence->signaled & DRM_VMW_FENCE_FLAG_EXEC) {
		struct list_head action_list;

		INIT_LIST_HEAD(&action_list);
		list_add_tail(&action->head, &action_list);
		vmw_fences_perform_actions(fman, &action_list);
	} else {
		list_add_tail(&action->head, &fence->seq_passed_actions);

		/*
		 * This function may set fman::seqno_valid, so it must
		 * be run with the goal_irq_mutex held.
		 */
		run_update = vmw_fence_goal_check_locked(fence);
	}

	spin_unlock_irqrestore(&fman->lock, irq_flags);

	if (run_update) {
		if (!fman->goal_irq_on) {
			fman->goal_irq_on = true;
			vmw_goal_waiter_add(fman->dev_priv);
		}
		vmw_fences_update(fman);
	}
	mutex_unlock(&fman->goal_irq_mutex);

}

/**
 * vmw_event_fence_action_create - Post an event for sending when a fence
 * object seqno has passed.
 *
 * @file_priv: The file connection on which the event should be posted.
 * @fence: The fence object on which to post the event.
 * @event: Event to be posted. This event should've been alloced
 * using k[mz]alloc, and should've been completely initialized.
 * @interruptible: Interruptible waits if possible.
 *
 * As a side effect, the object pointed to by @event may have been
 * freed when this function returns. If this function returns with
 * an error code, the caller needs to free that object.
 */

int vmw_event_fence_action_create(struct drm_file *file_priv,
				  struct vmw_fence_obj *fence,
				  struct drm_event *event,
				  uint32_t *tv_sec,
				  uint32_t *tv_usec,
				  bool interruptible)
{
	struct vmw_event_fence_action *eaction;
	struct ttm_mem_global *mem_glob =
		vmw_mem_glob(fence->fman->dev_priv);
	struct vmw_fence_manager *fman = fence->fman;
	uint32_t size = fman->event_fence_action_size +
		ttm_round_pot(event->length);
	int ret;

	/*
	 * Account for internal structure size as well as the
	 * event size itself.
	 */

	ret = ttm_mem_global_alloc(mem_glob, size, false, interruptible);
	if (unlikely(ret != 0))
		return ret;

	eaction = kzalloc(sizeof(*eaction), GFP_KERNEL);
	if (unlikely(eaction == NULL)) {
		ttm_mem_global_free(mem_glob, size);
		return -ENOMEM;
	}

	eaction->e.event = event;
	eaction->e.file_priv = file_priv;
	eaction->e.destroy = vmw_event_fence_action_delivered;

	eaction->action.seq_passed = vmw_event_fence_action_seq_passed;
	eaction->action.cleanup = vmw_event_fence_action_cleanup;
	eaction->action.type = VMW_ACTION_EVENT;

	eaction->fence = vmw_fence_obj_reference(fence);
	eaction->dev = fman->dev_priv->dev;
	eaction->size = size;
	eaction->tv_sec = tv_sec;
	eaction->tv_usec = tv_usec;

	kref_init(&eaction->kref);
	vmw_fence_obj_add_action(fence, &eaction->action);

	return 0;
}

int vmw_fence_event_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct vmw_private *dev_priv = vmw_priv(dev);
	struct drm_vmw_fence_event_arg *arg =
		(struct drm_vmw_fence_event_arg *) data;
	struct vmw_fence_obj *fence = NULL;
	struct vmw_fpriv *vmw_fp = vmw_fpriv(file_priv);
	struct drm_vmw_fence_rep __user *user_fence_rep =
		(struct drm_vmw_fence_rep __user *)(unsigned long)
		arg->fence_rep;
	uint32_t handle;
	unsigned long irq_flags;
	struct drm_vmw_event_fence *event;
	int ret;

	/*
	 * Look up an existing fence object,
	 * and if user-space wants a new reference,
	 * add one.
	 */
	if (arg->handle) {
		struct ttm_base_object *base =
			ttm_base_object_lookup(vmw_fp->tfile, arg->handle);

		if (unlikely(base == NULL)) {
			DRM_ERROR("Fence event invalid fence object handle "
				  "0x%08lx.\n",
				  (unsigned long)arg->handle);
			return -EINVAL;
		}
		fence = &(container_of(base, struct vmw_user_fence,
				       base)->fence);
		(void) vmw_fence_obj_reference(fence);

		if (user_fence_rep != NULL) {
			bool existed;

			ret = ttm_ref_object_add(vmw_fp->tfile, base,
						 TTM_REF_USAGE, &existed);
			if (unlikely(ret != 0)) {
				DRM_ERROR("Failed to reference a fence "
					  "object.\n");
				goto out_no_ref_obj;
			}
			handle = base->hash.key;
		}
		ttm_base_object_unref(&base);
	}

	/*
	 * Create a new fence object.
	 */
	if (!fence) {
		ret = vmw_execbuf_fence_commands(file_priv, dev_priv,
						 &fence,
						 (user_fence_rep) ?
						 &handle : NULL);
		if (unlikely(ret != 0)) {
			DRM_ERROR("Fence event failed to create fence.\n");
			return ret;
		}
	}

	BUG_ON(fence == NULL);

	spin_lock_irqsave(&dev->event_lock, irq_flags);

	ret = (file_priv->event_space < sizeof(*event)) ? -EBUSY : 0;
	if (likely(ret == 0))
		file_priv->event_space -= sizeof(*event);

	spin_unlock_irqrestore(&dev->event_lock, irq_flags);

	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed to allocate event space for this file.\n");
		goto out_no_event_space;
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (unlikely(event == NULL)) {
		DRM_ERROR("Failed to allocate an event.\n");
		goto out_no_event;
	}

	event->base.type = DRM_VMW_EVENT_FENCE_SIGNALED;
	event->base.length = sizeof(*event);
	event->user_data = arg->user_data;

	if (arg->flags & DRM_VMW_FE_FLAG_REQ_TIME)
		ret = vmw_event_fence_action_create(file_priv, fence,
						    &event->base,
						    &event->tv_sec,
						    &event->tv_usec,
						    true);
	else
		ret = vmw_event_fence_action_create(file_priv, fence,
						    &event->base,
						    NULL,
						    NULL,
						    true);

	if (unlikely(ret != 0)) {
		if (ret != -ERESTARTSYS)
			DRM_ERROR("Failed to attach event to fence.\n");
		goto out_no_attach;
	}

	vmw_execbuf_copy_fence_user(dev_priv, vmw_fp, 0, user_fence_rep, fence,
				    handle);
	vmw_fence_obj_unreference(&fence);
	return 0;
out_no_attach:
	kfree(event);
out_no_event:
	spin_lock_irqsave(&dev->event_lock, irq_flags);
	file_priv->event_space += sizeof(*event);
	spin_unlock_irqrestore(&dev->event_lock, irq_flags);
out_no_event_space:
	if (user_fence_rep != NULL)
		ttm_ref_object_base_unref(vmw_fpriv(file_priv)->tfile,
					  handle, TTM_REF_USAGE);
out_no_ref_obj:
	vmw_fence_obj_unreference(&fence);
	return ret;
}
