print("In Script testcase.lua")

storage = {}
storage.clk_value = "0"

clk, err = vpi.get_signal_handle_by_path("clk")
if not clk then
    error(err)
end

rstn, err = vpi.get_signal_handle_by_path("rstn")
if not rstn then
    error(err)
end

out, err = vpi.get_signal_handle_by_path("out")
if not out then
    error(err)
end

for i,v in ipairs(out) do
    print(i, v, vpi.get_signal_path(v))
end

vpi.set_signal_value_binstr(rstn, "0")
vpi.set_signal_value_binstr(rstn, "1", 100000)
vpi.set_signal_value_binstr(clk, "0")

vpi.register_callback_on_simtime_interval(
    function(d, t)
        if d.clk_value == "0" then
            d.clk_value = "1"
        else
            d.clk_value = "0"
        end
        vpi.set_signal_value_binstr(clk, d.clk_value)
    end,
    100000, storage
)

print(vpi.get_timeunit())