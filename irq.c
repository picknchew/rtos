#include "irq.h"

#include <stddef.h>
#include <stdint.h>

#include "event_task_queue.h"
#include "rpi.h"
#include "task.h"
#include "timer.h"
#include "uart.h"

#define GIC_BASE ((char *) 0xff840000)

#define GICD_BASE (GIC_BASE + 0x1000)
#define GICC_BASE (GIC_BASE + 0x2000)

#define GICD_BIT_OFFSET(irq_id) (irq_id % 32)
#define GICD_BYTE_OFFSET(irq_id) (irq_id % 4)

#define GICD_ISENABLER_N(irq_id) (irq_id / 32)
#define GICD_ISENABLER_BASE (GICD_BASE + 0x100)
#define GICD_ISENABLER(n) (*(volatile uint32_t *) (GICD_ISENABLER_BASE + (4 * n)))

#define GICD_ITARGETSR_N(irq_id) (irq_id / 4)
#define GICD_ITARGETSR_BASE (GICD_BASE + 0x800)
#define GICD_ITARGETSR(n) (*(volatile uint32_t *) (GICD_ITARGETSR_BASE + (4 * n)))

#define GICD_ICPENDR_N(irq_id) (irq_id / 32)
#define GICD_ICPENDR_BASE (GICD_BASE + 0x280)
#define GICD_ICPENDR(n) (*(volatile uint32_t *) (GICD_ICPENDR_BASE + (4 * n)))

#define GICD_ISPENDR_N(irq_id) (irq_id / 32)
#define GICD_ISPENDR_BASE (GICD_BASE + 0x200)
#define GICD_ISPENDR(n) (*(volatile uint32_t *) (GICD_ISPENDR_BASE + (4 * n)))

static const volatile uint32_t *GICC_IAR = (uint32_t *) (GICC_BASE + 0xC);
static const uint32_t GICC_IAR_IRQ_ID_MASK = 0x3FF;
static const uint32_t GICC_IAR_ACK_MASK = 0xFFF;

static volatile uint32_t *GICC_EOIR = (uint32_t *) (GICC_BASE + 0x10);

// return back to user mode
extern void kern_exit();

static struct EventBlockedTaskQueue event_blocked_queue;

static bool missed_irq[EVENT_MAX];

void irq_init() {
  event_blocked_task_queue_init(&event_blocked_queue);

  for (unsigned int i = 0; i < EVENT_MAX; ++i) {
    missed_irq[i] = false;
  }
}

void irq_enable(enum InterruptSource irq_id) {
  // configure to be edge-triggered

  // route interrupt to IRQ on CPU 0
  // 1 = 0b00000001
  // each byte of register represents configures different irq_id
  // each bit targets a different CPU, rightmost is CPU interface 0
  GICD_ITARGETSR(GICD_ITARGETSR_N(irq_id)) |= 1 << (GICD_BYTE_OFFSET(irq_id) * 8);

  // each bit targets enables different irq_id
  GICD_ISENABLER(GICD_ISENABLER_N(irq_id)) = 1 << GICD_BIT_OFFSET(irq_id);
}

void irq_await_event(enum Event event) {
  struct TaskDescriptor *task = task_get_current_task();

  // for marklin cts
  if (!missed_irq[event]) {
    task->status = TASK_EVENT_BLOCKED;
    event_blocked_task_queue_push(&event_blocked_queue, task, event);
  } else {
    missed_irq[event] = false;
    task->context.registers[0] = 0;
  }
}

void handle_irq() {
  uint32_t iar = *GICC_IAR;
  uint32_t irq_id = iar & GICC_IAR_IRQ_ID_MASK;
  int retval = 0;
  enum Event event = EVENT_UNKNOWN;

  switch (irq_id) {
    case IRQ_TIMER_C1:
      timer_tick();
      retval = timer_get_time();
      event = EVENT_TIMER;
      break;
    case IRQ_UART:
      event = uart_handle_irq();
      break;
    case IRQ_SPURIOUS:
      break;
    default:
      printf("irq_handler: unknown irq_id!\r\n");
  }

  if (irq_id != IRQ_SPURIOUS) {
    if (event != EVENT_IGNORE && event != EVENT_UART_CONSOLE_TX &&
        event_blocked_task_queue_size(&event_blocked_queue, event) == 0) {
      if (missed_irq[event]) {
        printf("irq_handler: missed interrupt %d\r\n", event);
        while (true) {}
      }

      missed_irq[event] = true;
    }
    // unblock tasks waiting for this event
    while (event != EVENT_IGNORE &&
           event_blocked_task_queue_size(&event_blocked_queue, event) > 0) {
      struct TaskDescriptor *task = event_blocked_task_queue_pop(&event_blocked_queue, event);

      // set return value for AwaitEvent syscall
      task->context.registers[0] = retval;

      // unblock task
      task_schedule(task);
    }

    *GICC_EOIR = iar;
  }

  task_yield_current_task();

  if (task_get_current_task() == NULL) {
    // no more tasks to run
    printf("irq_handler: no current task\r\n");
    for (;;) {}  // spin forever
  }

  // run task
  kern_exit();
}
