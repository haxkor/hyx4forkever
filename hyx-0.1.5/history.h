#ifndef HISTORY_H
#define HISTORY_H

struct blob_t;

enum op_type {
    REPLACE,
    INSERT,
    DELETE,
};

struct diff;

void history_init(struct diff **history);
void history_free(struct diff **history);
void history_save(struct diff **history, enum op_type type, struct blob_t *blob, size_t pos, size_t len);
bool history_step(struct diff **history, struct blob_t *blob, struct diff **target, size_t *pos);

#endif
