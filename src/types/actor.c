#include "main.h"
#if RHO_THREADED

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "object.h"
#include "exc.h"
#include "util.h"
#include "actor.h"

#define SAFE(x) if ((x)) RHO_INTERNAL_ERROR()

static struct rho_mailbox_node *make_node(RhoValue *v)
{
	struct rho_mailbox_node *node = rho_malloc(sizeof(struct rho_mailbox_node));
	rho_retain(v);
	node->value = *v;
	node->next = NULL;
	return node;
}

void rho_mailbox_init(struct rho_mailbox *mb)
{
	static RhoValue empty = RHO_MAKE_EMPTY();
	struct rho_mailbox_node *node = make_node(&empty);
	mb->head = node;
	mb->tail = node;

	SAFE(pthread_mutex_init(&mb->mutex, NULL));
	SAFE(pthread_cond_init(&mb->cond, NULL));
}

void rho_mailbox_push(struct rho_mailbox *mb, RhoValue *v)
{
	struct rho_mailbox_node *node = make_node(v);
	SAFE(pthread_mutex_lock(&mb->mutex));
	SAFE(pthread_cond_signal(&mb->cond));
	struct rho_mailbox_node *prev = mb->head;
	mb->head = node;
	prev->next = node;
	SAFE(pthread_mutex_unlock(&mb->mutex));
}

RhoValue rho_mailbox_pop(struct rho_mailbox *mb)
{
	struct rho_mailbox_node *tail = mb->tail;
	struct rho_mailbox_node *next = tail->next;

	if (next == NULL) {
		SAFE(pthread_mutex_lock(&mb->mutex));
		while (mb->tail->next == NULL) {
			SAFE(pthread_cond_wait(&mb->cond, &mb->mutex));
		}
		SAFE(pthread_mutex_unlock(&mb->mutex));

		tail = mb->tail;
		next = tail->next;
	}

	mb->tail = next;
	free(tail);
	return next->value;
}

RhoValue rho_mailbox_pop_nowait(struct rho_mailbox *mb)
{
	struct rho_mailbox_node *tail = mb->tail;
	struct rho_mailbox_node *next = tail->next;

	if (next != NULL) {
		mb->tail = next;
		free(tail);
		return next->value;
	}

	return rho_makeempty();
}

void rho_mailbox_dealloc(struct rho_mailbox *mb)
{
	while (true) {
		RhoValue v = rho_mailbox_pop_nowait(mb);
		if (rho_isempty(&v)) {
			break;
		}
		rho_release(&v);
	}

	free(mb->head);
	mb->head = NULL;
	mb->tail = NULL;

	pthread_mutex_destroy(&mb->mutex);
	pthread_cond_destroy(&mb->cond);
}

RhoValue rho_actor_proxy_make(RhoCodeObject *co)
{
	RhoActorProxy *ap = rho_obj_alloc(&rho_actor_proxy_class);
	rho_retaino(co);
	ap->co = co;
	ap->defaults = (struct rho_value_array){.array = NULL, .length = 0};
	return rho_makeobj(ap);
}

static RhoActorObject *volatile actors = NULL;
static pthread_mutex_t link_mutex = PTHREAD_MUTEX_INITIALIZER;

static void actor_link(RhoActorObject *ao)
{
	SAFE(pthread_mutex_lock(&link_mutex));
	rho_retaino(ao);

	if (actors != NULL) {
		actors->prev = ao;
	}

	ao->next = actors;
	ao->prev = NULL;
	actors = ao;
	pthread_mutex_unlock(&link_mutex);
}

static void actor_unlink(RhoActorObject *ao)
{
	SAFE(pthread_mutex_lock(&link_mutex));
	rho_releaseo(ao);

	if (actors == ao) {
		actors = ao->next;
	}

	if (ao->prev != NULL) {
		ao->prev->next = ao->next;
	}

	if (ao->next != NULL) {
		ao->next->prev = ao->prev;
	}
	SAFE(pthread_mutex_unlock(&link_mutex));
}

void rho_actor_join_all(void)
{
	while (true) {
		SAFE(pthread_mutex_lock(&link_mutex));
		RhoActorObject *ao = actors;
		SAFE(pthread_mutex_unlock(&link_mutex));

		if (ao != NULL) {
			SAFE(pthread_join(ao->thread, NULL));
		} else {
			break;
		}
	}
}

