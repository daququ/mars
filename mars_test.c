// (c) 2010 Thomas Schoebel-Theuer / 1&1 Internet AG

//#define BRICK_DEBUGGING
//#define MARS_DEBUGGING

#define DEFAULT_ORDER    0
#define DEFAULT_BUFFERS (32768 / 2)
#define DEFAULT_MEM     (1024 / 4 * 256)

#define TRANS_ORDER    4
#define TRANS_BUFFERS (32)
#define TRANS_MEM     (1024 / 4)

#define CONF_TEST
#define CONF_BUF
#define CONF_USEBUF
#define CONF_TRANS


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include <linux/major.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>

#define _STRATEGY
#include "mars.h"

#include "mars_if_device.h"
#include "mars_check.h"
#include "mars_device_sio.h"
#include "mars_buf.h"
#include "mars_usebuf.h"
#include "mars_trans_logger.h"

GENERIC_ASPECT_FUNCTIONS(generic,mars_ref);

static struct generic_brick *if_brick = NULL;
static struct if_device_brick *_if_brick = NULL;
static struct generic_brick *usebuf_brick = NULL;

static struct generic_brick *trans_brick = NULL;
static struct trans_logger_brick *_trans_brick = NULL;
static struct generic_brick *tbuf_brick = NULL;
static struct buf_brick *_tbuf_brick = NULL;
static struct generic_brick *tdevice_brick = NULL;

static struct generic_brick *buf_brick = NULL;
static struct buf_brick *_buf_brick = NULL;
static struct generic_brick *device_brick = NULL;

static void test_endio(struct generic_callback *cb)
{
	MARS_DBG("test_endio() called! error=%d\n", cb->cb_error);
}

