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
    output reg [7:0] R_out,
    output reg [7:0] G_out,
    output reg [7:0] B_out,
    output reg HSYNC_out,
    output reg VSYNC_out,
    output reg DE_out,
    output reg [9:0] xpos,
    output reg [9:0] ypos
);

//Parameters for 720x480@59.94Hz (858px x 525lines, pclk 27MHz -> 59.94Hz)
parameter   H_SYNCLEN       =   10'd62;
parameter   H_BACKPORCH     =   10'd60;
parameter   H_ACTIVE        =   10'd720;
parameter   H_FRONTPORCH    =   10'd16;
parameter   H_TOTAL         =   10'd858;

parameter   V_SYNCLEN       =   10'd6;
parameter   V_BACKPORCH     =   10'd30;
parameter   V_ACTIVE        =   10'd480;
parameter   V_FRONTPORCH    =   10'd9;
parameter   V_TOTAL         =   10'd525;

parameter   H_OVERSCAN      =   10'd40; //at both sides
parameter   V_OVERSCAN      =   10'd16; //top and bottom
parameter   H_AREA          =   10'd640;
parameter   V_AREA          =   10'd448;
parameter   H_GRADIENT      =   10'd512;
parameter   V_GRADIENT      =   10'd256;
parameter   V_GRAYRAMP      =   10'd84;
parameter   H_BORDER        =   ((H_AREA-H_GRADIENT)>>1);
parameter   V_BORDER        =   ((V_AREA-V_GRADIENT)>>1);

parameter   X_START     =   H_SYNCLEN + H_BACKPORCH;
parameter   Y_START     =   V_SYNCLEN + V_BACKPORCH;

//Counters
reg [9:0] h_cnt; //max. 1024
reg [9:0] v_cnt; //max. 1024

//HSYNC gen (negative polarity)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        h_cnt <= 0;
        xpos <= 0;
        HSYNC_out <= 0;
    end else begin
        //Hsync counter
        if (h_cnt < H_TOTAL-1) begin
            h_cnt <= h_cnt + 1'b1;
            if (h_cnt >= X_START)
                xpos <= xpos + 1'b1;
        end else begin
            h_cnt <= 0;
            xpos <= 0;
        end

        //Hsync signal
        HSYNC_out <= (h_cnt < H_SYNCLEN) ? 1'b0 : 1'b1;
    end
end

//VSYNC gen (negative polarity)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        v_cnt <= 0;
        ypos <= 0;
        VSYNC_out <= 0;
    end else begin
        //Vsync counter
        if (h_cnt == H_TOTAL-1) begin
            if (v_cnt < V_TOTAL-1) begin
                v_cnt <= v_cnt + 1'b1;
                if (v_cnt >= Y_START)
                    ypos <= ypos + 1'b1;
            end else begin
                v_cnt <= 0;
                ypos <= 0;
            end
        end

        //Vsync signal
        VSYNC_out <= (v_cnt < V_SYNCLEN) ? 1'b0 : 1'b1;
    end
end

//Data and ENABLE gen
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        R_out <= 8'h00;
        G_out <= 8'h00;
        B_out <= 8'h00;
        DE_out <= 1'b0;
    end else begin
        if (lt_active) begin
            case (lt_mode)
                default: begin
                    {R_out, G_out, B_out} <= {3{8'h00}};
                end
                `LT_POS_TOPLEFT: begin
                    {R_out, G_out, B_out} <= {3{((xpos < (H_ACTIVE/`LT_WIDTH_DIV)) && (ypos < (V_ACTIVE/`LT_HEIGHT_DIV))) ? 8'hff : 8'h00}};
                end
                `LT_POS_CENTER: begin
                    {R_out, G_out, B_out} <= {3{((xpos >= ((H_ACTIVE/2)-(H_ACTIVE/(`LT_WIDTH_DIV*2)))) && (xpos < ((H_ACTIVE/2)+(H_ACTIVE/(`LT_WIDTH_DIV*2)))) && (ypos >= ((V_ACTIVE/2)-(V_ACTIVE/(`LT_HEIGHT_DIV*2)))) && (ypos < ((V_ACTIVE/2)+(V_ACTIVE/(`LT_HEIGHT_DIV*2))))) ? 8'hff : 8'h00}};
                end
                `LT_POS_BOTTOMRIGHT: begin
                    {R_out, G_out, B_out} <= {3{((xpos >= (H_ACTIVE-(H_ACTIVE/`LT_WIDTH_DIV))) && (ypos >= (V_ACTIVE-(V_ACTIVE/`LT_HEIGHT_DIV)))) ? 8'hff : 8'h00}};
                end
            endcase
        end else begin
            if ((xpos < H_OVERSCAN) || (xpos >= H_OVERSCAN+H_AREA) || (ypos < V_OVERSCAN) || (ypos >= V_OVERSCAN+V_AREA))
                {R_out, G_out, B_out} <= {3{(xpos[0] ^ ypos[0]) ? 8'hff : 8'h00}};
            else if ((xpos < H_OVERSCAN+H_BORDER) || (xpos >= H_OVERSCAN+H_AREA-H_BORDER) || (ypos < V_OVERSCAN+V_BORDER) || (ypos >= V_OVERSCAN+V_AREA-V_BORDER))
                {R_out, G_out, B_out} <= {3{8'h50}};
            else if (ypos >= V_OVERSCAN+V_BORDER+V_GRADIENT-V_GRAYRAMP)
                {R_out, G_out, B_out} <= {3{8'((((xpos - (H_OVERSCAN+H_BORDER)) >> 4) << 3) + (xpos - (H_OVERSCAN+H_BORDER) >> 6))}};
            else
                {R_out, G_out, B_out} <= {3{8'((xpos - (H_OVERSCAN+H_BORDER)) >> 1)}};
        end

        DE_out <= (h_cnt >= X_START && h_cnt < X_START + H_ACTIVE && v_cnt >= Y_START && v_cnt < Y_START + V_ACTIVE);
    end
end

endmodule
