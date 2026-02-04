#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/definitions.h"
#include "../include/objfile.h"

/*
    Single block vm, with stdin attached at the top and stdout at the bottom
    Debug mode features:
    - Display source code with current instruction highlighted
    - Step-through execution one instruction at a time
    - View block state (registers, stack, accumulator)
*/

#define TICK_LIMIT 1024
#define LINES_OF_CONTEXT 8

void display_debug_ui(grid *g, const block_object_file *obj, const u8 *out_buffer)
{
    if (!g || !obj || !obj->has_debug_info)
        return;

// Build entire frame into buffer to avoid flicker
#define FRAME_BUFFER_SIZE 4096
    char frame[FRAME_BUFFER_SIZE] = {0};
    int offset = 0;

    // Get current instruction
    u8 current_instr = g->blocks[0].current_instruction;
    u16 current_line = objfile_get_source_line(obj, current_instr);

    // Use cursor home instead of clearing screen
    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "\033[H");

    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "=== BLOCKLANG DEBUG VIEW ===\n\n");

    // Display source code with context
    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset,
                       "--- SOURCE CODE (current instruction at line ~%d) ---\n", current_line);

    // Extract lines around current position
    u16 start_line = current_line > LINES_OF_CONTEXT ? current_line - LINES_OF_CONTEXT : 1;
    u16 end_line = current_line + LINES_OF_CONTEXT;

    if (end_line > obj->source_length)
        end_line = obj->source_length;
    if (end_line < start_line)
        end_line = start_line;

    u16 line_num = 1;
    const char *ptr = obj->source;
    u16 lines_printed = 0;
    u16 max_source_lines = LINES_OF_CONTEXT * 2 + 1; // Fixed height

    while (ptr < obj->source + obj->source_length && line_num <= end_line && lines_printed < max_source_lines)
    {
        if (line_num >= start_line)
        {
            char is_current = (line_num == current_line) ? '>' : ' ';
            offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "%c %3d: ", is_current, line_num);

            // Append line until newline, max 80 chars to keep consistent width
            int col = 0;
            while (*ptr && *ptr != '\n' && col < 80 && offset < FRAME_BUFFER_SIZE - 1)
            {
                frame[offset++] = *ptr++;
                col++;
            }

            // Clear to end of line and move to next
            offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "\033[K\n");
            lines_printed++;
        }
        else
        {
            // Skip to next line
            while (*ptr && *ptr != '\n')
                ptr++;
        }

        if (*ptr == '\n')
        {
            ptr++;
            line_num++;
        }
    }

    // Pad remaining lines to maintain fixed height
    while (lines_printed < max_source_lines)
    {
        offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "  \033[K\n");
        lines_printed++;
    }

    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "\n--- BLOCK STATE ---\n");
    instruction instr = g->blocks[0].bytecode[current_instr];
    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "Instruction: %d / %d (0x%02X: %d, %d)\n",
                       current_instr, g->blocks[0].length, *(u8 *)&instr,
                       instr.operation, // opcode
                       instr.target);   // target

    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "Accumulator: %3d (0x%02X)\n",
                       g->blocks[0].accumulator, g->blocks[0].accumulator);
    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "Registers: RG0=%3d RG1=%3d RG2=%3d RG3=%3d\n",
                       g->blocks[0].registers[0], g->blocks[0].registers[1], g->blocks[0].registers[2],
                       g->blocks[0].registers[3]);

    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "Stack (%d items): ", g->blocks[0].stack_top + 1);
    for (int i = 0; i <= g->blocks[0].stack_top && i < 16; i++)
        offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "[%d] ", g->blocks[0].stack[i]);
    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "\033[K\n");

    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "Status: %s%s\n",
                       g->blocks[0].state_halted ? "HALTED " : "", g->blocks[0].waiting_ticks > 0 ? "WAITING " : "");

    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "\n--- OUTPUT ---\n");
    offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "%.256s\n", out_buffer);

    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "\n--- COMMANDS ---\n");
    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "s - step one instruction\n");
    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "c - continue execution\n");
    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "r - reset program\n");
    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "l - clear screen\n");
    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "q - quit\n");
    // offset += snprintf(frame + offset, FRAME_BUFFER_SIZE - offset, "> ");

    // Write entire frame atomically
    fwrite(frame, 1, offset, stdout);
    fflush(stdout);
}

void print_compact_state(grid *g, const block_object_file *obj, const u8 *out_buffer)
{
    if (!g || !obj || !obj->has_debug_info)
    {
        printf("%.256s\n", out_buffer);
        return;
    }

    u8 current_instr = g->blocks[0].current_instruction;
    u16 current_line = objfile_get_source_line(obj, current_instr);

    printf("[Line %d] ACC=%3d RG=[%d,%d,%d,%d] Stack=%d | Output: %.256s\n", current_line, g->blocks[0].accumulator,
           g->blocks[0].registers[0], g->blocks[0].registers[1], g->blocks[0].registers[2], g->blocks[0].registers[3],
           g->blocks[0].stack_top + 1, out_buffer);
}

