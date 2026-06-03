set pagination off
set confirm off
set architecture arm
target extended-remote localhost:3333
monitor reset halt
set breakpoint auto-hw on
hbreak ir_debug_frame_ready
commands
silent
printf "\n=== IR frame captured ===\n"
printf "rx_level=%u edges=%u timeouts=%u frames=%u start_level=%u count=%u\n", (unsigned char)g_ir_rx_level, (unsigned int)g_ir_rx_edge_counter, (unsigned int)g_ir_capture_timeout_counter, (unsigned int)g_ir_frame_counter, (unsigned char)g_ir_code_start_level, (unsigned short)g_ir_code_count
printf "g_ir_code_table_us first 80 entries:\n"
x/80uh &g_ir_code_table_us
continue
end
continue
