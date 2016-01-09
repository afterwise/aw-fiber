
#include "aw-fiber.h"
#include <stdio.h>

enum {
	STATUS_OK,
	STATUS_WAIT
};

const char *MSG_QUERY = "What's for lunch?";
const char *MSG_REPLY = "Enchiladas";

struct task {
	struct fiber self;
	struct fiber *peer;
	const char *name;
};

int task_a(struct task *task) {
	fibermsg_t msg;

	printf("%s was scheduled (%d co=%d)\n", task->name, task->self.state, task->self.coroutine);
	fiber_begin(&task->self);

	printf("%s will wait to receive\n", task->name);
	fiber_receive(&task->self, &task->peer, &msg, STATUS_WAIT);

	printf("%s->%s: %s\n", ((struct task *) task->peer)->name, task->name, (const char *) msg);

	printf("%s will reply\n", task->name);
	fiber_reply(&task->self, task->peer, (fibermsg_t) MSG_REPLY, STATUS_WAIT);

	fiber_end(&task->self);
	printf("%s is dead\n", task->name);

	return STATUS_OK;
}

int task_b(struct task *task) {
	fibermsg_t reply;

	printf("%s was scheduled (%d co=%d)\n", task->name, task->self.state, task->self.coroutine);
	fiber_begin(&task->self);

	printf("%s will send\n", task->name);
	fiber_send(&task->self, task->peer, (fibermsg_t) MSG_QUERY, &reply, STATUS_WAIT);

	printf("%s<-%s: %s\n", task->name, ((struct task *) task->peer)->name, (const char *) reply);

	fiber_end(&task->self);
	printf("%s is dead\n", task->name);
	return STATUS_OK;
}

int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	struct task a, b;

	fiber_init(&a.self);
	a.name = "Dick";

	fiber_init(&b.self);
	b.name = "Jane";
	b.peer = &a.self;

	do ; while (
		task_a(&a) == STATUS_WAIT |
		task_b(&b) == STATUS_WAIT);

	printf("All done\n");
	return 0;
}

