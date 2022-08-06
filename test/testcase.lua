print("In Script testcase.lua")

storage = {}
storage.clk_value = "0"

clk = vpi.get_signal_handle_by_path("clk")
rstn = vpi.get_signal_handle_by_path("rstn")
out = vpi.get_signal_handle_by_path("out")

vpi.set_signal_value_binstr(rstn, "0")
vpi.set_signal_value_binstr(rstn, "1", 100000)

vpi.register_callback_on_simtime_interval(
    function(d, t)
        vpi.set_signal_value_binstr(clk, d.clk_value)
        if d.clk_value == "0" then
            d.clk_value = "1"
        else
            d.clk_value = "0"
        end
    end,
    100000, storage
)