RhoValue rho_actor_make(RhoActorProxy *gp)
{
	RhoActorObject *ao = rho_obj_alloc(&rho_actor_class);
	RhoCodeObject *co = gp->co;

	rho_mailbox_init(&ao->mailbox);

	RhoFrame *frame = rho_frame_make(co);
	frame->persistent = 1;
	frame->force_free_locals = 1;
	frame->mailbox = &ao->mailbox;

	rho_retaino(co);
	ao->co = gp->co;
	ao->frame = frame;
	ao->vm = rho_vm_new();
	ao->retval = rho_makeempty();
	ao->state = RHO_ACTOR_STATE_READY;
	ao->next = NULL;
	ao->prev = NULL;
	return rho_makeobj(ao);
}

static void release_defaults(RhoActorProxy *ap);

static void actor_proxy_free(RhoValue *this)
{
	RhoActorProxy *ap = rho_objvalue(this);
	rho_releaseo(ap->co);
	release_defaults(ap);
	rho_obj_class.del(this);
}

static void actor_free(RhoValue *this)
{
	RhoActorObject *ao = rho_objvalue(this);

	if (ao->state == RHO_ACTOR_STATE_RUNNING) {
		SAFE(pthread_join(ao->thread, NULL));
		actor_unlink(ao);
	}

	rho_mailbox_dealloc(&ao->mailbox);
	rho_releaseo(ao->co);
	rho_frame_free(ao->frame);
	rho_vm_free(ao->vm);

	if (ao->retval.type == RHO_VAL_TYPE_ERROR) {
		rho_err_free(rho_errvalue(&ao->retval));
	} else {
		rho_release(&ao->retval);
	}

	rho_obj_class.del(this);
}

void rho_actor_proxy_init_defaults(RhoActorProxy *ap, RhoValue *defaults, const size_t n_defaults)
{
	release_defaults(ap);
	ap->defaults.array = rho_malloc(n_defaults * sizeof(RhoValue));
	ap->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		ap->defaults.array[i] = defaults[i];
		rho_retain(&defaults[i]);
	}
}

static void release_defaults(RhoActorProxy *ap)
{
	RhoValue *defaults = ap->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = ap->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		rho_release(&defaults[i]);
	}

	free(defaults);
	ap->defaults = (struct rho_value_array){.array = NULL, .length = 0};
}

static RhoValue actor_proxy_call(RhoValue *this,
                                 RhoValue *args,
                                 RhoValue *args_named,
                                 size_t nargs,
                                 size_t nargs_named)
{
	RhoActorProxy *ap = rho_objvalue(this);
	RhoValue actor_v = rho_actor_make(ap);
	RhoActorObject *go = rho_objvalue(&actor_v);
	RhoCodeObject *co = ap->co;
	RhoFrame *frame = go->frame;
	RhoValue status = rho_codeobj_load_args(co,
	                                        &ap->defaults,
	                                        args,
	                                        args_named,
	                                        nargs,
	                                        nargs_named,
	                                        frame->locals);

	if (rho_iserror(&status)) {
		actor_free(&actor_v);
		return status;
	}

	return rho_makeobj(go);
}

static void *actor_start_routine(void *args)
{
	RhoActorObject *ao = args;
	RhoCodeObject *co = ao->co;
	RhoFrame *frame = ao->frame;
	RhoVM *vm = ao->vm;
	rho_current_vm_set(vm);

	rho_retaino(co);
	frame->co = co;

	rho_vm_push_frame_direct(vm, frame);
	rho_vm_eval_frame(vm);
	ao->retval = frame->return_value;
	rho_vm_pop_frame(vm);

	ao->frame = NULL;
	rho_frame_free(frame);

	ao->state = RHO_ACTOR_STATE_FINISHED;
	actor_unlink(ao);
	return NULL;
}

RhoValue rho_actor_start(RhoActorObject *ao)
{
	if (ao->state != RHO_ACTOR_STATE_READY) {
		return RHO_ACTOR_EXC("cannot restart stopped actor");
	}

	actor_link(ao);
	ao->state = RHO_ACTOR_STATE_RUNNING;
	if (pthread_create(&ao->thread, NULL, actor_start_routine, ao)) {
		actor_unlink(ao);
		ao->state = RHO_ACTOR_STATE_FINISHED;
		return RHO_ACTOR_EXC("could not spawn thread for actor");
	}

	return rho_makenull();
}

