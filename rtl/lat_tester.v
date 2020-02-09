//
// Copyright (C) 2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
//
// This file is part of Open Source Scan Converter project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

`define LT_STATE_IDLE       2'h0
`define LT_STATE_LAT_MEAS   2'h1
`define LT_STATE_STB_MEAS   2'h2
`define LT_STATE_FINISHED   2'h3

module lat_tester (
    input clk27,
    input pclk,
    input active,
    input armed,
    input sensor,
    input trigger,
    input VSYNC_in,
    input [1:0] mode_in,
    output reg [2:0] mode_synced,
    output reg [15:0] lat_result,
    output reg [11:0] stb_result,
    output trig_waiting,
    output reg finished
);

reg VSYNC_in_L, VSYNC_in_LL, trigger_L, trigger_LL;
reg [8:0] clk27_ctr;
reg [1:0] state;

assign trig_waiting = (state == `LT_STATE_LAT_MEAS);

always @(posedge pclk) begin
    VSYNC_in_L <= VSYNC_in;
    VSYNC_in_LL <= VSYNC_in_L;
end

always @(posedge pclk) begin
    if (VSYNC_in_LL && !VSYNC_in_L)
        mode_synced <= mode_in;
end

always @(posedge clk27) begin
    trigger_L <= trigger;
    trigger_LL <= trigger_L;
end

always @(posedge clk27) begin
    if (!active) begin
        state <= `LT_STATE_IDLE;
    end else begin
        case (state)
            default: begin //STATE_IDLE
                finished <= 1'b0;
                lat_result <= 0;
                stb_result <= 0;
                clk27_ctr <= 0;
                if (armed && trigger_LL)
                    state <= `LT_STATE_LAT_MEAS;
            end
            `LT_STATE_LAT_MEAS: begin
                if (sensor==0) begin
                    state <= `LT_STATE_STB_MEAS;
                    clk27_ctr <= 0;
                end else if (lat_result==16'hffff) begin
                    state <= `LT_STATE_FINISHED;
                end else begin
                    if (clk27_ctr == 270-1) begin
                        clk27_ctr <= 0;
                        lat_result <= lat_result + 1'b1;
                    end else begin
                        clk27_ctr <= clk27_ctr + 1'b1;
                    end
                end
            end    
            `LT_STATE_STB_MEAS: begin
                if (((sensor==1) && (stb_result >= 12'd100)) || (stb_result == 12'hfff)) begin
                    state <= `LT_STATE_FINISHED;
                end else begin
                    if (clk27_ctr == 270-1) begin
                        clk27_ctr <= 0;
                        stb_result <= stb_result + 1'b1;
                    end else begin
                        clk27_ctr <= clk27_ctr + 1'b1;
                    end
                end
            end
            `LT_STATE_FINISHED: begin
                finished <= 1'b1;
                if (!armed)
                    state <= `LT_STATE_IDLE;
            end
        endcase
    end
end

endmodule
