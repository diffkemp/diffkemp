; Description
; Tests function name matching using regular expressions.
;
; Diff:
; Found differences in functions called by wait_for_completion
;
; __wait_for_common differs:
;   Callstack (snapshots/linux-4.18.0-240.el8):
;   wait_for_common at kernel/sched/completion.c:136
;   __wait_for_common at kernel/sched/completion.c:115
;
;   Callstack (snapshots/linux-4.18.0-305.el8):
;   wait_for_common at kernel/sched/completion.c:138
;   __wait_for_common at kernel/sched/completion.c:117
;
;   Diff:
;   *************** __wait_for_common(struct completion *x,
;   *** 102,106 ***
;
;   ! 	spin_lock_irq(&x->wait.lock);
;     	timeout = do_wait_for_common(x, action, timeout, state);
;   ! 	spin_unlock_irq(&x->wait.lock);
;
;   --- 104,108 ---
;
;   ! 	raw_spin_lock_irq(&x->wait.lock);
;     	timeout = do_wait_for_common(x, action, timeout, state);
;   ! 	raw_spin_unlock_irq(&x->wait.lock);

; Metadata
!0 = !{ !"pattern-start",
        !"function-name-regex", !"_*spin_lock", !"diffkemp.regex.1",
        !"function-name-regex", !"_*spin_unlock", !"diffkemp.regex.2",
        !"function-name-regex", !"_*spin_lock_irq", !"diffkemp.regex.3",
        !"function-name-regex", !"_*spin_unlock_irq", !"diffkemp.regex.4" }
!1 = !{ !"pattern-start",
        !"function-name-regex", !"_*raw_spin_lock", !"diffkemp.regex.1",
        !"function-name-regex", !"_*raw_spin_unlock", !"diffkemp.regex.2",
        !"function-name-regex", !"_*raw_spin_lock_irq", !"diffkemp.regex.3",
        !"function-name-regex", !"_*raw_spin_unlock_irq", !"diffkemp.regex.4" }
!2 = !{ !"pattern-end" }
!3 = !{ !"pattern-start" }

; Functions
declare void @spin_lock(...)
declare void @_raw_spin_lock(...)

; Globals
@diffkemp.regex.1 = constant i32 0
@diffkemp.regex.2 = constant i32 0
@diffkemp.regex.3 = constant i32 0
@diffkemp.regex.4 = constant i32 0

; Structures
%struct.spinlock = type {}
%struct.raw_spinlock = type {}

define void @diffkemp.old.__wait_for_common(%struct.spinlock*) {
  call void (...) @spin_lock(%struct.spinlock* %0), !diffkemp.pattern !0
  ret void, !diffkemp.pattern !2
}

define void @diffkemp.new.__wait_for_common(%struct.raw_spinlock*) {
  call void (...) @_raw_spin_lock(%struct.raw_spinlock* %0), !diffkemp.pattern !1
  ret void, !diffkemp.pattern !2
}
