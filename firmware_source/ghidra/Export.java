import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class Export extends GhidraScript {
  public void run() throws Exception {
    DecompInterface di=new DecompInterface(); di.openProgram(currentProgram);
    PrintWriter pw=new PrintWriter(new FileWriter("/home/claude/gh/decomp.c"));
    int n=0;
    for (Function f: currentProgram.getFunctionManager().getFunctions(true)) {
      DecompileResults r=di.decompileFunction(f,60,monitor);
      pw.println("// ==== "+f.getEntryPoint()+" "+f.getName());
      if (r!=null && r.decompileCompleted()) pw.println(r.getDecompiledFunction().getC()); else pw.println("// FAILED");
      n++;
    }
    pw.close(); println("functions: "+n);
  }
}
