#include <stdio.h>
#include <stdlib.h> /* malloc, exit */
#include <math.h>   /* pow */
#include <string.h> /* strlen */

u_int32_t *memory = NULL; /* 16 bit index-"pointer", 16 bit (2 byte) content */
const u_int32_t POINTER_MASK = 0xffff0000;
const int BLOCK_COUNT = 1000;

typedef struct file_data
{
    int filePtr;    /* Next block index in memory array */
    char *fileName;
} file_data;

file_data *files;
const int MAX_FILENAME_LENGTH = 10;
const int MIN_FILENAME_LENGTH = 1;
const int MAX_FILE_COUNT = 20;
int currentFileCount = 0;

const char *FILE_NOT_FOUND_ERROR = "Error: No file with the name %s could be found\n";
const char *MAX_FILENAME_LENGTH_ERROR = "Error: The filename '%s' is too long\n";
const char *MIN_FILENAME_LENGTH_ERROR = "Error: The filename '%s' is too short\n";
const char *FILE_EXISTS_ERROR = "Error: The file %s exists already\n";
const char *NO_MEM_FOR_FILE_ERROR = "Error: There is no free memory for the new file left\n";


/* Sets block at index to 0xf...f0...0 */
int set_memory_free(int memoryIndex)
{
    *(memory + memoryIndex) = POINTER_MASK;
}

/* Checks if memory at block index is free */
int is_memory_free(int memoryIndex)
{
    return *(memory + memoryIndex) == POINTER_MASK;
}

/* Gets the index of the next block stored in the current block */
int get_next_block_index(int currentBlockIndex)
{
    return *(memory + currentBlockIndex) >> (2 * 8);
}

/* Sets the index of the next block in the current block */
int set_next_block_index(int currentBlockIndex, int nextBlockIndex)
{
    int blockContent = *(memory + currentBlockIndex) & ~POINTER_MASK;
    *(memory + currentBlockIndex) = nextBlockIndex << (2 * 8) | blockContent;
}

/* Finds the first free memory block at or after index */
int find_first_free()
{
    for (int i = 0; i < BLOCK_COUNT; ++i)
    {
        if (is_memory_free(i))
        {
            return i;
        }
    }
    return -1;
}

/* Gets the index of the first block for a file */
int get_first_block_index_from_filename(char *filename)
{
    for (int i = 0; i < currentFileCount; ++i)
    {
        if (strncmp((files + i)->fileName, filename, MAX_FILENAME_LENGTH) == 0)
        {
            return (files + i)->filePtr;
        }
    }
    return -1;
}

/* Adds new file at first free block */
int add_new_file(char *filename)
{
    if (strlen(filename) > MAX_FILENAME_LENGTH)
    {
        error_and_exit(MAX_FILENAME_LENGTH_ERROR, filename);
    }
    if (strlen(filename) < MIN_FILENAME_LENGTH)
    {
        error_and_exit(MIN_FILENAME_LENGTH_ERROR, filename);
    }
    if (get_first_block_index_from_filename(filename) != -1) {
        error_and_exit(FILE_EXISTS_ERROR, filename);
    }

    int first_free_index = find_first_free();
    if (first_free_index == -1)
    {
        error_and_exit(NO_MEM_FOR_FILE_ERROR, NULL);
    }
    *(memory + first_free_index) += 1; /* To avoid another file be located here */

    (files + currentFileCount)->filePtr = first_free_index; /* !! BUFF ERR POSSIBLE !! */
    strncpy((files + currentFileCount)->fileName, filename, MAX_FILENAME_LENGTH + 1);
    ++currentFileCount;
}

int count_used_blocks_for_file(char *filename) {
    int blockIndex = get_first_block_index_from_filename(filename);
    int blockCount = 0;
    if (blockIndex == -1)
    {
        error_and_exit(FILE_NOT_FOUND_ERROR, filename);
    }

    while (blockIndex != 0xffff) {
        blockIndex = get_next_block_index(blockIndex);
        ++blockCount;
    }
    return blockCount;
}

int print_used_blocks_for_file(char *filename) {
    int blockIndex = get_first_block_index_from_filename(filename);
    if (blockIndex == -1)
    {
        error_and_exit(FILE_NOT_FOUND_ERROR, filename);
    }
    while (blockIndex != 0xffff) {
        printf("%d ", blockIndex);
        blockIndex = get_next_block_index(blockIndex);
    }
    printf("\n");
}

