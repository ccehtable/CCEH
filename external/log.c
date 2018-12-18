#include "external/log.h"

/*
Function: log_create() 
        Create a log;
*/
level_log* log_create(uint64_t log_length)
{
    level_log* log = (level_log*)malloc(sizeof(level_log));
    if (!log)
    {
        printf("Log creation fails: 1\n");
        exit(1);
    }

    log->entry = (log_entry*)malloc(log_length*sizeof(log_entry));
    if (!log->entry)
    {
        printf("Log creation fails: 2");
        exit(1);
    }

    log->log_length = log_length;
    log->current = 0;

    return log;
}

/*
Function: log_write() 
        Write a log entry;
*/
void log_write(level_log *log, Key_t& key, Value_t value)
{
    memcpy(&log->entry[log->current].key, &key, sizeof(Key_t));
    memcpy(&log->entry[log->current].value, &value, sizeof(Value_t));
    clflush((char*)&log->entry[log->current].key, sizeof(Key_t));
    clflush((char*)&log->entry[log->current].value, sizeof(Value_t));
    mfence();
    
    log->entry[log->current].flag = 1;
    clflush((char*)&log->entry[log->current].flag, sizeof(uint8_t));
    mfence();
}

/*
Function: log_clean() 
        Clean up a log entry;
*/
void log_clean(level_log *log)
{
    log->entry[log->current].flag = 0;
    clflush((char*)&log->entry[log->current].flag, sizeof(uint8_t));
    mfence();

    log->current ++;
    if(log->current == log->log_length)
        log->current = 0;
    clflush((char*)&log->current, sizeof(uint8_t));
    mfence();
}
