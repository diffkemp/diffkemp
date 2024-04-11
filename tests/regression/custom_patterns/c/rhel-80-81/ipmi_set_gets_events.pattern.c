void cleanup_srcu_struct();
void cleanup_srcu_struct_quiesced();

#define PATTERN_NAME free_user
#define PATTERN_ARGS struct srcu_struct *release_barrier

PATTERN_OLD { cleanup_srcu_struct(release_barrier); }

PATTERN_NEW { cleanup_srcu_struct_quiesced(release_barrier); }
