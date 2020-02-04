#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include "nt_smc_call.h"
#include "utdriver_macro.h"
#include "sched_status.h"

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#define CANCEL_SYS_NO	110
#define GK_SYS_NO 	120

#define VFS_SYS_NO	0x08
#define REETIME_SYS_NO	0x07

#define UT_BOOT_CORE   	0

#define UT_SWITCH_CORE 	4

extern void secondary_init_cmdbuf(void *info);
extern void secondary_boot_stage2(void *info);
extern void secondary_invoke_fastcall(void *info);
extern void secondary_load_tee(void *info);
extern void secondary_boot_stage1(void *info);
extern void secondary_load_func(void);
extern int handle_switch_core(int cpu);

struct switch_head_struct
{
	struct list_head head;
};
struct service_handler {
	unsigned int sysno;
	void *param_buf;
	unsigned size;
	long (*init)(struct service_handler *handler); 
	void (*deinit)(struct service_handler *handler);
	int (*handle)(struct service_handler *handler); 
};

struct switch_call_struct
{
	int switch_type;
	unsigned long buff_addr;
};

static void switch_fn(struct kthread_work *work);

struct smc_call_struct {
	unsigned long local_cmd;
	u32 teei_cmd_type;
	u32 dev_file_id;
	u32 svc_id;
	u32 cmd_id;
	u32 context;
	u32 enc_id;
	void *cmd_buf;
	size_t cmd_len;
	void *resp_buf;
	size_t resp_len;
	void *meta_data;
	void *info_data;
	size_t info_len;
	int *ret_resp_len;
	int *error_code;
	struct semaphore *psema;
	int retVal;
};

extern struct mutex pm_mutex;
extern unsigned long ut_pm_count;

extern struct kthread_worker ut_fastcall_worker;

extern int forward_call_flag;
extern int fp_call_flag;
extern int keymaster_call_flag;
extern int irq_call_flag;

extern void nt_sched_t_call(void);
extern int __send_fp_command(unsigned long share_memory_size);
extern int __send_cancel_command(unsigned long share_memory_size);
extern int __send_gatekeeper_command(unsigned long share_memory_size);
extern int __send_keymaster_command(unsigned long share_memory_size);
extern int __vfs_handle(struct service_handler *handler);
extern int __reetime_handle(struct service_handler *handler);
extern int __teei_smc_call(unsigned long local_smc_cmd,
				u32 teei_cmd_type,
				u32 dev_file_id,
				u32 svc_id,
				u32 cmd_id,
				u32 context,
				u32 enc_id,
				const void *cmd_buf,
				size_t cmd_len,
				void *resp_buf,
				size_t resp_len,
				const void *meta_data,
				const void *info_data,
				size_t info_len,
				int *ret_resp_len,
				int *error_code,
				struct semaphore *psema);
extern unsigned int need_mig_flag;
extern unsigned int nt_core;
extern struct task_struct *teei_switch_task;
extern int get_current_cpuid(void);

static struct switch_call_struct *create_switch_call_struct(void)
{
	struct switch_call_struct *tmp_entry = NULL;

	tmp_entry = kmalloc(sizeof(struct switch_call_struct), GFP_KERNEL);

	if (tmp_entry == NULL)
		IMSG_ERROR("[%s][%d] kmalloc failed!!!\n", __func__, __LINE__);

	return tmp_entry;
}

