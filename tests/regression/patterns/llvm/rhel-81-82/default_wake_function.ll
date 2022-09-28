; Description
; Tests basic arbitraty type functionality, including arbitrary parameter types.
;
; Diff:
; Found differences in functions called by default_wake_function
;
; task_cpu differs:
;   Callstack (snapshots/linux-4.18.0-147.el8):
;   try_to_wake_up at kernel/sched/core.c:3735
;   task_cpu at kernel/sched/core.c:1987
;
;   Callstack (snapshots/linux-4.18.0-193.el8):
;   try_to_wake_up at kernel/sched/core.c:3707
;   task_cpu at kernel/sched/core.c:2009
;
;   Diff:
;   *************** static inline unsigned int task_cpu(const struct task_struct *p)
;   *** 1797,1801 ***
;     #ifdef CONFIG_THREAD_INFO_IN_TASK
;   ! 	return p->cpu;
;     #else
;   ! 	return task_thread_info(p)->cpu;
;     #endif
;   --- 1806,1810 ---
;     #ifdef CONFIG_THREAD_INFO_IN_TASK
;   ! 	return READ_ONCE(p->cpu);
;     #else
;   ! 	return READ_ONCE(task_thread_info(p)->cpu);
;     #endif

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }

; Functions
declare void @__read_once_size(...)

; Globals
@diffkemp.any.1 = constant i32 0

; Structures and unions
%struct.diffkemp.any.1 = type {}
%struct.diffkemp.any.2 = type {}

define %struct.diffkemp.any.1 @diffkemp.old.task_cpu(%struct.diffkemp.any.1*) {
  %2 = load %struct.diffkemp.any.1, %struct.diffkemp.any.1* %0, !diffkemp.pattern !0
  ret %struct.diffkemp.any.1 %2, !diffkemp.pattern !1
}

define %struct.diffkemp.any.1 @diffkemp.new.task_cpu(%struct.diffkemp.any.1*) {
  %2 = load i32, i32* @diffkemp.any.1
  %3 = alloca %struct.diffkemp.any.2
  %4 = bitcast %struct.diffkemp.any.2* %3 to i8*
  %5 = bitcast %struct.diffkemp.any.1* %0 to i8*
  %6 = bitcast %struct.diffkemp.any.2* %3 to [1 x i8]*
  %7 = getelementptr inbounds [1 x i8], [1 x i8]* %6, i64 0, i64 0
  call void (...) @__read_once_size(i8* %5, i8* %7, i32 %2), !diffkemp.pattern !0
  %8 = bitcast %struct.diffkemp.any.2* %3 to %struct.diffkemp.any.1*
  %9 = load %struct.diffkemp.any.1, %struct.diffkemp.any.1* %8
  ret %struct.diffkemp.any.1 %9, !diffkemp.pattern !1
}
