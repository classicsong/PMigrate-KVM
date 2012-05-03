/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/mman.h>
#endif
#include "config.h"
#include "monitor.h"
#include "sysemu.h"
#include "arch_init.h"
#include "audio/audio.h"
#include "hw/pc.h"
#include "hw/pci.h"
#include "hw/audiodev.h"
#include "kvm.h"
#include "migration.h"
#include "net.h"
#include "gdbstub.h"
#include "hw/smbios.h"

//classicsong
#include "migr-vqueue.h"

#ifdef TARGET_SPARC
int graphic_width = 1024;
int graphic_height = 768;
int graphic_depth = 8;
#else
int graphic_width = 800;
int graphic_height = 600;
int graphic_depth = 15;
#endif

const char arch_config_name[] = CONFIG_QEMU_CONFDIR "/target-" TARGET_ARCH ".conf";
extern __thread Byte *decomped_buf;

#if defined(TARGET_ALPHA)
#define QEMU_ARCH QEMU_ARCH_ALPHA
#elif defined(TARGET_ARM)
#define QEMU_ARCH QEMU_ARCH_ARM
#elif defined(TARGET_CRIS)
#define QEMU_ARCH QEMU_ARCH_CRIS
#elif defined(TARGET_I386)
#define QEMU_ARCH QEMU_ARCH_I386
#elif defined(TARGET_M68K)
#define QEMU_ARCH QEMU_ARCH_M68K
#elif defined(TARGET_MICROBLAZE)
#define QEMU_ARCH QEMU_ARCH_MICROBLAZE
#elif defined(TARGET_MIPS)
#define QEMU_ARCH QEMU_ARCH_MIPS
#elif defined(TARGET_PPC)
#define QEMU_ARCH QEMU_ARCH_PPC
#elif defined(TARGET_S390X)
#define QEMU_ARCH QEMU_ARCH_S390X
#elif defined(TARGET_SH4)
#define QEMU_ARCH QEMU_ARCH_SH4
#elif defined(TARGET_SPARC)
#define QEMU_ARCH QEMU_ARCH_SPARC
#endif

const uint32_t arch_type = QEMU_ARCH;

/***********************************************************/
/* ram save/restore */

#define RAM_SAVE_FLAG_FULL     0x01 /* Obsolete, not used anymore */
#define RAM_SAVE_FLAG_COMPRESS 0x02
#define RAM_SAVE_FLAG_MEM_SIZE 0x04
#define RAM_SAVE_FLAG_PAGE     0x08
#define RAM_SAVE_FLAG_EOS      0x10
#define RAM_SAVE_FLAG_CONTINUE 0x20

/* classicsong
 * mem_vnum
 */
#define MEM_VNUM_OFFSET        6
#define MEM_VNUM_MASK          (0x3f << MEM_VNUM_OFFSET)

#define DEBUG_BLK_ARCH_INIT

