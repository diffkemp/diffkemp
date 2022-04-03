; Description
; Tests complex interactions between arbitrary values and arbitrary types.
;
; Diff:
; Found differences in functions called by wake_up_process
;
; __set_task_cpu differs:
;   Callstack (snapshots/linux-4.18.0-147.el8):
;   try_to_wake_up at kernel/sched/core.c:2141
;   set_task_cpu at kernel/sched/core.c:2057
;   __set_task_cpu at kernel/sched/core.c:1194
;
;   Callstack (snapshots/linux-4.18.0-193.el8):
;   try_to_wake_up at kernel/sched/core.c:2113
;   set_task_cpu at kernel/sched/core.c:2079
;   __set_task_cpu at kernel/sched/core.c:1228
;
;   Diff:
;   *************** static inline void __set_task_cpu(struct task_struct *p, unsigned int cpu)
;   *** 1452,1456 ***
;     #ifdef CONFIG_THREAD_INFO_IN_TASK
;   ! 	p->cpu = cpu;
;     #else
;   ! 	task_thread_info(p)->cpu = cpu;
;     #endif
;   --- 1491,1495 ---
;     #ifdef CONFIG_THREAD_INFO_IN_TASK
;   ! 	WRITE_ONCE(p->cpu, cpu);
;     #else
;   ! 	WRITE_ONCE(task_thread_info(p)->cpu, cpu);
;     #endif

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }
!2 = !{ !"pattern-start",
        !"arbitrary-constant", !"diffkemp.any.3" }
!3 = !{ !"arbitrary-constant", !"diffkemp.any.3" }
!4 = !{ !"arbitrary-constant", !"diffkemp.any.4" }
!5 = !{ !"pattern-start",
        !"arbitrary-constant", !"diffkemp.any.4" }

; Functions
declare void @diffkemp.output_mapping(...)
declare void @__write_once_size(...)

; Globals
@diffkemp.any.1 = constant i32 0
@diffkemp.any.2 = constant i32 0
@diffkemp.any.3 = constant i32 0
@diffkemp.any.4 = constant i32 0

; Structures and unions
%struct.diffkemp.any.1 = type { %struct.diffkemp.any.2 }
%struct.diffkemp.any.2 = type {}
%struct.diffkemp.any.3 = type { %struct.diffkemp.any.4 }
%struct.diffkemp.any.4 = type {}
%struct.diffkemp.any.5 = type { %struct.diffkemp.any.6* }
%struct.diffkemp.any.6 = type {}
%struct.diffkemp.any.7 = type {}

define %struct.diffkemp.any.2* @diffkemp.old.__set_task_cpu(%struct.diffkemp.any.1*, %struct.diffkemp.any.2) {
  %3 = getelementptr inbounds %struct.diffkemp.any.1, %struct.diffkemp.any.1* %0, i32 0, i32 0, !diffkemp.pattern !2
  store %struct.diffkemp.any.2 %1, %struct.diffkemp.any.2* %3
  ret %struct.diffkemp.any.2* %3, !diffkemp.pattern !1
}

define %struct.diffkemp.any.2* @diffkemp.new.__set_task_cpu(%struct.diffkemp.any.1*, %struct.diffkemp.any.2) {
  %3 = load i32, i32* @diffkemp.any.1
  %4 = alloca %struct.diffkemp.any.7
  %5 = bitcast %struct.diffkemp.any.7* %4 to i8*
  %6 = bitcast %struct.diffkemp.any.7* %4 to %struct.diffkemp.any.2*
  store %struct.diffkemp.any.2 %1, %struct.diffkemp.any.2* %6, !diffkemp.pattern !0
  %7 = getelementptr inbounds %struct.diffkemp.any.1, %struct.diffkemp.any.1* %0, i32 0, i32 0, !diffkemp.pattern !3
  %8 = bitcast %struct.diffkemp.any.2* %7 to i8*
  %9 = bitcast %struct.diffkemp.any.7* %4 to [1 x i8]*
  %10 = getelementptr inbounds [1 x i8], [1 x i8]* %9, i64 0, i64 0
  call void (...) @__write_once_size(i8* %8, i8* %10, i32 %3)
  ret %struct.diffkemp.any.2* %7, !diffkemp.pattern !1
}
