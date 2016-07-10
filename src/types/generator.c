#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "codeobject.h"
#include "funcobject.h"
#include "iter.h"
#include "exc.h"
#include "vm.h"
#include "util.h"
#include "generator.h"

RhoValue rho_gen_proxy_make(RhoCodeObject *co)
{
	RhoGeneratorProxy *gp = rho_obj_alloc(&rho_gen_proxy_class);
	rho_retaino(co);
	gp->co = co;
	gp->defaults = (struct rho_value_array){.array = NULL, .length = 0};
	return rho_makeobj(gp);
}

RhoValue rho_gen_make(RhoGeneratorProxy *gp)
{
	RhoGeneratorObject *go = rho_obj_alloc(&rho_gen_class);
	RHO_INIT_SAVED_TID_FIELD(go);

	RhoCodeObject *co = gp->co;
	RhoFrame *frame = rho_frame_make(co);
	frame->persistent = 1;

	rho_retaino(co);
	go->co = gp->co;
	go->frame = frame;
	return rho_makeobj(go);
}

static void release_defaults(RhoGeneratorProxy *gp);

static void gen_proxy_free(RhoValue *this)
{
	RhoGeneratorProxy *gp = rho_objvalue(this);
	rho_releaseo(gp->co);
	release_defaults(gp);
	rho_obj_class.del(this);
}

static void gen_free(RhoValue *this)
{
	RhoGeneratorObject *go = rho_objvalue(this);
	rho_releaseo(go->co);
	rho_frame_free(go->frame);
	rho_obj_class.del(this);
}

void rho_gen_proxy_init_defaults(RhoGeneratorProxy *gp, RhoValue *defaults, const size_t n_defaults)
{
	release_defaults(gp);
	gp->defaults.array = rho_malloc(n_defaults * sizeof(RhoValue));
	gp->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		gp->defaults.array[i] = defaults[i];
		rho_retain(&defaults[i]);
	}
}

static void release_defaults(RhoGeneratorProxy *gp)
{
	RhoValue *defaults = gp->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = gp->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		rho_release(&defaults[i]);
	}

	free(defaults);
	gp->defaults = (struct rho_value_array){.array = NULL, .length = 0};
}

static RhoValue gen_proxy_call(RhoValue *this,
                               RhoValue *args,
                               RhoValue *args_named,
                               size_t nargs,
                               size_t nargs_named)
{
	RhoGeneratorProxy *gp = rho_objvalue(this);
	RhoValue gen_v = rho_gen_make(gp);
	RhoGeneratorObject *go = rho_objvalue(&gen_v);
	RhoCodeObject *co = gp->co;
	RhoFrame *frame = go->frame;
	RhoValue status = rho_codeobj_load_args(co,
	                                        &gp->defaults,
	                                        args,
	                                        args_named,
	                                        nargs,
	                                        nargs_named,
	                                        frame->locals);

	if (rho_iserror(&status)) {
		gen_free(&gen_v);
		return status;
	}

	return rho_makeobj(go);
}

static RhoValue gen_iter(RhoValue *this)
{
	RhoGeneratorObject *go = rho_objvalue(this);
	rho_retaino(go);
	return *this;
}

static RhoValue gen_iternext(RhoValue *this)
{
	RhoGeneratorObject *go = rho_objvalue(this);
	RHO_CHECK_THREAD(go);

	if (go->frame == NULL) {
		return rho_get_iter_stop();
	}

	RhoCodeObject *co = go->co;
	RhoFrame *frame = go->frame;
	RhoVM *vm = rho_current_vm_get();

	rho_retaino(co);
	frame->co = co;

	rho_vm_push_frame_direct(vm, frame);
	rho_vm_eval_frame(vm);
	RhoValue res = frame->return_value;
	rho_vm_pop_frame(vm);

	if (rho_iserror(&res) || rho_is_iter_stop(&res)) {
		rho_frame_free(go->frame);
		go->frame = NULL;
	}

	return res;
}

RhoClass rho_gen_proxy_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "GeneratorProxy",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoGeneratorProxy),

	.init = NULL,
	.del = gen_proxy_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = gen_proxy_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

RhoClass rho_gen_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Generator",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoGeneratorObject),

	.init = NULL,
	.del = gen_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = gen_iter,
	.iternext = gen_iternext,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