#ifdef DEBUG_BLK_ARCH_INIT
#define DPRINTF(fmt, ...) \
    do { printf("arch_init: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

static int is_dup_page(uint8_t *page, uint8_t ch)
{
    uint32_t val = ch << 24 | ch << 16 | ch << 8 | ch;
    uint32_t *array = (uint32_t *)page;
    int i;

    for (i = 0; i < (TARGET_PAGE_SIZE / 4); i++) {
        if (array[i] != val) {
            return 0;
        }
    }

    return 1;
}

static RAMBlock *last_block;
static ram_addr_t last_offset;

static int ram_save_block(QEMUFile *f)
{
    RAMBlock *block = last_block;
    ram_addr_t offset = last_offset;
    ram_addr_t current_addr;
    int bytes_sent = 0;

    if (!block)
        block = QLIST_FIRST(&ram_list.blocks);

    current_addr = block->offset + offset;

    do {
//        DPRINTF("->cpu_physical_memory_get_dirty\n");
        if (cpu_physical_memory_get_dirty(current_addr, MIGRATION_DIRTY_FLAG)) {
            uint8_t *p;
            int cont = (block == last_block) ? RAM_SAVE_FLAG_CONTINUE : 0;

//            DPRINTF("<-cpu_physical_memory_get_dirty\n");
            cpu_physical_memory_reset_dirty(current_addr,
                                            current_addr + TARGET_PAGE_SIZE,
                                            MIGRATION_DIRTY_FLAG);

//            DPRINTF("<-cpu_physical_memory_reset_dirty\n");
            p = block->host + offset;

            if (is_dup_page(p, *p)) {
                qemu_put_be64(f, offset | cont | RAM_SAVE_FLAG_COMPRESS);
                if (!cont) {
                    qemu_put_byte(f, strlen(block->idstr));
                    qemu_put_buffer(f, (uint8_t *)block->idstr,
                                    strlen(block->idstr));
                }
                qemu_put_byte(f, *p);
                bytes_sent = 1;
            } else {
                qemu_put_be64(f, offset | cont | RAM_SAVE_FLAG_PAGE);
                if (!cont) {
                    qemu_put_byte(f, strlen(block->idstr));
                    qemu_put_buffer(f, (uint8_t *)block->idstr,
                                    strlen(block->idstr));
                }
                qemu_put_buffer(f, p, TARGET_PAGE_SIZE);
                bytes_sent = TARGET_PAGE_SIZE;
            }

            break;
        }

        offset += TARGET_PAGE_SIZE;
        if (offset >= block->length) {
            offset = 0;
            block = QLIST_NEXT(block, next);
            if (!block)
                block = QLIST_FIRST(&ram_list.blocks);
        }

        current_addr = block->offset + offset;

    } while (current_addr != last_block->offset + last_offset);

    last_block = block;
    last_offset = offset;

    return bytes_sent;
}

static uint64_t bytes_transferred;

static ram_addr_t ram_save_remaining(void)
{
    RAMBlock *block;
    ram_addr_t count = 0;

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        ram_addr_t addr;
        for (addr = block->offset; addr < block->offset + block->length;
             addr += TARGET_PAGE_SIZE) {
//            DPRINTF("->cpu_physical_memory_get_dirty@ram_save_remaining\n");
            if (cpu_physical_memory_get_dirty(addr, MIGRATION_DIRTY_FLAG)) {
//                DPRINTF("<-cpu_physical_memory_get_dirty@ram_save_remaining\n");
                count++;
            }
        }
    }

    return count;
}

uint64_t ram_bytes_remaining(void)
{
    return ram_save_remaining() * TARGET_PAGE_SIZE;
}

uint64_t ram_bytes_transferred(void)
{
    return bytes_transferred;
}

uint64_t ram_bytes_total(void)
{
    RAMBlock *block;
    uint64_t total = 0;

    QLIST_FOREACH(block, &ram_list.blocks, next)
        total += block->length;

    return total;
}

static int block_compar(const void *a, const void *b)
{
    RAMBlock * const *ablock = a;
    RAMBlock * const *bblock = b;
    if ((*ablock)->offset < (*bblock)->offset) {
        return -1;
    } else if ((*ablock)->offset > (*bblock)->offset) {
        return 1;
    }
    return 0;
}

static void sort_ram_list(void)
{
    RAMBlock *block, *nblock, **blocks;
    int n;

    n = 0;
    QLIST_FOREACH(block, &ram_list.blocks, next) {
        ++n;
    }
    blocks = qemu_malloc(n * sizeof *blocks);
    n = 0;
    QLIST_FOREACH_SAFE(block, &ram_list.blocks, next, nblock) {
        blocks[n++] = block;
        QLIST_REMOVE(block, next);
    }
    qsort(blocks, n, sizeof *blocks, block_compar);
    while (--n >= 0) {
        QLIST_INSERT_HEAD(&ram_list.blocks, blocks[n], next);
    }
    qemu_free(blocks);
}


unsigned long ram_save_block_slave(ram_addr_t offset, uint8_t *p, void *block_p,
                         struct FdMigrationStateSlave *s, int mem_vnum);
unsigned long
ram_putbuf_block_slave(ram_addr_t offset, uint8_t *p, void *block_p, 
                     Byte *f, int mem_vnum, int *actual_size); 

unsigned long
ram_putbuf_block_slave(ram_addr_t offset, uint8_t *p, void *block_p, 
                     Byte *f, int  mem_vnum, int *actual_size) {
    Byte *oldptr = f;
    unsigned long len;
    RAMBlock *block = (RAMBlock *)block_p;

    if (is_dup_page(p, *p)) {
        len = buf_put_be64(f, offset | (block == NULL ? RAM_SAVE_FLAG_CONTINUE : 0) | 
                      RAM_SAVE_FLAG_COMPRESS | (mem_vnum << MEM_VNUM_OFFSET));
        f = &f[len];
        if (block) {
            len = buf_put_byte(f, strlen(block->idstr));
            f = &f[len];
            len = strlen(block->idstr);
            memcpy(f, (uint8_t *)block->idstr, len);
        }
        len = buf_put_byte(f, *p);
        f = &f[len];
        actual_size = 1;
        return &f[0] - &oldptr[0];
    } else {
        len = buf_put_be64(f, offset | (block == NULL ? RAM_SAVE_FLAG_CONTINUE : 0) | RAM_SAVE_FLAG_PAGE | (mem_vnum << MEM_VNUM_OFFSET));
         f = &f[len];
        if (block) {
            len = buf_put_byte(f, strlen(block->idstr));
            f = &f[len];
            len = strlen(block->idstr);
            memcpy(f, (uint8_t *)block->idstr, len);
            f = &f[len];
        }
        memcpy(f, p, TARGET_PAGE_SIZE);
        f = &f[TARGET_PAGE_SIZE];
        actual_size =  TARGET_PAGE_SIZE;
        return &f[0] - &oldptr[0];
    }
}

//classicsong
unsigned long
ram_save_block_slave(ram_addr_t offset, uint8_t *p, void *block_p, 
                     struct FdMigrationStateSlave *s, int mem_vnum) {
    RAMBlock *block = (RAMBlock *)block_p;
    QEMUFile *f = s->file;

    if (is_dup_page(p, *p)) {
        qemu_put_be64(f, offset | (block == NULL ? RAM_SAVE_FLAG_CONTINUE : 0) | 
                      RAM_SAVE_FLAG_COMPRESS | (mem_vnum << MEM_VNUM_OFFSET));
        if (block) {
            qemu_put_byte(f, strlen(block->idstr));
            qemu_put_buffer(f, (uint8_t *)block->idstr,
                            strlen(block->idstr));
        }
        qemu_put_byte(f, *p);

        return 1;
    } else {
        qemu_put_be64(f, offset | (block == NULL ? RAM_SAVE_FLAG_CONTINUE : 0) | RAM_SAVE_FLAG_PAGE | (mem_vnum << MEM_VNUM_OFFSET));
        if (block) {
            qemu_put_byte(f, strlen(block->idstr));
            qemu_put_buffer(f, (uint8_t *)block->idstr,
                            strlen(block->idstr));
        }
        qemu_put_buffer(f, p, TARGET_PAGE_SIZE);

        return TARGET_PAGE_SIZE;
    }
}

static unsigned long
ram_save_block_master(struct migration_task_queue *task_queue) {
    RAMBlock *block = QLIST_FIRST(&ram_list.blocks);
    RAMBlock *last_block = NULL;
    ram_addr_t offset = 0;
    ram_addr_t current_addr;
    unsigned long bytes_sent = 0;
    struct task_body *body = NULL;
    int body_len = 0;

    current_addr = block->offset + offset;

    //DPRINTF("Start ram_save_block_master %d\n", DEFAULT_MEM_BATCH_LEN);
    do {
        if (cpu_physical_memory_get_dirty(current_addr, MIGRATION_DIRTY_FLAG)) {
            uint8_t *p;
            int cont;

            if (block == last_block)
                cont = RAM_SAVE_FLAG_CONTINUE;
            else {
                last_block = block;
                cont = 0;
            }

            //DPRINTF("reset dirty %lx, %lx\n", current_addr, block->offset);
            cpu_physical_memory_reset_dirty(current_addr,
                                            current_addr + TARGET_PAGE_SIZE,
                                            MIGRATION_DIRTY_FLAG);
            p = block->host + offset;

            /*
             * batch and add new task
             */
            if (body_len == 0) {
                body = (struct task_body *)malloc(sizeof(struct task_body));
                body->type = TASK_TYPE_MEM;
                body->iter_num = task_queue->iter_num;
            }

            if ((offset & 0xfff) != 0) 
                fprintf(stderr, "error offset %lx, %p\n", offset, p);

            body->pages[body_len].ptr = p;
            body->pages[body_len].block = (cont == 0 ? block : NULL);
            body->pages[body_len].addr = offset;
            body_len ++;

            if (body_len == DEFAULT_MEM_BATCH_LEN) {
                body->len = body_len;

                //finish one batch, for next batch, the RAM_SAVE_FLAG_CONTINUE should not be set
                last_block = NULL;
                body_len = 0;

                if (queue_push_task(task_queue, body) < 0)
                    fprintf(stderr, "Enqueue task error\n");
            }
        }

        offset += TARGET_PAGE_SIZE;
         
        if (offset >= block->length) {
            offset = 0;
            block = QLIST_NEXT(block, next);
            //hit the iteration end
            if (!block) {
                DPRINTF("Hit memory iteration end\n");
                break;
            }
        }

        current_addr = block->offset + offset;
    } while (1);

    /*
     * handle last task this iteration
     */
    if (body_len > 0) {
        body->len = body_len;

        if (queue_push_task(task_queue, body) < 0)
            fprintf(stderr, "Enqueue task error\n");
    }

    return bytes_sent;
}

unsigned long ram_save_iter(int stage, struct migration_task_queue *task_queue, QEMUFile *f);
extern void create_host_memory_master(void *opaque);

//classicsong
unsigned long
ram_save_iter(int stage, struct migration_task_queue *task_queue, QEMUFile *f) {
    unsigned long bytes_transferred = 0;

    if (stage == 3) {
        /* flush all remaining blocks regardless of rate limiting */ 
        bytes_transferred = ram_save_block_master(task_queue);
        DPRINTF("Total memory sent last iter %lx\n", bytes_transferred);
        cpu_physical_memory_set_dirty_tracking(0);
    } else {
        /* try transferring iterative blocks of memory */
        bytes_transferred = ram_save_block_master(task_queue);
    }

    return bytes_transferred;
}

int ram_save_live(Monitor *mon, QEMUFile *f, int stage, void *opaque) //opaque is FdMigrationState
{
    ram_addr_t addr;

    if (stage < 0) {
        cpu_physical_memory_set_dirty_tracking(0);
        return 0;
    }
    
    /*
     * if stage == 1, we do not transfer memory
     */
    if (stage == 1) {
        RAMBlock *block;

        /*
         * Get dirty bitmap first
         * And start dirty tracking
         */
        if (cpu_physical_sync_dirty_bitmap(0, TARGET_PHYS_ADDR_MAX) != 0) {
            fprintf(stderr, "get dirty bitmap error\n");
            qemu_file_set_error(f);
            return -1;
        }

        bytes_transferred = 0;
        last_block = NULL;
        last_offset = 0;
        sort_ram_list();

        /* Make sure all dirty bits are set */
        QLIST_FOREACH(block, &ram_list.blocks, next) {
            for (addr = block->offset; addr < block->offset + block->length;
                 addr += TARGET_PAGE_SIZE) {
                if (!cpu_physical_memory_get_dirty(addr,
                                                   MIGRATION_DIRTY_FLAG)) {
                    cpu_physical_memory_set_dirty(addr);
                }
            }
        }

        /* Enable dirty memory tracking */
        cpu_physical_memory_set_dirty_tracking(1);

        qemu_put_be64(f, ram_bytes_total() | RAM_SAVE_FLAG_MEM_SIZE);

        QLIST_FOREACH(block, &ram_list.blocks, next) {
            DPRINTF("Put mem block %s, size %lx[%lx]\n", block->idstr, block->offset, block->length);
            qemu_put_byte(f, strlen(block->idstr));
            qemu_put_buffer(f, (uint8_t *)block->idstr, strlen(block->idstr));
            qemu_put_be64(f, block->length);
        }

        /*
         * classicsong
         * step 1 only init the memory in the dest
         * tell the end of initialization
         */
        qemu_put_be64(f, RAM_SAVE_FLAG_EOS);
        qemu_fflush(f);

        DPRINTF("Finish memory negotiation start memory master, total memory %lx\n", 
                ram_bytes_total());

        create_host_memory_master(opaque);

        return 0;
    }

    DPRINTF("Only stage one can reach this function\n");
    return -1;
}

static inline void *host_from_stream_offset(QEMUFile *f,
                                            ram_addr_t offset,
                                            int flags,
                                            unsigned long *index)
{
    static __thread RAMBlock *block = NULL;
    //static RAMBlock *block = NULL;
    char id[256];
    uint8_t len;

    if (flags & RAM_SAVE_FLAG_CONTINUE) {
        if (!block) {
            fprintf(stderr, "Ack, bad migration stream!\n");
            return NULL;
        }

        *index = (block->offset + offset) / TARGET_PAGE_SIZE;
        return block->host + offset;
    }

    len = qemu_get_byte(f);
    qemu_get_buffer(f, (uint8_t *)id, len);
    id[len] = 0;

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        if (!strncmp(id, block->idstr, sizeof(id))) {
            //DPRINTF("block host %p, block length %lx, %lx\n", block->host, block->length, 
		    //block->offset + offset);
            *index = (block->offset + offset) / TARGET_PAGE_SIZE;
            return block->host + offset;
        }
    }

    fprintf(stderr, "Can't find block %s!\n", id);
    return NULL;
}

