// Simple test program
void sys_write(const char* str, int len) {
    asm volatile(
        "mov $2, %%rax\n"    // SYS_WRITE
        "mov $1, %%rdi\n"    // stdout
        "mov %0, %%rsi\n"    // string
        "mov %1, %%rdx\n"    // length
        "int $0x80"
        :
        : "r"(str), "r"(len)
        : "memory"
    );
}

void sys_exit(int code) {
    asm volatile(
        "mov $1, %%rax\n"    // SYS_EXIT
        "mov %0, %%rdi\n"    // exit code
        "int $0x80"
        :
        : "r"(code)
        : "memory"
    );
}

void _start() {
    const char* msg = "Hello from ELF!\n";
    sys_write(msg, 16);
    sys_exit(0);
}