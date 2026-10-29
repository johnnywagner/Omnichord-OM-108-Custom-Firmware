import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
public class Setup extends GhidraScript {
  byte[] fl(long off,int len) throws Exception { byte[] b=new byte[len]; RandomAccessFile f=new RandomAccessFile("/home/claude/work/orig.bin","r"); f.seek(off); f.readFully(b); f.close(); return b; }
  public void run() throws Exception {
    Memory m=currentProgram.getMemory();
    AddressSpace sp=currentProgram.getAddressFactory().getDefaultAddressSpace();
    m.createInitializedBlock("ITCM", sp.getAddress(0x0), new ByteArrayInputStream(fl(0x3617C,0xA40)), 0xA40, monitor, false);
    m.createInitializedBlock("DTCM_data", sp.getAddress(0x20000000L), new ByteArrayInputStream(fl(0x37E3C,0x9E8)), 0x9E8, monitor, false);
    m.createUninitializedBlock("DTCM_bss", sp.getAddress(0x200009E8L), 0x10000-0x9E8, false);
    m.createInitializedBlock("OCRAM_data", sp.getAddress(0x20200000L), new ByteArrayInputStream(fl(0x36BBC,0x1280)), 0x1280, monitor, false);
    m.createUninitializedBlock("OCRAM_bss", sp.getAddress(0x20201280L), 0x3028+0x1000, false);
    MemoryBlock p=m.createUninitializedBlock("PERIPH", sp.getAddress(0x40000000L), 0x10000000L, false);
    p.setVolatile(true);
    MemoryBlock s=m.createUninitializedBlock("SCS", sp.getAddress(0xE0000000L), 0x100000L, false);
    s.setVolatile(true);
    for (MemoryBlock b: m.getBlocks()) if (b.getStart().getOffset()==0x60000000L) { b.setWrite(false); b.setExecute(true); b.setName("FLASH"); }
    // vector table
    for (int i=1;i<16+160;i++){ long v=getInt(toAddr(0x60002000L+4*i))&0xffffffffL; if(v>=0x60002000L && v<0x60040000L && (v&1)==1){ Address a=toAddr(v-1); disassemble(a); createFunction(a,null);} }
    for (long a=0x0;a<0xA40;){ break; }
  }
}
