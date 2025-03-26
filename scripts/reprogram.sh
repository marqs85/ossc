#!/bin/sh

make rv-reprogram

if [ $# -eq 1 ] && [ $1 = "jtag_uart" ] && [ $(pgrep -c nios2-terminal) = 0 ]; then
    nios2-terminal
fi
