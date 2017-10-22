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

module lat_tester (
    input clk27,
    input active,
    input armed,
    input sensor,
    input trigger,
    input VSYNC_in,
    input [1:0] mode_in,
    output reg [2:0] mode_synced,
    output reg [15:0] result
);

reg VSYNC_in_L, VSYNC_in_LL, VSYNC_in_LLL;
reg running;
reg [8:0] clk27_ctr;

always @(posedge clk27) begin
    VSYNC_in_L <= VSYNC_in;
    VSYNC_in_LL <= VSYNC_in_L;
    VSYNC_in_LLL <= VSYNC_in_LL;
end

always @(posedge clk27) begin
    if (VSYNC_in_LLL && !VSYNC_in_LL)
        mode_synced <= mode_in;
end

always @(posedge clk27) begin
    if (!active) begin
        running <= 0;
    end else begin
        if ((result==0) && (clk27_ctr==0) && armed && trigger) begin
            running <= 1;
        end else if (running && ((sensor==0) || (result==16'hffff))) begin
            running <= 0;
        end
    end
end

always @(posedge clk27) begin
    if (!active || !armed) begin
        result <= 0;
        clk27_ctr <= 0;
    end else if (running) begin
        if (clk27_ctr == 270-1) begin
            clk27_ctr <= 0;
            result <= result + 1'b1;
        end else begin
            clk27_ctr <= clk27_ctr + 1'b1;
        end
    end
end

endmodule
