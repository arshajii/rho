#include "main.h"
#if RHO_THREADED

#ifndef RHO_ACTOR_H
#define RHO_ACTOR_H

#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include "object.h"
#include "codeobject.h"
#include "vm.h"
#include "err.h"

struct rho_mailbox_node {
	RhoValue value;
	struct rho_mailbox_node *volatile next;
};

struct rho_mailbox {
	struct rho_mailbox_node *volatile head;
	struct rho_mailbox_node *tail;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

void rho_mailbox_init(struct rho_mailbox *mb);
void rho_mailbox_push(struct rho_mailbox *mb, RhoValue *v);
RhoValue rho_mailbox_pop(struct rho_mailbox *mb);
RhoValue rho_mailbox_pop_nowait(struct rho_mailbox *mb);
void rho_mailbox_dealloc(struct rho_mailbox *mb);

extern RhoClass rho_actor_proxy_class;
extern RhoClass rho_actor_class;
extern RhoClass rho_future_class;
extern RhoClass rho_message_class;

typedef struct {
	RhoObject base;
	RhoCodeObject *co;
	struct rho_value_array defaults;
} RhoActorProxy;

typedef struct rho_actor_object {
	RhoObject base;
	struct rho_mailbox mailbox;

	RhoCodeObject *co;
	RhoFrame *frame;
	RhoVM *vm;
	RhoValue retval;

	pthread_t thread;

	enum {
		RHO_ACTOR_STATE_READY,
		RHO_ACTOR_STATE_RUNNING,
		RHO_ACTOR_STATE_FINISHED
	} state;

	struct rho_actor_object *prev;
	struct rho_actor_object *next;
} RhoActorObject;

typedef struct {
	RhoObject base;

	RhoValue value;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} RhoFutureObject;

typedef struct {
	RhoObject base;
	RhoValue contents;  /* empty contents = kill message */
	RhoFutureObject *future;
} RhoMessage;

RhoValue rho_actor_proxy_make(RhoCodeObject *co);
RhoValue rho_actor_make(RhoActorProxy *ap);
void rho_actor_proxy_init_defaults(RhoActorProxy *ap, RhoValue *defaults, const size_t n_defaults);
void rho_actor_join_all(void);

RhoValue rho_future_make(void);
RhoValue rho_message_make(RhoValue *contents);
RhoValue rho_kill_message_make(void);

#endif /* RHO_ACTOR_H */
#endif /* RHO_THREADED */
