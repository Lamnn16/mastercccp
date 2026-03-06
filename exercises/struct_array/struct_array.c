/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  STRUCTS + ARRAYS — the embedded developer's bread and butter           ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * Topics covered (in order):
 *
 *  1. Array field inside a struct — layout, sizeof, offsetof
 *  2. 2D array inside a struct — row pointer, row-stepping iteration
 *  3. Pointer-to-array syntax for row traversal  uint8_t (*p)[WIDTH]
 *  4. Array of structs — struct DmaDescriptor descriptors[N]
 *  5. Pointer-to-array-of-struct — struct Foo (*p)[3]  (grouping)
 *  6. Flexible array member (C99) — run-time sized payload
 *  7. const-correct pointers — read-only access to struct arrays
 *  8. Packed structs — removing padding (embedded register maps)
 *
 * Build (MinGW):
 *   gcc -std=c17 -Wall -Wextra -o struct_array struct_array.c && struct_array
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h> /* offsetof */
#include <string.h> /* memset, memcpy */
#include <stdlib.h> /* malloc / free */

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1 — Array field inside a struct
 * ══════════════════════════════════════════════════════════════════════════
 *
 * A struct field can be any fixed-length array.
 * The array occupies contiguous bytes starting at the field's offset.
 *
 *  struct Packet {
 *      uint8_t  id;          offset 0
 *      uint8_t  payload[8];  offset 1  (no padding —  uint8_t needs align 1)
 *      uint16_t crc;         offset 9? NO — uint16_t needs align 2 → offset 10
 *      why? Because the compiler inserts 1 byte of padding after payload to align crc to a 2-byte boundary. So crc starts at offset 10, not 9. This is an example of how struct padding works to satisfy alignment requirements of the fields.
 *      but why not 4 bytes alignment for crc? Because uint16_t only requires 2-byte alignment, so the compiler only needs to insert 1 byte of padding to align crc to a 2-byte boundary. If crc were a uint32_t, then the compiler would need to insert 3 bytes of padding to align it to a 4-byte boundary, resulting in crc starting at offset 12 instead of 10.
 *      is the a fix number of bytes alignment or it depends on the previous data types?
 * It depends on the data type of the field. Each data type has its own alignment requirement, which determines how many bytes of padding the compiler needs to insert before that field to ensure it is properly aligned in memory. For example, uint8_t requires 1-byte alignment, uint16_t requires 2-byte alignment, and uint32_t requires 4-byte alignment. The compiler calculates the necessary padding based on the offsets of the previous fields and their alignment requirements to ensure that each field starts at an address that is a multiple of its alignment.
 *  };
 */
struct Packet
{
    uint8_t id;
    uint8_t payload[8];
    uint16_t crc;
};

