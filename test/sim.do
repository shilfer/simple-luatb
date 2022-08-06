if {![file exists work/_info]} {
    vlib work
}

quiet set PLILIB "../build/mingw/x86_64/release/luatb.dll"

vlog -work work counter.v
vlog -work work -sv toptb.sv

vsim work.toptb -pli $PLILIB +SCRIPT=testcase.lua