#ifndef BLOB_H
#define BLOB_H

#include "common.h"
#include "history.h"
#include "updater.h"


#include <pthread.h>

enum blob_alloc {
    BLOB_MALLOC = 0,
    BLOB_MMAP,
};

struct blob_t {
    enum blob_alloc alloc;

    size_t len;
    byte *data;
    pthread_mutex_t mutex_data;

    char *filename;

    uint8_t *dirty;

    struct diff *undo, *redo;
    ssize_t saved_dist;

    struct {
        size_t len;
        byte *data;
    } clipboard;

};

extern struct blob_t blob;

void blob_init(struct blob_t *blob);

void blob_replace(struct blob_t *blob, size_t pos, byte const *data, size_t len, bool save_history, bool sendUpdate);

void blob_insert(struct blob_t *blob, size_t pos, byte const *data, size_t len, bool save_history, bool update);

void blob_delete(struct blob_t *blob, size_t pos, size_t len, bool save_history);

void blob_free(struct blob_t *blob);

bool blob_can_move(struct blob_t const *blob);

bool blob_undo(struct blob_t *blob, size_t *pos);

bool blob_redo(struct blob_t *blob, size_t *pos);

void blob_yank(struct blob_t *blob, size_t pos, size_t len);

size_t blob_paste(struct blob_t *blob, size_t pos, enum op_type type);

ssize_t blob_search(struct blob_t *blob, byte const *needle, size_t len, size_t start, ssize_t dir);

void blob_load(struct blob_t *blob, char const *filename);

enum blob_save_error {
    BLOB_SAVE_OK = 0,
    BLOB_SAVE_FILENAME,
    BLOB_SAVE_NONEXISTENT,
    BLOB_SAVE_PERMISSIONS,
    BLOB_SAVE_BUSY,
} blob_save(struct blob_t *blob, char const *filename);

bool blob_is_saved(struct blob_t const *blob);

static inline size_t blob_length(struct blob_t const *blob) { return blob->len; }

byte const *blob_lookup(struct blob_t const *blob, size_t pos, size_t *len);

static inline byte blob_at(struct blob_t const *blob, size_t pos) { return *blob_lookup(blob, pos, NULL); }

void blob_read_strict(struct blob_t *blob, size_t pos, byte *buf, size_t len);

#endif
