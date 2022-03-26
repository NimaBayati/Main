// This file contains ALU control logic.

package dinocpu.components

import chisel3._
import chisel3.util._

/**
 * The ALU control unit
 *
 * Input: aluop         0 for ld/st, 1 for R-type
 * Input: itype         True if I-type (i.e., immediate should be used)
 * Input: funct7        The most significant bits of the instruction
 * Input: funct3        The middle three bits of the instruction (12-14)
 * Input: wordinst      True if the instruction *only* operates on 32-bit operands, False otherwise
 * Output: operation    What we want the ALU to do.
 *
 * For more information, see Section 4.4 and A.5 of Patterson and Hennessy
 * This follows figure 4.12
 */
class ALUControl extends Module {
  val io = IO(new Bundle {
    val aluop     = Input(Bool())
    val itype     = Input(Bool())
    val funct7    = Input(UInt(7.W))
    val funct3    = Input(UInt(3.W))
    val wordinst  = Input(Bool())

    val operation = Output(UInt(5.W))
  })

  // Your code goes here
  when (io.aluop === true.B) {

    val funct7_select = Wire(Bool())
    // ignore funct7 if it's itype
    when (io.itype === true.B) {
      // except when differentiating between srli and srai
      when (io.funct3 === "b101".U) {
        funct7_select := io.funct7(5)
      } .otherwise {
        funct7_select := false.B
      }
    } .otherwise {
      funct7_select := io.funct7(5)
    }

    io.operation := "b11111".U // invalid operation
    
    when (io.funct3 === "b000".U) {
      when (io.wordinst === 0.U) {
        io.operation := Mux(funct7_select, "b00100".U, "b00111".U)
      } .otherwise {
        io.operation := Mux(funct7_select, "b10100".U, "b10111".U)
      }
    } .elsewhen (io.funct3 === "b001".U) {
      io.operation := Mux(io.wordinst, "b11000".U, "b01000".U)
    } .elsewhen (io.funct3 === "b010".U) {
      io.operation := "b01001".U
    } .elsewhen (io.funct3 === "b011".U) {
      io.operation := "b00001".U
    } .elsewhen (io.funct3 === "b100".U) {
      io.operation := "b00000".U
    } .elsewhen (io.funct3 === "b101".U) {
      when (io.wordinst === 0.U) {
        io.operation := Mux(funct7_select, "b00011".U, "b00010".U)
      } .otherwise {
        io.operation := Mux(funct7_select, "b10011".U, "b10010".U)
      }
    } .elsewhen (io.funct3 === "b110".U) {
      io.operation := "b00101".U
    } .elsewhen (io.funct3 === "b111".U) {
      io.operation := "b00110".U
    }
  
  } .otherwise {
    io.operation := "b00111".U
  }
  
}
