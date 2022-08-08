module counter (
    clk,
    rstn,
    out
);
    input clk;
    input rstn;
    output reg[7:0] out [0:2];

    always @(posedge clk or negedge rstn) begin
        if (!rstn) begin
            out[0] <= 0;
            out[1] <= 0;
            out[2] <= 0;
        end else begin
            out[0] <= out[0] + 8'd1;
            out[1] <= out[0];
            out[2] <= out[1];
        end
    end
    
endmodule