static int init_switch_call_struct(struct switch_call_struct *ent, int work_type, unsigned char *buff)
{
	if (ent == NULL) {
		IMSG_ERROR("[%s][%d] the paraments are wrong!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ent->switch_type = work_type;
	ent->buff_addr = (unsigned long)buff;

	return 0;
}

static int destroy_switch_call_struct(struct switch_call_struct *ent)
{
	kfree(ent);

	return 0;
}

struct ut_smc_call_work {
	struct kthread_work work;
	void *data;
};

static int ut_smc_call(void *buff)
{
	struct ut_smc_call_work usc_work = {
		KTHREAD_WORK_INIT(usc_work.work, switch_fn),
		.data = buff,
	};

	if (!queue_kthread_work(&ut_fastcall_worker, &usc_work.work))
		return -1;

	flush_kthread_work(&usc_work.work);
	return 0;
}

static int check_work_type(int work_type)
{
	switch (work_type) {
	case CAPI_CALL:
	case FDRV_CALL:
	case BDRV_CALL:
	case SCHED_CALL:
	case LOAD_FUNC:
	case INIT_CMD_CALL:
	case BOOT_STAGE1:
	case BOOT_STAGE2:
	case INVOKE_FASTCALL:
	case LOAD_TEE:
	case LOCK_PM_MUTEX:
	case UNLOCK_PM_MUTEX:
	case SWITCH_CORE:
	case NT_DUMP_T:	
		return 0;

	default:
		return -EINVAL;
	}
}

int handle_dump_call(void *buff)
{
	IMSG_DEBUG("[%s][%d] handle_dump_call begin.\n", __func__, __LINE__);
	nt_dump_t();
	IMSG_DEBUG("[%s][%d] handle_dump_call end.\n", __func__, __LINE__);
	return 0;
}
void handle_lock_pm_mutex(struct mutex *lock)
{
	if (ut_pm_count == 0){
		mutex_lock(lock);
	}
	ut_pm_count++;
}


void handle_unlock_pm_mutex(struct mutex *lock)
{

	ut_pm_count--;

	if (ut_pm_count == 0){
		mutex_unlock(lock);
	}
}

int add_work_entry(int work_type, unsigned char *buff)
{
	struct switch_call_struct *work_entry = NULL;
	int retVal = 0;

	retVal = check_work_type(work_type);

	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] with wrong work_type!\n", __func__, __LINE__);
		return retVal;
	}

	work_entry = create_switch_call_struct();

	if (work_entry == NULL) {
		IMSG_ERROR("[%s][%d] There is no enough memory!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	retVal = init_switch_call_struct(work_entry, work_type, buff);

	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] init_switch_call_struct failed!\n", __func__, __LINE__);
		destroy_switch_call_struct(work_entry);
		return retVal;
	}

	retVal = ut_smc_call((void *)work_entry);

	return retVal;
}

int get_call_type(struct switch_call_struct *ent)
{
	if (ent == NULL)
		return -EINVAL;

	return ent->switch_type;
}

int handle_sched_call(void *buff)
{
	volatile unsigned long smc_type = 2;

	nt_sched_t((uint64_t *)(&smc_type));
	while(smc_type == 0x54) {
		//udelay(IRQ_DELAY);
		nt_sched_t((uint64_t *)(&smc_type));
	}
	return 0;
}


int handle_capi_call(void *buff)
{
	struct smc_call_struct *cd = NULL;
	cd = (struct smc_call_struct *)buff;

	/* with a rmb() */
	rmb();

	cd->retVal = __teei_smc_call(cd->local_cmd,
				cd->teei_cmd_type,
				cd->dev_file_id,
				cd->svc_id,
				cd->cmd_id,
				cd->context,
				cd->enc_id,
				cd->cmd_buf,
				cd->cmd_len,
				cd->resp_buf,
				cd->resp_len,
				cd->meta_data,
				cd->info_data,
				cd->info_len,
				cd->ret_resp_len,
				cd->error_code,
				cd->psema);

	/* with a wmb() */
	wmb();

	return 0;
}

struct fdrv_call_struct {
	int fdrv_call_type;
	int fdrv_call_buff_size;
	int retVal;
};


int handle_fdrv_call(void *buff)
{
	struct fdrv_call_struct *cd = NULL;
	cd = (struct fdrv_call_struct *)buff;

	/* with a rmb() */
	rmb();

	switch (cd->fdrv_call_type) {
	IMSG_DEBUG("cd->fdrv_call_type = %d \n", cd->fdrv_call_type);

	case FP_SYS_NO:
		cd->retVal = __send_fp_command(cd->fdrv_call_buff_size);
		break;

	case KEYMASTER_SYS_NO:
		cd->retVal = __send_keymaster_command(cd->fdrv_call_buff_size);
		break;

	case GK_SYS_NO:
		cd->retVal = __send_gatekeeper_command(cd->fdrv_call_buff_size);
		break;
	case CANCEL_SYS_NO:
		cd->retVal = __send_cancel_command(cd->fdrv_call_buff_size);
		break;
	#ifdef TUI_SUPPORT
	case TUI_DISPLAY_SYS_NO:
		cd->retVal = __send_tui_display_command(cd->fdrv_call_buff_size);
		break;
	case TUI_NOTICE_SYS_NO:
		cd->retVal = __send_tui_notice_command(cd->fdrv_call_buff_size);
		break;
	#endif
	default:
		cd->retVal = -EINVAL;
	}

	/* with a wmb() */
	wmb();

	return 0;
}

