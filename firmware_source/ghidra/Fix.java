import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.*;
public class Fix extends GhidraScript {
  public void run() throws Exception {
    for (MemoryBlock b: currentProgram.getMemory().getBlocks()) {
      boolean fl=b.getStart().getOffset()==0x60000000L;
      b.setWrite(!fl); b.setRead(true);
      if (b.getName().equals("PERIPH")||b.getName().equals("SCS")) b.setVolatile(true);
      println(b.getName()+" w="+b.isWrite());
    }
  }
}