#define STATE_CHECK_NOT_FINISHED(ao) \
	if ((ao)->state == RHO_ACTOR_STATE_FINISHED) \
		return RHO_ACTOR_EXC("actor has been stopped")

#define RETURN_RETVAL(ao) \
	do { \
		RhoValue _temp = ao->retval; \
		if (ao->retval.type == RHO_VAL_TYPE_ERROR) \
			ao->retval = rho_makeempty(); \
		else \
			rho_retain(&_temp); \
		return _temp; \
	} while (0)

static RhoValue actor_start(RhoValue *this,
                            RhoValue *args,
                            RhoValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "start"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoActorObject *ao = rho_objvalue(this);
	return rho_actor_start(ao);

#undef NAME
}

static RhoValue actor_check(RhoValue *this,
                            RhoValue *args,
                            RhoValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "check"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoActorObject *ao = rho_objvalue(this);

	if (ao->state != RHO_ACTOR_STATE_FINISHED) {
		return rho_makenull();
	} else {
		RETURN_RETVAL(ao);
	}

#undef NAME
}

static RhoValue actor_join(RhoValue *this,
                           RhoValue *args,
                           RhoValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "join"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoActorObject *ao = rho_objvalue(this);
	SAFE(pthread_join(ao->thread, NULL));
	RETURN_RETVAL(ao);

#undef NAME
}

static RhoValue actor_send(RhoValue *this,
                           RhoValue *args,
                           RhoValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "send"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	RhoActorObject *ao = rho_objvalue(this);
	STATE_CHECK_NOT_FINISHED(ao);
	RhoValue msg_v = rho_message_make(&args[0]);
	RhoMessage *msg = rho_objvalue(&msg_v);
	rho_mailbox_push(&ao->mailbox, &msg_v);
	RhoFutureObject *future = msg->future;
	rho_retaino(future);
	rho_releaseo(msg);
	return rho_makeobj(future);

#undef NAME
}

static RhoValue actor_stop(RhoValue *this,
                           RhoValue *args,
                           RhoValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "stop"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoActorObject *ao = rho_objvalue(this);
	STATE_CHECK_NOT_FINISHED(ao);

	RhoValue msg_v = rho_kill_message_make();
	RhoMessage *msg = rho_objvalue(&msg_v);
	rho_mailbox_push(&ao->mailbox, &msg_v);
	rho_releaseo(msg);
	return rho_makenull();

#undef NAME
}

struct rho_attr_method actor_methods[] = {
	{"start", actor_start},
	{"check", actor_check},
	{"join", actor_join},
	{"send", actor_send},
	{"stop", actor_stop},
	{NULL, NULL}
};


/* messages and futures */

RhoValue rho_future_make(void)
{
	RhoFutureObject *future = rho_obj_alloc(&rho_future_class);
	future->value = rho_makeempty();
	pthread_mutex_init(&future->mutex, NULL);
	pthread_cond_init(&future->cond, NULL);
	return rho_makeobj(future);
}

RhoValue rho_message_make(RhoValue *contents)
{
	RhoMessage *msg = rho_obj_alloc(&rho_message_class);
	rho_retain(contents);
	msg->contents = *contents;
	RhoValue future = rho_future_make();
	msg->future = rho_objvalue(&future);
	return rho_makeobj(msg);
}

RhoValue rho_kill_message_make(void)
{
	static RhoValue empty = rho_makeempty();
	return rho_message_make(&empty);
}

static void future_set_value(RhoFutureObject *future, RhoValue *v)
{
	rho_release(&future->value);
	rho_retain(v);
	future->value = *v;
}

static void future_free(RhoValue *this)
{
	RhoFutureObject *future = rho_objvalue(this);
	rho_release(&future->value);
	pthread_mutex_destroy(&future->mutex);
	pthread_cond_destroy(&future->cond);
	rho_obj_class.del(this);
}

