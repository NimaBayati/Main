// Logic to calculate the next pc

package dinocpu.components

import chisel3._

/**
 * Next PC unit. This takes various inputs and outputs the next address of the next instruction.
 *
 * Input: branch         True if executing a branch instruction, False otherwise
 * Input: jumptype       00 if not a jump inst, 10 if inst is a jal, 11 if inst is a jalr
 * Input: inputx         First input
 * Input: inputy         Second input
 * Input: funct3         The funct3 from the instruction
 * Input: pc             The *current* program counter for this instruction
 * Input: imm            The sign-extended immediate
 *
 * Output: nextpc        The address of the next instruction
 * Output: taken         True if the next pc is not pc+4
 *
 */
class NextPC extends Module {
  val io = IO(new Bundle {
    val branch   = Input(Bool())
    val jumptype = Input(UInt(2.W))
    val inputx   = Input(UInt(64.W))
    val inputy   = Input(UInt(64.W))
    val funct3   = Input(UInt(3.W))
    val pc       = Input(UInt(64.W))
    val imm      = Input(UInt(64.W))

    val nextpc   = Output(UInt(64.W))
    val taken    = Output(Bool())
  })

  io.nextpc := io.pc + 4.U
  io.taken  := false.B

  // Your code goes here
  // Branch
  when (io.branch === true.B) {
    when (io.funct3 === 0.U) {              //beq
      when (io.inputx.asSInt === io.inputy.asSInt) {
        io.nextpc := io.pc + io.imm
      }
    } .elsewhen (io.funct3 === 1.U) {       //bne
      when (io.inputx.asSInt =/= io.inputy.asSInt) {
        io.nextpc := io.pc + io.imm
      }
    } .elsewhen (io.funct3 === 4.U) {       //blt
      when (io.inputx.asSInt < io.inputy.asSInt) {
        io.nextpc := io.pc + io.imm
      }
    } .elsewhen (io.funct3 === 5.U) {       //bge
      when (io.inputx.asSInt >= io.inputy.asSInt) {
        io.nextpc := io.pc + io.imm
      }
    } .elsewhen (io.funct3 === 6.U) {       //bltu
      when (io.inputx.asUInt < io.inputy.asUInt) {
        io.nextpc := io.pc + io.imm
      }
    } .elsewhen (io.funct3 === 7.U) {       //bgeu
      when (io.inputx.asUInt >= io.inputy.asUInt) {
        io.nextpc := io.pc + io.imm
      }
    }
  }

  // Jump
  when (io.jumptype(1) === true.B) {    //jalr              //jal
    io.nextpc := Mux(io.jumptype(0), io.inputx + io.imm, io.pc + io.imm)
  }

  // Update "taken" output
  when (io.nextpc =/= io.pc + 4.U) {
    io.taken := true.B
  }

}
