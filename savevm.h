#ifndef SAVEVM_H
#define SAVEVM_H
typedef struct CompatEntry {
    char idstr[256];
    int instance_id;
} CompatEntry;

typedef struct SaveStateEntry {
    QTAILQ_ENTRY(SaveStateEntry) entry;
    char idstr[256];
    int instance_id;
    int alias_id;
    int version_id;
    int section_id;
    SaveSetParamsHandler *set_params;
    SaveLiveStateHandler *save_live_state;
    SaveStateHandler *save_state;
    LoadStateHandler *load_state;
    const VMStateDescription *vmsd;
    void *opaque;
    CompatEntry *compat;
    int no_migrate;
    uint32_t *version_queue;
    unsigned long total_size;
} SaveStateEntry;
#endif