static void message_free(RhoValue *this)
{
	RhoMessage *msg = rho_objvalue(this);
	rho_release(&msg->contents);
	if (msg->future != NULL) {
		rho_releaseo(msg->future);
	}
	rho_obj_class.del(this);
}

static RhoValue future_get(RhoValue *this,
                           RhoValue *args,
                           RhoValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "get"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK_AT_MOST(NAME, nargs, 1);

	RhoFutureObject *future = rho_objvalue(this);
	bool timeout = false;

	if (nargs > 0) {
		if (!rho_isint(&args[0])) {
			RhoClass *class = rho_getclass(&args[0]);
			return RHO_TYPE_EXC(NAME "() takes an integer argument (got a %s)", class->name);
		}

		const long ms = rho_intvalue(&args[0]);

		if (ms < 0) {
			RhoClass *class = rho_getclass(&args[0]);
			return RHO_TYPE_EXC(NAME "() got a negative argument", class->name);
		}

		struct timespec ts;
		struct timeval tv;

		SAFE(gettimeofday(&tv, NULL));
		TIMEVAL_TO_TIMESPEC(&tv, &ts);
		ts.tv_sec += ms/1000;
		ts.tv_nsec += (ms % 1000) * 1000000;

		SAFE(pthread_mutex_lock(&future->mutex));
		while (rho_isempty(&future->value)) {
			int n = pthread_cond_timedwait(&future->cond, &future->mutex, &ts);

			if (n == ETIMEDOUT) {
				timeout = true;
				break;
			} else if (n) {
				RHO_INTERNAL_ERROR();
			}
		}
		SAFE(pthread_mutex_unlock(&future->mutex));
	} else {
		SAFE(pthread_mutex_lock(&future->mutex));
		while (rho_isempty(&future->value)) {
			SAFE(pthread_cond_wait(&future->cond, &future->mutex));
		}
		SAFE(pthread_mutex_unlock(&future->mutex));
	}

	if (timeout) {
		return RHO_ACTOR_EXC(NAME "() timed out");
	} else {
		rho_retain(&future->value);
		return future->value;
	}

#undef NAME
}

static RhoValue message_contents(RhoValue *this,
                                 RhoValue *args,
                                 RhoValue *args_named,
                                 size_t nargs,
                                 size_t nargs_named)
{
#define NAME "contents"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoMessage *msg = rho_objvalue(this);
	rho_retain(&msg->contents);
	return msg->contents;

#undef NAME
}

static RhoValue message_reply(RhoValue *this,
                              RhoValue *args,
                              RhoValue *args_named,
                              size_t nargs,
                              size_t nargs_named)
{
#define NAME "reply"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	RhoMessage *msg = rho_objvalue(this);
	RhoFutureObject *future = msg->future;
	RhoValue ret;

	SAFE(pthread_mutex_lock(&future->mutex));
	if (future != NULL) {
		future_set_value(future, &args[0]);
		SAFE(pthread_cond_broadcast(&future->cond));
		rho_releaseo(future);
		msg->future = NULL;
		ret = rho_makenull();
	} else {
		ret = RHO_ACTOR_EXC("cannot reply to the same message twice");
	}
	SAFE(pthread_mutex_unlock(&future->mutex));
	return ret;

#undef NAME
}

struct rho_attr_method future_methods[] = {
	{"get", future_get},
	{NULL, NULL}
};

struct rho_attr_method message_methods[] = {
	{"contents", message_contents},
	{"reply", message_reply},
	{NULL, NULL}
};


/* classes */

RhoClass rho_actor_proxy_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "ActorProxy",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoActorProxy),

	.init = NULL,
	.del = actor_proxy_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = actor_proxy_call,

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

RhoClass rho_actor_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Actor",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoActorObject),

	.init = NULL,
	.del = actor_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = actor_methods,

	.attr_get = NULL,
	.attr_set = NULL
};

RhoClass rho_future_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Future",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoFutureObject),

	.init = NULL,
	.del = future_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = future_methods,

	.attr_get = NULL,
	.attr_set = NULL
};

RhoClass rho_message_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Message",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoMessage),

	.init = NULL,
	.del = message_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = message_methods,

	.attr_get = NULL,
	.attr_set = NULL
};

#else
extern int dummy;
#endif /* RHO_THREADED */
