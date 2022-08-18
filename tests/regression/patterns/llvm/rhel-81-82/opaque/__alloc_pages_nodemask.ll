; Description
; Tests whether global values work as intended in value patterns. In the
; underlying code, the value of RECLAIM_DISTANCE was equal to 30.
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

; Globals
@diffkemp.new.node_reclaim_distance = external global i32

define i32 @diffkemp.old.zone_allows_reclaim() {
  ret i32 30
}

define ptr @diffkemp.new.zone_allows_reclaim() {
  ret ptr @diffkemp.new.node_reclaim_distance
}