#include "savevm.h"

int ram_load(QEMUFile *f, void *opaque, int version_id)
{
    ram_addr_t addr;
    int flags;
    SaveStateEntry *se = (struct SaveStateEntry *)opaque;

    if (version_id < 3 || version_id > 4) {
        return -EINVAL;
    }
   
   DPRINTF("handle payload %x\n", decomped_buf);

    do {
        if(decomped_buf == NULL)
            addr = qemu_get_be64(f);
        else
           addr = buf_get_be64(decomped_buf);
        flags = addr & ~TARGET_PAGE_MASK;
        addr &= TARGET_PAGE_MASK;

        DPRINTF("se is %p, flags %x\n", se, flags);
        DPRINTF("se version queue is %p\n", se->version_queue);
        DPRINTF("addr is %lx:%lx, flags %x\n", addr, addr / TARGET_PAGE_SIZE, flags);
        if (flags & RAM_SAVE_FLAG_MEM_SIZE) {
            /*
             * classicsong add version queue for memory
             * The queue length is equals to the number of pages guest VM has
             */
            DPRINTF("init mem addr is %lx:%lx, flags %x\n", addr, addr / TARGET_PAGE_SIZE, flags);
            se->version_queue = (uint32_t *)calloc(addr / TARGET_PAGE_SIZE, sizeof(uint32_t));
            se->total_size = addr / TARGET_PAGE_SIZE;

            DPRINTF("total mem size is %lx\n", se->total_size);
            if (version_id == 3) {
                if (addr != ram_bytes_total()) {
                    return -EINVAL;
                }

                DPRINTF("RAM save function version 3\n");
            } else {
                /* Synchronize RAM block list */
                char id[256];
                ram_addr_t length;
                ram_addr_t total_ram_bytes = addr;

                while (total_ram_bytes) {
                    RAMBlock *block;
                    uint8_t len;

                    if (decomped_buf == NULL){
                        len = qemu_get_byte(f);
                        qemu_get_buffer(f, (uint8_t *)id, len);
                        id[len] = 0;
                        length = qemu_get_be64(f);
                        DPRINTF("get length %lx", length);
                    }else{
                        len = buf_get_byte(decomped_buf);
                        buf_get_buffer(decomped_buf, (uint8_t *)id, len);
                        id[len] = 0;
                        length = buf_get_be64(decomped_buf);
                        DPRINTF("get length %lx", length);
                    }

                    DPRINTF("memory id %s\n", id);

                    DPRINTF("Get mem block %s\n", id);
                    QLIST_FOREACH(block, &ram_list.blocks, next) {
                        if (!strncmp(id, block->idstr, sizeof(id))) {
                            DPRINTF("block host %p, block length %lx\n", block->host, block->length);
                            if (block->length != length)
                                return -EINVAL;
                            break;
                        }
                    }

                    if (!block) {
                        fprintf(stderr, "Unknown ramblock \"%s\", cannot "
                                "accept migration\n", id);
                        return -EINVAL;
                    }

                    total_ram_bytes -= length;
                }
            }

            DPRINTF("Mem block finish\n");
        }


        /*
         * classicsong
         * the memory patch is applied by slaves it self
         * So after receive block info, the host will send RAM_SAVE_FLAG_EOS to let dest 
         * to terminate the load_ram call.
         */

        if (flags & RAM_SAVE_FLAG_COMPRESS) {
            void *host;
            uint8_t ch;
            uint32_t mem_vnum = ((flags & MEM_VNUM_MASK) >> MEM_VNUM_OFFSET);
            uint32_t curr_vnum;
            volatile uint32_t *vnum_p;
            unsigned long index = 0;

            DPRINTF("handle compress\n");
            if (version_id == 3)
                host = qemu_get_ram_ptr(addr);
            else
                host = host_from_stream_offset(f, addr, flags, &index);
            if (!host) {
                return -EINVAL;
            }

            if (se->total_size < (addr / TARGET_PAGE_SIZE))
                fprintf(stderr, "error host memory addr %lx; %lx\n", se->total_size, addr / TARGET_PAGE_SIZE);

            assert(index < se->total_size);
            vnum_p = &(se->version_queue[index]);
        re_check_press:
            curr_vnum = *vnum_p;

            /*
             * some one is holding the page
             */
            while (curr_vnum % 2 == 1) {
                curr_vnum = *vnum_p;
            }

            /*
             * skip the current patch
             * update page path:
             * check current version of mem queue curr_vnum (init to 0)
             * current mem_vnum (from 0 to n)
             *
             * curr_vnum set to 2 means current memory version is updated to 0
             * curr_vnum set to 1 means current memory is updating to 0
             * curr_vnum set to 4 means current memory version is updated to 1
             * curr_vnum set to 3 means current memory is updating to 1
             */
            if (curr_vnum > mem_vnum * 2) {
                if (decomped_buf == NULL)
                    ch = qemu_get_byte(f);
                else
                    ch = buf_get_byte(decomped_buf);
                DPRINTF("skip page patch %d, %d\n", curr_vnum, mem_vnum*2);
                goto end;
            }

            /*
             * now we will hold the page
             */
            if (hold_page(vnum_p, curr_vnum, mem_vnum)) {
                /* fail holding the page */
                goto re_check_press;
            }
            if (decomped_buf == NULL)                
                ch = qemu_get_byte(f);
            else
                ch = buf_get_byte(decomped_buf);

            memset(host, ch, TARGET_PAGE_SIZE);
#ifndef _WIN32
            if (ch == 0 &&
                (!kvm_enabled() || kvm_has_sync_mmu())) {
                qemu_madvise(host, TARGET_PAGE_SIZE, QEMU_MADV_DONTNEED);
            }
#endif

            /*
             * now we release the page
             */
            release_page(vnum_p, mem_vnum);
        } else if (flags & RAM_SAVE_FLAG_PAGE) {
            void *host;
            uint32_t mem_vnum = ((flags & MEM_VNUM_MASK) >> MEM_VNUM_OFFSET);
            uint32_t curr_vnum;
            volatile uint32_t *vnum_p;
            unsigned long index = 0;

            DPRINTF("handle normal page\n");
            if (version_id == 3){
                DPRINTF("calling qemu_get_ram_ptr();\n");
                host = qemu_get_ram_ptr(addr);
            }else
	        host = host_from_stream_offset(f, addr, flags, &index);

            if (se->total_size < (addr / TARGET_PAGE_SIZE))
                fprintf(stderr, "error host memory addr %lx; %lx\n", se->total_size, addr / TARGET_PAGE_SIZE);

            assert(index < se->total_size);
            vnum_p = &(se->version_queue[index]);
        re_check_nor:
            curr_vnum = *vnum_p;

            /*
             * some one is holding the page
             */
            while (curr_vnum % 2 == 1) {
                curr_vnum = *vnum_p;
            }

            /*
             * skip the current patch
             * update page path:
             * check current version of mem queue curr_vnum (init to 0)
             * current mem_vnum (from 0 to n)
             *
             * curr_vnum set to 2 means current memory version is updated to 0
             * curr_vnum set to 1 means current memory is updating to 0
             * curr_vnum set to 4 means current memory version is updated to 1
             * curr_vnum set to 3 means current memory is updating to 1
             */
            if (curr_vnum > mem_vnum * 2) {
                /* the memory data is sent at host end so we must receive it */
                uint8_t buf[TARGET_PAGE_SIZE];
                if (decomped_buf == NULL)
                    qemu_get_buffer(f, buf, TARGET_PAGE_SIZE);
                else
                    buf_get_buffer(decomped_buf, buf, TARGET_PAGE_SIZE);
                DPRINTF("skip page patch %d, %d\n", curr_vnum, mem_vnum * 2);
                goto end;
            }

            /*
             * now we will hold the page
             */
            if (hold_page(vnum_p, curr_vnum, mem_vnum)) {
                /* fail holding the page */
                goto re_check_nor;
            }

            if (decomped_buf == NULL)
                qemu_get_buffer(f, host, TARGET_PAGE_SIZE);
            else
                buf_get_buffer(decomped_buf, host, TARGET_PAGE_SIZE);

            /*
             * now we release the page
             */
            release_page(vnum_p, mem_vnum);
        }

    end:
        if (qemu_file_has_error(f)) {
            return -EIO;
        }
    } while (!(flags & RAM_SAVE_FLAG_EOS));

    return 0;
}

