; Description
; Tests branching capabilities and input instruction analysis.
;
; Diff:
; Found differences in functions called by prepare_to_wait_event
;
; prepare_to_wait_event differs:
;   Diff:
;   *************** long prepare_to_wait_event(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry, int state)
;   *** 264,266 ***
;     	spin_lock_irqsave(&wq_head->lock, flags);
;   ! 	if (unlikely(signal_pending_state(state, current))) {
;     		/*
;   --- 281,283 ---
;     	spin_lock_irqsave(&wq_head->lock, flags);
;   ! 	if (signal_pending_state(state, current)) {
;     		/*

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }

define void @diffkemp.old.prepare_to_wait_event(i32) {
  %2 = icmp ne i32 %0, 0
  %3 = zext i1 %2 to i32
  %4 = sext i32 %3 to i64
  %5 = icmp ne i64 %4, 0, !diffkemp.pattern !0
  br i1 %5, label %6, label %7
6:
  br label %8, !diffkemp.pattern !1
7:
  br label %8, !diffkemp.pattern !1
8:
  ret void
}

define void @diffkemp.new.prepare_to_wait_event(i32) {
  %2 = icmp ne i32 %0, 0
  br i1 %2, label %3, label %4, !diffkemp.pattern !0
3:
  br label %5, !diffkemp.pattern !1
4:
  br label %5, !diffkemp.pattern !1
5:
  ret void
}
