module counter (
    clk,
    rstn,
    out
);
    input clk;
    input rstn;
    output reg[7:0] out;

    always @(posedge clk or negedge rstn) begin
        if (!rstn) begin
            out <= 0;
        end else begin
            out <= out + 8'd1;
        end
    end
    
endmodule