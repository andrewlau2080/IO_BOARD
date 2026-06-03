set confirm off
set architecture arm
set breakpoint auto-hw on
set remote hardware-breakpoint-limit 6
set remote hardware-watchpoint-limit 4
mem 0x08000000 0x08080000 ro 32
mem 0x20000000 0x20040000 rw 32
target extended-remote localhost:3333
monitor reset halt
