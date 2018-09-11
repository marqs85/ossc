//
// Copyright (C) 2015-2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

`include "lat_tester_includes.v"

module videogen (
    input clk27,
    input reset_n,
    input lt_active,
    input [1:0] lt_mode,
    output [7:0] R_out,
    output [7:0] G_out,
    output [7:0] B_out,
    output reg HSYNC_out,
    output reg VSYNC_out,
    output PCLK_out,
    output reg ENABLE_out,
    input [1:0] pat_id,
    input [3:0] pat_speed,
    input interlace,
    input [8:0] H_BACKPORCH,
    input [7:0] H_SYNCLEN,
    input [10:0] H_ACTIVE,
    input [11:0] H_TOTAL,
    input [5:0] V_BACKPORCH,
    input [2:0] V_SYNCLEN,
    input [10:0] V_ACTIVE,
    input [10:0] V_TOTAL
);

parameter   H_OVERSCAN      =   10'd40; //at both sides
parameter   V_OVERSCAN      =   10'd16; //top and bottom
parameter   H_AREA          =   10'd640;
parameter   V_AREA          =   10'd448;
parameter   H_GRADIENT      =   10'd512;
parameter   V_GRADIENT      =   10'd256;
parameter   V_GRAYRAMP      =   10'd84;
parameter   H_BORDER        =   ((H_AREA-H_GRADIENT)>>1);
parameter   V_BORDER        =   ((V_AREA-V_GRADIENT)>>1);

wire [9:0] X_START = H_BACKPORCH + H_SYNCLEN;
wire [6:0] Y_START = V_BACKPORCH + V_SYNCLEN;

//Counters
reg [11:0] h_cnt;
reg [10:0] v_cnt;
reg [10:0] x_offset;
reg [10:0] y_pos;
reg fid, frame_id;

assign PCLK_out = clk27;
wire [11:0] x_pat = h_cnt-x_offset;

//R, G and B should be 0 outside of active area
assign R_out = ENABLE_out ? V_gen : 8'h00;
assign G_out = ENABLE_out ? V_gen : 8'h00;
assign B_out = ENABLE_out ? V_gen : 8'h00;

reg [7:0] V_gen;


//HSYNC gen (negative polarity)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        h_cnt <= 0;
        HSYNC_out <= 0;
    end else begin
        //Hsync counter
        if (h_cnt < H_TOTAL-1)
            h_cnt <= h_cnt + 1'b1;
        else
            h_cnt <= 0;

        //Hsync signal
        HSYNC_out <= (h_cnt < H_SYNCLEN) ? 1'b0 : 1'b1;
    end
end

//VSYNC gen (negative polarity)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        v_cnt <= 0;
        y_pos <= 0;
        VSYNC_out <= 0;
    end else begin
        //Vsync counter
        if (!interlace) begin
            if (h_cnt == H_TOTAL-1) begin
                if (v_cnt < V_TOTAL-1) begin
                    v_cnt <= v_cnt + 1'b1;
                    if (v_cnt >= Y_START) begin
                        if (pat_id == 2)
                            y_pos <= y_pos + v_cnt[0];
                        else
                            y_pos <= y_pos + 1'b1;
                    end
                end else begin
                    v_cnt <= 0;
                    x_offset <= (x_offset < H_ACTIVE) ? (x_offset + pat_speed + 1'b1) : 0;
                    frame_id <= frame_id ^ 1;
                    if (pat_id == 1)
                        y_pos <= frame_id;
                    else
                        y_pos <= 0;
                end
            end
            
            //Vsync signal
            VSYNC_out <= (v_cnt < V_SYNCLEN) ? 1'b0 : 1'b1;
        end else begin
            if ((fid==1'b0) && (v_cnt==(V_TOTAL>>1)) && (h_cnt==(H_TOTAL>>1)-1)) begin // odd field end
                v_cnt <= 11'h7ff;
                y_pos <= 1;
                fid <= fid ^ 1'b1;
            end else if (((fid==1'b1) && (v_cnt==(V_TOTAL>>1)-1) && (h_cnt == H_TOTAL-1))) begin // even field end
                v_cnt <= 0;
                y_pos <= 0;
                fid <= fid ^ 1'b1;
            end else if (h_cnt == H_TOTAL-1) begin
                v_cnt <= v_cnt + 1'b1;
                if (v_cnt >= Y_START)
                    y_pos <= y_pos + 2;
            end
            
            //Vsync signal
            if (fid==1'b0)
                VSYNC_out <= (v_cnt < V_SYNCLEN) ? 1'b0 : 1'b1;
            else
                VSYNC_out <= ((v_cnt+1'b1 < V_SYNCLEN) | ((v_cnt+1'b1==V_SYNCLEN) & (h_cnt < (H_TOTAL>>1)))) ? 1'b0 : 1'b1;
            end
    end
end

//Data and ENABLE gen
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        V_gen <= 8'h00;
        ENABLE_out <= 1'b0;
    end else begin
        if (lt_active) begin
            case (lt_mode)
                default: begin
                    V_gen <= 8'h00;
                end
                `LT_POS_TOPLEFT: begin
                    V_gen <= ((h_cnt < (X_START+(H_ACTIVE/`LT_WIDTH_DIV))) && (v_cnt < (Y_START+(V_ACTIVE/`LT_HEIGHT_DIV)))) ? 8'hff : 8'h00;
                end
                `LT_POS_CENTER: begin
                    V_gen <= ((h_cnt >= (X_START+(H_ACTIVE/2)-(H_ACTIVE/(`LT_WIDTH_DIV*2)))) && (h_cnt < (X_START+(H_ACTIVE/2)+(H_ACTIVE/(`LT_WIDTH_DIV*2)))) && (v_cnt >= (Y_START+(V_ACTIVE/2)-(V_ACTIVE/(`LT_HEIGHT_DIV*2)))) && (v_cnt < (Y_START+(V_ACTIVE/2)+(V_ACTIVE/(`LT_HEIGHT_DIV*2))))) ? 8'hff : 8'h00;
                end
                `LT_POS_BOTTOMRIGHT: begin
                    V_gen <= ((h_cnt >= (X_START+H_ACTIVE-(H_ACTIVE/`LT_WIDTH_DIV))) && (v_cnt >= (Y_START+V_ACTIVE-(V_ACTIVE/`LT_HEIGHT_DIV)))) ? 8'hff : 8'h00;
                end
            endcase
        end else begin
            if (pat_id < 3) begin
                if ((h_cnt < X_START+H_OVERSCAN) || (h_cnt >= X_START+H_OVERSCAN+H_AREA) || (y_pos < V_OVERSCAN) || (y_pos >= V_OVERSCAN+V_AREA))
                    V_gen <= (h_cnt[0] ^ y_pos[0]) ? 8'hff : 8'h00;
                else if ((h_cnt < X_START+H_OVERSCAN+H_BORDER) || (h_cnt >= X_START+H_OVERSCAN+H_AREA-H_BORDER) || (y_pos < V_OVERSCAN+V_BORDER) || (y_pos >= V_OVERSCAN+V_AREA-V_BORDER))
                    V_gen <= 8'h50;
                else if (y_pos >= V_OVERSCAN+V_BORDER+V_GRADIENT-V_GRAYRAMP)
                    V_gen <= (((h_cnt - (X_START+H_OVERSCAN+H_BORDER)) >> 4) << 3) + (h_cnt - (X_START+H_OVERSCAN+H_BORDER) >> 6);
                else
                    V_gen <= (h_cnt - (X_START+H_OVERSCAN+H_BORDER)) >> 1;
            end else begin
                V_gen <= ((h_cnt >= (X_START+x_offset)) && (h_cnt < (X_START+x_offset+(H_ACTIVE/(`LT_WIDTH_DIV)))) && (y_pos >= ((V_ACTIVE/2)-(V_ACTIVE/(`LT_HEIGHT_DIV*2)))) && (y_pos < ((V_ACTIVE/2)+(V_ACTIVE/(`LT_HEIGHT_DIV*2))))) ? {8{x_pat[3]^y_pos[3]}} : 8'h00;
            end
        end

        ENABLE_out <= (h_cnt >= X_START && h_cnt < X_START + H_ACTIVE && v_cnt >= Y_START && v_cnt < Y_START + V_ACTIVE);
    end
end

endmodule
