# UROS - Micro Real-time Operating System

UROS is micro real-time operating system designed for embedded system.

## Environment

### Target Architecture

- ARM Cortex-M3 processor
- Stellaris LM3S6965 Evaluation Board (QEMU)

### Development Environment

- Ubuntu 12.04 LTS (GNU/Linux 3.2.0-23-generic x86_64)
- binutils 2.26
- gcc 4.6.3

## How To Run

On the described environment above, compile and run can easily be executed by `make` command.

```console
$ make
$ make run
```

Some predefined sample tasks run at the same time. QEMU is executed with the `-nographic` option and all outputs from UART is displayed on the standard output (console). QEMU can be stopped by pressing `C-a x`.

## Specification

This RTOS is begin developed to aim at being a minimal, simple and efficient kernel and the specification is based on OSEK/VDX.

### Task Management

#### activate_task(*task_id*)

The task *task_id* is moved from SUSPENDED state to READY state.

#### terminate_task()

The calling task is moved into SUSPENDED state and the internal resources which the calling task has owned is released.

### Interrupt Handling

N/A

### Resource Management

Resource management is essentially the same thing as mutex.

#### get_resource(*res_id*)

Resource *res_id* is allocated for the current task. Another task cannot get or release the resource until allocating task release it. If another task tries to get the resource, the task is enqueued into the wait list for this resource and moved to WAITING state.

#### release_resource(*res_id*)

Resource *res_id* is released by the current task. If the wait list for this resource is not empty, one of them are dequeued from the list and moved to READY state.

### Event Control

Event objects are assigned to each task and used to inform that events occured.

#### set_event(*task_id*, *event*)

Event object which the task *task_id* has is ORed with the given *event*. If the result is matched with an event that the task is waiting for, the state of the task is changed to READY state.

#### clear_event(*task_id*, *event*)

Event object which the task *task_id* has is cleared by the given *event*.

#### get_event(*task_id*)

Return the event which the task *task_id* has.

#### wait_event(*wait*)

The *wait* event is set to the calling task and the task is moved to WAIT state and waits until any *wait* events arrive. If the *wait* event occurs, the task is moved to READY state and *wait* is cleared automatically.

### Alerms

Alarm objects manage time expiration. If it expires, it activates a task, set event to a task or executes call back routine. Each alarm is tied up a counter object which counts system timer ticks.

#### get_alarm_base(*alarm_id*, *alarm_base*)

Return the alarm *alarm_id* information into *alarm_base*. *Alarm_base* contains *maxallowedvalue*, *ticksperbase*, *mincycle*.

* *maxallowedvalue* defines the maximum allowed counter value.
* *ticksperbase* defines the number of ticks required to reach a counter specific unit.
* *mincycle* defines the minimum allowed counter value for the cycle-parameter.

#### get_alarm(*alarm_id*)

Return the relative value in ticks since previous expiration occured.

#### set_rel_alarm(*alarm_id*, *increment*, *cycle*)

Set the relative alarm which expires after *increment* and every *cycle* count.

#### set_abs_alarm(*alarm_id*, *start*, *cycle*)

Set the absolute alarm which expires at *start* and every *cycle* count.

#### cancel_alarm(*alarm_id*)

Stop alarm *alarm_id*.
