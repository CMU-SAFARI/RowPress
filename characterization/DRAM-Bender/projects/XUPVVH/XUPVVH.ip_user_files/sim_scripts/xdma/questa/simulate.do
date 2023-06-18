onbreak {quit -f}
onerror {quit -f}

vsim -t 1ps -lib xil_defaultlib xdma_opt

do {wave.do}

view wave
view structure
view signals

do {xdma.udo}

run -all

quit -force