void qemu_service_io(void)
{
    qemu_notify_event();
}

#ifdef HAS_AUDIO
struct soundhw {
    const char *name;
    const char *descr;
    int enabled;
    int isa;
    union {
        int (*init_isa) (qemu_irq *pic);
        int (*init_pci) (PCIBus *bus);
    } init;
};

static struct soundhw soundhw[] = {
#ifdef HAS_AUDIO_CHOICE
#if defined(TARGET_I386) || defined(TARGET_MIPS)
    {
        "pcspk",
        "PC speaker",
        0,
        1,
        { .init_isa = pcspk_audio_init }
    },
#endif

#ifdef CONFIG_SB16
    {
        "sb16",
        "Creative Sound Blaster 16",
        0,
        1,
        { .init_isa = SB16_init }
    },
#endif

#ifdef CONFIG_CS4231A
    {
        "cs4231a",
        "CS4231A",
        0,
        1,
        { .init_isa = cs4231a_init }
    },
#endif

#ifdef CONFIG_ADLIB
    {
        "adlib",
#ifdef HAS_YMF262
        "Yamaha YMF262 (OPL3)",
#else
        "Yamaha YM3812 (OPL2)",
#endif
        0,
        1,
        { .init_isa = Adlib_init }
    },
#endif

#ifdef CONFIG_GUS
    {
        "gus",
        "Gravis Ultrasound GF1",
        0,
        1,
        { .init_isa = GUS_init }
    },
#endif

#ifdef CONFIG_AC97
    {
        "ac97",
        "Intel 82801AA AC97 Audio",
        0,
        0,
        { .init_pci = ac97_init }
    },
#endif

#ifdef CONFIG_ES1370
    {
        "es1370",
        "ENSONIQ AudioPCI ES1370",
        0,
        0,
        { .init_pci = es1370_init }
    },
#endif

#ifdef CONFIG_HDA
    {
        "hda",
        "Intel HD Audio",
        0,
        0,
        { .init_pci = intel_hda_and_codec_init }
    },
#endif

#endif /* HAS_AUDIO_CHOICE */