static void section1_array_field(void)
{
    printf("══ 1. Array field inside a struct ══════════════════════════\n");

    struct Packet pkt = {
        .id = 0xA5,
        .payload = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
        .crc = 0xBEEF};
    // can we use typedef here? Yes, we could typedef struct Packet to a name like Packet_t
    // for convenience, but it's not strictly necessary in this context since we're only using the struct in one place. Using typedef can make the code cleaner and more concise when you need to declare multiple variables of that struct type, but for a single instance, it's optional.
    // EXAMPLE:
    typedef struct
    {
        uint8_t id = 0xA5;
        uint8_t payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        uint16_t crc = 0xBEEF;
        // are these assignments valid in a typedef struct? No, you cannot assign values to struct members directly within a typedef declaration. The typedef is only for defining the structure type, and it does not allow for initializing the members. You would need to create an instance of the struct and then assign values to its members separately, like this:
        // Packet_t pkt = { .id = 0xA5, .payload =
        // so how the industry doing? In practice, it's common to define the struct type with typedef and then create instances of that struct with initial values as needed. The typedef is used to simplify the syntax when declaring variables of that struct type, but the initialization of the struct members is typically done separately when you create an instance of the struct.
    } Packet_t;
    // SHOW me a typical practice of using typedef struct in embedded code? A common practice in embedded C is to use typedef struct to define a new type for a hardware peripheral or a data structure, and then create instances of that type to represent specific peripherals or data. For example, you might define a struct for a UART peripheral like this:
    typedef struct
    {
        volatile uint32_t *base_addr;
        uint32_t baud_rate;
        uint8_t data_bits;
        uint8_t stop_bits;
        uint8_t parity;
    } UART_t;
    // Then you could create an instance of this UART_t struct to represent a specific UART peripheral and initialize its members:
    UART_t uart1 = {
        .base_addr = (volatile uint32_t *)0x40011000, // example base address
        .baud_rate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = 0 // no parity
    }; // is there any practice to initialize the uart in embedded ? Yes, in embedded systems, it's common to initialize hardware peripherals like UARTs by creating an instance of the corresponding struct (like UART_t) and then calling an initialization function that configures the peripheral based on the values in the struct. For example, you might have a function like void UART_Init(UART_t *uart) that takes a pointer to a UART_t struct, reads its members, and writes the appropriate values to the hardware registers to set up the UART peripheral according to the specified configuration. This approach allows for a clean separation between the data structure that represents the peripheral's configuration and the code that performs the actual initialization of the hardware.
    // show me the code:
    void UART_Init(UART_t *uart)
    {
        // Example code to initialize the UART peripheral based on the configuration in the UART_t struct
        // This is just a placeholder and would need to be implemented according to the specific hardware registers and requirements of the UART peripheral
        // For example, you might write to the baud rate register, configure data bits, stop bits, and parity settings based on the values in the uart struct
        // volatile uint32_t *base = uart->base_addr;
        // *base = uart->baud_rate; // Set baud rate (example)
        // *base = uart->data_bits; // Set data bits (example)
        // *base = uart->stop_bits; // Set stop bits (example)
        // *base = uart->parity;    // Set parity (example)
        // assume that the base address of uart is 0x40011000, the baud rate register is at offset 0x00, data bits register at offset 0x04, stop bits register at offset 0x08, and parity register at offset 0x0C, the code might look like this:
        volatile uint32_t *base = uart->base_addr; // where will this base_addr declare? It would be declared in the UART_t struct as a member variable, and when you create an instance of UART_t (like uart1), you would initialize base_addr with the actual base address of the UART peripheral in memory. For example:
        UART_t uart1 = {
            .base_addr = (volatile uint32_t *)0x40011000, // example base address
            .baud_rate = 115200,
            .data_bits = 8,
            .stop_bits = 1,
            .parity = 0 // no parity
        };  // Then in the UART_Init function, you would use uart->base_addr to access the hardware registers of the UART peripheral and configure it according to the settings in the uart struct.
        *base = uart->baud_rate; // Set baud rate (example)
        *base = uart->data_bits; // Set data bits (example)
        *base = uart->stop_bits; // Set stop bits (example)
        *base = uart->parity;    // Set parity (example)

    }

    printf("  sizeof(struct Packet) = %zu bytes\n", sizeof(struct Packet));
    printf("  offsetof id      = %zu\n", offsetof(struct Packet, id));
    printf("  offsetof payload = %zu\n", offsetof(struct Packet, payload));
    printf("  offsetof crc     = %zu  (padding inserted before crc)\n",
           offsetof(struct Packet, crc));

    /* Accessing array field elements */
    printf("  payload bytes: ");
    for (size_t i = 0; i < sizeof(pkt.payload); i++)
    {
        printf("0x%02X ", pkt.payload[i]);
    }
    printf("\n");

    /* &pkt.payload[0]  is the same address as  pkt.payload
     * pkt.payload decays to  uint8_t *  pointing at element 0            */
    uint8_t *p = pkt.payload;
    printf("  pkt.payload == &pkt.payload[0] ? %s\n",
           (p == &pkt.payload[0]) ? "YES" : "NO");

    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2 — 2D array inside a struct (frame buffer)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * uint8_t pixels[ROWS][COLS]  is stored as ROWS×COLS contiguous bytes.
 * Row i starts at  &pixels[i][0]  = base + i*COLS.
 *
 * When you pass the struct by pointer, you need a pointer that steps one
 * *row* at a time — that is  uint8_t (*p_row)[COLS].
 */
#define FB_ROWS 4
#define FB_COLS 8

struct FrameBuffer
{
    uint8_t pixels[FB_ROWS][FB_COLS];
};
/*
Where to use 2D in embedded code?
2D arrays in embedded code are commonly used for representing data that has a natural two-dimensional structure, such as frame buffers for displays, sensor data grids, or lookup tables. For example, a frame buffer for an LCD display might be defined as a 2D array of pixels, where each pixel is represented by a struct containing color information. Similarly, sensor data from a grid of sensors (like a temperature sensor array) could be stored in a 2D array for easy access and processing. Additionally, lookup tables for functions like sine or cosine might be organized as 2D arrays to allow for efficient indexing based on input parameters. In these cases, using a 2D array helps to logically organize the data and can simplify the code when accessing elements based on their row and column indices.
Give me a typical example of using 2D array which is better than 1D array in embedded code which related to data buffer not the LCD

A typical example of using a 2D array in embedded code for a data buffer could be in the context of a sensor data grid, such as a temperature sensor array. For instance, if you have a grid of temperature sensors arranged in rows and columns, you could define a 2D array to store the temperature readings from each sensor. This would allow you to easily access the temperature data based on the sensor's position in the grid (row and column), rather than having to calculate the index in a 1D array manually. For example:
#define SENSOR_ROWS 4
#define SENSOR_COLS 4
struct SensorData {
    float temperature;
    // other sensor data fields
};  
struct SensorGrid {
    struct SensorData sensors[SENSOR_ROWS][SENSOR_COLS];
};
In this example, the SensorGrid struct contains a 2D array of SensorData structs, which allows you to access the temperature readings from each sensor using a simple row and column index, like grid.sensors[row][col].temperature. This is more intuitive and less error-prone than using a 1D array where you would need to calculate the index based on the row and column, such as grid.sensors[row * SENSOR_COLS + col].temperature. Using a 2D array in this case enhances code readability and maintainability when dealing with data that has a natural two-dimensional structure.


*/


/* Fills each pixel with its (row*COLS + col) index */
static void fb_fill(struct FrameBuffer *fb)
{
    /* fb->pixels decays to  uint8_t (*)[FB_COLS]  — pointer to row 0 */
    uint8_t (*p_row)[FB_COLS] = fb->pixels;

    for (size_t r = 0; r < FB_ROWS; r++)
    {
        for (size_t c = 0; c < FB_COLS; c++)
        {
            p_row[r][c] = (uint8_t)(r * FB_COLS + c);
            /*
             * p_row[r]    is  uint8_t[FB_COLS]  — the entire row r
             * p_row[r][c] is  uint8_t           — individual pixel
             * p_row + 1   advances by FB_COLS bytes (one full row)
             */
        }
    }
}

static void fb_print(const struct FrameBuffer *fb)
{
    /* const-correct: p_row is pointer to const row */
    const uint8_t (*p_row)[FB_COLS] = fb->pixels;
    for (size_t r = 0; r < FB_ROWS; r++)
    {
        printf("    row[%zu]: ", r);
        for (size_t c = 0; c < FB_COLS; c++)
        {
            printf("%3u", p_row[r][c]);
        }
        printf("\n");
    }
}

static void section2_2d_array_in_struct(void)
{
    printf("══ 2. 2D array inside a struct (frame buffer) ══════════════\n");

    struct FrameBuffer fb;
    fb_fill(&fb);
    fb_print(&fb);

    printf("\n  KEY — pointer-to-row arithmetic:\n");
    uint8_t (*p)[FB_COLS] = fb.pixels; /* points at row 0 */
    printf("    p     points to row 0, first byte = %u\n", (*p)[0]);
    printf("    (p+1) points to row 1, first byte = %u   (jumped %zu bytes)\n",
           (*(p + 1))[0], sizeof(*p));
    printf("    sizeof(*p) == FB_COLS == %zu\n", sizeof(*p));

    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3 — Pointer-to-array syntax deep dive
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  uint8_t  *p         pointer to uint8_t           steps 1 byte
 *  uint8_t (*p)[N]     pointer to uint8_t[N]        steps N bytes
 *  uint8_t (*p)[R][C]  pointer to uint8_t[R][C]     steps R*C bytes
 *
 * Reading the declaration inside-out (right-left rule):
 *   uint8_t (*p)[8]
 *     p  — is a pointer
 *     [8] — to an array of 8
 *     uint8_t — uint8_t elements
 *
 * Common confusion: uint8_t *p[8]  (NO parentheses)
 *   = array of 8 pointers to uint8_t  ← completely different!
 */
static void section3_pointer_to_array_syntax(void)
{
    printf("══ 3. Pointer-to-array syntax ══════════════════════════════\n");

    uint8_t grid[3][4] = {
        {0x11, 0x12, 0x13, 0x14},
        {0x21, 0x22, 0x23, 0x24},
        {0x31, 0x32, 0x33, 0x34},
    };

    /* ── a) Pointer-to-row: steps 4 bytes ── */
    uint8_t (*p_row)[4] = grid; /* grid decays to &grid[0], type uint8_t(*)[4] */
    printf("  uint8_t (*p_row)[4] = grid\n");
    printf("    p_row[0][0]=0x%02X  p_row[1][0]=0x%02X  p_row[2][0]=0x%02X\n",
           p_row[0][0], p_row[1][0], p_row[2][0]);
    printf("    sizeof(*p_row) = %zu  (stride = one row = 4 bytes)\n\n",
           sizeof(*p_row));

    /* ── b) Flat element pointer: steps 1 byte ── */
    uint8_t *p_elem = &grid[0][0]; /* or (uint8_t*)grid */
    printf("  uint8_t *p_elem = &grid[0][0]\n");
    printf("    p_elem[0]=0x%02X  p_elem[4]=0x%02X  p_elem[8]=0x%02X\n",
           p_elem[0], p_elem[4], p_elem[8]);
    printf("    (Accessing row starts by knowing stride manually)\n\n");

    /* ── c) Pointer-to-whole-grid: steps 3*4 = 12 bytes ── */
    uint8_t (*p_grid)[3][4] = &grid;
    printf("  uint8_t (*p_grid)[3][4] = &grid\n");
    printf("    (*p_grid)[1][2] = 0x%02X\n", (*p_grid)[1][2]);
    printf("    sizeof(*p_grid) = %zu  (stride = WHOLE grid)\n\n",
           sizeof(*p_grid));

    /* ── d) WRONG: array of pointers ── */
    /*  uint8_t *bad[4];   <-- This is array of 4 element-pointers, NOT row-pointer */
    printf("  NOTE: 'uint8_t *p[4]' = array-of-4-pointers,\n");
    printf("        'uint8_t (*p)[4]' = pointer-to-array-of-4\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4 — Array of structs (DMA descriptor table)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * struct DmaDescriptor descriptors[N]  stores N descriptors contiguously.
 * Each element is sizeof(struct DmaDescriptor) bytes.
 * ptr++ advances by exactly one descriptor.
 */
#define DMA_NUM_DESC 6

struct DmaDescriptor
{
    uint32_t src_addr;
    uint32_t dst_addr;
    uint32_t length;
    uint32_t control; /* flags: EN, INT_EN, CIRC, etc. */
};

#define DMA_CTRL_EN (1U << 0)
#define DMA_CTRL_INT_EN (1U << 1)
#define DMA_CTRL_CIRC (1U << 2)

static void dma_print_table(const struct DmaDescriptor *descs, size_t n)
{
    printf("  %-3s  %-12s  %-12s  %-8s  %-8s\n",
           "#", "src_addr", "dst_addr", "length", "ctrl");
    for (size_t i = 0; i < n; i++)
    {
        printf("  %-3zu  0x%08X    0x%08X    %-8u  0x%02X\n",
               i,
               descs[i].src_addr,
               descs[i].dst_addr,
               descs[i].length,
               descs[i].control);
    }
}

/* Process descriptors one-by-one via pointer arithmetic */
static uint32_t dma_total_bytes(const struct DmaDescriptor *descs, size_t n)
{
    uint32_t total = 0U;
    const struct DmaDescriptor *p = descs; /* pointer to first descriptor */
    for (size_t i = 0; i < n; i++)
    {
        total += p->length;
        p++; /* advances sizeof(struct DmaDescriptor) = 16 bytes */
    }
    return total;
}

static void section4_array_of_structs(void)
{
    printf("══ 4. Array of structs (DMA descriptor table) ══════════════\n");

    struct DmaDescriptor dma_table[DMA_NUM_DESC] = {
        {0x20000000, 0x40020000, 512, DMA_CTRL_EN | DMA_CTRL_INT_EN},
        {0x20000200, 0x40020000, 256, DMA_CTRL_EN},
        {0x20000300, 0x40020000, 128, DMA_CTRL_EN},
        {0x20002000, 0x20003000, 1024, DMA_CTRL_EN | DMA_CTRL_CIRC},
        {0x20003000, 0x20004000, 512, DMA_CTRL_EN},
        {0x20004000, 0x40013800, 64, DMA_CTRL_EN | DMA_CTRL_INT_EN},
    };

    printf("  sizeof(struct DmaDescriptor) = %zu bytes\n",
           sizeof(struct DmaDescriptor));
    printf("  sizeof(dma_table)            = %zu bytes  (%d × %zu)\n\n",
           sizeof(dma_table), DMA_NUM_DESC, sizeof(struct DmaDescriptor));

    dma_print_table(dma_table, DMA_NUM_DESC);
    printf("\n  Total DMA transfer bytes: %u\n\n",
           dma_total_bytes(dma_table, DMA_NUM_DESC));

    /* Pointer to first element vs. pointer to whole array */
    struct DmaDescriptor *p_desc = dma_table;                /* steps 16 bytes */
    struct DmaDescriptor(*p_all)[DMA_NUM_DESC] = &dma_table; /* steps 96 bytes */
    printf("  p_desc + 1 steps %zu bytes (one descriptor)\n", sizeof(*p_desc));
    printf("  p_all  + 1 steps %zu bytes (whole table)\n\n", sizeof(*p_all));
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5 — Pointer-to-array-of-struct (grouping descriptors)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * struct DmaDescriptor (*p)[3]  steps by sizeof(struct DmaDescriptor[3])
 * = 3 descriptors at a time.  Useful for "ping-pong" DMA buffers or
 * processing fixed-size batches from a larger flat table.
 */
static void process_in_groups(const struct DmaDescriptor *descs,
                              size_t n, size_t group_size)
{
    /* Cast flat pointer to pointer-to-group */
    const struct DmaDescriptor(*p_group)[3] =
        (const struct DmaDescriptor(*)[3])descs;

    size_t num_groups = n / group_size;
    printf("  Processing %zu descriptors in groups of %zu (%zu groups):\n",
           n, group_size, num_groups);

    for (size_t g = 0; g < num_groups; g++)
    {
        uint32_t group_total = 0U;
        for (size_t i = 0; i < group_size; i++)
        {
            group_total += p_group[g][i].length;
        }
        printf("    group[%zu]: first src=0x%08X, total_bytes=%u\n",
               g, p_group[g][0].src_addr, group_total);
        /*
         * p_group + 1  jumps sizeof(struct DmaDescriptor[3]) = 48 bytes
         * = exactly 3 descriptors — no manual offset arithmetic needed
         */
    }
}

static void section5_pointer_to_array_of_struct(void)
{
    printf("══ 5. Pointer-to-array-of-struct (batched processing) ══════\n");

    struct DmaDescriptor table[6] = {
        {0x20000000, 0x40020000, 100, DMA_CTRL_EN},
        {0x20000100, 0x40020000, 200, DMA_CTRL_EN},
        {0x20000200, 0x40020000, 300, DMA_CTRL_EN},
        {0x20001000, 0x40013800, 400, DMA_CTRL_EN},
        {0x20001400, 0x40013800, 500, DMA_CTRL_EN},
        {0x20001800, 0x40013800, 600, DMA_CTRL_EN},
    };

    process_in_groups(table, 6, 3);

    printf("\n  sizeof(struct DmaDescriptor[3]) = %zu bytes  (group stride)\n\n",
           sizeof(struct DmaDescriptor[3]));
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 6 — Flexible array member (C99)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * A struct can end with  type data[];  — its size is determined at runtime
 * via malloc.  Common in embedded message queues, protocol frames, RTOS TCBs.
 *
 *  struct Frame {
 *      uint8_t  id;
 *      uint16_t len;
 *      uint8_t  data[];   // <-- FAM: zero-sized in sizeof(), actual data follows
 *  };
 *
 *  Frame *f = malloc(sizeof(Frame) + payload_len);
 *  f->len = payload_len;
 *  memcpy(f->data, src, payload_len);
 */
struct Frame
{
    uint8_t id;
    uint16_t len;
    uint8_t data[]; /* Flexible Array Member */ 
    // why data[] is zero-sized? Because the flexible array member (FAM) is defined with an empty set of brackets (data[]), it does not contribute to the size of the struct when using sizeof(struct Frame). The actual size of the data array is determined at runtime when you allocate memory for the struct, allowing you to create a struct that can hold a variable amount of data. This is useful for cases where the amount of data is not known at compile time, such as when receiving messages of varying lengths or when implementing a dynamic buffer.
    // how to use it later?
    // To use a flexible array member, you typically allocate memory for the struct using malloc, specifying the size of the struct plus the size of the data you want to store in the flexible array. For example:
    /*
    uint8_t payload_len = 8;
    struct Frame *f = malloc(sizeof(struct Frame) + payload_len);
    if (f) {
        f->id = 0x42;
        f->len = payload_len;
        memcpy(f->data, src, payload_len); // Copy data into the flexible array
    }
        if i have to use static allocate?
    If you need to use static allocation for a struct with a flexible array member, you cannot directly declare an instance of the struct with a flexible array member because its size is not known at compile time. However, you can define a struct with a fixed-size array instead of a flexible array member if you know the maximum size of the data you will be storing. For example:
    #define MAX_PAYLOAD_LEN 256
    struct Frame {
    uint8_t id;
    uint16_t len;
    uint8_t data[MAX_PAYLOAD_LEN]; // Fixed-size array instead of flexible array member
        };   
    */
};

static void section6_flexible_array_member(void)
{
    printf("══ 6. Flexible array member (C99) ══════════════════════════\n");

    printf("  sizeof(struct Frame) = %zu  (does NOT include data[])\n",
           sizeof(struct Frame));

    /* Allocate a Frame with 8 bytes of payload */
    uint8_t payload_len = 8;
    struct Frame *f = malloc(sizeof(struct Frame) + payload_len);
    if (!f)
    {
        return;
    }

    f->id = 0x42;
    f->len = payload_len;
    for (uint8_t i = 0; i < payload_len; i++)
    {
        f->data[i] = (uint8_t)(0x10 + i);
    }

    printf("  Frame: id=0x%02X  len=%u  data=[ ", f->id, f->len);
    for (uint16_t i = 0; i < f->len; i++)
    {
        printf("0x%02X ", f->data[i]);
    }
    printf("]\n");
    printf("  Total heap allocation: %zu + %u = %zu bytes\n\n",
           sizeof(struct Frame), payload_len,
           sizeof(struct Frame) + payload_len);

    free(f);
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 7 — const-correct access to struct arrays
 * ══════════════════════════════════════════════════════════════════════════
 *
 * In embedded code, read-only tables (lookup tables, config tables) should
 * be declared const to:
 *   • Place them in FLASH instead of SRAM
 *   • Let the compiler/static-analyzer catch accidental writes
 *
 * const struct Reg  *p              read-only struct, pointer can advance
 * struct Reg * const p              mutable struct, pointer is pinned
 * const struct Reg * const p        both read-only
 */
struct RegEntry
{
    uint32_t addr;
    const char *name;
    uint32_t reset_val;
};

/* const table: lives in FLASH on STM32 */
static const struct RegEntry g_reg_table[] = {
    {0x40011000, "USART1_SR", 0x00C0},
    {0x40011004, "USART1_DR", 0x0000},
    {0x40011008, "USART1_BRR", 0x0000},
    {0x4001100C, "USART1_CR1", 0x0000},
};
#define REG_TABLE_SIZE (sizeof(g_reg_table) / sizeof(g_reg_table[0]))

/* Must take  const struct RegEntry *  to accept both const and non-const tables */
static void print_reg_table(const struct RegEntry *table, size_t n)
{
    printf("  %-12s  %-10s  %-8s\n", "addr", "name", "reset");
    for (size_t i = 0; i < n; i++)
    {
        printf("  0x%08X    %-10s  0x%04X\n",
               table[i].addr, table[i].name, table[i].reset_val);
    }
}

static void section7_const_struct_arrays(void)
{
    printf("══ 7. const-correct struct arrays (FLASH tables) ════════════\n");
    print_reg_table(g_reg_table, REG_TABLE_SIZE);

    /* const pointer to first entry, cannot write through it */
    const struct RegEntry *p = g_reg_table;
    printf("\n  Walking with const pointer:\n");
    for (size_t i = 0; i < REG_TABLE_SIZE; i++, p++)
    {
        printf("    [%zu] 0x%08X  %s\n", i, p->addr, p->name);
        /* p->reset_val = 0;  <-- compile error: read-only */
    }
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 8 — Packed structs (removing padding for register maps)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * By default the compiler inserts padding to satisfy alignment requirements.
 * In embedded code you sometimes need ZERO padding, e.g.:
 *   • When casting a raw byte buffer to a struct (protocol frames)
 *   • When mapping a struct onto a hardware register block
 *
 * GCC: __attribute__((packed))
 * MSVC: #pragma pack(push,1) ... #pragma pack(pop)
 *
 * WARNING: Packed structs can cause unaligned accesses on ARM Cortex-M,
 *          leading to a HardFault unless the CPU supports unaligned access
 *          (Cortex-M3/M4/M7 do, Cortex-M0 does NOT).
 */
struct NormalHeader
{
    uint8_t type;    /* 1 byte → 1 byte padding (align uint16_t to 2) */
    uint16_t length; /* 2 bytes */
    uint32_t seq;    /* 4 bytes */
    uint8_t flags;   /* 1 byte → 3 bytes padding (align uint32_t is done) */
}; /* likely 12 bytes with padding */

struct __attribute__((packed)) PackedHeader
{
    uint8_t type;    /* 1 */
    uint16_t length; /* 2 — unaligned on ARM! */
    uint32_t seq;    /* 4 — unaligned on ARM! */
    uint8_t flags;   /* 1 */
}; /* exactly 8 bytes */

static void section8_packed_structs(void)
{
    printf("══ 8. Packed structs — removing alignment padding ═══════════\n");

    printf("  sizeof(struct NormalHeader) = %zu bytes  (with padding)\n",
           sizeof(struct NormalHeader));
    printf("  sizeof(struct PackedHeader) = %zu bytes  (packed, no padding)\n",
           sizeof(struct PackedHeader));

    printf("\n  offsetof NormalHeader:  type=%zu  length=%zu  seq=%zu  flags=%zu\n",
           offsetof(struct NormalHeader, type),
           offsetof(struct NormalHeader, length),
           offsetof(struct NormalHeader, seq),
           offsetof(struct NormalHeader, flags));

    printf("  offsetof PackedHeader:  type=%zu  length=%zu  seq=%zu  flags=%zu\n\n",
           offsetof(struct PackedHeader, type),
           offsetof(struct PackedHeader, length),
           offsetof(struct PackedHeader, seq),
           offsetof(struct PackedHeader, flags));

    /* Simulate receiving raw bytes over UART and casting to struct */
    uint8_t raw_bytes[8] = {0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x07, 0x00};
    /*                      type  length(LE) seq(LE)              flags      */
    struct PackedHeader *hdr = (struct PackedHeader *)raw_bytes;
    printf("  Casting raw UART bytes to PackedHeader:\n");
    printf("    type=%u  length=%u  seq=%u  flags=%u\n",
           hdr->type, hdr->length, hdr->seq, hdr->flags);

    printf("\n  RULE: Only use __attribute__((packed)) when:\n");
    printf("    1. Receiving/sending wire-format byte streams\n");
    printf("    2. You are on Cortex-M3/M4/M7 (unaligned access supported)\n");
    printf("    3. You avoid passing packed-field *addresses* to functions\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Structs + Arrays — embedded patterns exercise              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    section1_array_field();
    section2_2d_array_in_struct();
    section3_pointer_to_array_syntax();
    section4_array_of_structs();
    section5_pointer_to_array_of_struct();
    section6_flexible_array_member();
    section7_const_struct_arrays();
    section8_packed_structs();

    printf("══════════════════════════════════════════════════════════════\n");
    printf("EXERCISES:\n\n");

    printf("  1. [FRAME BUFFER]\n");
    printf("     Add a function  uint32_t fb_checksum(const struct FrameBuffer *fb)\n");
    printf("     that sums every pixel using only a  uint8_t (*p)[FB_COLS]  row\n");
    printf("     pointer (no direct indexing with fb->pixels[r][c]).\n\n");

    printf("  2. [DMA TABLE MUTATION]\n");
    printf("     Write  dma_disable_all(struct DmaDescriptor *table, size_t n)\n");
    printf("     that clears DMA_CTRL_EN in every descriptor's control field.\n");
    printf("     Use pointer arithmetic (p++) instead of table[i].\n\n");

    printf("  3. [PACKED vs NORMAL]\n");
    printf("     Declare an array of 3 PackedHeader and 3 NormalHeader.\n");
    printf("     Print their total sizes and the byte addresses of each [i].seq.\n");
    printf("     What pattern do you notice about the address differences?\n\n");

    printf("  4. [FLEXIBLE ARRAY MEMBER — ARRAY OF FRAMES]\n");
    printf("     You CANNOT make  struct Frame frames[10]  when Frame has a FAM.\n");
    printf("     Instead, declare  struct Frame *frames[10]  and malloc each one\n");
    printf("     individually with a different payload size. Free all at the end.\n\n");

    printf("  5. [CONST TABLE BINARY SEARCH]\n");
    printf("     Add a 10-entry const RegEntry table sorted by .addr.\n");
    printf("     Implement  const struct RegEntry *reg_find(uint32_t addr)\n");
    printf("     using binary search with only  const struct RegEntry *  pointers.\n\n");

    return 0;
}