void make_test_instance(void)
{
	static char *names[] = { "brick" };
	struct generic_input *last = NULL;

	void *brick(const void *_brick_type)
	{
		const struct generic_brick_type *brick_type = _brick_type;
		const struct generic_input_type **input_types;
		const struct generic_output_type **output_types;
		void *mem;
		int size;
		int i;
		int status;

		size = brick_type->brick_size +
			(brick_type->max_inputs + brick_type->max_outputs) * sizeof(void*);
		input_types = brick_type->default_input_types;
		for (i = 0; i < brick_type->max_inputs; i++) {
			const struct generic_input_type *type = *input_types++;
			size += type->input_size;
		}
		output_types = brick_type->default_output_types;
		for (i = 0; i < brick_type->max_outputs; i++) {
			const struct generic_output_type *type = *output_types++;
			size += type->output_size;
		}

		mem = kzalloc(size, GFP_MARS);
		if (!mem) {
			MARS_ERR("cannot grab test memory for %s\n", brick_type->type_name);
			msleep(60000);
			return NULL;
		}
		status = generic_brick_init_full(mem, size, brick_type, NULL, NULL, names);
		MARS_INF("init '%s' (status=%d)\n", brick_type->type_name, status);
		if (status) {
			MARS_ERR("cannot init brick %s\n", brick_type->type_name);
			msleep(60000);
			return NULL;
		}
		return mem;
	}

	void connect(struct generic_input *a, struct generic_output *b)
	{
		int status;
#ifdef CONF_TEST
		struct generic_brick *tmp = brick(&check_brick_type);
		
		status = generic_connect(a, tmp->outputs[0]);
		MARS_DBG("connect (status=%d)\n", status);
		if (status < 0)
			msleep(60000);

		status = generic_connect(tmp->inputs[0], b);
#else
		status = generic_connect(a, b);
#endif
		MARS_DBG("connect (status=%d)\n", status);
		if (status < 0)
			msleep(60000);
	}


	MARS_DBG("starting....\n");

	device_brick = brick(&device_sio_brick_type);
	device_brick->outputs[0]->output_name = "/tmp/testfile.img";
	device_brick->ops->brick_switch(device_brick, true);

	if_brick = brick(&if_device_brick_type);

#if defined(CONF_USEBUF) && defined(CONF_BUF) // usebuf zwischenschalten
	usebuf_brick = brick(&usebuf_brick_type);

	connect(if_brick->inputs[0], usebuf_brick->outputs[0]);

	last = usebuf_brick->inputs[0];
#else
	(void)usebuf_brick;
	last = if_brick->inputs[0];
#endif

#ifdef CONF_BUF // Standard-buf zwischenschalten

	buf_brick = brick(&buf_brick_type);
	_buf_brick = (void*)buf_brick;
	_buf_brick->outputs[0]->output_name = "/tmp/testfile.img";
	_buf_brick->backing_order = DEFAULT_ORDER;
	_buf_brick->backing_size = PAGE_SIZE << _buf_brick->backing_order;
#ifdef DEFAULT_BUFFERS
	_buf_brick->max_count = DEFAULT_BUFFERS;
#else
	_buf_brick->max_count = DEFAULT_MEM >> _buf_brick->backing_order;
#endif

	connect(buf_brick->inputs[0], device_brick->outputs[0]);

#ifdef CONF_TRANS // trans_logger plus Infrastruktur zwischenschalten

	tdevice_brick = brick(&device_sio_brick_type);
	tdevice_brick->outputs[0]->output_name = "/tmp/testfile.log";
	tdevice_brick->ops->brick_switch(tdevice_brick, true);

	tbuf_brick = brick(&buf_brick_type);
	_tbuf_brick = (void*)tbuf_brick;
	_tbuf_brick->outputs[0]->output_name = "/tmp/testfile.log";
	_tbuf_brick->backing_order = TRANS_ORDER;
	_tbuf_brick->backing_size = PAGE_SIZE << _tbuf_brick->backing_order;
#ifdef TRANS_BUFFERS
	_tbuf_brick->max_count = TRANS_BUFFERS;
#else
	_tbuf_brick->max_count = TRANS_MEM >> _tbuf_brick->backing_order;
#endif

	connect(tbuf_brick->inputs[0], tdevice_brick->outputs[0]);

	trans_brick = brick(&trans_logger_brick_type);
	_trans_brick = (void*)trans_brick;
	_trans_brick->log_reads = true;
	_trans_brick->allow_reads_after = HZ * 1;
	_trans_brick->max_queue = 1000;
	_trans_brick->outputs[0]->q_phase2.q_max_flying = 1;
	_trans_brick->outputs[0]->q_phase4.q_max_flying = 1;

	connect(trans_brick->inputs[0], buf_brick->outputs[0]);
	connect(trans_brick->inputs[1], tbuf_brick->outputs[0]);

	connect(last, trans_brick->outputs[0]);
#else
	(void)trans_brick;
	(void)tbuf_brick;
	(void)_tbuf_brick;
	(void)tdevice_brick;
	connect(last, buf_brick->outputs[0]);
#endif

	if (false) { // ref-counting no longer valid
		struct buf_output *output = _buf_brick->outputs[0];
		struct mars_ref_object *mref = NULL;
		struct generic_object_layout ol = {};

		mref = generic_alloc_mars_ref((struct generic_output*)output, &ol);

		if (mref) {
			int status;
			mref->ref_pos = 0;
			mref->ref_len = PAGE_SIZE;
			mref->ref_may_write = READ;

			status = GENERIC_OUTPUT_CALL(output, mars_ref_get, mref);
			MARS_DBG("buf_get (status=%d)\n", status);
			if (true) {
				struct generic_callback cb = {
					.cb_fn = test_endio,
				};
				mref->ref_cb = &cb;

				GENERIC_OUTPUT_CALL(output, mars_ref_io, mref, READ);
				status = cb.cb_error;
				MARS_DBG("buf_io (status=%d)\n", status);
			}
			GENERIC_OUTPUT_CALL(output, mars_ref_put, mref);
		}
	}
#else
	(void)test_endio;
	connect(last, device_brick->outputs[0]);
#endif

	msleep(200);

	MARS_INF("------------- END INIT --------------\n");

	_if_brick = (void*)if_brick;
	_if_brick->is_active = true;

	msleep(2000);
	MARS_INF("------------- DONE --------------\n");
}

void destroy_test_instance(void)
{
}

static void __exit exit_test(void)
{
	MARS_DBG("destroy_test_instance()\n");
	destroy_test_instance();
}

static int __init init_test(void)
{
	MARS_DBG("make_test_instance()\n");
	make_test_instance();
	return 0;
}

MODULE_DESCRIPTION("MARS TEST");
MODULE_AUTHOR("Thomas Schoebel-Theuer <tst@1und1.de>");
MODULE_LICENSE("GPL");

module_init(init_test);
module_exit(exit_test);
