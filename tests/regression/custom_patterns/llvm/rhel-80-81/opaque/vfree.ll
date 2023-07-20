; Description
; Tests function call modification with parameter addition. This includes
; checking input instructions before the call.
;
; Diff:
; Found differences in functions called by vfree
;
; __vunmap differs:
;   Callstack (snapshots/linux-4.18.0-80.el8):
;   __vunmap at mm/vmalloc.c:1593
;
;   Callstack (snapshots/linux-4.18.0-147.el8):
;   __vunmap at mm/vmalloc.c:1658
;
;   Diff:
;   *************** static void __vunmap(const void *addr, int deallocate_pages)
;   *** 1517,1519 ***
;
;   ! 	remove_vm_area(addr);
;     	if (deallocate_pages) {
;   --- 1581,1584 ---
;
;   ! 	vm_remove_mappings(area, deallocate_pages);
;   ! 
;     	if (deallocate_pages) {

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }

; Structures
%struct.vmap_area = type { i1, i1, i1, i1, i1, i1, %struct.vm_struct* }
%struct.vm_struct = type {}

; Functions
declare ptr @find_vmap_area(...)
declare ptr @remove_vm_area(...)
declare void @vm_remove_mappings(...)

define void @diffkemp.old.__vunmap(ptr, i32) {
  %3 = ptrtoint ptr %0 to i64
  %4 = call ptr (...) @find_vmap_area(i64 %3)
  br label %aBB

aBB:
  %5 = call ptr (...) @remove_vm_area(ptr %0), !diffkemp.pattern !0
  br label %bBB, !diffkemp.pattern !1

bBB:
  ret void
}

define void @diffkemp.new.__vunmap(ptr, i32) {
  %3 = ptrtoint ptr %0 to i64
  %4 = call ptr (...) @find_vmap_area(i64 %3)
  %5 = getelementptr inbounds %struct.vmap_area, ptr %4, i32 0, i32 6
  %6 = load ptr, ptr %5, align 8
  br label %aBB

aBB:
  call void (...) @vm_remove_mappings(ptr %6, i32 %1), !diffkemp.pattern !0
  br label %bBB, !diffkemp.pattern !1

bBB:
  ret void
}