    { NULL, NULL, 0, 0, { NULL } }
};

void select_soundhw(const char *optarg)
{
    struct soundhw *c;

    if (*optarg == '?') {
    show_valid_cards:

        printf("Valid sound card names (comma separated):\n");
        for (c = soundhw; c->name; ++c) {
            printf ("%-11s %s\n", c->name, c->descr);
        }
        printf("\n-soundhw all will enable all of the above\n");
        exit(*optarg != '?');
    }
    else {
        size_t l;
        const char *p;
        char *e;
        int bad_card = 0;

        if (!strcmp(optarg, "all")) {
            for (c = soundhw; c->name; ++c) {
                c->enabled = 1;
            }
            return;
        }

        p = optarg;
        while (*p) {
            e = strchr(p, ',');
            l = !e ? strlen(p) : (size_t) (e - p);

            for (c = soundhw; c->name; ++c) {
                if (!strncmp(c->name, p, l) && !c->name[l]) {
                    c->enabled = 1;
                    break;
                }
            }

            if (!c->name) {
                if (l > 80) {
                    fprintf(stderr,
                            "Unknown sound card name (too big to show)\n");
                }
                else {
                    fprintf(stderr, "Unknown sound card name `%.*s'\n",
                            (int) l, p);
                }
                bad_card = 1;
            }
            p += l + (e != NULL);
        }

        if (bad_card) {
            goto show_valid_cards;
        }
    }
}

