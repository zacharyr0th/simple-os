#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stddef.h>

#define PIPE_SIZE 4096

typedef struct {
    uint8_t buffer[PIPE_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    int reader_closed;
    int writer_closed;
} pipe_t;

// Create a pipe and return file descriptors
int sys_pipe(int pipefd[2]);

// Internal pipe operations
pipe_t* pipe_create(void);
void pipe_destroy(pipe_t* pipe);
int pipe_read(pipe_t* pipe, void* buffer, size_t count);
int pipe_write(pipe_t* pipe, const void* buffer, size_t count);

#endif