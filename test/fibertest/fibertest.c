
#include "aw-fiber.h"
#include <stdio.h>
#include <string.h>

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
	int (*func)(struct task *task);
};

int task_a(struct task *task) {
	fibermsg_t msg;

	printf("%s was scheduled (%d:%d)\n", task->name, task->self.state, task->self.coroutine);
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

	printf("%s was scheduled (%d:%d)\n", task->name, task->self.state, task->self.coroutine);
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

	struct task t[2];
	bool loop;

	memset(t, 0, sizeof t);

	fiber_init(&t[0].self);
	t[0].name = "Dick";
	t[0].func = task_a;

	fiber_init(&t[1].self);
	t[1].name = "Jane";
	t[1].peer = &t[0].self;
	t[1].func = task_b;

	do {
		loop = false;
		for (int i = 0; i < 2; ++i)
			if (fiber_ready(&t[i].self))
				loop |= (t[i].func(&t[i]) == STATUS_WAIT);
	} while (loop);

	printf("All done\n");
	return 0;
}