struct bdrv_call_struct {
	int bdrv_call_type;
	struct service_handler *handler;
	int retVal;
};

int handle_bdrv_call(void *buff)
{
	struct bdrv_call_struct *cd = NULL;
	cd = (struct bdrv_call_struct *)buff;

	/* with a rmb() */
	rmb();

	switch (cd->bdrv_call_type) {
	case VFS_SYS_NO:
		cd->retVal = __vfs_handle(cd->handler);
		kfree(buff);
		break;

	case REETIME_SYS_NO:
		cd->retVal = __reetime_handle(cd->handler);
		kfree(buff);
		break;

	default:
		cd->retVal = -EINVAL;
	}

	/* with a wmb() */
	wmb();

	return 0;
}

int handle_switch_call(void *buff)
{


	unsigned long smc_type = 2;

	nt_sched_t((uint64_t *)(&smc_type));

	while (smc_type == 0x54) {
		//udelay(IRQ_DELAY);
		nt_sched_t((uint64_t *)(&smc_type));
	}
	return 0;
}


static void switch_fn(struct kthread_work *work)
{
	struct ut_smc_call_work *switch_work = NULL;
	struct switch_call_struct *switch_ent = NULL;
	int call_type = 0;
	int retVal = 0;

	switch_work = container_of(work, struct ut_smc_call_work, work);

	switch_ent = (struct switch_call_struct *)switch_work->data;

	call_type = get_call_type(switch_ent);

	switch (call_type) {
	case LOAD_FUNC:
		secondary_load_func();
		break;		
	case BOOT_STAGE1:
		secondary_boot_stage1((void *)(switch_ent->buff_addr));
		break;
	case INIT_CMD_CALL:
		secondary_init_cmdbuf((void *)(switch_ent->buff_addr));
		break;
	case BOOT_STAGE2:
		secondary_boot_stage2(NULL);
		break;
	case INVOKE_FASTCALL:
		secondary_invoke_fastcall(NULL);
		break;
	case LOAD_TEE:
		secondary_load_tee(NULL);
		break;
	case CAPI_CALL:
		retVal = handle_capi_call((void *)(switch_ent->buff_addr));

		if (retVal < 0) {
			IMSG_ERROR("[%s][%d] fail to handle ClientAPI!\n", __func__, __LINE__);
		}

		break;

	case FDRV_CALL:
		retVal = handle_fdrv_call((void *)(switch_ent->buff_addr));

		if (retVal < 0) {
			IMSG_ERROR("[%s][%d] fail to handle F-driver!\n", __func__, __LINE__);
		}

		break;

	case BDRV_CALL:
		retVal = handle_bdrv_call((void *)(switch_ent->buff_addr));

		if (retVal < 0) {
			IMSG_ERROR("[%s][%d] fail to handle B-driver!\n", __func__, __LINE__);
		}

		break;

	case SCHED_CALL:
		retVal = handle_sched_call((void *)(switch_ent->buff_addr));

		if (retVal < 0) {
			IMSG_ERROR("[%s][%d] fail to handle sched-Call!\n", __func__, __LINE__);
		}
		break;
	case LOCK_PM_MUTEX:
		handle_lock_pm_mutex((struct mutex *)(switch_ent->buff_addr));
		break;
	case UNLOCK_PM_MUTEX:
		handle_unlock_pm_mutex((struct mutex *)(switch_ent->buff_addr));
		break;
	case SWITCH_CORE:
		handle_switch_core((int)(switch_ent->buff_addr));
		break;
	case NT_DUMP_T:
		retVal = handle_dump_call((void *)(switch_ent->buff_addr));
		if (retVal < 0) {
			IMSG_ERROR("[%s][%d] fail to handle dump-Call!\n", __func__, __LINE__);
		}
		break;		
	default:
		IMSG_ERROR("switch fn handles a undefined call!\n");
		break;
	}

	retVal = destroy_switch_call_struct(switch_ent);

	if (retVal != 0) {
                IMSG_ERROR("[%s][%d] destroy_switch_call_struct failed %d!\n", __func__, __LINE__, retVal);
                return;
	}

	return;
}