void audio_init(qemu_irq *isa_pic, PCIBus *pci_bus)
{
    struct soundhw *c;

    for (c = soundhw; c->name; ++c) {
        if (c->enabled) {
            if (c->isa) {
                if (isa_pic) {
                    c->init.init_isa(isa_pic);
                }
            } else {
                if (pci_bus) {
                    c->init.init_pci(pci_bus);
                }
            }
        }
    }
}
#else
void select_soundhw(const char *optarg)
{
}
void audio_init(qemu_irq *isa_pic, PCIBus *pci_bus)
{
}
#endif

int qemu_uuid_parse(const char *str, uint8_t *uuid)
{
    int ret;

    if (strlen(str) != 36) {
        return -1;
    }

    ret = sscanf(str, UUID_FMT, &uuid[0], &uuid[1], &uuid[2], &uuid[3],
                 &uuid[4], &uuid[5], &uuid[6], &uuid[7], &uuid[8], &uuid[9],
                 &uuid[10], &uuid[11], &uuid[12], &uuid[13], &uuid[14],
                 &uuid[15]);

    if (ret != 16) {
        return -1;
    }
#ifdef TARGET_I386
    smbios_add_field(1, offsetof(struct smbios_type_1, uuid), 16, uuid);
#endif
    return 0;
}

void do_acpitable_option(const char *optarg)
{
#ifdef TARGET_I386
    if (acpi_table_add(optarg) < 0) {
        fprintf(stderr, "Wrong acpi table provided\n");
        exit(1);
    }
#endif
}

void do_smbios_option(const char *optarg)
{
#ifdef TARGET_I386
    if (smbios_entry_add(optarg) < 0) {
        fprintf(stderr, "Wrong smbios provided\n");
        exit(1);
    }
#endif
}

void cpudef_init(void)
{
#if defined(cpudef_setup)
    cpudef_setup(); /* parse cpu definitions in target config file */
#endif
}

int audio_available(void)
{
#ifdef HAS_AUDIO
    return 1;
#else
    return 0;
#endif
}

int kvm_available(void)
{
#ifdef CONFIG_KVM
    return 1;
#else
    return 0;
#endif
}

int xen_available(void)
{
#ifdef CONFIG_XEN
    return 1;
#else
    return 0;
#endif
}
