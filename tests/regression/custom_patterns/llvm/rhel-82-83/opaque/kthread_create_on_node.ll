; Description
; Tests more complex input handling (including bitcasts). Additionally, tests
; that output mapping can be defined using the output_mapping function.
;
; Diff:
; Found differences in functions called by kthread_create_on_node
;
; __kthread_create_on_node differs:
;   Callstack (snapshots/linux-4.18.0-193.el8):
;   __kthread_create_on_node at kernel/kthread.c:380
;
;   Callstack (snapshots/linux-4.18.0-240.el8):
;   __kthread_create_on_node at kernel/kthread.c:414
;
;   Diff:
;   *************** struct task_struct *__kthread_create_on_node(int (*threadfn)(void *data),
;   *** 341,343 ***
;     		sched_setscheduler_nocheck(task, SCHED_NORMAL, &param);
;   ! 		set_cpus_allowed_ptr(task, cpu_all_mask);
;     	}
;   --- 374,377 ---
;     		sched_setscheduler_nocheck(task, SCHED_NORMAL, &param);
;   ! 		set_cpus_allowed_ptr(task,
;   ! 				     housekeeping_cpumask(HK_FLAG_KTHREAD));
;     	}

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }

; Structures
%struct.kthread_create_info = type { ptr, ptr, i32, ptr, ptr, %struct.list_head }
%struct.task_struct = type {}
%struct.completion = type {}
%struct.list_head = type {}
%struct.cpumask = type {}

; Globals
@diffkemp.old.cpu_all_bits = external constant [128 x i64], align 16

; Functions
declare void @diffkemp.output_mapping(...)
declare ptr @diffkemp.new.housekeeping_cpumask(i32)
declare ptr @kmalloc(...)
declare i32 @set_cpus_allowed_ptr(...)

define void @diffkemp.old.__kthread_create_on_node() {
  %1 = call ptr (...) @kmalloc(i64 56, i32 6291648)
  %2 = getelementptr inbounds %struct.kthread_create_info, ptr %1, i32 0, i32 3
  %3 = load ptr, ptr %2, align 8
  %4 = call i32 (...) @set_cpus_allowed_ptr(ptr %3, ptr @diffkemp.old.cpu_all_bits), !diffkemp.pattern !0
  br label %aBB, !diffkemp.pattern !1

aBB:
  call void (...) @diffkemp.output_mapping(i32 %4)
  ret void
}

define void @diffkemp.new.__kthread_create_on_node() {
  %1 = call ptr (...) @kmalloc(i64 56, i32 6291648)
  %2 = getelementptr inbounds %struct.kthread_create_info, ptr %1, i32 0, i32 3
  %3 = load ptr, ptr %2, align 8
  %4 = call ptr @diffkemp.new.housekeeping_cpumask(i32 256), !diffkemp.pattern !0
  %5 = call i32 (...) @set_cpus_allowed_ptr(ptr %3, ptr %4)
  br label %aBB, !diffkemp.pattern !1

aBB:
  call void (...) @diffkemp.output_mapping(i32 %5)
  ret void
}
