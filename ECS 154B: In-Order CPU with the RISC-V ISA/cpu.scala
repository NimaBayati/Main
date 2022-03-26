// This file is where all of the CPU components are assembled into the whole CPU

package dinocpu

import chisel3._
import chisel3.util._
import dinocpu.components._

/**
 * The main CPU definition that hooks up all of the other components.
 *
 * For more information, see section 4.4 of Patterson and Hennessy
 * This follows figure 4.21
 */
class SingleCycleCPU(implicit val conf: CPUConfig) extends BaseCPU {
  // All of the structures required
  val pc         = dontTouch(RegInit(0.U))
  val control    = Module(new Control())
  val registers  = Module(new RegisterFile())
  val aluControl = Module(new ALUControl())
  val alu        = Module(new ALU())
  val immGen     = Module(new ImmediateGenerator())
  val nextpc     = Module(new NextPC())
  val (cycleCount, _) = Counter(true.B, 1 << 30)

  //FETCH
  io.imem.address := pc
  io.imem.valid := true.B

  val instruction = io.imem.instruction

  /* Your code goes here */
  
  // Control Unit
  control.io.opcode := instruction(6,0)
  
  // ALU Control
  aluControl.io.funct7 := instruction(31,25)
  aluControl.io.funct3 := instruction(14,12)
  aluControl.io.wordinst := control.io.wordinst
  aluControl.io.aluop := control.io.aluop
  aluControl.io.itype := control.io.itype

  // Imm Gen
  immGen.io.instruction := instruction

  // RegisterFile
  registers.io.readreg1 := instruction(19,15)
  registers.io.readreg2 := instruction(24,20)
  registers.io.writereg := instruction(11,7)
  
  val wd_mux = Wire(UInt(64.W))
  wd_mux := Mux(control.io.resultselect, immGen.io.sextImm, alu.io.result)
  registers.io.writedata := Mux(control.io.toreg, io.dmem.readdata, wd_mux)
  
  when (registers.io.writereg === 0.U) {
    registers.io.wen := false.B
  } .otherwise {
    registers.io.wen := control.io.regwrite
  }
  
  // ALU
  alu.io.operation := aluControl.io.operation
  alu.io.inputx := Mux(control.io.src1, pc, registers.io.readdata1)
  val inputy_mux = Wire(UInt(64.W))
  inputy_mux := Mux(control.io.src2(0), immGen.io.sextImm, registers.io.readdata2)
  alu.io.inputy := Mux(control.io.src2(1), 4.U, inputy_mux)

  // Next PC Unit
  nextpc.io.pc := pc
  nextpc.io.branch := control.io.branch
  nextpc.io.jumptype := control.io.jumptype
  nextpc.io.inputx := registers.io.readdata1
  nextpc.io.inputy := registers.io.readdata2
  nextpc.io.funct3 := instruction(14,12)
  nextpc.io.imm := immGen.io.sextImm

  pc := nextpc.io.nextpc

  // Data Memory
  io.dmem.address := alu.io.result
  io.dmem.writedata := registers.io.readdata2
  when (control.io.memop === 2.U) {
    io.dmem.memread := 1.U
  } .otherwise {
    io.dmem.memread := 0.U
  }
  when (control.io.memop === 3.U) {
    io.dmem.memwrite := 1.U
  } .otherwise {
    io.dmem.memwrite := 0.U
  }
  io.dmem.valid := control.io.memop(1)
  io.dmem.maskmode := instruction(13,12)
  io.dmem.sext := !instruction(14)

  /*****/
}

/*
 * Object to make it easier to print information about the CPU
 */
object SingleCycleCPUInfo {
  def getModules(): List[String] = {
    List(
      "dmem",
      "imem",
      "control",
      "registers",
      "csr",
      "aluControl",
      "alu",
      "immGen",
      "nextpc"
    )
  }
}
