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

`define STATE_IDLE              2'b00
`define STATE_LEADVERIFY        2'b01
`define STATE_DATARCV           2'b10

module ir_rcv (
    input clk27,
    input reset_n,
    input ir_rx,
    output reg [15:0] ir_code,
    output reg ir_code_ack,
    output reg [7:0] ir_code_cnt
);

// ~37ns clock period

parameter LEADCODE_LO_THOLD     = 200000;  //7.4ms
parameter LEADCODE_HI_THOLD     = 100000;  //3.7ms
parameter LEADCODE_HI_TIMEOUT   = 160000;  //5.9ms
parameter LEADCODE_HI_RPT_THOLD = 54000;   //2.0ms
parameter RPT_RELEASE_THOLD     = 3240000; //120ms
parameter BIT_ONE_THOLD         = 27000;   //1.0ms
parameter BIT_DETECT_THOLD      = 10800;   //0.4ms
parameter IDLE_THOLD            = 141480;  //5.24ms

reg [1:0] state;            // 3 states
reg [31:0] databuf;         // temp. buffer
reg [5:0] bits_detected;    // max. 63, effectively between 0 and 33
reg [17:0] act_cnt;         // max. 9.7ms
reg [17:0] leadvrf_cnt;     // max. 9.7ms
reg [17:0] datarcv_cnt;     // max. 9.7ms
reg [21:0] rpt_cnt;         // max. 155ms

reg ir_rx_r;

// activity when signal is low
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        act_cnt <= 0;
    else
        begin
            if ((state == `STATE_IDLE) & (~ir_rx_r))
                act_cnt <= act_cnt + 1'b1;
            else
                act_cnt <= 0;
        end
end

// lead code verify counter
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        leadvrf_cnt <= 0;
    else
        begin
            if ((state == `STATE_LEADVERIFY) & ir_rx_r)
                leadvrf_cnt <= leadvrf_cnt + 1'b1;
            else
                leadvrf_cnt <= 0;
        end
end


// '0' and '1' are differentiated by high phase duration preceded by constant length low phase
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            datarcv_cnt <= 0;
            bits_detected <= 0;
            databuf <= 0;
        end
    else
        begin
            if (state == `STATE_DATARCV)
                begin
                    if (ir_rx_r)
                        datarcv_cnt <= datarcv_cnt + 1'b1;
                    else
                        datarcv_cnt <= 0;

                    if (datarcv_cnt == BIT_DETECT_THOLD)
                        bits_detected <= bits_detected + 1'b1;
                        
                    if (datarcv_cnt == BIT_ONE_THOLD)
                        databuf[32-bits_detected] <= 1'b1;
                end
            else
                begin
                    datarcv_cnt <= 0;
                    bits_detected <= 0;
                    databuf <= 0;
                end
        end
end

// read and validate data after 32 bits detected (last bit may change to '1' at any time)
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            ir_code_ack <= 1'b0;
            ir_code <= 16'h00000000;
        end
    else
        begin
            if ((bits_detected == 32) & (databuf[31:24] == ~databuf[23:16]) & (databuf[15:8] == ~databuf[7:0]))
                begin
                    ir_code <= {databuf[31:24], databuf[15:8]};
                    ir_code_ack <= 1'b1;
                end
            else if (rpt_cnt >= RPT_RELEASE_THOLD)
                begin
                    ir_code <= 16'h00000000;
                    ir_code_ack <= 1'b0;
                end
            else
                ir_code_ack <= 1'b0;
        end
end

always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n)
        begin
            state <= `STATE_IDLE;
            rpt_cnt <= 0;
            ir_code_cnt <= 0;
            ir_rx_r <= 0;
        end
    else
        begin
            rpt_cnt <= rpt_cnt + 1'b1;
            ir_rx_r <= ir_rx;
        
            case (state)
                `STATE_IDLE:
                    begin
                        if ((act_cnt >= LEADCODE_LO_THOLD) & ir_rx_r)
                            state <= `STATE_LEADVERIFY;
                        if (rpt_cnt >= RPT_RELEASE_THOLD)
                            ir_code_cnt <= 0;
                    end
                `STATE_LEADVERIFY:
                    begin
                        if (leadvrf_cnt == LEADCODE_HI_RPT_THOLD)
                            begin
                                if (ir_code != 0)
                                    ir_code_cnt <= ir_code_cnt + 1;
                                rpt_cnt <= 0;
                            end
                        if (!ir_rx_r)
                            state <= (leadvrf_cnt >= LEADCODE_HI_THOLD) ? `STATE_DATARCV : `STATE_IDLE;
                        else if (leadvrf_cnt >= LEADCODE_HI_TIMEOUT)
                            state <= `STATE_IDLE;
                    end    
                `STATE_DATARCV:
                    begin
                        if (ir_code_ack == 1'b1)
                            ir_code_cnt <= 1;
                        if ((datarcv_cnt >= IDLE_THOLD)|bits_detected >= 33)
                            state <= `STATE_IDLE;
                    end
                default:
                    state <= `STATE_IDLE;
            endcase
        end
end

endmodule
