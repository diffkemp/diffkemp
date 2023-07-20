; Description
; Tests more complex conditional statement handling. The pattern uses the same
; control flow branching as the underlying code.
;
; Diff:
; Found differences in functions called by down_read
;
; __down_read differs:
;   Callstack (snapshots/linux-4.18.0-147.el8):
;   __down_read at kernel/locking/rwsem.c:26
;
;   Callstack (snapshots/linux-4.18.0-193.el8):
;   __down_read at kernel/locking/rwsem.c:1505
;
;   Diff:
;   *************** static inline void __down_read(struct rw_semaphore *sem)
;   *** 175,181 ***
;   ! static inline void __down_read(struct rw_semaphore *sem)
;     {
;   ! 	if (unlikely(atomic_long_inc_return_acquire(&sem->count) <= 0)) {
;   ! 		rwsem_down_read_failed(sem);
;   ! 		DEBUG_RWSEMS_WARN_ON(!((unsigned long)sem->owner &
;   ! 					RWSEM_READER_OWNED), sem);
;     	} else {
;   --- 1349,1354 ---
;   ! inline void __down_read(struct rw_semaphore *sem)
;     {
;   ! 	if (!rwsem_read_trylock(sem)) {
;   ! 		rwsem_down_read_slowpath(sem, TASK_UNINTERRUPTIBLE);
;   ! 		DEBUG_RWSEMS_WARN_ON(!is_rwsem_reader_owned(sem), sem);
;     	} else {

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }

; Structures
%struct.rw_semaphore = type { %struct.atomic64_t, %struct.list_head, %struct.raw_spinlock, %struct.optimistic_spin_queue, ptr }
%struct.atomic64_t = type {}
%struct.list_head = type {}
%struct.raw_spinlock = type {}
%struct.optimistic_spin_queue = type {}
%struct.task_struct = type {}

; Functions
declare i64 @atomic_long_inc_return_acquire(...)
declare ptr @rwsem_down_read_failed(...)
declare void @rwsem_set_reader_owned(...)
declare i1 @rwsem_read_trylock(...)
declare ptr @rwsem_down_read_slowpath(...)

define void @diffkemp.old.__down_read(ptr) {
  %2 = call i64 (...) @atomic_long_inc_return_acquire(ptr %0), !diffkemp.pattern !0
  %3 = icmp sle i64 %2, 0
  %4 = zext i1 %3 to i32
  %5 = sext i32 %4 to i64
  %6 = icmp ne i64 %5, 0
  br i1 %6, label %aBB, label %bBB

aBB:
  %7 = call ptr (...) @rwsem_down_read_failed(ptr %0)
  br label %cBB, !diffkemp.pattern !1

bBB:
  call void (...) @rwsem_set_reader_owned(ptr %0)
  br label %cBB, !diffkemp.pattern !1

cBB:
  ret void
}

define void @diffkemp.new.__down_read(ptr) {
  %2 = call zeroext i1 (...) @rwsem_read_trylock(ptr %0), !diffkemp.pattern !0
  br i1 %2, label %bBB, label %aBB

aBB:
  %3 = call ptr(...) @rwsem_down_read_slowpath(ptr %0, i32 2)
  br label %cBB, !diffkemp.pattern !1

bBB:
  call void (...) @rwsem_set_reader_owned(ptr %0)
  br label %cBB, !diffkemp.pattern !1

cBB:
  ret void
}