int main(int argc, char *argv[])
{
    int c;
    const char *input_file = NULL;

    bool run_immediately = false;
    bool debug_mode = false;

    while ((c = getopt(argc, argv, "drf:")) != -1)
        switch (c)
        {
        case 'd':
            debug_mode = true;
            break;
        case 'r':
            run_immediately = true;
            break;
        case 'f':
            input_file = optarg;
            break;
        case ':':
            fprintf(stderr, "Option needs a value\n");
            break;
        case '?':
            if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
        usage:
            fprintf(stderr, "Usage: -f <bytecode file> [-d] [-r]\n");
            fprintf(stderr, "  -f: bytecode file (required)\n");
            fprintf(stderr, "  -d: debug mode (interactive stepping)\n");
            fprintf(stderr, "  -r: run immediately (no stdin for first execution)\n");
            return 1;
        }

    if (!input_file)
        goto usage;

    // Read object file (handles both debug and raw bytecode formats)
    block_object_file obj = {0};
    if (!objfile_read_file(input_file, &obj))
    {
        fprintf(stderr, "Failed to read object file: %s\n", input_file);
        return 1;
    }

    u8 bytecode_len = obj.bytecode_length;

    if (bytecode_len > BYTECODE_LIMIT)
    {
        fprintf(stderr, "Bytecode cannot be longer than %d bytes\n", BYTECODE_LIMIT);
        return 1;
    }

    grid *g = initialize_grid(1, 1);

    u8 in_buffer[256] = {};
    u8 out_buffer[256] = {};

    attach_input(g, up, 0, in_buffer, 255);
    attach_output(g, down, 0, out_buffer, 255);

    load_program(g, 0, 0, obj.bytecode, bytecode_len);

    bool in_debug_mode = debug_mode && obj.has_debug_info;

    if (in_debug_mode)
    {
        // Interactive debug mode
        printf("Entering debug mode. Type 'h' for help.\n\n");
        display_debug_ui(g, &obj, out_buffer);

        bool stepping = true;
        char last_cmd = '\0';
        while (stepping && !g->blocks[0].state_halted)
        {
            char cmd = '\0';
            scanf("%c", &cmd);
        exec_last_command:
            switch (cmd)
            {
            case 's':
            case 'S':
            {
                // Step one instruction
                run_grid(g, 1);

                if (g->ticks == 0 && !g->any_ticked)
                {
                    printf("Program completed.\n");
                    stepping = false;
                }
                else
                {
                    display_debug_ui(g, &obj, out_buffer);
                }
                break;
            }
            case 'c':
            case 'C':
            {
                // Continue execution
                printf("Continuing execution...\n");
                run_grid(g, TICK_LIMIT);

                if (g->ticks >= TICK_LIMIT)
                {
                    printf("Execution limit reached. Output: %.256s\n", out_buffer);
                }
                else if (!g->any_ticked || g->blocks[0].state_halted)
                {
                    printf("Program completed.\n");
                    printf("Output: %.256s\n", out_buffer);
                }
                stepping = false;
                break;
            }
            case 'r':
            case 'R':
            {
                // Reset program
                memset(out_buffer, 0, 256);
                free_grid(g);
                g = initialize_grid(1, 1);
                attach_input(g, up, 0, in_buffer, 255);
                attach_output(g, down, 0, out_buffer, 255);
                load_program(g, 0, 0, obj.bytecode, bytecode_len);
                display_debug_ui(g, &obj, out_buffer);
                break;
            }
            case 'q':
            case 'Q':
            {
                // Quit
                stepping = false;
                break;
            }
            case 'h':
            case 'H':
            case '?':
            {
                printf("\nCommands:\n");
                printf("  i - change the input buffer\n");
                printf("  s - step one instruction\n");
                printf("  c - continue execution\n");
                printf("  r - reset program\n");
                printf("  l - clear screen\n");
                printf("  q - quit\n");
                printf("  h - show this help\n");
                printf("> ");
                break;
            }
            case 'l':
            case 'L':
            {
                // Clear screen
                printf("\033[2J\033[H");
                fflush(stdout);
                display_debug_ui(g, &obj, out_buffer);
                break;
            }
            case 'i':
            case 'I':
            {
                // Change input buffer
                printf("Current input buffer: %.255s\n", in_buffer);
                printf("Enter new input (max 255 chars): ");
                getchar(); // consume leftover newline
                fgets((char *)in_buffer, 255, stdin);
                // Remove newline if present
                size_t len = strlen((char *)in_buffer);
                if (len > 0 && in_buffer[len - 1] == '\n')
                    in_buffer[len - 1] = '\0';
                display_debug_ui(g, &obj, out_buffer);
                break;
            }
            case '\n':
                // Repeat the last command
                if (last_cmd != '\0')
                {
                    cmd = last_cmd;
                    // continue;
                    goto exec_last_command;
                }
                break;
            default:
                printf("Unknown command '%c'. Type 'h' for help.\n> ", cmd);
                break;
            }
            last_cmd = cmd;
        }
    }
    else
    {
        // Original behavior: run with input/output
        if (!run_immediately)
            printf("Single block VM. Type input and press enter to run the program.\n");

        for (;;)
        {
            if (!run_immediately)
            {
                printf("> ");
                fgets((void *)in_buffer, 255, stdin);
            }

            run_grid(g, TICK_LIMIT);

            if (g->ticks >= TICK_LIMIT)
            {
                printf("Grid ticked for %d ticks, aborting\n", g->ticks);
                printf("Current output: %.255s\n", out_buffer);
                abort();
            }

            printf("%.255s\n", out_buffer);

            if (g->any_ticked == false)
            {
                break;
            }

            g->ticks = 0;
        }
    }

    free_grid(g);

    return 0;
}