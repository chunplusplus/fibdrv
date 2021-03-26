sudo taskset -c 7 ./client > syscall_input
gnuplot scripts/system-call.gp 
