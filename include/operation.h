// Defines the core Operation struct with fields:
// (op_type, line_no, col_start, col_end, new_text, timestamp, user_id)
// Will help to serialize and de-serialize -> struct->byteArray->struct

#ifndef OPERATION_H
#define OPERATION_H
#include <time.h>
#define MAX_TEXT_LEN 1024

typedef struct {
    int user_id;
    char op_type;           // 'I' = insert, 'D' = delete, 'R' = replace
    int line_no;
    int col_start;
    int col_end;
    char old_text[MAX_TEXT_LEN];
    char new_text[MAX_TEXT_LEN];
    time_t timestamp;
} Operation;

#endif