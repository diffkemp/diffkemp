; Description
; Tests that a value pattern can be correctly expressed as a standard
; instruction pattern if desired.
;
; Diff:
; Found differences in functions called by __alloc_pages_nodemask
;
; zone_allows_reclaim differs:
;   Diff:
;   *************** static bool zone_allows_reclaim(struct zone *local_zone, struct zone *zone)
;   *** 3265,3267 ***
;     	return node_distance(zone_to_nid(local_zone), zone_to_nid(zone)) <=
;   ! 				RECLAIM_DISTANCE;
;     }
;   --- 3260,3262 ---
;     	return node_distance(zone_to_nid(local_zone), zone_to_nid(zone)) <=
;   ! 				node_reclaim_distance;
;     }

; Metadata
!0 = !{ !"pattern-start" }
!1 = !{ !"pattern-end" }

; Functions
declare i32 @__node_distance(...)

; Globals
@diffkemp.new.node_reclaim_distance = external global i32

define i1 @diffkemp.old.change_reclaim_distance.1(i32, i32) {
  %3 = call i32 (...) @__node_distance(i32 %0, i32 %1)
  %4 = icmp sle i32 %3, 30, !diffkemp.pattern !0
  ret i1 %4, !diffkemp.pattern !1
}

define i1 @diffkemp.new.change_reclaim_distance.1(i32, i32) {
  %3 = call i32 (...) @__node_distance(i32 %0, i32 %1)
  %4 = load i32, i32* @diffkemp.new.node_reclaim_distance, !diffkemp.pattern !0
  %5 = icmp sle i32 %3, %4
  ret i1 %5, !diffkemp.pattern !1
}

; This pattern can be represented as a so-called value pattern as well (and is
; presented as such in the commented section below). Value patterns specify
; concrete values (in this case, an integer constant and a global variable),
; which should always be considered equal. Since value patterns are, generally,
; only syntactically simplified versions of their standard counterparts, they
; have been omitted in the paper for brevity.

; define i32 @diffkemp.old.change_reclaim_distance.2() {
;   ret i32 30
; }
;
; define i32* @diffkemp.new.change_reclaim_distance.2() {
;   ret i32* @diffkemp.new.node_reclaim_distance
; }
