; Description
; Tests more complex conditional statement handling. The pattern uses the same
; control flow branching as the underlying code and has two parts corresponding
; to the two main differences in the included diff. These parts (i.e., the
; differing pairs of function calls) are separated by the instructions in
; between them. Additionally, the name-only structure comparison is tested
; as well (%struct.custom is compared type-wise, not by name).
;
; Diff:
; Found differences in functions called by bio_endio
;
; percpu_ref_put_many differs:
;   Callstack (snapshots/linux-4.18.0-193.el8):
;   bio_uninit at block/bio.c:1865
;   bio_disassociate_blkg at block/bio.c:248
;   blkg_put at block/bio.c:2060
;   percpu_ref_put at ./include/linux/blk-cgroup.h:537
;   percpu_ref_put_many at ./include/linux/percpu-refcount.h:301
;
;   Callstack (snapshots/linux-4.18.0-240.el8):
;   bio_uninit at block/bio.c:1924
;   bio_disassociate_blkg at block/bio.c:249
;   blkg_put at block/bio.c:2119
;   percpu_ref_put at ./include/linux/blk-cgroup.h:542
;   percpu_ref_put_many at ./include/linux/percpu-refcount.h:325
;
;   Diff:
;   *************** static inline void percpu_ref_put_many(struct percpu_ref *ref, unsigned long nr)
;   *** 279,281 ***
;
;   ! 	rcu_read_lock_sched();
;
;   --- 303,305 ---
;
;   ! 	rcu_read_lock();
;
;   *************** static inline void percpu_ref_put_many(struct percpu_ref *ref, unsigned long nr)
;   *** 286,288 ***
;
;   ! 	rcu_read_unlock_sched();
;     }
;   --- 310,312 ---
;
;   ! 	rcu_read_unlock();
;     }

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }
!2 = !{ !"disable-name-comparison" }

; Structures
%struct.custom = type { %struct.atomic64_t, i64, ptr, ptr, i8, %struct.callback_head }
%struct.atomic64_t = type {}
%struct.callback_head = type {}

; Functions
declare void @diffkemp.output_mapping(...)
declare void @diffkemp.old.rcu_read_lock_sched()
declare void @diffkemp.old.rcu_read_unlock_sched()
declare void @diffkemp.new.rcu_read_lock()
declare void @diffkemp.new.rcu_read_unlock()
declare zeroext i1 @__ref_is_percpu(ptr, ptr)
declare i32 @atomic_long_sub_and_test(i64, ptr)

define i1 @diffkemp.old.percpu_ref_put_many(ptr, i64) {
  %3 = alloca ptr, align 8
  call void @diffkemp.old.rcu_read_lock_sched(), !diffkemp.pattern !0
  %4 = call zeroext i1 @__ref_is_percpu(ptr %0, ptr %3), !diffkemp.pattern !2
  br i1 %4, label %aBB, label %bBB

aBB:
  br label %dBB

bBB:
  %5 = call i32 @atomic_long_sub_and_test(i64 %1, ptr %0)
  %6 = icmp ne i32 %5, 0
  %7 = zext i1 %6 to i32
  %8 = sext i32 %7 to i64
  %9 = icmp ne i64 %8, 0
  br i1 %9, label %cBB, label %dBB

cBB:
  br label %dBB

dBB:
  call void @diffkemp.old.rcu_read_unlock_sched()
  br label %eBB, !diffkemp.pattern !1

eBB:
  ret i1 %4
}

define i1 @diffkemp.new.percpu_ref_put_many(ptr, i64) {
  %3 = alloca ptr, align 8
  call void @diffkemp.new.rcu_read_lock(), !diffkemp.pattern !0
  %4 = call zeroext i1 @__ref_is_percpu(ptr %0, ptr %3), !diffkemp.pattern !2
  br i1 %4, label %aBB, label %bBB

aBB:
  br label %dBB

bBB:
  %5 = call i32 @atomic_long_sub_and_test(i64 %1, ptr %0)
  %6 = icmp ne i32 %5, 0
  %7 = zext i1 %6 to i32
  %8 = sext i32 %7 to i64
  %9 = icmp ne i64 %8, 0
  br i1 %9, label %cBB, label %dBB

cBB:
  br label %dBB

dBB:
  call void @diffkemp.new.rcu_read_unlock()
  br label %eBB, !diffkemp.pattern !1

eBB:
  ret i1 %4
}
