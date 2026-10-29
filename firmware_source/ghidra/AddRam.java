import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class AddRam extends GhidraScript {
  public void run() throws Exception {
    for (String l: Files.readAllLines(Paths.get("/home/claude/gh/veneers.txt"))) {
      long v=Long.decode(l.trim()); if ((v&1)==0) continue; v-=1;
      if (v>=0x60000000L) continue;
      disassemble(toAddr(v)); createFunction(toAddr(v),null);
    }
    analyzeAll(currentProgram);
  }
}
