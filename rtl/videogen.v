//
// Copyright (C) 2015-2016  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

module videogen (
    input clk27,
    input reset_n,
    output [7:0] R_out,
    output [7:0] G_out,
    output [7:0] B_out,
    output reg HSYNC_out,
    output reg VSYNC_out,
    output PCLK_out,
    output reg ENABLE_out
);

//Parameters for 720x480@59.94Hz (858px x 525lines, pclk 27MHz -> 59.94Hz)
parameter   H_SYNCLEN       =   62;
parameter   H_BACKPORCH     =   60;
parameter   H_ACTIVE        =   720;
parameter   H_FRONTPORCH    =   16;
parameter   H_TOTAL         =   858;

parameter   V_SYNCLEN       =   6;
parameter   V_BACKPORCH     =   30;
parameter   V_ACTIVE        =   480;
parameter   V_FRONTPORCH    =   9;
parameter   V_TOTAL         =   525;

parameter   H_OVERSCAN      =   40; //at both sides
parameter   V_OVERSCAN      =   16; //top and bottom
parameter   H_AREA          =   640;
parameter   V_AREA          =   448;
parameter   H_BORDER        =   (H_AREA-512)/2;
parameter   V_BORDER        =   (V_AREA-256)/2;

parameter   X_START     =   H_SYNCLEN + H_BACKPORCH;
parameter   Y_START     =   V_SYNCLEN + V_BACKPORCH;

//Counters
reg [9:0] h_cnt; //max. 1024
reg [9:0] v_cnt; //max. 1024

reg [9:0] xpos;
reg [9:0] ypos;

assign PCLK_out = clk27;

//R, G and B should be 0 outside of active area
assign R_out = ENABLE_out ? V_gen : 8'h00;
assign G_out = ENABLE_out ? V_gen : 8'h00;
assign B_out = ENABLE_out ? V_gen : 8'h00;

reg [7:0] V_gen;


//HSYNC gen (negative polarity)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            h_cnt <= 0;
            HSYNC_out <= 0;
        end
    else
        begin
            //Hsync counter
            if (h_cnt < H_TOTAL-1 )
                h_cnt <= h_cnt + 1;
            else
                h_cnt <= 0;
            
            //Hsync signal
            HSYNC_out <= (h_cnt < H_SYNCLEN) ? 0 : 1;
        end
end

//VSYNC gen (negative polarity)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            v_cnt <= 0;
            VSYNC_out <= 0;
        end
    else
        begin
            if (h_cnt == 0)
                begin
                    //Vsync counter
                    if (v_cnt < V_TOTAL-1 )
                        v_cnt <= v_cnt + 1;
                    else
                        v_cnt <= 0;
                    
                    //Vsync signal
                    VSYNC_out <= (v_cnt < V_SYNCLEN) ? 0 : 1;
                end
        end
end

//Data gen
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            V_gen <= 8'h00;
        end
    else
        begin
            if ((h_cnt < X_START+H_OVERSCAN) || (h_cnt >= X_START+H_OVERSCAN+H_AREA) || (v_cnt < Y_START+V_OVERSCAN) || (v_cnt >= Y_START+V_OVERSCAN+V_AREA))
                V_gen <= (h_cnt[0] ^ v_cnt[0]) ? 8'hff : 8'h00;
            else if ((h_cnt < X_START+H_OVERSCAN+H_BORDER) || (h_cnt >= X_START+H_OVERSCAN+H_AREA-H_BORDER) || (v_cnt < Y_START+V_OVERSCAN+V_BORDER) || (v_cnt >= Y_START+V_OVERSCAN+V_AREA-V_BORDER))
                V_gen <= 8'h50;
            else
                V_gen <= (h_cnt - (X_START+H_OVERSCAN+H_BORDER)) >> 1;
            /*else
                V_gen <= 8'h00;*/
        end
end

//Enable gen
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            ENABLE_out <= 8'h00;
        end
    else
        begin
            ENABLE_out <= (h_cnt >= X_START && h_cnt < X_START + H_ACTIVE && v_cnt >= Y_START && v_cnt < Y_START + V_ACTIVE);
        end
end

endmodule
