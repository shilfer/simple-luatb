module counter (
    clk,
    rstn,
    out
);
    input clk;
    input rstn;
    output reg[7:0] out [1:3];

    always @(posedge clk or negedge rstn) begin
        if (!rstn) begin
            out[1] <= 0;
            out[2] <= 0;
            out[3] <= 0;
        end else begin
            out[1] <= out[1] + 8'd1;
            out[2] <= out[1];
            out[3] <= out[2];
        end
    end
    
endmodule