/* Assigns the file content to some blocks */
int set_file_content(char *filename, char *content)
{
    int blockIndex = get_first_block_index_from_filename(filename);
    if (blockIndex == -1)
    {
        error_and_exit(FILE_NOT_FOUND_ERROR, filename);
    }
    for (int i = 0; i < strlen(content) + 1; i += 2)
    {
        *(memory + blockIndex) = 0;
        for (int j = 0; j < 2; ++j)
        {
            /* \0 termination char reached -> don't set pointer */
            if (*(content + i + j) == '\0') {
                /* If zero terminator is first byte in new block 
                    -> set the rest to != 0 to avoid allocation by another file*/
                if (j == 0) {
                    *(memory + blockIndex) |= 0x1;
                }
                set_next_block_index(blockIndex, 0xffff);
                return 0;
            }
            *(memory + blockIndex) |= *(content + i + j) << ((1 - j) * 8);
        }

        int newBlockIndex = find_first_free();
        if (newBlockIndex == -1)
        {
            error_and_exit(NO_MEM_FOR_FILE_ERROR, NULL);
        }
        set_next_block_index(blockIndex, newBlockIndex);
        blockIndex = newBlockIndex;
    }
}

/* Gets the file content that is scattered over blocks */
char *read_file_content(char *filename) {
    char *fileContent = (char *)malloc(2 * count_used_blocks_for_file(filename) + 1);

    int blockIndex = get_first_block_index_from_filename(filename);
    if (blockIndex == -1)
    {
        error_and_exit(FILE_NOT_FOUND_ERROR, filename);
    }
    int buffPos = 0;
    while (blockIndex != 0xffff)
    {
        u_int32_t blockData = *(memory + blockIndex);
        for (int j = 0; j < 2; ++j)
        {
            /* \0 termination char reached -> don't set pointer */
            if (blockData & (0xff << ((1 - j) * 8)) == '\0') {
                return 0;
            }
            int dataToSet = blockData & (0xff << ((1 - j) * 8));
            dataToSet >>= ((1 - j) * 8);
            *(fileContent + buffPos) = dataToSet;
            ++buffPos;
        }
        blockIndex = get_next_block_index(blockIndex);
    }
    return fileContent;
}

int delete_file(char *filename) {
    int blockIndex = get_first_block_index_from_filename(filename);
    if (blockIndex == -1)
    {
        error_and_exit(FILE_NOT_FOUND_ERROR, filename);
    }

    /* Delete from file array */
    for (int i = 0; i < currentFileCount; ++i) {
        if (strncmp((files + i)->fileName, filename, MAX_FILENAME_LENGTH) == 0) {
            memset((files + i)->fileName, 0, MAX_FILENAME_LENGTH);
            (files + i)->filePtr = 0;
        }
    }

    /* Free used blocks */
    while (blockIndex != 0xffff)
    {
        int nextBlockIndex = get_next_block_index(blockIndex);
        *(memory + blockIndex) = POINTER_MASK;
        blockIndex = nextBlockIndex;
    }
}

/* Print the error message (optionally formatted for a string) and exit with code 1 */
int error_and_exit(char *errorMsgOrFormat, char *optionalParam) {
    if (optionalParam == NULL) {
        printf(errorMsgOrFormat);
    } else {
        printf(errorMsgOrFormat, optionalParam);
    }
    exit(1);
}

int finally_free() {
    free(memory);

    for (int i = 0; i < MAX_FILE_COUNT; ++i) {
        free((files + i)->fileName);
    }
    free(files);
}

int main()
{
    memory = (u_int32_t *)malloc(sizeof(u_int32_t) * BLOCK_COUNT); /* init memory */
    for (int i = 0; i < BLOCK_COUNT; ++i)
    {
        set_memory_free(i);
    }

    files = (file_data *)malloc(MAX_FILE_COUNT * sizeof(file_data));
    for (int i = 0; i < MAX_FILE_COUNT; ++i)
    {
        (files + i)->fileName = (char *)malloc(MAX_FILENAME_LENGTH + 1);
    }

    add_new_file("haha.h");
    set_file_content("haha.h", "rdrr");
    char *hahaFileContent = read_file_content("haha.h");
    printf("file content: %s\n", hahaFileContent);
    print_used_blocks_for_file("haha.h");
    printf("Block count: %d\n", count_used_blocks_for_file("haha.h"));
    free(hahaFileContent);

    add_new_file("muh.txt");
    set_file_content("muh.txt", "mi\nma\nmau");
    char *muhFileContent = read_file_content("muh.txt");
    printf("file content: %s\n", muhFileContent);
    print_used_blocks_for_file("muh.txt");
    printf("Block count: %d\n", count_used_blocks_for_file("muh.txt"));
    free(muhFileContent);

    add_new_file("third.c");
    set_file_content("third.c", "veryshort");
    char *thirdFileContent = read_file_content("third.c");
    printf("file content: %s\n", thirdFileContent);
    print_used_blocks_for_file("third.c");
    printf("Block count: %d\n", count_used_blocks_for_file("third.c"));
    free(thirdFileContent);

    delete_file("haha.h");

    add_new_file("lngfle.asm");
    set_file_content("lngfle.asm", "Hey there. Do you know RISC V ASM? addi t0, t0, 0x10");
    char *longFileContent = read_file_content("lngfle.asm");
    printf("file content: %s\n", longFileContent);
    print_used_blocks_for_file("lngfle.asm");
    printf("Block count: %d\n", count_used_blocks_for_file("lngfle.asm"));
    free(longFileContent);

    finally_free();
}