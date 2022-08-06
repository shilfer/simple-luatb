`timescale 1ps/1ps

module toptb;

    int     code;
    string  filename;

    counter uut(.clk(), .rstn(), .out());

    initial begin
        if (!$value$plusargs("SCRIPT=%s", filename)) begin
            $fatal(0, "[luatb:E] testcase script must be specified using +SCRIPT=<filename>\n");
            $finish();
        end else begin
            code = $luatb_start(uut, filename);
        end
    end

endmodule