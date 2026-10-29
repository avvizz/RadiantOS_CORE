/* Планировщик и kernel-задачи (cooperative + soft-preempt от таймера) */
#include "task.h"
#include "kmalloc.h"
#include "kstring.h"
#include "interrupts.h"

extern void task_switch(uint64_t *old_rsp, uint64_t new_rsp);

static struct task tasks[TASK_MAX];
static struct task *current = 0;
static struct task *ready_head = 0;   /* кольцо ready-задач */
static uint64_t     next_id = 1;
static int          scheduling_enabled = 0;

/* ---------- вспомогательные ---------- */

static void ready_add(struct task *t) {
    t->state = TASK_READY;
    if (!ready_head) {
        ready_head = t;
        t->next = t;
    } else {
        /* вставляем после head (перед текущим «хвостом») */
        t->next = ready_head->next;
        ready_head->next = t;
    }
}

static void ready_remove(struct task *t) {
    if (!ready_head) return;
    if (ready_head->next == ready_head) {
        /* один элемент */
        if (ready_head == t) ready_head = 0;
        return;
    }
    struct task *prev = ready_head;
    while (prev->next != t) {
        prev = prev->next;
        if (prev == ready_head) return; /* не нашли */
    }
    prev->next = t->next;
    if (ready_head == t) ready_head = prev;
    t->next = 0;
}

/* Обёртка: вызывает entry(arg) и завершает задачу */
static void task_entry_wrapper(void) {
    struct task *t = current;
    if (t && t->entry)
        t->entry(t->arg);
    task_exit();
}

/* ---------- публичный API ---------- */

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));
    next_id = 1;

    /* Текущий поток → задача #0 "kernel" */
    struct task *t = &tasks[0];
    t->id         = next_id++;
    t->state      = TASK_RUNNING;
    t->rsp        = 0;
    t->stack      = 0;
    t->stack_size = 0;
    t->entry      = 0;
    t->arg        = 0;
    t->name       = "kernel";
    t->next       = t;

    current    = t;
    ready_head = t;
    scheduling_enabled = 1;
}

uint64_t task_create(const char *name, task_fn_t entry, void *arg) {
    struct task *t = 0;
    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            t = &tasks[i];
            break;
        }
    }
    if (!t) return (uint64_t)-1;

    void *stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) return (uint64_t)-1;

    memset(t, 0, sizeof(*t));
    t->id         = next_id++;
    t->state      = TASK_READY;
    t->stack      = stack;
    t->stack_size = TASK_STACK_SIZE;
    t->entry      = entry;
    t->arg        = arg;
    t->name       = name ? name : "task";

    /* Стек растёт вниз. Кладём фейковый кадр для task_switch. */
    uint64_t *sp = (uint64_t *)((uint8_t *)stack + TASK_STACK_SIZE);

    /* ret в task_switch прыгнет сюда */
    *--sp = (uint64_t)task_entry_wrapper;

    /* callee-saved, которые task_switch сделает pop */
    *--sp = 0;   /* r15 */
    *--sp = 0;   /* r14 */
    *--sp = 0;   /* r13 */
    *--sp = 0;   /* r12 */
    *--sp = 0;   /* rbx */
    *--sp = 0;   /* rbp */

    t->rsp = (uint64_t)sp;

    ready_add(t);
    return t->id;
}

void schedule(void) {
    if (!scheduling_enabled || !current || !ready_head)
        return;

    struct task *next = current->next;
    /* Ищем следующую READY (пропускаем DEAD/BLOCKED) */
    int guard = TASK_MAX + 1;
    while (next->state != TASK_READY && next->state != TASK_RUNNING && guard--) {
        next = next->next;
    }
    if (next == current || next->state == TASK_DEAD || next->state == TASK_UNUSED)
        return;

    struct task *old = current;
    if (old->state == TASK_RUNNING)
        old->state = TASK_READY;

    next->state = TASK_RUNNING;
    current = next;

    task_switch(&old->rsp, next->rsp);
    /* сюда возвращаемся уже в контексте old после следующего switch */
}

void task_yield(void) {
    schedule();
}

/* Вызывать в циклах: если таймер просил — переключиться */
void task_maybe_yield(void) {
    if (timer_need_resched())
        schedule();
}

void task_exit(void) {
    if (!current) for (;;) __asm__ volatile("cli; hlt");

    struct task *t = current;
    t->state = TASK_DEAD;

    /* Убираем из кольца */
    ready_remove(t);

    /* Освобождаем стек (нельзя free свой стек, пока на нём — отложим) */
    /* Простой вариант: стек утечёт до перезагрузки; позже — reaper */
    t->stack = 0;

    if (!ready_head) {
        /* некому выполнять — зависаем */
        for (;;) __asm__ volatile("cli; hlt");
    }

    /* Переключаемся на следующего, не сохраняя себя как READY */
    struct task *next = ready_head;
    while (next->state != TASK_READY && next->state != TASK_RUNNING)
        next = next->next;

    next->state = TASK_RUNNING;
    current = next;

    /* Старый rsp больше не нужен — switch без сохранения old */
    uint64_t dummy;
    task_switch(&dummy, next->rsp);

    for (;;) __asm__ volatile("cli; hlt");
}

struct task *task_current(void) { return current; }
uint64_t     task_count(void) {
    uint64_t n = 0;
    for (int i = 0; i < TASK_MAX; i++)
        if (tasks[i].state != TASK_UNUSED && tasks[i].state != TASK_DEAD)
            n++;
    return n;
}
